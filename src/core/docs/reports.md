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
| `report_xml.hpp/.cpp` | the four XML formats as pure functions: data in, text out              |
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

`<id>` is a new random UUID with the end of the player UUID, so every log upload has its own name (garlic-lib:
`createUuid().mid(0,23) + uuid.mid(23)`). Other types (`TaskSchedule`) are not reports.

- **When**: at once when a subscription is new or changed, then every `refreshInterval` seconds. An unchanged
  subscription in a new index keeps its schedule; garlic-lib created all managers again and sent at every index.
- **Without `refreshInterval`** (0): only once. garlic-lib started a timer of 0 ms, which fires all the time.
- **After a failure** (transport error, HTTP status not 2xx): the next try after `retryInterval` seconds, without it
  after `refreshInterval`. garlic-lib read `retryInterval` but did not use it.
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
`SystemInfo` function. Unknown values stay empty. The module for system information will provide it for the native
platforms.
