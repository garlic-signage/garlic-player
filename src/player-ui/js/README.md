# js

The browser frontend of the garlic-player on the core. The core runs as WebAssembly in the page (`garlic-core.js`,
`garlic-core.wasm`), the player itself is JavaScript (`web/`). It behaves like the [qwidget](../qwidget/README.md) and
[qml](../qml/README.md) players: the same menu, dialogs and shortcuts.

Only with Emscripten (see [the core](../../core/README.md#pinned-versions)) and vcpkg:

```
src/player-ui/js/build.sh                               # release; debug: build.sh debug
src/player-ui/js/build.sh release ~/garlic-hub          # and copies it to <hub>/public/player-preview/

cd src/player-ui/js                                     # or by hand
cmake --preset wasm-release && cmake --build --preset wasm-release
python3 -m http.server -d build/wasm-release 8800      # any web server; file:// does not work
```

Then open `http://localhost:8800/` (or with `?index=https://hub/index.smil`). `build/wasm-release` holds everything to
deploy: `index.html`, `player.css`, the modules `*.js`, `sw.js`, `garlic-core.js`, `garlic-core.wasm`. Over https or
on localhost: the service worker for web pages and widgets needs a secure origin.

## URL parameters, menu, keys

- `index=<URI>`: the SMIL index, like `SMIL_INDEX` on the command line; empty: the one in the settings.
- `storage=memory`: the cache only in memory, gone with the page (e.g. for a preview). Default: kept in IndexedDB.
- `uuid=<uuid>&name=<player name>`: preview of a player. They are written into the configuration before the start, so
  the player asks the hub as this player and gets its playlist; everything else works as in the standalone player.

## Preview in garlic-hub

garlic-hub opens the preview from the context menu of its player list ("Preview", only for a player with a playlist)
in a new tab: `/player-preview/index.html?index=<hub>/smil-index&uuid=…&name=…&storage=memory`. The files of
`build/wasm-release` go to `public/player-preview/` of the hub. Index and media come from the same origin, so CORS
plays no part; the agent goes in `X-Signage-Agent`, which the hub reads for `/smil-index` and `delivery.php`. The page
is a static file, the headers of the hub (CSP without `wasm-unsafe-eval`) do not apply to it. The hub counts the
preview as the player itself (last access, reports).

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
| `web/player.js`    | `GarlicPlayer`: start, loop, keys, cursor, end                                       |
| `web/stage.js`     | `Stage`, `Region`, `Media`: what is shown                                            |
| `web/dialogs.js`   | `Menu` with its gesture, `SettingsDialog`, `DebugDialog`                             |
| `web/storage.js`   | `Storage`: file system of the core, IndexedDB, URLs of the cached files              |
| `web/sw.js`        | service worker: the cache for web pages and widgets                                  |
| `web/player.css`   |                                                                                      |

The JS is ECMAScript 2022 modules, classes with private fields and methods.

## Flow

No threads: JS calls `poll()` of the core and plans the next call with `msUntilNext()` (at once after every call into
the core, every 100 ms while downloads run). The core loads with `fetch()` (`FetchTransport`) into its file system.

```
player.js                          bindings.cpp (WASM)
tick() → player.poll()        →    PlayerRuntimeApi::poll() → events as JS objects
  ├── layout      regions anew: divs placed in %, background color and image
  ├── background  a background image that was not there yet
  ├── start       an element per region of the instance: img, video, audio, iframe (web, widget), div (brush)
  ├── stop        the elements go, blob URLs are released
  ├── pause/resume, repeat
  ├── clearCaches → cachesCleared()
  └── screenshot  → screenshotTaken() without a picture (the browser cannot take one)
mediaFinished / mediaFailed /  →   the end of a video or audio, a file that cannot be played, a click, a key
activated / accessKey
```

- **Files of the cache**: the core gives local paths of its file system (`/garlic/cache/…`). Images, video and audio
  are read from there into a blob and shown with a blob URL. Web pages and widgets need their relative links (scripts,
  styles, images of the extracted widget), so they get `garlic-fs/garlic/cache/…/index.html?query`: the service worker
  (`sw.js`) answers these requests and asks the player page for the file through a `MessageChannel`. URLs (uncachable
  media) are used as they are.
- **Storage**: `/garlic` (configuration, cache, logs, inventory database) is IDBFS: the files are in memory and are
  written to IndexedDB at the start, every 10 s, after the settings and when the page goes. At the start they are read
  back, so the cache and the settings survive a reload. A file downloaded in the last 10 s before the browser is
  killed is loaded again at the next start.
- **Like the other players**: only the topmost medium with a picture is visible, audio covers nothing; a StartMedia is
  shown in all regions with its name, only the first one reports end and errors; fit and mediaAlign as `object-fit`
  and `object-position` (meetBest: `scale-down`).

## Requirements of the servers

The page and the content come from different origins, so every server the player loads from (index, media, reports)
must allow CORS, including the preflight for `If-None-Match`, `If-Modified-Since` and `X-Signage-Agent`, and should
expose `ETag` and `Last-Modified` (`Access-Control-Expose-Headers`); see
[FetchTransport](../../core/docs/downloader.md#fetchtransport). The player identifies itself with
`X-Signage-Agent`, the browser does not let it set `User-Agent`.

## Not yet

- **Sound**: browsers block it without a user gesture; a video then plays muted. A kiosk browser can allow it
  (Chromium: `--autoplay-policy=no-user-gesture-required`).
- **Memory**: IDBFS keeps all files in memory as well; many large videos need a lot of it. OPFS would avoid that, but
  needs the core in a worker.
- **Menu over web pages**: clicks into an iframe do not reach the page, there the gesture does not work.
- No HTTP server (a browser cannot accept connections), no screenshot, no launcher (reboot, software installation).
