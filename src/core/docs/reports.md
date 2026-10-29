# reports

Reports to the hub, as the `subscriptions` in the head of the index say. Namespace `garlic::core::reports`.
garlic-lib: `Reporting::*Manager`, `reports/create/*`, `WebDav`.

```cpp
Reporter reporter(transport, fs, clock, logger, config, inventory, cache_dir, [] { return systemInfo(); });
reporter.subscribe(document.head.subscriptions);   // with every new index
// host loop: reporter.poll(); transport.poll(); … sleep until reporter.nextDeadline()
```

| File                  | Content                                                                 |
|-----------------------|-------------------------------------------------------------------------|
| `report_xml.hpp/.cpp` | the XML formats as pure functions: data in, text out                   |
| `reporter.hpp/.cpp`   | `Reporter`: schedules and uploads, one upload at a time per subscription |
| `system_info.hpp`     | `SystemInfo`, `PlayerInfo`: what the system report tells                |

## Subscriptions

Every upload is an HTTP `PUT` with `Content-Type: application/xml` and the user agent of the player.

| `type`               | Content                                   | Target                              | After success          |
|----------------------|-------------------------------------------|-------------------------------------|------------------------|
| `PlaylogCollection`  | `play_log.xml*`, oldest first             | `<action>/playlog-<id>.xml`         | file deleted, next one |
| `EventlogCollection` | `event_log.xml*`, oldest first            | `<action>/event-<id>.xml`           | file deleted, next one |
| `InventoryReport`    | the inventory table: files in the cache   | `<action>`                          | –                      |
| `SystemReport`       | the device: disk, memory, network, config | `<action>`                          | –                      |
| `TaskExecutionReport`| last state of every task, `task_execution_log.xml*` | `<action>`                | – (log kept)           |

`<id>` is a new random UUID with the end of the player UUID, so every log upload has its own name (garlic-lib:
`createUuid().mid(0,23) + uuid.mid(23)`). Other types (`TaskSchedule`) are not reports.

- **When**: at once when a subscription is new or changed, then every `refreshInterval` seconds. An unchanged
  subscription in a new index keeps its schedule; garlic-lib created all managers again and sent at every index.
- **Without `refreshInterval`** (0): only once. garlic-lib started a timer of 0 ms, which fires all the time.
- **After a failure** (transport error, HTTP status not 2xx): the next try after `retryInterval` seconds, without it
  after `refreshInterval`. garlic-lib read `retryInterval` but did not use it.
- **Task states** (`TaskExecutionReport`, new; garlic-lib wrote the log but sent nothing): the hub gives one fixed
  file per player (`task_execution-<uuid>.xml`) and overwrites it with every upload. So the report holds the last
  state of every task (`pending`, `completed`, `aborted`), taken from the rotated task logs (oldest first) and the
  current one, in the order the tasks first appear. The log is not deleted. A task whose log file was rotated away is
  no longer reported.
- **Logs**: the current log is rotated first (`Logger::rotate`), then the oldest rotated file is renamed to
  `<log>.sending` and uploaded. So a rotation of the logger while uploading cannot move it. It is deleted only after
  a successful upload; after a failure it stays and is sent first at the next try. New entries meanwhile go to the
  current log and follow.

## Formats

The same elements as garlic-lib, so the hub reads them as before. The text is written without line breaks; values
are escaped.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<report xmlns="http://schemas.garlic-player.com/gapi-1.0">
  <date>2026-10-28T07:30:00Z</date><version>1.0</version>
  <player id="<uuid>">
    play log:   <contentPlayLog> lines of play_log.xml </contentPlayLog>
    event log:  <playerEventLog> lines of event_log.xml </playerEventLog>
    tasks:      <taskState><task id="…"><lastUpdateTime>local ISO</lastUpdateTime><state>…</state></task> …</taskState>
    inventory:  <systemInfo>…</systemInfo>
                <contentDirectory><objectInfo>
                  <object id="<file name up to the first dot>" persistent="false">
                    <resourceURI/><state/><contentType/><contentLength/><lastModified/><expires/><transferLength/>
                  </object> …
                </objectInfo></contentDirectory>
    system:     <systemInfo> … <network><interface id>…</interface></network>
                  <configuration><modelInfo/><factoryDefault/><userPref/></configuration></systemInfo>
                <hardwareInfo><hardware id="display:0">…</hardware></hardwareInfo>
  </player>
</report>
```

- `<systemInfo>`: `systemStartTime`, `systemTimeZone`, `totalCapacity`, `totalFreeSpace`, `cpuUsage` (empty),
  `memoryTotal`, `memoryUsed`, `hdmiOutput` (empty).
- Inventory `state`: `COMPLETE`, or `TRANSFER` while downloading (garlic-lib wrote `COMPLETE` for both);
  `transferLength` is the size of the file in the cache, of `<file>.part` while downloading.
- The log lines are taken as they are: the logger writes them as escaped XML. garlic-lib parsed them; a log with one
  unescaped `&` sent an empty report and the file was deleted afterwards, so the entries were lost.

## SystemInfo

Disk, memory and network depend on the platform (ESP32 is very different from Linux), so the host gives them as a
`SystemInfo` function. Unknown values stay empty. On the native platforms `system::collectSystemInfo()` fills it
([system](system.md)); on WASM and ESP32 the host does.

## Decisions

- **The formats of garlic-lib**, so the hub reads them as before; without line breaks, because the hub reads the
  reports with an XML parser.
- **An unchanged subscription keeps its schedule.** garlic-lib created all report managers again with every index and
  sent at once (the head parser did that, commented there as "Violation SRP").
- **`retryInterval` is used** after a failure (adapi: the waiting time until the next try); garlic-lib read it and
  waited for the next `refreshInterval`.
- **`refreshInterval` 0 sends once.** garlic-lib started a timer of 0 ms, which fired all the time.
- **Logs are uploaded as `<log>.sending`**, so a rotation of the logger during the upload cannot move the file, and it
  is deleted only after success. What was good in garlic-lib stays: rotate before sending, delete after success, so
  nothing is lost and nothing is sent twice.
- **The log lines are wrapped, not parsed.** garlic-lib parsed them; one unescaped `&` gave an empty report, and the
  file was deleted afterwards.
- **`SystemInfo` comes from the host**, because disk, memory and network are very different per platform (ESP32 and
  Linux).
- **`TaskExecutionReport`**: the hub gives one fixed file per player and overwrites it with every upload, so the
  report holds the last state of every task, read from the task log. The log is the only source: a `pending` survives a
  restart without anything stored in the database, and the log is not deleted after sending. Namespace gapi like the
  other reports (the adapi namespace of the standard is not needed by the hub).

## When something goes wrong

- `debug.log`, category `Reporting`: `upload not started: …` (transport refused) and `upload failed: <action>` (no
  answer or status not 2xx).
- A `play_log.xml.sending` or `event_log.xml.sending` in the log directory is a failed upload; it goes first at the
  next try.
- No reports at all: the index has no `<subscription>`, or its `action` is wrong (values are trimmed, so line breaks
  around the URL do no harm).
- A task missing in the `TaskExecutionReport`: its rotated log file is gone.
- Tests: `tests/reports/test_report_xml.cpp` (formats byte for byte), `test_reporter.cpp` (schedule, retry, `.sending`).
