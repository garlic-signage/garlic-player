# sdl

The third frontend of the garlic-player, beside `qwidget` and `qml`: SDL3 on the core (`PlayerRuntimeApi` in a
`ThreadedRuntime`), without Qt. Everything is drawn by one SDL renderer (OpenGL, OpenGL ES on the Raspberry Pi), so
images, video and web pages stack exactly by z-index, which the Qt 6 players cannot guarantee for video and
web pages (native windows). If it runs smoother than the Qt players, Qt goes.

| File                         | Content                                                                         |
|------------------------------|---------------------------------------------------------------------------------|
| `main.cpp`                   | options like the qwidget player, supervisor, `ThreadedRuntime`, the main loop   |
| `paths.hpp/.cpp`             | ini, cache and logs at the places of the Qt players and garlic-lib              |
| `player_window.hpp/.cpp`     | `PlayerWindow`: core events, regions, input, 10-click menu, dialogs, drawing    |
| `media.hpp/.cpp`             | `Media`, image (SDL3_image), brush, web (through `WebView`), what cannot play |
| `place.hpp/.cpp`             | `place()`: fit and mediaAlign of images and videos |
| `mpv_media.hpp/.cpp`         | `MpvMedia`: video and audio with libmpv, rendered into a texture of the renderer |
| `web_view.hpp`               | `WebView`: the interface for web pages per platform, and what the engine needs from `main()` and the loop |
| `cef_web_view.cpp`           | `WebView` with CEF off-screen rendering (Linux)                                 |
| `web_view.cpp`               | the build without a web engine: web pages fail, the core goes on                |
| `color.hpp/.cpp`             | SMIL colors as the Qt style sheets took them (#rgb, #rrggbb, #aarrggbb, rgb(), rgba(), CSS names) |
| `dialogs.hpp/.cpp`           | settings dialog and debug dialog, drawn over the content                        |
| `blank_screen.hpp/.cpp`      | `adapi:blankScreen`, `ScreenControl`, `DeviceControl` (as in `../supervisor`, without Qt) |
| `supervisor_client.hpp/.cpp` | the supervisor protocol over a Unix socket (as in `../supervisor`, without Qt)  |
| `ui/`                        | controls copied from the DJ workstation: canvas, text, button, text field, check box, menu |

## Build

Off by default, so the Qt builds stay as they are:

```
cmake -S . -B build-sdl -G Ninja -DGARLIC_PLAYER_QWIDGET=OFF -DGARLIC_PLAYER_QML=OFF -DGARLIC_PLAYER_SDL=ON
cmake --build build-sdl --target sdl
build-sdl/src/player-ui/sdl/garlic-player-sdl [-m fullscreen] [SMIL_INDEX]
```

The font (`../qwidget/fonts/DejaVuSans.ttf`) is copied to `fonts/` next to the program.

| Dependency                  | Where from                                                           |
|-----------------------------|----------------------------------------------------------------------|
| SDL3, SDL3_ttf, SDL3_image  | the system (or vcpkg) if there (3.2 or newer), otherwise downloaded by CMake (pinned tags) |
| libmpv                      | pkg-config `mpv` 2.0 or newer (mpv 0.35); without it no video and audio (`PLAYER_SDL_WITH_MPV`) |
| nlohmann/json               | the one of the core                                                  |
| CEF                         | `-DPLAYER_SDL_WITH_CEF=ON` (Linux x86_64 or aarch64, by `CMAKE_SYSTEM_PROCESSOR`): the minimal binary distribution is downloaded (about 330 MB for x86_64, 430 MB for aarch64), its wrapper built, `libcef.so`, resources and locales copied next to the program. `-DFETCHCONTENT_SOURCE_DIR_CEF=<dir>` takes an extracted one instead |
| libwebp                     | for SDL3_image when it is downloaded (WebP images)                   |

Targets: Arch Linux (built here), Debian Trixie x86_64 and aarch64 with GCC 14 (the anchor for minimum versions:
SDL3 3.2, libmpv 0.40). Debian Bookworm has no SDL3 packages; the download covers it, but it is not a target.
Packages on Debian: `libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev libmpv-dev libwebp-dev`.

## How it works

- **Main loop** (`PlayerWindow::run`): waits for SDL events at most until the next timer (menu hold, dialog refresh,
  100 ms with a supervisor, otherwise 1 s). The core thread wakes it with an SDL user event (code 0: new core events),
  libmpv with code 1 (a new frame). Drawn only when something changed.
- **Regions** from bottom to top by z-index: background color, background image (tiled like CSS `repeat`, `repeatX`,
  `repeatY`, `noRepeat`), then the newest medium of the region, clipped to it. A resumed medium comes to the top again.
- **Image quality**: an image keeps its original as surface and gets a texture in the size it is drawn, made anew
  when the region changes its size (qwidget: `Qt::SmoothTransformation`). Shrunk, it is halved step by step and
  scaled to the exact size last; the GPU alone scales linearly from 4 pixels, and shrunk to a third or less the edges
  fray. Enlarged, the GPU scales the original.
- **Fit and mediaAlign** by `place()` (`place.hpp/.cpp`, tested in `tests/`), for images and videos alike (SMIL
  standard): `fill` stretches, `meet` keeps the aspect inside, `meetBest` like meet but never larger, `slice` covers
  (cut by the region), without fit the original size; then placed by mediaAlign. For video mpv reports its size
  (`dwidth`/`dheight` after `MPV_EVENT_VIDEO_RECONFIG`) and renders into a texture of exactly that place
  (`keepaspect=no`). The qwidget and the legacy players always kept the aspect inside and centered videos.
- **Video and audio**: one mpv per medium. Video renders through the render API (OpenGL) into a framebuffer on a
  texture of the SDL renderer. mpv has an OpenGL context of its own that shares its textures with the one of the
  renderer (one for all media): in the same context the state SDL leaves broke mpv (NVIDIA: crash in the driver with
  `nvdec`, artefacts with `nvdec-copy`). A fence (OpenGL 3.2 / ES 3, else `glFinish`) lets SDL read the texture
  only when mpv has written it. The Wayland or X11 display goes to mpv for hardware frames without a copy. `hwdec=auto-safe`;
  `GARLIC_MPV_HWDEC` overrides it (e.g. `no`, `auto-copy-safe`) on a device whose driver fails with it.
- **Web pages** (CEF, Linux): off-screen rendering into a texture of the renderer, so a page lies in its region by
  z-index and is cut by the regions above it. Two ways, chosen at the first web page (logged once):
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
  8 % CPU on NVIDIA. ANGLE runs on Vulkan (`GARLIC_WEB_ANGLE` overrides it, e.g. `gl`, `swiftshader`): with its OpenGL
  backend the GPU process went through GLX and crashed on X11. With `ozone-platform=wayland` Chromium refuses Vulkan
  ("not compatible with Vulkan") and uses OpenGL over EGL, which works. WebGL in software if the GPU process fails.

  CEF runs in the main thread with an external message pump (`pumpWebViews()` in every loop turn, at least every
  33 ms). The helper processes (renderer, GPU, network) are this program again: `runWebHelperProcess()` is the first
  thing in `main()`; the GPU process is a child of the zygote without sandbox. Like the Qt players: local files may
  read remote data and other files (`allow-file-access-from-files`, `disable-web-security`), media play without a
  click, permissions are granted, no popups, after loading `overflow: hidden`. Profile and cache in
  `~/.local/share/garlic-player/cef` (the Qt players: `QtWebEngine` next to it), log in `logs/cef.log`. No sandbox
  (`chrome-sandbox` would have to be setuid root).
- **Input for web pages**: a click on a page gives it the mouse and the keyboard (no `elementActivated`, no access
  keys while it has the focus; the Ctrl shortcuts of the player stay); a click elsewhere takes the keyboard back.
- **Reports** of the media (end, error) are handed to the core after the event that caused them, only from the
  first region of an instance, like the qwidget player.
- **Input**: ten fast clicks (< 500 ms apart) or holding 10 s open the menu (Settings, Debug infos, Exit), like every
  frontend. Keys: Ctrl+F full screen, Ctrl+B over all screens, Ctrl+S, Ctrl+D, Ctrl+Q; other keys without modifier
  are access keys. A click on a region activates its top medium. Locked by the supervisor: no menu, no dialogs, no
  shortcuts.
- **Dialogs** are drawn over the content, modal; the menu and the dialogs scale with the screen (a 900 pixel high
  screen is the base).
- **Without an index** the settings dialog comes first; Cancel ends the player with 1 (garlic-lib).

## What differs from the qwidget player

- Video and web pages lie in their region by z-index (the Qt 6 players put native windows on top).
- The screenshot is the window (in full screen mode the screen), not the whole screen.
- No `--media-backend`: libmpv only.
- The supervisor client speaks over Unix sockets only; Windows (named pipe) is not done.

## Not done yet

- **Native web views** for the other platforms behind `WebView`: WebView2 (Windows), WKWebView (macOS, iOS),
  Android WebView. Without CEF a build has no web pages.
- Web pages: popups of a page (an open `<select>`), touch input (only the mouse goes to a page), the cursor shape.
- Transitions.
