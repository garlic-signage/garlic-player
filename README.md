# garlic-player - SMIL Player for Digital Signage

[![CI](https://github.com/garlic-signage/garlic-player/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/garlic-signage/garlic-player/actions/workflows/ci.yml)
![coverage](https://raw.githubusercontent.com/garlic-signage/garlic-player/badges/coverage.svg)
[![mutation score](https://raw.githubusercontent.com/garlic-signage/garlic-player/badges/mutation.svg)](https://github.com/garlic-signage/garlic-player/actions/workflows/mutation.yml)

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
([src/core](src/core/README.md)), a library in C++23 without Qt: download and cache, SMIL parser and timing engine,
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

The frontends are in [src/player-ui](src/player-ui) and are operated the same way:
 - **sdl** ([src/player-ui/sdl](src/player-ui/sdl/README.md)), the desktop player with SDL3 for Linux, Windows and
   macOS. Video and audio with libmpv, web pages with CEF (Chromium) off-screen, so images, videos and web pages
   stack by z-index.
 - **android** ([src/player-ui/android](src/player-ui/android/README.md)), Java with the core through JNI, video with
   Media3 (ExoPlayer) and web pages in the WebView of the system; Android 7.1.1 and newer.
 - **js** ([src/player-ui/js](src/player-ui/js/README.md)), the core as WebAssembly with a player in JavaScript for
   browsers, Tizen and webOS.

The beta runs on Linux, Windows, macOS and Android; iOS follows later. The code of version 1.x
(Qt 5, garlic-lib and the old frontends) is kept in [legacy](legacy) for reference; it is not built anymore.

## Build (2.0)

Needs CMake 3.31+, a C++23 compiler (GCC 14, Clang 18, MSVC 2022 or Apple Clang), libcurl, libmicrohttpd, SQLite,
libxml2, SDL3 and libmpv. On Debian Trixie (x86_64 and aarch64):

```
apt install cmake ninja-build g++ pkg-config libcurl4-openssl-dev libmicrohttpd-dev libsqlite3-dev libxml2-dev \
    nlohmann-json3-dev libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev libmpv-dev libwebp-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLAYER_SDL_WITH_CEF=ON
cmake --build build
build/src/player-ui/sdl/garlic-player -m fullscreen https://example.com/index.smil
```

CMake downloads CEF (the minimal binary distribution of Spotify's CEF builds) and puts it next to the program.

**Windows (x64)** and **macOS (arm64)** take the libraries of the core from vcpkg (`src/core/vcpkg.json`), CMake
downloads CEF and libmpv (Windows: the official libmpv of mpv, macOS: the build of media_kit):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPLAYER_SDL_WITH_CEF=ON \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_DIR=src/core
```

On Windows from the x64 Developer Command Prompt; on macOS add `-DCMAKE_OSX_ARCHITECTURES=arm64`. The details are in
the [README of the sdl player](src/player-ui/sdl/README.md).

### Version

The version of all players (sdl, Android, JS) is in one file, [src/player-ui/VERSION](src/player-ui/VERSION):
`2.0.0-dev` during development, `2.0.0` for a release. It goes into the user agent, into `--version` and into the
title of the information dialog. The build server always appends its build number with a dot before building, for a release
as for a dev build:

```
echo "$(cat src/player-ui/VERSION).$BUILD_NUMBER" > src/player-ui/VERSION   # 2.0.0.997, 2.0.0-dev.996
```

The number after the last dot is the build: garlic-hub gives the full index (multizone, prefetch, priority classes)
only from build 566 on, and Android updates by it (`versionCode` 20000000 + build). A release build of Android stops
when the number is missing, otherwise it would lie below the dev builds and Android would refuse the update.
CMake configures again by itself when the file changes.

## Features
 - full screen on one screen, one window over all screens (bigscreen) or windowed, start screen selectable
 - multiple and overlapping zones e.g. splitscreens
 - logging: debug, event, play and task logs
 - resume downloads
 - HTTP redirects (up to 5) for index and media
 - video and audio with libmpv (FFmpeg), hardware decoding where the platform offers it
 - images: Jpg, Png, Gif, Bmp, Svg, WebP
 - media cache
 - SMIL3 subset, HTML5, HTML5-Widgets and (depending on device) WebGL support
 - WebWidgets with also support parameters
 - Webserver on port 8080 (default) for the REST API

## Operating Systems Supported (2.0)
 - Linux x86_64 and aarch64, e.g. Raspberry Pi 4
 - Windows x64 and macOS arm64
 - Android 7.1.1 and newer: arm64-v8a, armeabi-v7a, x86_64 and x86 
 - WebOS, Tizen and BrightSignOS via WASM / JS
 - iOS via alternative store (planned)

Raspberry Pi 4 with **1 GB**: 1080p videos and web pages. For **4K** (H.265, at most 30 fps) together with web pages
use a Pi 4 with **2 GB or more**. Details in the [README of the sdl player](src/player-ui/sdl/README.md).

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
| `--supervisor` | Endpoint of the supervisor (garlic-daemon: Unix socket) |
| `--video-quality` | Scaling and filtering of videos: `auto` (default: `low` on a Raspberry Pi, otherwise `normal`), `low`, `normal` or `high` |

Arguments:
SMIL_INDEX        URI or path of the SMIL index; without it the one of the settings is used. Without any index the
                  settings dialog opens.

The index of the command line wins over the one in a `config.xml`.

### Environment variables (sdl)

For devices whose drivers need a different way; not needed normally. Details in the
[README of the sdl player](src/player-ui/sdl/README.md).

| Variable | Description |
|-------------------------------|------------------------------------------------------------------------------|
| `GARLIC_MPV_HWDEC` | Hardware decoding of mpv (`hwdec`), default `v4l2m2m,drm,auto-safe`; e.g. `no` (software), `auto-copy-safe` |
| `GARLIC_MPV_FENCES=0` | mpv gets no `glFenceSync` (as before 2.0.0-dev.968); a test switch for the memory on a Raspberry Pi |
| `GARLIC_WEB_SHARED_TEXTURE=0` | Web pages come as pixels through the CPU instead of as GPU buffers (Linux) |
| `GARLIC_WEB_ANGLE` | Graphics backend of CEF (`use-angle`) on Linux: `gl`, `gl-egl`, `vulkan`, `swiftshader`; default `vulkan` on X11, `gl-egl` under KMSDRM, otherwise `gl` |

Not of the player, but useful: `MALLOC_ARENA_MAX=2` (glibc) limits the memory arenas of the threads; it slows the
growth of the player's memory on a desktop with NVIDIA in the first hours, but does not stop it.

## Menu

Ten fast clicks or touches (less than 500 ms apart) or one held for 10 seconds, anywhere on the screen, open a menu:
 - **Settings**: player name, content URL, HTTP server on/off and port
 - **Information**: the version in the title; playlist, user agent, disk, memory of the system and of the player,
   threads, CPU load of the player and of the system, temperatures of CPU, RAM and SSD with the throttling (a red or
   green circle), the running downloads and the media playing in each region
 - **Exit**

Holding is meant for interactive content, where fast clicks reach the page too. All players work the same way.

## HotKeys
 - toggle fullscreen with Ctrl-F
 - bigscreen with Ctrl-B
 - open settings dialog with Ctrl-S
 - open the information with Ctrl-D
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

## Reports and events (2.0)

Details in [core/docs/reports.md](src/core/docs/reports.md) and [core/docs/content.md](src/core/docs/content.md).

- **System report** like the Android players of IAdea, with our namespace `http://schemas.garlic-player.com/gapi-1.0`.
  Disk and memory in bytes like garlic-lib (IAdea seems to give the memory in KB: an evaluation treats IAdea reports
  apart), `cpuUsage` with `%`, `hdmiOutput` from the frontend (`1920x1080p-60`).
  - `network`: `type`, `mac`, `ip`, `netmask` (first IPv4), IPv6 as `ip_v6`, `netmask_v6`.
  - `modelInfo`: `modelName` garlic-player, `modelDescription` the version, `licenseModel` the operating system, `PCB`
    the hardware. The player reads it itself, without supervisor: Android `ro.product.model`, Linux the device tree
    (Raspberry Pi) or the DMI (PCs), Windows the BIOS in the registry, macOS `hw.model`.
  - `userPref`: what the player knows (player name, locale, rotation, daily reboot, master volume, brightness, time
    zone, content URL), nothing it does not know (proxy, wifi, adb). Own values as `garlic.*`: device manufacturer,
    uptime of player and device, memory and threads of the player with their maxima, its CPU load, temperatures,
    throttling.
  - `hardwareInfo` (the screen): from a supervisor that knows it, on Philips over SICP (`modelName` "43BDL4550D/00",
    `serialNumber`, `vendorId` Philips); otherwise empty, the player does not read the EDID.
- **Events** of IAdea: `FETCH_FAILED` (also `E_CHECKSUM_NOT_MATCHED`), `OBJECT_UPDATED`, `OBJECT_REMOVED` (a file
  deleted over the REST API), `LRU_OBJECT_REMOVED` (the cleanup of a full disk), `BROKEN_OBJECT_REMOVED`,
  `MEDIA_NOT_AVAILABLE`, `MEDIA_OPEN_ERROR`, `MEDIA_PLAYBACK_ERROR`, `SMIL_LOADED`, `SMIL_NOT_AVAILABLE`,
  `SMIL_OPEN_ERROR`; of garlic-lib: `OBJECTS_REMOVED` (the sum of a cleanup; clearing the player cache is one such
  event, not one per file), `FAILED_FREEING_SPACE`, `DISK_FULL`, the task scheduler and configuration events; new:
  `MEDIA_UNSUPPORTED`. A media error is reported once per medium and index version. Not used: `STORAGE_FORMATTED`, `STORAGE_TRUNCATED` (the player never formats a storage).
- **Digest of a download**: only if the server sends one (`Repr-Digest`, `Content-Digest`, `Digest`, `Content-MD5`;
  SHA-256 or MD5), else nothing is checked. A wrong one is `FETCH_FAILED` with `E_CHECKSUM_NOT_MATCHED` and the
  calculated digest (`md5` or `sha256`), the file is loaded again.

Contact: [Niko Sagiadinos](mailto:garlic@sagiadinos.de)
