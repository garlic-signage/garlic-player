# qml

The Qt Quick frontend of the garlic-player on the core, for Android (with launcher 1.x and Philips displays, like the
legacy player), meant for iOS (and on the desktop to try it). It behaves like
the [qwidget](../qwidget/README.md) player: the same command line, menu, dialogs and shortcuts. Like there, the core
(`PlayerRuntimeApi`) runs in a thread of its own (`ThreadedRuntime`); the frontend only shows.

Qt 6.5 or newer with Quick, Quick Controls, Multimedia and WebView. The executable is called `garlic-player-qml`.

```
cmake -S . -B build && cmake --build build            # in the repository, with the core and qwidget
build/src/player-ui/qml/garlic-player-qml -m fullscreen http://hub/index.smil
```

`-DGARLIC_PLAYER_QML=OFF` (and `-DGARLIC_PLAYER_QWIDGET=OFF`) in the repository build leave a player out.

## Command line, menu, keys

As in qwidget: `SMIL_INDEX`, `-c/--config`, `-m/--windows-mode`, `-z/--windows-size`, `-s/--screen-select`; there is
no `--media-backend`, video and audio always play with Qt Multimedia (on iOS and Android with the decoders of the
system).

Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s anywhere open the menu with Settings, Debug
infos and Exit, the shortcuts right-aligned. Keys: Ctrl+F full screen, Ctrl+B over all screens, Ctrl+S settings, Ctrl+D
debug infos, Ctrl+Q quit; every other key is an accesskey for the SMIL. Without an index the settings dialog opens at
the start; cancelled, the player ends.

## Files

| File                     | Content                                                                         |
|--------------------------|---------------------------------------------------------------------------------|
| `main.cpp`               | command line, PlayerRuntimeConfig, ThreadedRuntime, QML engine, window mode     |
| `player.hpp/.cpp`        | `Player`: the bridge, QML singleton of the module `Garlic`                      |
| `region_model.hpp/.cpp`  | `RegionModel`: the regions of the layout, each with its `MediaModel`            |
| `media_model.hpp/.cpp`   | `MediaModel`: the media shown in one region                                     |
| `Main.qml`               | window, root background, regions, shortcuts, accesskeys, menu and dialogs       |
| `RegionView.qml`         | one region: geometry 0..1, background color and image, its media                |
| `MediaView.qml`          | one medium: image, video, audio, web, brush                                     |
| `PlayerMenu.qml`         | the menu with the shortcuts                                                     |
| `SettingsDialog.qml`     | player name, content URL, HTTP server on/off and port                           |
| `DebugDialog.qml`        | resources from the core and the media playing now, every second                 |
| `android_launcher.hpp/.cpp` | Android: launcher 1.x or Philips through `GarlicActivity` (supervisor, screen, uuid, content URL), storage, CA bundle |
| `android/`               | Android: manifest, icons, the Java classes (`GarlicActivity`, `GarlicLauncher`, `PhilipsLauncher`, receivers) |

## Flow

C++ holds the state, QML binds to it; C++ never calls into QML.

```
core thread                                   UI thread
ThreadedRuntime: poll() → events, wake()  →   Player::takeEvents()
                                                ├── NewLayout        RegionModel anew, root background, document kept
                                                ├── BackgroundImage  background of the root layout / a region
                                                ├── StartMedia       a row in the MediaModel of each region of the instance
                                                ├── StopMedia        the rows go, QML destroys the delegates
                                                ├── Pause/Resume     role paused (resumed: moved to the top)
                                                ├── RepeatMedia      role generation + 1: QML plays again from the start
                                                ├── Screenshot       screen as JPEG (quality 60) → screenshotTaken
                                                ├── ClearCaches      the player restarts itself
                                                └── adapi:blankScreen: ScreenControl (Android: the launcher)
Player.mediaFinished / mediaFailed /     ←    MediaView: end of a video or audio, a file that cannot be played,
       activated / accessKey                    a click, a key (Main.qml)
```

- **Models instead of calls**: the rows hold plain values (kind, URL, fit, mediaAlign, volume, color), no pointer into
  the core. The old player-c2qml called QML functions from C++.
- **Instances**: a StartMedia is shown in all regions with its name; only the first one (`primary`) reports end and
  errors, the core expects one per instance.
- **Only the topmost medium with a picture is visible** (`onTop`), like the stack of qwidget; audio covers nothing.
- **Menu gesture**: an event filter on the QQuickWindow, through which all input of the scene goes (also over videos),
  counts the clicks and starts the 10 s timer; QML opens the menu on `menuRequested`.
- **Settings and debug infos** come from the core (`playerSettings`, `changePlayerSettings`, `debugInfo`); the bridge
  waits for the core thread for them, the dialogs only show.

## Media

| Kind          | QML                         | Behaviour                                                              |
|---------------|-----------------------------|------------------------------------------------------------------------|
| image         | `Image`                     | fit `fill`, `meet`, `meetBest`, `slice`, else original size; mediaAlign |
| video         | `MediaPlayer`, `VideoOutput`| fit `fill` / `slice`, else aspect ratio kept; soundLevel; end → mediaFinished |
| audio         | `MediaPlayer`               | soundLevel                                                             |
| text, widget  | `WebView` (Qt WebView)      | local files and remote data allowed, scroll bars hidden; paused = hidden |
| brush         | `Rectangle`                 | area in `color`                                                        |

Qt WebView is the system WebView on iOS (WKWebView) and Android, WebEngine on the desktop; there it reports
`setAllowFileAccess() not supported with the WebEngine backend`, which is harmless (the setting is for the mobiles).

## Build notes

- The QML import scan, which links the QML plugins of a static Qt (iOS), is switched off for a shared Qt
  (`NO_IMPORT_SCAN`): Debian's Qt 6.8 scans anyway and warns about every plugin.
- Debian Trixie needs at runtime `qml6-module-qtquick`, `-qtquick-controls`, `-qtquick-layouts`, `-qtquick-templates`,
  `-qtquick-window`, `-qtqml-workerscript`, `-qtmultimedia` and `-qtwebview`.

## Android

The same package as the legacy player, `com.sagiadinos.garlic.player` with the activity
`com.sagiadinos.garlic.player.java.GarlicActivity`: launcher 1.x and the Philips firmware start and update the player
by these names, and the update keeps the ini (uuid, settings). versionCode from `VERSION` (`2.0.0-beta.566` →
20000566), above the legacy 671. Android 9 (API 28) or newer, arm64-v8a and armeabi-v7a (one APK each).

**Launcher.** `GarlicActivity` looks for it before Qt starts: `com.sagiadinos.garlic.launcher` (launcher 1.x), else
`com.tpv.app.tpvlauncher` (Philips), else none. The protocol of launcher 1.x stays as it is (frozen); no heartbeat.

| Player                                   | launcher 1.x                                                      | Philips                     |
|------------------------------------------|-------------------------------------------------------------------|-----------------------------|
| start: uuid, content URL, version        | ContentProvider `SettingsProvider` (`uuid`, `smil_content_url`, `launcher_version`) | SICP: model-serial, firmware |
| `reboot(task_id)` (core)                 | broadcast `CommandReceiver`, `command=reboot`, `task_id`           | `php.intent.action.REBOOT`  |
| `installSoftware(path)` (core)           | broadcast `InstallAppReceiver`, `apk_path`                         | `php.intent.action.UPDATE_APK` |
| `adapi:blankScreen`, `partially`         | `command=screen_off`, at the end `screen_on`                       | SICP backlight off/on       |
| `adapi:blankScreen`, `deep`              | `command=deep_standby`, `seconds_to_wakeup` (rest of dur/end)      | -                           |
| Exit (menu, keys, debug dialog)          | broadcast `PlayerClosedReceiver`: the launcher does not restart it | -                           |
| launcher → `ConfigReceiver`, `config_path` | `applyConfigurationFile()`: applied, index loaded again          |                             |
| launcher → `SmilIndexReceiver`, `smil_index_path` | stored as index URI and played (USB stick)                |                             |

- **uuid** of the launcher only for a new player (no uuid in the ini yet); a stored one stays.
- **Content URL** of the garlic-launcher wins at every start, as in the legacy player; the settings dialog shows name
  and URL but does not change them, and does not open at the start without an index (the launcher sends one). Philips:
  `https://indexes.smil-control.com` only as default when the settings have no index.
- **Broadcasts** name the package of the launcher: its receivers are declared in its manifest, and Android 8+ does not
  deliver implicit broadcasts to those.
- **Storage**: cache and logs in `/sdcard/garlic-player/{cache,logs}` like the legacy player, because launcher 1.x
  installs the downloaded APK from there. targetSdk 29 with `requestLegacyExternalStorage`, so `WRITE_EXTERNAL_STORAGE`
  is enough also on Android 11+. Without the permission (20 s for the dialog at the start) inside the app, with a
  warning: then the launcher cannot install updates.
- **HTTPS**: libcurl gets the CA certificates of the system as one PEM (`ca-bundle.pem`, written at every start from
  `/apex/com.android.conscrypt/cacerts` or `/system/etc/security/cacerts`).
- **Permissions**: only internet, network and Wi-Fi state, storage, wake lock; the ones Qt Multimedia would add
  (camera, microphone, location, bluetooth) are left out.

### Build

Qt 6.8 for Android (arm64-v8a, armv7) and for the host, Android SDK with NDK 26.1, JDK 17, vcpkg (the triplet builds curl,
OpenSSL, SQLite, libxml2 and libmicrohttpd).

```
export ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/26.1.10909125
export ANDROID_NDK_HOME=$ANDROID_NDK_ROOT JAVA_HOME=/usr/lib/jvm/java-17-openjdk VCPKG_ROOT=~/vcpkg
QT=~/Qt/6.8.5
cmake -S . -B build/android-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$QT/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-android -DVCPKG_MANIFEST_DIR=$PWD/src/core \
  -DQT_HOST_PATH=$QT/gcc_64 -DANDROID_PLATFORM=android-28 \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT \
  -DGARLIC_PLAYER_QWIDGET=OFF -DGARLIC_CORE_BUILD_TESTS=OFF
cmake --build build/android-arm64
```

32-bit boxes (armeabi-v7a): the same into `build/android-armv7` with the Qt kit `$QT/android_armv7` and
`-DVCPKG_TARGET_TRIPLET=arm-neon-android`. Both APKs have the same versionCode; a box gets the one of its ABI.

The APK is `build/<dir>/src/player-ui/qml/android-build/build/outputs/apk/release/android-build-release-unsigned.apk`.
It has to be signed with the key of the legacy player, otherwise Android refuses it as update and launcher 1.x cannot
install it over the old one.

## Not yet

- iOS.
- The supervisor protocol of [supervisor_protocol](../../core/docs/supervisor_protocol.md) (new launcher, garlic-daemon,
  garlic-service): until then Android only with launcher 1.x or Philips, the desktop without supervisor.
