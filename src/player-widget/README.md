# player-widget

The Qt Widgets frontend of the garlic-player on the core. It only shows: timing, downloads, play log, reports, tasks
and the REST API are in the core (`PlayerRuntimeApi`), which runs in a thread of its own (`ThreadedRuntime`).
garlic-lib and the old frontend are in `../../legacy` for reference.

Qt 6.5 or newer, Widgets and WebEngine. The executable is called `garlic-player`.

```
cmake -S . -B build && cmake --build build            # in the repository: core and player-widget together
build/src/player-widget/garlic-player -m fullscreen http://hub/index.smil
```

## Command line

| Option                  | Meaning                                                                 |
|-------------------------|-------------------------------------------------------------------------|
| `SMIL_INDEX`            | URI or path of the index; empty: the one in the settings                |
| `-m`, `--windows-mode`  | `fullscreen`, `bigscreen` (one window over all screens) or `windowed` (default) |
| `-z`, `--windows-size`  | size in windowed mode, default `980x540`                                |
| `-s`, `--screen-select` | number of the screen to start on                                        |
| `--media-backend`       | `qtavplayer` (default), `qtmultimedia` or `vlc`; only the ones built in |
| `-c`, `--config`        | a `config.xml` (e.g. of a launcher), applied by the core at every start before the index; an index on the command line still wins |

Keys: Ctrl+F full screen on/off, Ctrl+B over all screens, Ctrl+S settings, Ctrl+D debug infos, Ctrl+Q quit; other
keys go to the core as accessKey. The debug infos also open after six fast clicks or touches (less than 500 ms apart)
or one held for 10 s, anywhere in the window (garlic-lib).

Without an index the settings dialog opens at the start; cancelled, the player ends (garlic-lib).

Settings, cache and logs are where garlic-lib had them, so an installed player keeps them: the ini of
`QSettings(IniFormat, UserScope, "SmilControl", "garlic-player")` (Linux: `~/.config/SmilControl/garlic-player.ini`),
the cache in `CacheLocation` (Windows: `AppLocalDataLocation/cache`), the logs in `AppLocalDataLocation/logs`.

## Files

| File                          | Content                                                                 |
|-------------------------------|-------------------------------------------------------------------------|
| `main.cpp`                    | command line, PlayerRuntimeConfig, ThreadedRuntime, window mode         |
| `main_window.hpp/.cpp`        | takes the events of the core and hands them on; keys, screenshot        |
| `settings_dialog.hpp/.cpp`    | player name, content URL, HTTP server on/off and port                   |
| `debug_dialog.hpp/.cpp`       | resources from the core and the media that play now, every second       |
| `region.hpp/.cpp`             | one `<region>`: geometry 0..1 of the window, background, stacked media  |
| `media.hpp/.cpp`              | `Media` base class, factory by `MediaKind`, brush, command              |
| `image.hpp/.cpp`              | images with fit and mediaAlign                                          |
| `web.hpp/.cpp`                | text/HTML and widgets in a `QWebEngineView`                             |
| `av_media.hpp/.cpp`           | video and audio with a `MediaPlayer`                                    |
| `media_player.hpp/.cpp`       | `MediaPlayer` interface, backend names, `createMediaPlayer()`           |
| `media_player_qtav.*`         | QtAVPlayer of valbok (FFmpeg)                                           |
| `media_player_qtmultimedia.*` | Qt Multimedia                                                           |
| `media_player_vlc.*`          | libvlc                                                                  |
| `qtavplayer/CMakeLists.txt`   | QtAVPlayer downloaded (fixed tag) and built as static library           |

## Flow

```
core thread                                   UI thread
ThreadedRuntime: poll() → events, wake()  →   MainWindow::takeEvents()
                                                ├── NewLayout        regions anew (sorted by z-index), document kept
                                                ├── BackgroundImage  background of the root layout / regions
                                                ├── StartMedia       a Media per region of the instance, start()
                                                ├── Stop/Pause/Resume/RepeatMedia → the Media of the instance
                                                ├── Screenshot       screen as JPEG (quality 60) → screenshotTaken
                                                ├── ClearCaches      web cache cleared, the player restarts itself
                                                └── Reboot, InstallSoftware: only logged, no launcher yet
post(mediaFinished / mediaFailed /       ←    end of a video or audio, a file that cannot be played,
     elementActivated / accessKey)              a click on a region, a key
```

- **Instances**: a StartMedia shows the medium in all regions with its name (`StartMedia::regions`); only the first
  one reports end and errors, the core expects one per instance.
- **Order of destruction**: the media go before the regions (their widgets lie in the regions), the ThreadedRuntime
  stops before the window goes.
- **QT_NO_KEYWORDS**: the core has members called `slots`; the frontend uses `Q_SIGNALS`/`Q_EMIT`.

## Settings dialog

`SettingsDialog` (Ctrl+S): player name, content URL, HTTP server active and port. The values come from the core
(`playerSettings`), OK hands them to `changePlayerSettings`; the dialog closes only when the core has stored them,
otherwise it shows the error text of the core (invalid URL, port in use). Both calls wait for the core thread
(`post` with a `std::promise`). A QML frontend gets a dialog of its own on the same two calls.

## Debug dialog

`DebugDialog` refreshes every second: playlist title, user agent, disk and memory of the system, memory and threads of
the player with their maxima (all from `debugInfo()` of the core, waited for with `post` and a `std::promise`) and the
media that play now, one line per region with title or src (from the MainWindow; garlic-lib had the code for it but
did not show it). Buttons: quit the player, close. Clicks and touches are counted by an event filter on the
application, so web pages and videos count too; an event handed on to the parent widgets counts once (by its
timestamp). garlic-lib labelled free and total disk space the wrong way round.

## Media

| Kind          | Class     | Behaviour                                                                       |
|---------------|-----------|---------------------------------------------------------------------------------|
| image         | `Image`   | read from the data (a png named jpg works); fit `fill`, `meet`, `meetBest`, `slice`, else original size; mediaAlign |
| video         | `AvMedia` | fit `fill` / `slice`, else aspect ratio kept; soundLevel; end → `mediaFinished` |
| audio         | `AvMedia` | without widget; soundLevel                                                      |
| text, widget  | `Web`     | `QWebEngineView` with the settings of garlic-lib (local files, remote data, autoplay); scroll bars hidden |
| brush         | `Brush`   | area in `color`                                                                 |
| `adapi:…`     | `Command` | `adapi:blankScreen` does nothing without launcher (garlic-lib the same); others fail |

fit, mediaAlign and soundLevel come from the medium, otherwise from its region. An image with a URL as source (not
cacheable) fails like in garlic-lib.

## Media backends

`MediaPlayer` hides the library: `load`, `play`, `pause`, `resume`, `stop`, `restart` (repeatCount), `setVolume`,
`setAspectRatioMode`, signals `finished` and `failed`. The implementations make the libraries equal for the core:
exactly one `finished` per run, nothing after `stop()`.

| Backend        | CMake option              | Notes                                                                |
|----------------|---------------------------|----------------------------------------------------------------------|
| `qtavplayer`   | `PLAYER_WITH_QTAVPLAYER`  | default; FFmpeg, frames to `QVideoWidget`, sound to `QAVAudioOutput`; downloaded by FetchContent (tag `v2026-10-04`) |
| `qtmultimedia` | `PLAYER_WITH_QTMULTIMEDIA`| `QMediaPlayer` (FFmpeg backend of Qt)                                 |
| `vlc`          | `PLAYER_WITH_VLC`         | draws into a native window: on Linux X11 (`QT_QPA_PLATFORM=xcb`), not Wayland; one libvlc instance for all; no fit (like garlic-lib); events of libvlc are handed to the UI thread |

At least one has to be built; `--media-backend` with one that is not built in ends with the list of the available.

## Not yet

- Launcher/daemon: reboot, software installation, screen off (`adapi:blankScreen`).
