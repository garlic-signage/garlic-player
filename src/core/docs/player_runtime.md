# player_runtime

`PlayerRuntimeApi` is the one interface of the core for the frontends (QWidget, QML, Java, JS, ESP32). Namespace
`garlic::core`. garlic-lib: `LibFacade`. It owns all modules, connects them and is driven by `poll()` from one
thread. The frontend only shows: it gets events and reports what only it knows. It never talks to the downloader,
the engine or the database directly.

```cpp
StdFileSystem fs;           // WASM: OPFS, ESP32: LittleFS …
SystemClock   clock;
CurlTransport http;         // WASM: Fetch, ESP32: esp_http_client

PlayerRuntimeConfig config;
config.ini_path  = …;  config.cache_dir = …;  config.log_dir = …;
config.version   = "2.0";
config.index_uri = command_line_uri;                          // empty: the one in the ini
config.supports  = [](const smil::Media &m) { return …; };    // what the frontend can show
config.random_seed = std::random_device{}();

PlayerRuntimeApi runtime(fs, clock, http, config);
if (auto started = runtime.start(); !started) … started.error()

while (running)
{
    for (const CoreEvent &event : runtime.poll())
        std::visit(frontend, event);
    wait until runtime.nextDeadline() or input
}
// input: runtime.mediaFinished(id), mediaFailed(id), elementActivated(id), accessKey('a')
```

## Interface

| Frontend → core            | Meaning                                                           |
|----------------------------|-------------------------------------------------------------------|
| `start()`                  | load the configuration, open the database, start the HTTP server and the index |
| `poll()`                   | work off what is due, return the events                           |
| `nextDeadline()`           | when `poll()` is needed next                                      |
| `mediaFinished(id)`        | a video or audio ended by itself                                  |
| `mediaFailed(id)`          | a medium could not be played                                      |
| `elementActivated(id)`     | touch or click on a medium                                        |
| `accessKey(key)`           | a key was pressed                                                 |
| `notify(name)`             | a notification for `notify(…)` triggers                           |
| `screenshotTaken(path)`    | the JPEG of a `Screenshot` event is saved: the REST API answers   |
| `playerSettings()`         | what the settings dialog shows: player name, index URI, HTTP server on/off and port |
| `changePlayerSettings(s)`  | check and store them; error text of the core if not valid         |
| `debugInfo()`              | resources and identity for the debug dialog                       |
| `configuration()`          | read and change any setting                                       |
| `stop()`                   | stop the playback (standby, quitting); the `StopMedia` come with the next `poll()` |

`poll()` returns **one ordered list** of `CoreEvent`:

| Event                                   | Meaning                                                     |
|-----------------------------------------|-------------------------------------------------------------|
| `StartMedia`, `StopMedia`, `RepeatMedia`, `PauseMedia`, `ResumeMedia` | from the engine ([smil](smil.md#engine)) |
| `NewLayout{document, backgrounds}`      | a new index: build the regions anew (`document->head`); background images that are there |
| `BackgroundImage{src, path}`            | a background image of the current layout is downloaded      |
| `InstallSoftware{path}`                 | a firmware update is ready ([tasks](tasks.md))              |
| `Reboot{task_id}`                       | restart the device: a task, or `"DailyReboot"` from the schedule |
| `ClearCaches{player, web}`              | the core has cleared its cache if `player`; the host clears the web cache and restarts |
| `Screenshot{path}`                      | REST `/v2/task/screenshot`: save a JPEG of the screen to `path`, then `screenshotTaken(path)` |

One list instead of several, because the order counts: at a new index first the `StopMedia` of the old one, then
`NewLayout`, then the `StartMedia` of the new one.

**The document of `NewLayout`** is a `shared_ptr<const smil::Document>`: the frontend keeps it until the next
`NewLayout`, because `StartMedia::media` points into it. So the old document lives exactly as long as the frontend can
still read it, also when the core runs in another thread.

**Background images** (`backgroundImage` of root-layout and regions) are downloaded like media, but the index does not
wait for them. `NewLayout::backgrounds` maps the value of the SMIL to the local path (or URL, if not cacheable) of the
ones that are already there; each other one follows as `BackgroundImage` when it is downloaded. `none`, `inherited`
and empty values are no images.

## Inside

```
start()
├── configuration (version, cache and log directory, index URI from the command line)
├── inventory database <log_dir>/garlic.db (database_path)
├── download queue, IndexManager, MediaManager, Reporter, TaskScheduler (need the loaded configuration)
├── tasks.checkPending()            a restart or firmware update from before is confirmed
├── configuration_file (-c)         applied like the task updateSettings; then the index URI of the command line
│                                   again (it wins) and the user agent anew; missing or broken: an event only
├── HTTP server with the REST API, if httpd/enabled (and built with libmicrohttpd)
├── restart schedule
└── index.start()

poll()
├── transport, download queue       results: index → IndexManager, task files → TaskScheduler, else → MediaManager
├── IndexManager, Reporter, HTTP server
├── scheduled restart due?          → Reboot{"DailyReboot"}
├── every 5 minutes                 disk and memory to debug.log (garlic-lib: ResourceMonitor)
└── engine                          → StartMedia …

new valid index
├── the old engine stops            → StopMedia
├── document and engine are swapped; the old document lives on in the NewLayout the frontend still keeps
├── media.load(), reporter.subscribe(), tasks.subscribe()
├── NewLayout with the background images that are there; the others are remembered for BackgroundImage
└── the new engine starts
```

- **Tasks**: `ApplyConfiguration` loads the index again and plans the restart anew (the core does it).
  `ClearCaches` with `player`: the core empties the cache directory and the inventory table, then the event goes to
  the host. `InstallSoftware` and `Reboot` go to the host.
- **Restart schedule** like garlic-lib (`tools/reboot`): `schedule/reboot_days` English weekdays separated by spaces,
  `schedule/reboot_time` `HH:mm:ss` (03:00 if missing), the next time within a week (`nextReboot()`, tested).
- **nextDeadline()**: the earliest time of engine, index, reports, restart and resource log. While downloads run or
  the HTTP server is on at most 100 ms, because curl and libmicrohttpd run without threads of their own. An ESP32
  without HTTP server can really sleep.
- **System information**: `config.system_info` for the reports; empty means `system::collectSystemInfo` where the
  module exists (native platforms). WASM and ESP32 give their own.
- **Space in the cache**: `config.free_space` gives the free bytes of the cache's disk; empty means
  `system::diskSpace` where the module exists. With it the downloader, the widgets and `/v2/files/new` remove the
  longest unread files when the disk is full ([content](content.md#space-in-the-cache-cachespace)).
- **One thread**: everything runs in the thread that calls `poll()`; `ThreadedRuntime` moves that into a thread of
  its own (below).
- Playlists from the hub repeat their body (`repeatCount="indefinite"`): media that are still downloading at the
  first start are skipped and follow in a later run, like in garlic-lib.

- **REST API**: its actions go to the runtime: `notify` to the engine, `reboot` and `screenshot` as events,
  `app/start` and `app/switch` load the stored index again, `app/exec` plays a temporary index
  ([rest_api](rest_api.md)).

## Settings dialog

The frontends only show the fields; reading, checking and storing is done here (garlic-lib: `ConfigDialog`, the same
for QWidget and QML). Standby and restarts come from the CMS and are not in the dialog.

```
changePlayerSettings({player_name, index_uri, httpd_enabled, httpd_port})
├── index URI checked (Configuration::validateIndexUri)   invalid: its error text, nothing stored
├── port 1..65535 if the server is on                     otherwise "invalid port"
├── HTTP server changed? the new one starts first, then the old one stops
│     port in use: error text, nothing stored, the old server runs on
├── stored: player_name, index_uri, httpd/enabled, httpd/port
├── user agent of the downloads anew (the reports read it at every upload)
└── index loaded again (garlic-lib: initParser after the dialog, also without a change)
```

Running requests of a stopped server get 503, like when the player quits.

## Debug dialog

`debugInfo()` gives what the debug dialog of garlic-lib showed (DebugInfos, ResourceMonitor): title of the index, user
agent, disk of the cache and memory of the system (from `system_info`), memory and threads of the player
(`system::processInfo`) with their maxima since the start and the time of the memory maximum. The maxima are updated at
every call and with the resource log every 5 minutes, which now writes them to debug.log too. Without the system module
(WASM, ESP32) the player values stay 0 and the others empty, unless the frontend gives a `system_info`.

## ThreadedRuntime

Header: `player_runtime/threaded_runtime.hpp`. For native frontends whose UI thread must not wait for the core
(downloads, SQLite, HTTP server). The `PlayerRuntimeApi` is created, started, polled and destroyed in one thread of
the helper, so curl, libmicrohttpd and SQLite still see only one thread. WASM (no threads) and ESP32 (own loop) use
the `PlayerRuntimeApi` directly.

```cpp
ThreadedRuntime runtime(fs, clock, http, config, [&] { /* in the core thread: wake the UI thread */ });
if (auto started = runtime.start(); !started) … started.error()      // waits for PlayerRuntimeApi::start()

runtime.post([id](PlayerRuntimeApi &r) { r.mediaFinished(id); });   // from any thread, runs before the next poll()
for (const CoreEvent &event : runtime.takeEvents()) …                 // in the UI thread after the wake
runtime.stop();                                                       // also the destructor: ends and joins the thread
```

```
core thread
├── PlayerRuntimeApi runtime(…); runtime.start()   → result to start(), an error ends the thread
└── until stop():
    ├── the posted calls, in their order
    ├── runtime.poll() → events appended under the mutex, then wake() if there are new ones
    └── wait until nextDeadline() (measured with the runtime's clock) or until post()/stop()
```

- Every call of the frontend goes through `post()`, also `configuration()`; nothing returns a value directly. A
  result goes back the way the frontend chooses (e.g. a promise or a queued signal in the call).
- `wake` runs in the core thread; with Qt e.g. `QMetaObject::invokeMethod(…, Qt::QueuedConnection)`.
- `fs`, `clock` and `transport` are used only by the core thread after `start()`.
- After `stop()` the events not taken can still be taken; `start()` again creates a new runtime.

## Not yet

- Windows and macOS: `platform_windows.cpp` and `platform_apple.cpp` are not compiled yet.
