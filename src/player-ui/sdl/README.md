# sdl

The third frontend of the garlic-player, beside `qwidget` and `qml`: SDL3 on the core (`PlayerRuntimeApi` in a
`ThreadedRuntime`), without Qt. Everything is drawn by one SDL renderer (OpenGL, OpenGL ES on the Raspberry Pi), so
images, video and later web pages stack exactly by z-index, which the Qt 6 players cannot guarantee for video and
web pages (native windows). If it runs smoother than the Qt players, Qt goes.

| File                         | Content                                                                         |
|------------------------------|---------------------------------------------------------------------------------|
| `main.cpp`                   | options like the qwidget player, supervisor, `ThreadedRuntime`, the main loop   |
| `paths.hpp/.cpp`             | ini, cache and logs at the places of the Qt players and garlic-lib              |
| `player_window.hpp/.cpp`     | `PlayerWindow`: core events, regions, input, 10-click menu, dialogs, drawing    |
| `media.hpp/.cpp`             | `Media`, image (SDL3_image), brush, web (through `WebView`), what cannot play; `place()` for fit and mediaAlign |
| `mpv_media.hpp/.cpp`         | `MpvMedia`: video and audio with libmpv, rendered into a texture of the renderer |
| `web_view.hpp/.cpp`          | `WebView`: the interface for web pages per platform; none built yet             |
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
- **Fit and mediaAlign** like the qwidget player: images by `place()`; video by mpv (`fill` stretches, `slice` is
  `panscan=1`, everything else keeps the aspect), centered.
- **Video and audio**: one mpv per medium. Video renders through the render API (OpenGL) into a framebuffer on a
  texture of the SDL renderer; SDL is flushed before and sets its state again afterwards. `hwdec=auto-safe`;
  `GARLIC_MPV_HWDEC` overrides it (e.g. `no`, `auto-copy-safe`) on a device whose driver fails with it.
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

- **Web pages**: `WebView` exists, `createWebView()` returns nullptr, so web pages and widgets fail and the core goes
  on. Planned: CEF off-screen rendering on Linux only (x86_64 and aarch64, the distribution picked by
  `CMAKE_SYSTEM_PROCESSOR`), the page as a texture; clicks forwarded to the page. Later the native views: WebView2
  (Windows), WKWebView (macOS, iOS), Android WebView.
- Clearing the web cache (`applyCommand clear_webcache`): nothing to clear without web pages, `cachesCleared()` comes at once.
- Transitions.
