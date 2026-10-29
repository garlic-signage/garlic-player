# qml

The Qt Quick frontend of the garlic-player on the core, meant for iOS (and on the desktop to try it). It behaves like
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
                                                └── Reboot, InstallSoftware, adapi:blankScreen: only logged, no launcher
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

## Not yet

- iOS and Android builds (the core is not built for them yet; Android is meant to get a native Java frontend later).
- Launcher/daemon: reboot, software installation, screen off.
