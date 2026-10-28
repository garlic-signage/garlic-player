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
#include "tasks/task_scheduler.hpp"

#include "common/md5.hpp"
#include "common/time_format.hpp"

#include <algorithm>

namespace garlic::core::tasks
{
    namespace
    {
        /// The ini key of garlic-lib with the last done task id of a kind.
        std::string doneKey(Task::Kind kind)
        {
            switch (kind)
            {
                case Task::Kind::UpdateSettings: return "update_settings_task_id";
                case Task::Kind::FirmwareUpdate: return "firmware_update_task_id";
                case Task::Kind::ShutdownPlayer: return "shutdown_player_task_id";
                case Task::Kind::ApplyCommand: return "apply_command_task_id";
            }
            return "task_id";
        }

        std::vector<std::string> splitList(std::string_view list)
        {
            std::vector<std::string> items;
            std::size_t              start = 0;
            while (start < list.size())
            {
                auto end = list.find(',', start);
                if (end == std::string_view::npos)
                    end = list.size();
                if (end > start)
                    items.emplace_back(list.substr(start, end - start));
                start = end + 1;
            }
            return items;
        }

        std::string lower(std::string text)
        {
            for (char &c : text)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            return text;
        }

        constexpr std::size_t kept_ids = 20; // done ids remembered per kind
    }

    TaskScheduler::TaskScheduler(downloader::DownloadQueue &q, common::FileSystem &f, logger::Logger &log,
                                 configuration::Configuration &cfg, std::string cache, OnAction action)
        : queue(q), fs(f), logger(log), config(cfg), cache_dir(std::move(cache)), on_action(std::move(action))
    {
    }

    // ------------------------------------------------------------------ restart

    /**
     * A task that handed a restart or an installation to the host left its id as pending. The player runs again, so
     * the restart worked; after a firmware update the running version tells whether the installation worked.
     */
    void TaskScheduler::checkPending()
    {
        std::string id = config.value("tasks/pending_id");
        if (id.empty())
            return;
        std::string version = config.value("tasks/pending_version");
        bool        ok      = version.empty() || version == config.version();
        logger.taskState(id, ok ? "completed" : "aborted");
        config.setValue("tasks/pending_id", "");
        config.setValue("tasks/pending_version", "");
    }

    void TaskScheduler::waitForRestart(const Task &task)
    {
        config.setValue("tasks/pending_id", task.id);
        config.setValue("tasks/pending_version", task.kind == Task::Kind::FirmwareUpdate ? task.version : "");
        logger.taskState(task.id, "pending");
    }

    // ------------------------------------------------------------------ task list

    void TaskScheduler::subscribe(const std::vector<smil::Subscription> &subscriptions)
    {
        auto found = std::ranges::find(subscriptions, std::string_view("TaskSchedule"), &smil::Subscription::type);
        if (found == subscriptions.end() || found->action.empty())
        {
            list_url.clear();
            return;
        }
        list_url = found->action;
        queue.add(list_url, "task_scheduler.xml"); // with every new index, no refresh of its own
    }

    bool TaskScheduler::downloaded(const downloader::DownloadResult &result)
    {
        if (!list_url.empty() && result.url == list_url)
        {
            std::string path = takeFile(result);
            if (path.empty())
                return true;
            auto text = fs.readFile(path);
            if (!text)
            {
                logger.event(logger::Severity::Error, "TaskScheduler", "TASK_SCHEDULER_OPEN_ERROR", metadata(path));
                return true;
            }
            auto tasks = parseTaskList(*text);
            if (!tasks)
            {
                auto data = metadata(path);
                data.emplace_back("errorMessage", tasks.error().message);
                data.emplace_back("errorLine", std::to_string(tasks.error().line));
                logger.event(logger::Severity::Error, "TaskScheduler", "TASK_SCHEDULER_XML_ERROR", data);
                return true;
            }
            logger.event(logger::Severity::Informational, "TaskScheduler", "TASK_SCHEDULER_LOADED", metadata(path));
            run(*tasks);
            return true;
        }

        auto task = loading.find(result.url);
        if (task == loading.end())
            return false;
        Task copy = task->second;
        loading.erase(task);
        finish(copy, result);
        return true;
    }

    /// garlic-lib: TaskScheduler::doSucceed - in the order of the list; done tasks are skipped.
    void TaskScheduler::run(const std::vector<Task> &tasks)
    {
        for (const Task &task : tasks)
        {
            if (task.id.empty() || isDone(task))
                continue;
            switch (task.kind)
            {
                case Task::Kind::UpdateSettings:
                    loading[task.source_uri] = task;
                    queue.add(task.source_uri, "configuration.xml");
                    break;
                case Task::Kind::FirmwareUpdate:
                    loading[task.source_uri] = task;
                    queue.add(task.source_uri, firmwareName());
                    break;
                case Task::Kind::ShutdownPlayer:
                    markDone(task); // before the restart, otherwise it would restart again and again
                    waitForRestart(task);
                    on_action(Reboot{task.id});
                    break;
                case Task::Kind::ApplyCommand:
                {
                    bool all = task.command == "clear_all_caches";
                    markDone(task);
                    waitForRestart(task);
                    on_action(ClearCaches{task.id, all || task.command == "clear_playercache" || task.command == "clear_cache",
                                          all || task.command == "clear_webcache"});
                    break;
                }
            }
        }
    }

    /// The file of updateSettings or firmwareUpdate has arrived: check, then apply or hand over.
    void TaskScheduler::finish(const Task &task, const downloader::DownloadResult &result)
    {
        std::string path = takeFile(result);
        auto        data = path.empty() ? std::nullopt : readChecked(task, path);
        if (!data)
        {
            logger.taskState(task.id, "aborted"); // not done: tried again with the next index
            return;
        }

        if (task.kind == Task::Kind::UpdateSettings)
        {
            if (!applySettings(*data, path))
            {
                logger.taskState(task.id, "aborted");
                return;
            }
            markDone(task);
            logger.taskState(task.id, "completed");
            on_action(ApplyConfiguration{});
            return;
        }
        markDone(task);
        waitForRestart(task);
        on_action(InstallSoftware{task.id, path});
    }

    // ------------------------------------------------------------------ settings

    bool TaskScheduler::applyConfigurationFile(std::string_view path)
    {
        auto text = fs.readFile(path);
        if (!text)
        {
            logger.event(logger::Severity::Error, "TaskScheduler", "CONFIGURATION_OPEN_ERROR", metadata(std::string(path)));
            return false;
        }
        return applySettings(*text, std::string(path));
    }

    /// garlic-lib: XMLConfiguration::parse - the same props and ini keys.
    bool TaskScheduler::applySettings(std::string_view xml, const std::string &path)
    {
        auto settings = parseSettings(xml);
        if (!settings)
        {
            auto data = metadata(path);
            data.emplace_back("errorMessage", settings.error().message);
            data.emplace_back("errorLine", std::to_string(settings.error().line));
            logger.event(logger::Severity::Error, "TaskScheduler", "CONFIGURATION_XML_ERROR", data);
            return false;
        }
        logger.event(logger::Severity::Informational, "TaskScheduler", "CONFIGURATION_LOADED", metadata(path));

        for (const auto &[name, value] : *settings)
        {
            if (name == "info.playerName")
                config.setPlayerName(value);
            else if (name == "content.serverUrl")
                config.setIndexUri(value);
            else if (name == "display.brightness" || name == "brightness")
                config.setValue("display/brightness", value);
            else if (name == "hardware.videoOut.0.rotation")
                config.setValue("display/rotation", value);
            else if (name == "audio.soundlevel" || name == "volume" || name == "hardware.audioOut.0.masterSoundLevel")
                config.setValue("audio/volume", value);
            else if (name == "time.tzDescription")
                config.setValue("time/tzDescription", value);
            else if (name == "time.tzCode")
                config.setValue("time/tzCode", value);
            else if (name == "time.autoUpdate.protocol")
                config.setValue("time/protocol", value);
            else if (name == "time.autoUpdate.server")
                config.setValue("time/server", value);
            else if (name == "task.scheduledReboot.days" || name == "schedule.reboot.days")
                config.setRebootDays(value);
            else if (name == "task.scheduledReboot.time" || name == "schedule.reboot.time")
                config.setRebootTime(value);
            else if (name == "standby_mode")
                config.setStandbyMode(value);
        }
        return true;
    }

    // ------------------------------------------------------------------ helpers

    /**
     * Done ids per kind as a list; the last one also under the old key of garlic-lib, which is still honoured, so an
     * update of the software runs no task twice. garlic-lib kept only the last id, so of two tasks of the same kind
     * the first ran again with every index.
     */
    bool TaskScheduler::isDone(const Task &task) const
    {
        std::string key = doneKey(task.kind);
        if (config.value(key) == task.id)
            return true;
        auto done = splitList(config.value(key + "s"));
        return std::ranges::find(done, task.id) != done.end();
    }

    void TaskScheduler::markDone(const Task &task)
    {
        std::string key  = doneKey(task.kind);
        auto        done = splitList(config.value(key + "s"));
        done.push_back(task.id);
        if (done.size() > kept_ids)
            done.erase(done.begin(), done.end() - static_cast<std::ptrdiff_t>(kept_ids));
        std::string list;
        for (const auto &id : done)
            list += (list.empty() ? "" : ",") + id;
        config.setValue(key + "s", list);
        config.setValue(key, task.id);
    }

    /**
     * The content if its checksum is right: contentChecksum with checksumMethod MD5 (also without method). An unknown
     * method cannot be checked and counts as wrong. garlic-lib read the checksum but never checked it.
     */
    std::optional<std::string> TaskScheduler::readChecked(const Task &task, const std::string &path)
    {
        auto data = fs.readFile(path);
        if (!data)
            return std::nullopt;
        if (task.content_checksum.empty())
            return std::move(*data);

        std::string method = lower(task.checksum_method);
        if (!method.empty() && method != "md5")
        {
            logger.event(logger::Severity::Error, "TaskScheduler", "CHECKSUM_UNKNOWN_METHOD",
                         {{"taskId", task.id}, {"checksumMethod", task.checksum_method}});
            return std::nullopt;
        }
        if (common::Md5::hex(*data) != lower(task.content_checksum))
        {
            logger.event(logger::Severity::Error, "TaskScheduler", "CHECKSUM_ERROR",
                         {{"taskId", task.id}, {"resourceURI", task.source_uri}});
            return std::nullopt;
        }
        return std::move(*data);
    }

    /// The downloaded file; a new version waiting as ".ready" replaces the old one (garlic-lib: renameDownloadedFile).
    std::string TaskScheduler::takeFile(const downloader::DownloadResult &result)
    {
        if (result.local_path.empty())
            return {};
        std::string ready = result.local_path + ".ready";
        if (fs.exists(ready) && !fs.rename(ready, result.local_path))
            return {};
        return fs.exists(result.local_path) ? result.local_path : std::string();
    }

    /// garlic-lib: FirmwareDownloader - the name the launcher of the platform expects.
    std::string TaskScheduler::firmwareName() const
    {
        std::string_view os = configuration::Configuration::os();
        if (os == "android")
            return "garlic-player.apk";
        if (os == "windows")
            return "garlic-player.exe";
        if (os == "macOS")
            return "garlic-player.dmg";
        if (os == "linux")
            return "garlic-player.tar.xz";
        return "garlic-player.zip";
    }

    logger::Metadata TaskScheduler::metadata(const std::string &path) const
    {
        logger::Metadata data{{"instanceId", "display:0"}, {"resourceURI", path}};
        if (auto size = fs.fileSize(path))
            data.emplace_back("contentLength", std::to_string(*size));
        if (auto modified = fs.lastModified(path))
            data.emplace_back("lastModifiedTime", common::isoLocalTime(*modified));
        return data;
    }
}
