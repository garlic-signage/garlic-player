/*
 garlic-player: SMIL Player for Digital Signage
 Copyright (C) 2026 Nikolaos Sagiadinos <garlic@saghiadinos.de>
 This file is part of the GarlicSignage source code

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License, version 3,
 as published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// The bridge between the core and the Java player (NativeCore.java). The core runs in a ThreadedRuntime; its events
// go to Java as JSON (org.json reads them), like the JS player gets plain objects: no pointer into the core reaches
// Java. Colors are converted here with the parser of the sdl player, so Java gets ARGB numbers.
#include "color.hpp"

#include "common/clock.hpp"
#include "common/text.hpp"
#include "common/std_file_system.hpp"
#include "configuration/configuration.hpp"
#include "downloader/curl_transport.hpp"
#include "player_runtime/threaded_runtime.hpp"
#include "smil/document.hpp"

#include <android/log.h>
#include <jni.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <future>
#include <memory>
#include <string>

namespace
{
    using namespace garlic::core;
    using nlohmann::json;

    JavaVM *java_vm = nullptr;

    template <typename... Functions> struct Overloaded : Functions...
    {
        using Functions::operator()...;
    };
    template <typename... Functions> Overloaded(Functions...) -> Overloaded<Functions...>;

    std::string text(JNIEnv *env, jstring value)
    {
        if (value == nullptr)
            return {};
        const char *chars = env->GetStringUTFChars(value, nullptr);
        std::string result(chars != nullptr ? chars : "");
        if (chars != nullptr)
            env->ReleaseStringUTFChars(value, chars);
        return result;
    }

    jstring javaString(JNIEnv *env, const std::string &value)
    {
        return env->NewStringUTF(value.c_str());
    }

    std::string lower(std::string value)
    {
        std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    /// A SMIL color as ARGB, null if the Qt players drew nothing (player::parseColor of the sdl player)
    json color(const std::string &value)
    {
        auto parsed = player::parseColor(value);
        if (!parsed)
            return nullptr;
        return static_cast<std::int64_t>((static_cast<std::uint32_t>(parsed->alpha) << 24U) |
                                         (static_cast<std::uint32_t>(parsed->red) << 16U) |
                                         (static_cast<std::uint32_t>(parsed->green) << 8U) |
                                         static_cast<std::uint32_t>(parsed->blue));
    }

    std::string kindName(smil::MediaKind kind)
    {
        switch (kind)
        {
            case smil::MediaKind::Image: return "image";
            case smil::MediaKind::Video: return "video";
            case smil::MediaKind::Audio: return "audio";
            case smil::MediaKind::Web:
            case smil::MediaKind::Widget: return "web";
            case smil::MediaKind::Brush: return "brush";
            case smil::MediaKind::Command: return "command";
            case smil::MediaKind::Prefetch: return {};
        }
        return {};
    }

    /// soundLevel "50%" → 0.5, else 1 (like the other players)
    double volumeOf(const std::string &sound_level)
    {
        if (sound_level.size() < 2 || sound_level.back() != '%')
            return 1;
        double percent = 0;
        auto   parsed  = common::fromChars(sound_level.data(), sound_level.data() + sound_level.size() - 1, percent);
        if (parsed.ec != std::errc{})
            return 1;
        return std::clamp(percent / 100, 0.0, 1.0);
    }

    json optionalNumber(const std::optional<std::uint64_t> &value)
    {
        return value ? json(*value) : json(nullptr);
    }

    /// Calls the core in its thread and waits for the answer (the dialogs ask for settings and information)
    template <typename T> T ask(ThreadedRuntime &runtime, std::function<T(PlayerRuntimeApi &)> call)
    {
        auto promise = std::make_shared<std::promise<T>>();
        auto answer  = promise->get_future();
        runtime.post([promise, call = std::move(call)](PlayerRuntimeApi &api) { promise->set_value(call(api)); });
        return answer.get();
    }

    PlayerRuntimeConfig supervisedBy(PlayerRuntimeConfig config, Supervisor *supervisor)
    {
        config.supervisor = supervisor;
        return config;
    }

    /**
     * One player: file system, clock, transport and the core in its thread; the layout for the start events. With a
     * launcher the supervisor of the core: restarts and installations go to NativeCore (onReboot, onInstall), which
     * passes them to the launcher in the UI thread.
     */
    class Bridge final : public Supervisor
    {
        public:
            Bridge(jobject owner, PlayerRuntimeConfig config, downloader::CurlTransportConfig transport_config,
                   bool supervised)
                : java(owner), http(std::move(transport_config)),
                  runtime(fs, clock, http, supervisedBy(std::move(config), supervised ? this : nullptr),
                          [this] { callJava("onWake"); })
            {
            }

            // Supervisor, in the core thread
            void reboot(const std::string &task_id) override { callJava("onReboot", &task_id); }
            void installSoftware(const std::string &path) override { callJava("onInstall", &path); }

            ~Bridge()
            {
                runtime.stop();
                JNIEnv *env = nullptr;
                if (java_vm != nullptr && java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK)
                    env->DeleteGlobalRef(java);
            }

            ThreadedRuntime &core() { return runtime; }

            std::string start()
            {
                auto started = runtime.start();
                return started ? std::string() : started.error();
            }

            /// The events since the last call as a JSON array (in the UI thread)
            std::string takeEvents()
            {
                json events = json::array();
                for (const CoreEvent &event : runtime.takeEvents())
                {
                    json converted = toJson(event);
                    if (!converted.is_null())
                        events.push_back(std::move(converted));
                }
                return events.dump();
            }

        private:
            jobject                 java;
            common::StdFileSystem   fs;
            common::SystemClock     clock;
            downloader::CurlTransport http;
            ThreadedRuntime         runtime;
            NewLayout               layout; // the document is kept until the next one (UI thread only)

            /// In the core thread: a method of NativeCore without or with one string, which posts the work to the
            /// UI thread (onWake, onReboot, onInstall)
            void callJava(const char *name, const std::string *argument = nullptr)
            {
                if (java_vm == nullptr)
                    return;
                JNIEnv *env      = nullptr;
                bool    attached = false;
                if (java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
                {
                    if (java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
                        return;
                    attached = true;
                }
                jclass    type   = env->GetObjectClass(java);
                jmethodID method = env->GetMethodID(type, name, argument != nullptr ? "(Ljava/lang/String;)V" : "()V");
                if (method != nullptr && argument != nullptr)
                {
                    jstring value = javaString(env, *argument);
                    env->CallVoidMethod(java, method, value);
                    env->DeleteLocalRef(value);
                }
                else if (method != nullptr)
                    env->CallVoidMethod(java, method);
                env->DeleteLocalRef(type);
                if (env->ExceptionCheck())
                    env->ExceptionClear();
                if (attached)
                    java_vm->DetachCurrentThread();
            }

            static json command(const char *type, smil::InstanceId id) { return {{"type", type}, {"id", id}}; }

            std::string background(const std::string &src) const
            {
                auto found = layout.backgrounds.find(src);
                return found == layout.backgrounds.end() ? std::string() : found->second;
            }

            json toJson(const CoreEvent &event)
            {
                return std::visit(
                    Overloaded{
                        [this](const smil::StartMedia &e) { return startMedia(e); },
                        [](const smil::StopMedia &e) { return command("stop", e.id); },
                        [](const smil::RepeatMedia &e) { return command("repeat", e.id); },
                        [](const smil::PauseMedia &e) { return command("pause", e.id); },
                        [](const smil::ResumeMedia &e) { return command("resume", e.id); },
                        [this](const NewLayout &e) { return newLayout(e); },
                        [](const BackgroundImage &e)
                        { return json{{"type", "background"}, {"src", e.src}, {"path", e.path}}; },
                        [](const tasks::ClearCaches &e)
                        { return json{{"type", "clearCaches"}, {"player", e.player}, {"web", e.web}}; },
                        [](const Screenshot &e) { return json{{"type", "screenshot"}, {"path", e.path}}; },
                        [](const DeviceSettings &e)
                        {
                            return json{{"type", "deviceSettings"},
                                        {"volume", e.volume ? json(*e.volume) : json(nullptr)},
                                        {"brightness", e.brightness ? json(*e.brightness) : json(nullptr)}};
                        },
                        [](const Downloads &e)
                        {
                            return json{{"type", "downloads"},       {"done", e.done},
                                        {"total", e.total},          {"bytesDone", e.bytes_done},
                                        {"bytesTotal", e.bytes_total}};
                        },
                    },
                    event);
            }

            json newLayout(const NewLayout &next)
            {
                layout                 = next;
                const smil::Head &head = layout.document->head;
                json              regions = json::array();
                for (const smil::Region &region : head.regions)
                    regions.push_back({{"name", region.region_name.empty() ? region.id : region.region_name},
                                       {"left", region.left},
                                       {"top", region.top},
                                       {"width", region.width},
                                       {"height", region.height},
                                       {"z", region.z_index},
                                       {"color", color(region.background_color)},
                                       {"imageSrc", region.background_image},
                                       {"image", background(region.background_image)},
                                       {"repeat", lower(region.background_repeat)}});
                return {{"type", "layout"},
                        {"title", head.title},
                        {"rootWidth", head.root_layout.width},
                        {"rootHeight", head.root_layout.height},
                        {"rootColor", color(head.root_layout.background_color)},
                        {"rootImageSrc", head.root_layout.background_image},
                        {"rootImage", background(head.root_layout.background_image)},
                        {"rootRepeat", lower(head.root_layout.background_repeat)},
                        {"regions", regions}};
            }

            /// fit, mediaAlign and soundLevel of the medium, else of the region: per region of the instance
            json startMedia(const smil::StartMedia &command)
            {
                const smil::Media &media = *command.media;
                std::string        kind  = kindName(media.kind);
                if (kind.empty() || !layout.document)
                    return nullptr;
                const std::vector<smil::Region> &definitions = layout.document->head.regions;
                json                             regions     = json::array();
                for (std::size_t index : command.regions)
                {
                    if (index >= definitions.size())
                        continue;
                    const smil::Region &region = definitions[index];
                    regions.push_back(
                        {{"index", index},
                         {"fit", lower(media.fit.empty() ? region.fit : media.fit)},
                         {"align", lower(media.media_align.empty() ? region.media_align : media.media_align)},
                         {"volume", volumeOf(media.sound_level.empty() ? region.sound_level : media.sound_level)}});
                }
                return {{"type", "start"},
                        {"id", command.id},
                        {"kind", kind},
                        {"source", command.source},
                        {"color", color(media.color)},
                        {"label", media.title.empty() ? media.src : media.title},
                        {"regions", regions}};
            }
    };

    Bridge *bridge(jlong handle)
    {
        return reinterpret_cast<Bridge *>(handle);
    }

    template <typename Call> void post(jlong handle, Call call)
    {
        if (handle != 0)
            bridge(handle)->core().post(std::move(call));
    }
}

extern "C"
{
    JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *)
    {
        java_vm = vm;
        return JNI_VERSION_1_6;
    }

    /// paths: ini, cache, logs; index may be empty (the one of the ini), the content URL of the garlic-launcher wins;
    /// uuid of the launcher, taken only if the ini has none; launcher: "philips-FB01.09" for the user agent, empty
    /// without one; caBundle: PEM file of the system CAs; supervised: with a launcher, which restarts and installs
    JNIEXPORT jlong JNICALL Java_com_sagiadinos_garlic_player_NativeCore_create(JNIEnv *env, jobject self,
                                                                               jstring ini, jstring cache, jstring logs,
                                                                               jstring index, jstring uuid,
                                                                               jstring launcher,
                                                                               jstring ca_bundle, jint seed,
                                                                               jboolean supervised)
    {
        PlayerRuntimeConfig config;
        config.ini_path    = text(env, ini);
        config.cache_dir   = text(env, cache);
        config.log_dir     = text(env, logs);
        config.version     = PLAYER_VERSION;
        config.index_uri   = text(env, index);
        config.uuid        = text(env, uuid);
        config.launcher    = text(env, launcher);
        config.random_seed = static_cast<std::uint32_t>(seed);
        downloader::CurlTransportConfig transport;
        transport.ca_bundle = text(env, ca_bundle);
        return reinterpret_cast<jlong>(
            new Bridge(env->NewGlobalRef(self), std::move(config), std::move(transport), supervised == JNI_TRUE));
    }

    /// Empty, or the error text of the start
    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_start(JNIEnv *env, jclass, jlong handle)
    {
        return javaString(env, bridge(handle)->start());
    }

    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_takeEvents(JNIEnv *env, jclass,
                                                                                     jlong handle)
    {
        return javaString(env, bridge(handle)->takeEvents());
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_destroy(JNIEnv *, jclass, jlong handle)
    {
        delete bridge(handle);
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_mediaFinished(JNIEnv *, jclass, jlong handle,
                                                                                     jlong id)
    {
        post(handle, [id](PlayerRuntimeApi &api) { api.mediaFinished(static_cast<smil::InstanceId>(id)); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_mediaFailed(JNIEnv *, jclass, jlong handle,
                                                                                   jlong id)
    {
        post(handle, [id](PlayerRuntimeApi &api) { api.mediaFailed(static_cast<smil::InstanceId>(id)); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_activated(JNIEnv *, jclass, jlong handle,
                                                                                 jlong id)
    {
        post(handle, [id](PlayerRuntimeApi &api) { api.elementActivated(static_cast<smil::InstanceId>(id)); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_accessKey(JNIEnv *, jclass, jlong handle,
                                                                                 jchar key)
    {
        char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
        post(handle, [lowered](PlayerRuntimeApi &api) { api.accessKey(lowered); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_screenshotTaken(JNIEnv *env, jclass,
                                                                                       jlong handle, jstring path)
    {
        post(handle, [file = text(env, path)](PlayerRuntimeApi &api) { api.screenshotTaken(file); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_cachesCleared(JNIEnv *, jclass, jlong handle)
    {
        post(handle, [](PlayerRuntimeApi &api) { api.cachesCleared(); });
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_playIndex(JNIEnv *env, jclass, jlong handle,
                                                                                 jstring uri)
    {
        post(handle,
             [given = text(env, uri)](PlayerRuntimeApi &api)
             {
                 auto valid = configuration::Configuration::validateIndexUri(given);
                 if (!valid)
                 {
                     __android_log_print(ANDROID_LOG_WARN, "garlic-player", "index not taken: %s", valid.error().c_str());
                     return;
                 }
                 PlayerSettings next = api.playerSettings();
                 next.index_uri      = *valid;
                 if (auto changed = api.changePlayerSettings(next); !changed)
                     __android_log_print(ANDROID_LOG_WARN, "garlic-player", "index not taken: %s", changed.error().c_str());
             });
    }

    JNIEXPORT jboolean JNICALL Java_com_sagiadinos_garlic_player_NativeCore_applyConfiguration(JNIEnv *env, jclass,
                                                                                              jlong handle,
                                                                                              jstring path)
    {
        std::string file = text(env, path);
        return ask<bool>(bridge(handle)->core(), [file](PlayerRuntimeApi &api) { return api.applyConfigurationFile(file); })
                   ? JNI_TRUE
                   : JNI_FALSE;
    }

    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_settings(JNIEnv *env, jclass, jlong handle)
    {
        PlayerSettings current =
            ask<PlayerSettings>(bridge(handle)->core(), [](PlayerRuntimeApi &api) { return api.playerSettings(); });
        json result{{"playerName", current.player_name},
                    {"indexUri", current.index_uri},
                    {"httpdEnabled", current.httpd_enabled},
                    {"httpdPort", current.httpd_port}};
        return javaString(env, result.dump());
    }

    /// Empty, or the error text of the core
    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_changeSettings(JNIEnv *env, jclass,
                                                                                        jlong handle, jstring values)
    {
        json           given = json::parse(text(env, values), nullptr, false);
        PlayerSettings next{given.value("playerName", ""), given.value("indexUri", ""),
                            given.value("httpdEnabled", true),
                            static_cast<std::uint16_t>(given.value("httpdPort", 8080))};
        std::string error = ask<std::string>(bridge(handle)->core(),
                                             [next](PlayerRuntimeApi &api)
                                             {
                                                 auto changed = api.changePlayerSettings(next);
                                                 return changed ? std::string() : changed.error();
                                             });
        return javaString(env, error);
    }

    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_debugInfo(JNIEnv *env, jclass, jlong handle)
    {
        DebugInfo info = ask<DebugInfo>(bridge(handle)->core(), [](PlayerRuntimeApi &api) { return api.debugInfo(); });
        json      downloads = json::array();
        for (const auto &download : info.downloads)
            downloads.push_back({{"name", download.name}, {"received", download.received}, {"total", download.total}});
        json result{{"title", info.index_title},
                    {"userAgent", info.user_agent},
                    {"version", info.version},
                    {"diskFree", optionalNumber(info.disk_free)},
                    {"diskTotal", optionalNumber(info.disk_total)},
                    {"memoryFree", optionalNumber(info.memory_free)},
                    {"memoryTotal", optionalNumber(info.memory_total)},
                    {"memoryUsed", info.memory_used},
                    {"memoryMax", info.memory_max},
                    {"memoryMaxTime", info.memory_max_time},
                    {"threads", info.threads},
                    {"threadsMax", info.threads_max},
                    {"cpu", info.cpu ? json(*info.cpu) : json(nullptr)},
                    {"cpuMax", info.cpu_max ? json(*info.cpu_max) : json(nullptr)},
                    {"cpuMaxTime", info.cpu_max_time},
                    {"cpuSystem", info.cpu_system ? json(*info.cpu_system) : json(nullptr)},
                    {"temperatureCpu", info.temperature_cpu ? json(*info.temperature_cpu) : json(nullptr)},
                    {"temperatureRam", info.temperature_ram ? json(*info.temperature_ram) : json(nullptr)},
                    {"temperatureSsd", info.temperature_ssd ? json(*info.temperature_ssd) : json(nullptr)},
                    {"throttling", info.throttling ? json(*info.throttling) : json(nullptr)},
                    {"downloads", downloads},
                    {"downloadsWaiting", info.downloads_waiting}};
        return javaString(env, result.dump());
    }

    /// standby_mode of the configuration (adapi:blankScreen): partially, deep, no_standby
    JNIEXPORT jstring JNICALL Java_com_sagiadinos_garlic_player_NativeCore_standbyMode(JNIEnv *env, jclass, jlong handle)
    {
        return javaString(env, ask<std::string>(bridge(handle)->core(),
                                                [](PlayerRuntimeApi &api) { return api.configuration().standbyMode(); }));
    }

    /// The remaining time of an instance in seconds, -1 if unknown (the deep standby of launcher 1.x)
    JNIEXPORT jlong JNICALL Java_com_sagiadinos_garlic_player_NativeCore_remainingTime(JNIEnv *, jclass, jlong handle,
                                                                                      jlong id)
    {
        auto remaining = ask<std::optional<std::chrono::seconds>>(
            bridge(handle)->core(),
            [id](PlayerRuntimeApi &api) { return api.remainingTime(static_cast<smil::InstanceId>(id)); });
        return remaining ? static_cast<jlong>(remaining->count()) : -1;
    }

    JNIEXPORT void JNICALL Java_com_sagiadinos_garlic_player_NativeCore_stopPlayback(JNIEnv *, jclass, jlong handle)
    {
        post(handle, [](PlayerRuntimeApi &api) { api.stop(); });
    }
}
