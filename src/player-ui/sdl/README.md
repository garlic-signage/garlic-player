# sdl

The desktop frontend of the garlic-player for Linux, Windows and macOS: SDL3 on the core (`PlayerRuntimeApi` in a
`ThreadedRuntime`). Everything is drawn by one SDL renderer (OpenGL, OpenGL ES on the Raspberry Pi), so images, video
and web pages stack exactly by z-index; native video and web windows would always lie on top.

| File                         | Content                                                                         |
|------------------------------|---------------------------------------------------------------------------------|
| `main.cpp`                   | options like the players before, supervisor, `ThreadedRuntime`, the main loop   |
| `paths.hpp/.cpp`             | ini, cache and logs at the places of the players before (garlic-lib)            |
| `player_window.hpp/.cpp`     | `PlayerWindow`: core events, regions, input, 10-click menu, dialogs, drawing    |
| `media.hpp/.cpp`             | `Media`, image (SDL3_image), brush, web (through `WebView`), what cannot play |
| `place.hpp/.cpp`             | `place()`: fit and mediaAlign of images and videos |
| `mpv_media.hpp/.cpp`         | `MpvMedia`: video and audio with libmpv, rendered into a texture of the renderer |
| `web_view.hpp`               | `WebView`: the interface for web pages per platform, and what the engine needs from `main()` and the loop |
| `cef_web_view.cpp`           | `WebView` with CEF off-screen rendering (Linux, Windows, macOS)                 |
| `file_url.hpp/.cpp`          | `fileUrl()`: a local path as `file://` URL, query and fragment kept             |
| `mac/`                       | macOS: the CEF helper apps (`cef_helper.cpp`, their Info.plist), `CefAppProtocol` for the application class of SDL |
| `windows/`                   | Windows: the manifest (Windows 10/11)                                           |
| `web_view.cpp`               | the build without a web engine: web pages fail, the core goes on                |
| `color.hpp/.cpp`             | SMIL colors like the players before (#rgb, #rrggbb, #aarrggbb, rgb(), rgba(), CSS names) |
| `dialogs.hpp/.cpp`           | settings dialog and debug dialog, drawn over the content                        |
| `blank_screen.hpp/.cpp`      | `adapi:blankScreen`, `ScreenControl`, `DeviceControl`                           |
| `supervisor_client.hpp/.cpp` | the supervisor protocol over a Unix socket                                      |
| `ui/`                        | controls copied from the DJ workstation: canvas, text, button, text field, check box, menu |

## Build

```
cmake -S . -B build-sdl -G Ninja -DCMAKE_BUILD_TYPE=Release -DGARLIC_PLAYER_QWIDGET=OFF -DGARLIC_PLAYER_QML=OFF \
    -DGARLIC_PLAYER_SDL=ON -DPLAYER_SDL_WITH_CEF=ON
cmake --build build-sdl --target sdl
build-sdl/src/player-ui/sdl/garlic-player [-m fullscreen] [SMIL_INDEX]
```

Windows (x64 Developer Command Prompt) and macOS (arm64) take the libraries of the core from vcpkg, in addition:

```
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_DIR=<garlic>/src/core
-DCMAKE_OSX_ARCHITECTURES=arm64   # macOS
```

Windows: `garlic-player.exe` without console window, CEF and `libmpv-2.dll` next to it. macOS: the app bundle
`garlic-player.app` with the CEF framework, the five CEF helper apps and the dylibs of libmpv in
`Contents/Frameworks`, the font in `Contents/Resources`.

The font (`../fonts/DejaVuSans.ttf`) is copied to `fonts/` next to the program.

| Dependency                  | Where from                                                           |
|-----------------------------|----------------------------------------------------------------------|
| SDL3, SDL3_ttf, SDL3_image  | the system (or vcpkg) if there (3.2 or newer), otherwise downloaded by CMake (pinned tags) |
| libmpv                      | Linux: pkg-config `mpv` 2.0 or newer (mpv 0.35). Windows: the official libmpv 0.42 of mpv (lgpl, one DLL), downloaded; the import library for MSVC is made from the headers. macOS: libmpv 0.36 of media_kit (`libmpv-darwin-build`, `video-full`), downloaded, the headers from the sources of mpv 0.36. `-DMPV_DIR=<dir>` takes another one (`include/mpv`, `libmpv*.dll`, `mpv.def` optional). Without it no video and audio (`PLAYER_SDL_WITH_MPV`) |
| nlohmann/json               | the one of the core                                                  |
| CEF                         | `-DPLAYER_SDL_WITH_CEF=ON`: the minimal binary distribution of CEF 156 is downloaded for Linux x86_64 and aarch64, Windows x64 or macOS arm64 (about 330 MB for x86_64, 430 MB for aarch64), its wrapper built. Linux and Windows: libcef, resources and locales next to the program; macOS: the framework in the bundle. `-DFETCHCONTENT_SOURCE_DIR_CEF=<dir>` takes an extracted one instead |
| libwebp                     | for SDL3_image when it is downloaded (WebP images)                   |

Targets: Arch Linux (built here), Debian Trixie x86_64 and aarch64 with GCC 14 (the anchor for minimum versions:
SDL3 3.2, libmpv 0.40), Windows x64 (MSVC), macOS arm64. Debian Bookworm has no SDL3 packages; the download covers it,
but it is not a target. Windows and macOS have not been built yet.

Raspberry Pi 4 with **1 GB**: 1080p videos and web pages. For **4K** (H.265, at most 30 fps) together with web pages use
a Pi 4 with **2 GB or more**. A 4K video with 10 bit takes 270-390 MB of the 512 MB CMA (the memory of the GPU, the same
on every Pi 4); on a Pi 4 with 1 GB about 400 MB are left for the system, the player and CEF, and with the desktop a 4K
video ran out of CMA and the Pi hung.
Packages on Debian: `libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev libmpv-dev libwebp-dev`.

### Runtime

The AppImage takes libmpv (with FFmpeg), SDL3, NSS and VA-API from the system: on a Raspberry Pi the FFmpeg of
Raspberry Pi OS is the one that decodes H.265 in hardware. Debian Trixie and later (Bookworm has no SDL3); a Raspberry
Pi lacks some of them even with the full install. The same for both AppImages (aarch64 and x86_64):

```
sudo apt install libmpv2 libsdl3-0 libsdl3-ttf0 libsdl3-image0 \
 libva-wayland2 libva-x11-2 libva-drm2 \
 libcurl4t64 libsqlite3-0 libmicrohttpd12t64 \
 libglib2.0-0t64 libnss3 libnspr4 libatk1.0-0t64 libatk-bridge2.0-0t64 libatspi2.0-0t64 \
 libdbus-1-3 libcups2t64 libexpat1 libudev1 libasound2t64 \
 libx11-6 libxcb1 libxext6 libxfixes3 libxrandr2 libxcomposite1 libxdamage1 libxkbcommon0 \
 libgbm1 libcairo2 libpango-1.0-0
```

sdl: ANGLE on gl-egl under KMSDRM on Raspberry Pi OS Lite also libfuse2: the AppImage runtime loads `libfuse.so.2` to mount itself, Lite has only fuse3
(the full install has both):

```
sudo apt install libfuse2t64
```

On x86_64 also the VA-API driver of the graphics, otherwise mpv decodes in software (NVIDIA: NVDEC through the
proprietary driver, no package needed):

```
sudo apt install intel-media-va-driver   # Intel (Broadwell and newer)
sudo apt install mesa-va-drivers         # AMD
```

## How it works

- **Main loop** (`PlayerWindow::run`): waits for SDL events at most until the next timer (menu hold, dialog refresh,
  100 ms with a supervisor, otherwise 1 s). The core thread wakes it with an SDL user event (code 0: new core events),
  libmpv with code 1 (a new frame). Drawn only when something changed.
- **Regions** from bottom to top by z-index: background color, background image (tiled like CSS `repeat`, `repeatX`,
  `repeatY`, `noRepeat`), then the newest medium of the region, clipped to it. A resumed medium comes to the top again.
- **Image quality**: an image keeps its original as surface and gets a texture in the size it is drawn, made anew
  when the region changes its size. Shrunk, it is halved step by step and
  scaled to the exact size last; the GPU alone scales linearly from 4 pixels, and shrunk to a third or less the edges
  fray. Enlarged, the GPU scales the original.
- **Fit and mediaAlign** by `place()` (`place.hpp/.cpp`, tested in `tests/`), for images and videos alike (SMIL
  standard): `fill` stretches, `meet` keeps the aspect inside, `meetBest` like meet but never larger, `slice` covers
  (cut by the region), without fit the original size; then placed by mediaAlign. For video mpv reports its size
  (`dwidth`/`dheight` after `MPV_EVENT_VIDEO_RECONFIG`) and renders into a texture of exactly that place
  (`keepaspect=no`). The players before always kept the aspect inside and centered videos.
- **Video and audio**: one mpv per medium. Video renders through the render API (OpenGL) into a framebuffer on a
  texture of the SDL renderer. mpv has an OpenGL context of its own that shares its textures with the one of the
  renderer (one for all media): in the same context the state SDL leaves broke mpv (NVIDIA: crash in the driver with
  `nvdec`, artefacts with `nvdec-copy`). A fence (OpenGL 3.2 / ES 3, else `glFinish`) lets SDL read the texture
  only when mpv has written it. The Wayland or X11 display goes to mpv for hardware frames without a copy. `hwdec=v4l2m2m,drm,auto-safe`:
  first the decoders of a Raspberry Pi without copy, which `auto-safe` does not take: `v4l2m2m` for H.264 up to 1080p
  (a Pi 4 with a 1080p H.264: about 140 % CPU decoded in software, 70 % with `v4l2m2m-copy`, 40 % without copy),
  `drm` for H.265 up to 4K, which needs the FFmpeg of Raspberry Pi OS (a 4K60 10 bit H.265 decodes at 75 fps without
  copy, 22 fps with). Without them mpv goes on with `auto-safe`. `GARLIC_MPV_HWDEC` overrides it (e.g. `no`,
  `auto-copy-safe`) on a device whose driver fails with it. On a Raspberry Pi (renderer `V3D`) `gpu-hwdec-interop=drmprime`,
  the only interop its decoders need: with `auto` every mpv loaded all interops, also the one of CUDA, and wrote
  "Cannot load libcuda.so.1" into the log for each video.
- **Video quality** (`--video-quality`): `low` is mpv's profile `fast` (bilinear, no dither, no deband), `normal` its
  defaults, `high` its profile `high-quality`; `auto` takes `low` on the GPU of a Raspberry Pi (OpenGL renderer
  `V3D`), otherwise `normal`. A Pi 4 under KMSDRM with `low` plays 4K H.265 with 25 fps at 10 bit and nearly 30 fps
  at 8 bit (27 of 30 frames); 30 fps at 10 bit only 21-23: importing the 10 bit frames costs its GPU about 35 ms per
  frame. Under the Wayland desktop the compositor costs another 10-15 ms per frame.
- **Web pages** (CEF): off-screen rendering into a texture of the renderer, so a page lies in its region by
  z-index and is cut by the regions above it. Windows and macOS take the CPU way below; Linux has two, chosen at the
  first web page (logged once):
  - **GPU** (shared textures, `OnAcceleratedPaint`): CEF hands every frame as dmabuf; it is imported as EGL image
    (`EGL_EXT_image_dma_buf_import`, with modifiers if there) and copied on the GPU (`glBlitFramebuffer`) into the
    texture of the page, in the context of the SDL renderer. No read back, no upload. Needs EGL: Wayland is EGL, X11
    is made EGL by `SDL_VIDEO_FORCE_EGL` (`prepareWebViews()`, before `SDL_Init`; libmpv runs on it too). CEF then
    runs on the platform of SDL (`ozone-platform=wayland` or `x11`): the GPU process allocates the frames from the GPU
    buffers (GBM) of the display; headless has none.
  - **CPU** (`OnPaint`): the page as BGRA, uploaded every frame. When the GPU way cannot be used (KMSDRM, a renderer
    without EGL or the extensions, `GARLIC_WEB_SHARED_TEXTURE=0`), when an import fails, or when no frame comes within
    10 s (X11 without DRI3 has no GBM): the page starts again this way. Headless when it is chosen at the start, so CEF
    needs no display (a Raspberry Pi without desktop).

  The copy is submitted (`glFlush`), not waited for: CEF reuses a buffer only frames later, and `glFinish` cost about
  8 % CPU on NVIDIA. ANGLE runs on OpenGL (the default of Chromium), on X11 on Vulkan, under KMSDRM on OpenGL through
  EGL (`gl-egl`); `GARLIC_WEB_ANGLE` overrides it, e.g. `gl`, `gl-egl`, `vulkan`, `swiftshader`. With its OpenGL backend
  (`gl`) the GPU process went through GLX: it crashed on X11, and under KMSDRM it failed for want of an X display,
  SwiftShader crashed after it and CEF ran without GPU (`--use-gl=disabled`). With Vulkan the GPU process of a Raspberry
  Pi 4 crashed under Wayland and KMSDRM, and after the fallbacks Chromium ended the player ("GPU process isn't usable.
  Goodbye."). WebGL in software if the GPU process fails.

  CEF runs in the main thread with an external message pump (`pumpWebViews()` in every loop turn, at least every
  33 ms). The helper processes (renderer, GPU, network) are this program again: `runWebHelperProcess()` is the first
  thing in `main()`; the GPU process is a child of the zygote without sandbox. On macOS the subprocesses run in the helper apps of the
  bundle; the framework is loaded at the start (`CefScopedLibraryLoader`), and the application class of SDL gets
  `CefAppProtocol` (`mac/cef_application.mm`). Like the players before: local files may
  read remote data and other files (`allow-file-access-from-files`, `disable-web-security`), media play without a
  click, permissions are granted, no popups, after loading `overflow: hidden`. Profile and cache in
  `~/.local/share/garlic-player/cef`, log in `logs/cef.log`; `clear_webcache` removes the whole profile at the next
  start. No sandbox
  (`chrome-sandbox` would have to be setuid root).
- **Input for web pages**: a click on a page gives it the mouse and the keyboard (no `elementActivated`, no access
  keys while it has the focus; the Ctrl shortcuts of the player stay); a click elsewhere takes the keyboard back.
- **Reports** of the media (end, error) are handed to the core after the event that caused them, only from the
  first region of an instance, like the players before.
- **Input**: ten fast clicks (< 500 ms apart) or holding 10 s open the menu (Settings, Debug infos, Exit), like every
  frontend. Keys: Ctrl+F full screen, Ctrl+B over all screens, Ctrl+S, Ctrl+D, Ctrl+Q; other keys without modifier
  are access keys. A click on a region activates its top medium. Locked by the supervisor: no menu, no dialogs, no
  shortcuts.
- **Dialogs** are drawn over the content, modal; the menu and the dialogs scale with the screen (a 900 pixel high
  screen is the base).
- **Loading**: while downloads run and no medium was shown since the layout, "Loading 2 / 7" and a bar in the middle
  of the screen (`Downloads` of the core), so the screen is not black while the first files come; gone with the first
  medium and not back in gaps between media, again with the next layout.
- **Without an index** the settings dialog comes first; Cancel ends the player with 1 (garlic-lib).

## What differs from the players before

- Video and web pages lie in their region by z-index (the players before put native windows on top).
- The screenshot is the window (in full screen mode the screen), not the whole screen.
- No `--media-backend`: libmpv only.
- The supervisor client speaks over Unix sockets only; Windows (named pipe) is not done.

## Not done yet

- Windows and macOS: the first build and run.
- Web pages: popups of a page (an open `<select>`), touch input (only the mouse goes to a page), the cursor shape.
- Transitions.
