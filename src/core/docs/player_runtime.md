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
// input: runtime.mediaFinished(id), mediaFailed(id), elementActivated(id), accessKey('a'), locate(src)
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
| `locate(src)`              | where a background image of the layout is                         |
| `configuration()`          | read and change settings, e.g. in a settings dialog               |
| `stop()`                   | stop the playback (standby, quitting); the `StopMedia` come with the next `poll()` |

`poll()` returns **one ordered list** of `CoreEvent`:

| Event                                   | Meaning                                                     |
|-----------------------------------------|-------------------------------------------------------------|
| `StartMedia`, `StopMedia`, `RepeatMedia`, `PauseMedia`, `ResumeMedia` | from the engine ([smil](smil.md#engine)) |
| `NewLayout{head}`                       | a new index: build the regions anew (`head->root_layout`, `head->regions`) |
| `InstallSoftware{path}`                 | a firmware update is ready ([tasks](tasks.md))              |
| `Reboot{task_id}`                       | restart the device: a task, or `"DailyReboot"` from the schedule |
| `ClearCaches{player, web}`              | the core has cleared its cache if `player`; the host clears the web cache and restarts |
| `Screenshot{path}`                      | REST `/v2/task/screenshot`: save a JPEG of the screen to `path`, then `screenshotTaken(path)` |

One list instead of several, because the order counts: at a new index first the `StopMedia` of the old one, then
`NewLayout`, then the `StartMedia` of the new one.

## Inside

```
start()
├── configuration (version, cache and log directory, index URI from the command line)
├── inventory database <log_dir>/garlic.db (database_path)
├── download queue, IndexManager, MediaManager, Reporter, TaskScheduler (need the loaded configuration)
├── tasks.checkPending()            a restart or firmware update from before is confirmed
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
├── document and engine are swapped; the old document lives until the next poll(), so a frontend that kept
│   media pointers can still handle its StopMedia
├── media.load(), reporter.subscribe(), tasks.subscribe()
└── NewLayout, the new engine starts
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
- **One thread**: everything runs in the thread that calls `poll()`. A helper that runs the core in a thread of its
  own follows later as a small separate part.
- Playlists from the hub repeat their body (`repeatCount="indefinite"`): media that are still downloading at the
  first start are skipped and follow in a later run, like in garlic-lib.

- **REST API**: its actions go to the runtime: `notify` to the engine, `reboot` and `screenshot` as events,
  `app/start` and `app/switch` load the stored index again, `app/exec` plays a temporary index
  ([rest_api](rest_api.md)).

## Not yet

- Sending the task execution log, cleaning up the cache when space is short.
