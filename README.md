# garlic-player - SMIL Player for Digital Signage

[![CI](https://github.com/sagiadinos/garlic-player/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/sagiadinos/garlic-player/actions/workflows/ci.yml)
![coverage](https://raw.githubusercontent.com/sagiadinos/garlic-player/badges/coverage.svg)

> This README describes version 2.0 (beta). For version 1.x, the Qt 5 player with garlic-lib and the old launcher,
> see [README-v1.md](README-v1.md).

The garlic-player is a SMIL 3.0 compatible media player that was developed specifically for the requirements of the digital signage industry. 

It is used in numerous projects as well as on various Android hardware solutions.

The garlic-player offers you more independence and security for your investment by being based on the open [SMIL standard](https://garlic-player.com/resources/digital-signage-smil/) standard. It works together with every compatible [digital signage software](https://smil-control.com/software/). This helps also to avoid vendor lock-ins.

[Documentation & Tutorials](https://garlic-player.com/garlic-player/docs/)

[![Raspberry videos](https://img.youtube.com/vi/yNVzrWpZ93U/hqdefault.jpg)](https://youtu.be/yNVzrWpZ93U)

Click on the image to see how a Raspberry Pi 4 playing four videos in zones.

Binaries can be [downloaded](https://garlic-signage.com/garlic-player/downloads/). You will find stable releases and nightly build there.

## Version 2.0 (Beta): the rewrite

Version 2.0 is a rewrite. Everything a player needs besides showing media is now in **garlic-core**
([src/core](src/core/README.md)), a Qt-free library in C++23: download and cache, SMIL parser and timing engine,
reports, tasks, REST API, configuration and logs. It runs in a thread of its own and drives the frontend through one
interface, the `PlayerRuntimeApi`. The frontends only show: regions, images, videos, web pages.

Why:
 - **One core for every platform.** The old garlic-lib was bound to Qt. The new core also fits frontends in Java
   (Android), JavaScript/WASM (browsers, Tizen, webOS) and on an ESP32.
 - **Robust and testable.** The timing engine follows the SMIL standard and is tested with scenario tests; the
   core has unit tests for every module, run in Debian Trixie with every push (see the badges above).
 - **Compatible.** Same settings file, cache, logs, reports, REST API and SMIL features as before, so existing
   installations and CMS keep working. Old bugs were fixed on the way and are documented in the
   [module docs](src/core/docs).

The frontends are in [src/player-ui](src/player-ui), both with Qt 6 and operated the same way:
 - **qwidget** ([src/player-ui/qwidget](src/player-ui/qwidget/README.md)), the desktop player with Qt Widgets. Video
   and audio can be played with QtAVPlayer (default), Qt Multimedia or libvlc.
 - **qml** ([src/player-ui/qml](src/player-ui/qml/README.md)) with Qt Quick, meant for iOS; video and audio with Qt
   Multimedia, web pages with Qt WebView.

The beta runs on Linux. Windows, macOS, Android and iOS follow. The code of version 1.x (Qt 5, garlic-lib and the
old frontends) is kept in [legacy](legacy) for reference; it is not built anymore.

## Build (2.0)

Needs CMake 3.25+, a C++23 compiler (GCC 14 or Clang 18), Qt 6.8+ with Widgets, WebEngine and Multimedia, libvlc,
libcurl, libmicrohttpd, SQLite and libxml2. On Debian Trixie:

```
apt install cmake ninja-build g++ pkg-config libcurl4-openssl-dev libmicrohttpd-dev libsqlite3-dev libxml2-dev \
    nlohmann-json3-dev qt6-base-dev qt6-webengine-dev qt6-multimedia-dev libvlc-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLAYER_WITH_QTAVPLAYER=OFF
cmake --build build
build/src/player-ui/qwidget/garlic-player -m fullscreen https://example.com/index.smil
```

The qml player needs in addition `qt6-declarative-dev qt6-webview-dev` and at runtime the `qml6-module-*` packages
listed in its [README](src/player-ui/qml/README.md); `-DGARLIC_PLAYER_QML=OFF` or `-DGARLIC_PLAYER_QWIDGET=OFF` leave
a player out.

The media backends are switched with `-DPLAYER_WITH_QTAVPLAYER`, `-DPLAYER_WITH_QTMULTIMEDIA` and `-DPLAYER_WITH_VLC`
(all `ON` by default, at least one has to stay). Without `--media-backend` the player takes QtAVPlayer, or the first
one built in.

QtAVPlayer is downloaded by CMake and needs FFmpeg (`libavcodec-dev libavformat-dev libavdevice-dev libavfilter-dev
libswscale-dev libswresample-dev`) and the private headers of Qt Multimedia. Debian Trixie does not ship those, so
there it is switched off as above; Qt from the Qt installer and Arch Linux have them.

## Features
 - full screen on one screen, one window over all screens (bigscreen) or windowed, start screen selectable
 - multiple and overlapping zones e.g. splitscreens
 - logging: debug, event, play and task logs
 - resume downloads
 - HTTP redirects (up to 5) for index and media
 - video and audio with QtAVPlayer, Qt Multimedia or libvlc (qwidget), Qt Multimedia (qml)
 - images in the formats of the Qt image plugins (Jpg, Png, Gif, Svg …)
 - media cache
 - SMIL3 subset, HTML5, HTML5-Widgets and (depending on device) WebGL support
 - WebWidgets with also support parameters
 - Webserver on port 8080 (default) for the REST API

## Operating Systems Supported (2.0)
 - Linux (beta)
 - Windows, macOS, Android and iOS follow

The operating systems and devices of version 1.x are listed in [README-v1.md](README-v1.md).

## Command Line Options

*garlic-player [options] [SMIL_INDEX]*

| Option | Description |
|-------------------------|------------------------------------------------------------------------------|
| `-h, --help` | Displays help |
| `-v, --version` | Displays version information |
| `-c, --config` | Path to a `config.xml`, applied at every start before the index (e.g. by a launcher) |
| `-m, --windows-mode` | `fullscreen`, `bigscreen` (one window over all screens) or `windowed` (default) |
| `-z, --windows-size` | Window size in window mode, e.g. `980x540` (default) |
| `-s, --screen-select` | Number of the screen to start on |
| `--media-backend` | Video and audio with `qtavplayer` (default), `qtmultimedia` or `vlc`; only the ones built in |

Arguments:
SMIL_INDEX        URI or path of the SMIL index; without it the one of the settings is used. Without any index the
                  settings dialog opens.

Another media backend for video and audio, only for this start (it is not stored):

```
garlic-player --media-backend qtmultimedia
garlic-player --media-backend vlc --platform xcb -m windowed https://example.com/index.smil
```

**libvlc needs `--platform xcb`**, especially under Wayland: libvlc draws into an X11 window, and without it a second
window opens for the video. `QT_QPA_PLATFORM=xcb` does the same.

`garlic-player --help` lists the backends built in.

The index of the command line wins over the one in a `config.xml`. Positioning on a screen (`-s`, `bigscreen`) needs X11
too (`--platform xcb`).

## Menu

Ten fast clicks or touches (less than 500 ms apart) or one held for 10 seconds, anywhere on the screen, open a menu:
 - **Settings**: player name, content URL, HTTP server on/off and port
 - **Debug infos**: playlist, user agent, disk, memory, threads and the media playing in each region
 - **Exit**

Holding is meant for interactive content, where fast clicks reach the page too. All players work the same way.

## HotKeys
 - toggle fullscreen with Ctrl-F
 - bigscreen with Ctrl-B
 - open settings dialog with Ctrl-S
 - open debug infos with Ctrl-D
 - quit app with Ctrl-Q
 - every other key is an accesskey for the SMIL

## Currently supported SMIL-Features

### Time container elements
- **seq**
- **par**
- **excl**
- **priorityClass**

#### Attributes for excl/priorityClass time container
- **peers** stop|never|pause|defer
- **higher** stop|pause
- **lower** never|defer

### Media-Elements
- **video**
- **audio**
- **images**
- **brush**
- **html/text**
- **HTML Widgets**

### Other Elements
- **prefetch**
- **meta** for refresh index via http-equiv="Refresh"

#### Attributes for par and excl
- **endsync** first|last|id-value

#### Attributes for media
- **region**
- **fit** fill|meet|meetBest|slice
- **mediaAlign** topLeft|topMid|topRight|midLeft|center|midRight|bottomLeft|bottomMid|bottomRight
- **soundLevel** for audio and video in %

#### Attributes for mbrush
- **color** color in CSS Style

### Attributes for media and container (Timings and Synchronization)
- **xml:id** with Fallback for SMIL 2.0 id
- **title** stub
- **restart** always|never|whenNotActive|default
- **class** stub
- **xml:lang** stub
- **repeatCount** positive integer values and indefinite

#### Begin and End Attributes
- **Clock values** like dur
- **Wallclock** with weekday (-/+) periods and repeats. Date is supported in full ISO 8601 format not short date like 201634W.
- **Events** beginEvent, endEvent, activateEvent with additional clock value like id.endEvent+2s
- **Syncbase values** with additional clock value like id.begin+2s
- **accesskey** with additional clock value like accesskey('w')+2s
- **notify** network trigger via the REST API with additional clock value like notify(door_open)+2s
- **indefinite**
- **value lists** seperated by ";"

#### Timings with Dur
- **indefinite**
- **media**
- **clock values** (full+partial clock),  normal (h min s and ms)

### Layered multizone layouts elements:
- **root-layout**
- **layout**

### Multizone attributes
- **regionName**
- **top** values in px or %
- **left** values in px or %
- **width** values in px or %
- **height** values in px or %
- **z-index** integer values
- **backgroundColor** values are compatible with CSS style
- **backgroundImage** png and jpg images
- **backgroundRepeat**  values: repeat | repeatX | repeatY | noRepeat

<!-- -->  HTML-like comments in SMIL-File are supported and content inside will ignored

## additional features supported not in w3c standard
 - **shuffle play** in seq container via metadata
 - **automatic refresh** via metatags in SMIL header
 - **Logs/reports** system and inventory-reports, events- and playlogs and the states of tasks via subscription meta and delivery by HTTP PUT described in [Reporting](https://garlic-signage.com/garlic-player/docs/essentials/logs-reports/)
 - **expr-attribute** in media tags [conditional play](https://garlic-signage.com/garlic-player/docs/essentials/conditional-play/)
 - REST-API ([documentation](https://garlic-signage.com/garlic-player/docs/rest-api/))
 - placeholders in head with state and data tags
 - http downloads and caching
 - caching of remote SMIL index file
 - caching of remote media files
 - refresh via meta tag in cyclic timer periods
 - change config.xml during playback (task `updateSettings`)
 - remote update (task `firmwareUpdate`; installing is the job of a launcher or daemon)
 - removes the longest unused files from the cache when the disk is full

Contact: [Niko Sagiadinos](mailto:garlic@sagiadinos.de)
