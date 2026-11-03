# js

The browser frontend of the garlic-player on the core. The core runs as WebAssembly in the page (`garlic-core.js`,
`garlic-core.wasm`), the player itself is JavaScript (`web/`). It behaves like the [qwidget](../qwidget/README.md) and
[qml](../qml/README.md) players: the same menu, dialogs and shortcuts.

Only with Emscripten (see [the core](../../core/README.md#pinned-versions)) and vcpkg:

```
cd src/player-ui/js
cmake --preset wasm-release && cmake --build --preset wasm-release
python3 -m http.server -d build/wasm-release 8800      # any web server; file:// does not work
```

Then open `http://localhost:8800/` (or with `?index=https://hub/index.smil`). `build/wasm-release` holds everything to
deploy: `index.html`, `player.js`, `player.css`, `garlic-core.js`, `garlic-core.wasm`.

## URL parameters, menu, keys

- `index=<URI>`: the SMIL index, like `SMIL_INDEX` on the command line; empty: the one in the settings.
- `storage=memory`: the cache only in memory, gone with the page (e.g. for a preview). Default: kept in IndexedDB.

Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s open the menu with Settings, Debug infos and
Exit. Keys: Ctrl+F full screen, Ctrl+B full screen too (over all screens is not possible in a browser), Ctrl+S
settings, Ctrl+D debug infos, Ctrl+Q quit; every other key is an accesskey for the SMIL. Without an index the settings
dialog opens at the start; cancelled, the player ends.

## Files

| File               | Content                                                                              |
|--------------------|--------------------------------------------------------------------------------------|
| `bindings.cpp`     | `Player` (Embind): owns the core, turns its events into plain JS objects             |
| `CMakeLists.txt`   | the module `garlic-core.js` (ES module, `createGarlicCore`), copies `web/` next to it |
| `CMakePresets.json`| `wasm-debug`, `wasm-release`; the vcpkg manifest of the core                          |
| `web/index.html`   | stage, menu, settings and debug dialog                                               |
| `web/player.js`    | the player: loop, regions, media, menu, dialogs, keys, IndexedDB                     |
| `web/player.css`   |                                                                                      |

## Flow

No threads: JS calls `poll()` of the core and plans the next call with `msUntilNext()` (at once after every call into
the core, every 100 ms while downloads run). The core loads with `fetch()` (`FetchTransport`) into its file system.

```
player.js                          bindings.cpp (WASM)
tick() → player.poll()        →    PlayerRuntimeApi::poll() → events as JS objects
  ├── layout      regions anew: divs placed in %, background color and image
  ├── background  a background image that was not there yet
  ├── start       an element per region of the instance: img, video, audio, iframe, div (brush)
  ├── stop        the elements go, blob URLs are released
  ├── pause/resume, repeat
  ├── clearCaches → cachesCleared()
  └── screenshot  → screenshotTaken() without a picture (the browser cannot take one)
mediaFinished / mediaFailed /  →   the end of a video or audio, a file that cannot be played, a click, a key
activated / accessKey
```

- **Files of the cache**: the core gives local paths of its file system (`/garlic/cache/…`). `player.js` reads the
  file from there into a blob and shows it with a blob URL; URLs (uncachable media) are used as they are.
- **Storage**: `/garlic` (configuration, cache, logs, inventory database) is IDBFS: the files are in memory and are
  written to IndexedDB every 10 s, after the settings and when the page goes. At the start they are read back, so the
  cache and the settings survive a reload.
- **Like the other players**: only the topmost medium with a picture is visible, audio covers nothing; a StartMedia is
  shown in all regions with its name, only the first one reports end and errors; fit and mediaAlign as `object-fit`
  and `object-position` (meetBest: `scale-down`).

## Requirements of the servers

The page and the content come from different origins, so every server the player loads from (index, media, reports)
must allow CORS, including the preflight for `If-None-Match`, `If-Modified-Since` and `X-Garlic-User-Agent`, and should
expose `ETag` and `Last-Modified` (`Access-Control-Expose-Headers`); see
[FetchTransport](../../core/docs/downloader.md#fetchtransport). The player identifies itself with
`X-Garlic-User-Agent`, the browser does not let it set `User-Agent`.

## Not yet

- **Widgets** (`application/widget`): they are directories in the cache whose relative links cannot be loaded from
  blob URLs; the core skips them (`supports`).
- **Sound**: browsers block it without a user gesture; a video then plays muted. A kiosk browser can allow it
  (Chromium: `--autoplay-policy=no-user-gesture-required`).
- **Memory**: IDBFS keeps all files in memory as well; many large videos need a lot of it. OPFS would avoid that, but
  needs the core in a worker.
- **Menu over web pages**: clicks into an iframe do not reach the page, there the gesture does not work.
- No HTTP server (a browser cannot accept connections), no screenshot, no launcher (reboot, software installation).
