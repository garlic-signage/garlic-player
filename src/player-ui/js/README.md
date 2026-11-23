# js

The browser frontend of the garlic-player on the core. The core runs as WebAssembly in the page (`garlic-core.js`,
`garlic-core.wasm`), the player itself is JavaScript (`web/`). It behaves like the [sdl](../sdl/README.md) player: the same
menu, dialogs and shortcuts.

Only with Emscripten (see [the core](../../core/README.md#pinned-versions)) and vcpkg:

```
scripts/player-builds/wasm.sh                           # release; debug: wasm.sh debug
scripts/player-builds/wasm.sh release ~/garlic-hub      # and copies it to <hub>/public/js/player/preview/

cd src/player-ui/js                                     # or by hand
cmake --preset wasm-release && cmake --build --preset wasm-release
```

`build/wasm-release` holds everything to deploy: `index.html`, `player.css`, the modules `*.js`, `sw.js`,
`garlic-core.js`, `garlic-core.wasm`.

### Running it

The player needs a web server; from `file://` the browser loads neither the modules nor the WASM. Any server for
static files will do, e.g.:

```
npx serve build/wasm-release -l 8800                   # Node
php -S localhost:8800 -t build/wasm-release            # PHP
python3 -m http.server -d build/wasm-release 8800      # Python
```

Then open `http://localhost:8800/` (or with `?index=https://hub/index.smil`). garlic-hub, nginx or Apache serve it as
well; `.wasm` should come as `application/wasm`, otherwise the browser compiles it more slowly.

**A secure origin is needed: `https://` or `localhost`.** Only there the browser starts the service worker, which
serves web pages and widgets from the cache. Over plain `http://` with an IP address or a host name (e.g. a NAS in the
local network) the player runs, but shows no widgets and no web pages from the cache.

## URL parameters, menu, keys

- `index=<URI>`: the SMIL index, like `SMIL_INDEX` on the command line; empty: the one in the settings.
- `storage=memory`: the cache only in memory, gone with the page (e.g. for a preview). Default: kept in IndexedDB.
- `uuid=<uuid>&name=<player name>`: they are written into the configuration before the start, so the player asks the
  hub as this player and gets its playlist.
- `preview=1`: preview in garlic-hub. The core loads only index and media: it sends no reports and fetches no tasks.
  There is no menu (neither ten clicks nor holding) and of the shortcuts only Ctrl+D (information); accesskeys and
  clicks on media work as in the player.

## Preview in garlic-hub

garlic-hub opens the preview from the context menu of its player list ("Preview", only for a player with a playlist)
in an overlay: `/js/player/preview/index.html?index=<hub>/async/player/preview-index/<id>&uuid=…&name=…&storage=memory&preview=1`. The files of
`build/wasm-release` go to `public/js/player/preview/` of the hub. Index and media come from the same origin, so CORS
plays no part; the agent goes in `X-Signage-Agent`, which the hub reads for `/smil-index` and `delivery.php`. The page
is a static file, the headers of the hub (CSP without `wasm-unsafe-eval`) do not apply to it. The preview index of
the hub needs the login and leaves no trace: the player is not shown as online, and the preview sends no reports.

Ten fast clicks or touches (less than 500 ms apart) or one held for 10 s open the menu with Settings, Information and
Exit. Keys: Ctrl+F full screen, Ctrl+B full screen too (over all screens is not possible in a browser), Ctrl+S
settings, Ctrl+D information, Ctrl+Q quit; every other key is an accesskey for the SMIL. Without an index the settings
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
  and `object-position` (meetBest: `scale-down`), for images and videos alike (SMIL standard; the legacy players
  always centered videos and kept their aspect inside).

## Requirements of the servers

The page and the content come from different origins, so every server the player loads from (index, media, reports)
must allow CORS, including the preflight for `If-None-Match`, `If-Modified-Since` and `X-Signage-Agent`, and should
expose `ETag` and `Last-Modified` (`Access-Control-Expose-Headers`); see
[FetchTransport](../../core/docs/downloader.md#fetchtransport). The player identifies itself with
`X-Signage-Agent`, the browser does not let it set `User-Agent`.

## Not yet

- **Loading**: like the sdl player, "Loading 2 / 7" and a bar while downloads run, only until the first medium of a
  layout.
- **Sound**: browsers block it without a user gesture; a video then plays muted. A kiosk browser can allow it
  (Chromium: `--autoplay-policy=no-user-gesture-required`).
- **Memory**: IDBFS keeps all files in memory as well; many large videos need a lot of it. OPFS would avoid that, but
  needs the core in a worker.
- **Menu over web pages**: clicks into an iframe do not reach the page, there the gesture does not work.
set min values for - No HTTP server (a browser cannot accept connections), no screenshot, no launcher (reboot, software installation);
  on signage displays see below.

## Signage displays (LG webOS, Samsung Tizen, BrightSign, ChromeOS) - planned

As a web page in the URL launcher of the display the player runs already, but it cannot be maintained: no restart,
no screen off, no update, no master volume. So on these displays it comes as a **packaged app** (LG: IPK, Samsung:
WGT), signed and installed as signage app. Only then the platform gives its JavaScript APIs for the hardware.

No separate launcher is needed, like on Philips: the firmware has the functions itself. Unlike Philips (broadcasts,
SICP on a socket) they are JavaScript in the same app. One small adapter per platform maps the supervisor of the
core (restart, installation, screen off/on, deep standby, master volume, brightness;
[supervisor](../../core/docs/player_runtime.md#supervisor)) to the API, like `PhilipsLauncher` on Android maps it
to SICP.

| Platform           | API                                        | Rights                                                    |
|--------------------|--------------------------------------------|-----------------------------------------------------------|
| LG webOS Signage   | SCAP (Signage Custom API), e.g. `Power.executePowerCommand` for the restart; screen, on/off timers, volume, time and time zone, app update | installed as signage app |
| Samsung Tizen SSSP | `b2bapis` (e.g. `b2bcontrol`): restart, panel off, serial number, timers, network | privileges in the `config.xml` of the app, some at partner level |

- **Minimum versions: Chromium 85** in the engine of the display. Emscripten 6.0.13 builds for
  `MIN_CHROME_VERSION` 85 by default (`src/settings.js` of the emsdk; the build sets no other), the JavaScript in
  `web/` needs Chromium 84 (private methods `#name()`) and 80 (`??`, `?.`). That means, by the versions as far as
  known here (to be checked against the documents of the makers):

  | Platform | Chromium | |
  |---|---|---|
  | Tizen 5.5 (models of 2020) | ~69 | too old |
  | Tizen 6.0 (2021) | ~76 | too old |
  | **Tizen 6.5 (2022)** | ~85 | **minimum** |
  | Tizen 7.0 (2023) | ~94 | |
  | webOS 5 (2020) | ~68 | too old |
  | webOS 6 (2021) | ~79 | too old |
  | **webOS 22** (and the signage release with the same engine) | ~87 | **minimum** |
  | webOS 23 / 24 | ~94 / ~108 | |

  Roughly displays from 2022 on. Older ones would need a lower `MIN_CHROME_VERSION` and the JavaScript without
  private methods, `??` and `?.`; not planned.
- The names above are from memory, not from documents here: check them against the SDK of each platform and its
  firmware versions before building the adapter.
- The plain browser of the display has none of these APIs: the packaged app is the condition, not an option.
- Nothing of it is built yet.

### BrightSign

Media players of their own, common in signage. BrightSign OS is a closed Linux: no Android, and no native programs
of others (extensions only signed by BrightSign), so neither the Android nor the sdl player; the WASM player is the
way. Its HTML engine is Chromium, above 85 in the newer OS versions (8.x/9.x, to be checked).

- The page runs in a `roHtmlWidget`, started by a small `autorun.brs` (BrightScript) on the SD card. That is a start
  script in the package, not a launcher app.
- The hardware through JavaScript: Node.js modules `@brightsign/…` (system, restart, video output, …). Like on LG and
  Samsung a third adapter for the supervisor; analogous to Philips, no separate launcher.
- Updates over BSN.cloud, or the adapter writes the new files to the SD card and restarts.

### ChromeOS

Chromeboxes, or ChromeOS Flex on older PCs, in kiosk mode: always a current Chromium. ChromeOS runs Android apps
too (ARC, Managed Google Play), but not as signage: no device owner on the Android side (so no garlic-launcher),
Android kiosk apps have been cut back by Google, video goes through one more layer. So the WASM player as kiosk web
app. Restarts, updates and their schedule are done by the Google Admin console, not by the player; an adapter is
hardly needed.

The details of BrightSign and ChromeOS are from memory as well; check them before building.
