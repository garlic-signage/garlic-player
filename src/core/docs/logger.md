# logger

Logs of the player. Namespace `garlic::core::logger`.

```cpp
LoggerConfig cfg;
cfg.log_dir = config.logDir();                    // given by the frontend
cfg.mirror  = [](Severity, std::string_view, std::string_view text) { std::cerr << text << "\n"; };
Logger logger{fs, clock, cfg};

logger.warning("Database", "inventory could not be opened");
logger.event(Severity::Warning, "ContentManager", "MEDIA_UNSUPPORTED", {{"resourceURI", url}});
logger.played(content_id, start_time);
logger.taskState(task_id, "completed");
```

No singleton: the logger is passed to the modules like everything else. Each entry is appended and the file is
closed again, so nothing is lost on a crash or power cut. Write errors are ignored, logging never stops the player.

## Files

Same names and formats as garlic-lib; event and play logs are sent to the server.

| File                     | Method                         | Content                                     |
|--------------------------|--------------------------------|---------------------------------------------|
| `debug.log`              | `debug/info/warning/error/log` | `2026-10-28 03:30:56,123 warning Database file.cpp(42) text` |
| `event_log.xml`          | `event`                        | `<event><eventType>…</eventType><eventTime>…</eventTime><eventSource>…</eventSource><eventName>…</eventName><metadata><meta name="…" content="…"/></metadata></event>` |
| `play_log.xml`           | `played`                       | `<contentPlayed><contentId>…</contentId><startTime>…</startTime><endTime>…</endTime></contentPlayed>` |
| `task_execution_log.xml` | `taskState`                    | `<taskState><task id="…"><lastUpdateTime>…</lastUpdateTime><state>…</state></task></taskState>` |

- Times in the XML logs are local time with offset like Qt::ISODate: `2026-10-28T03:30:56+01:00`, `…Z` at UTC
  (`common/time_format.hpp`).
- Severities: `debug`, `informational`, `warning`, `error`, `critical`.
- One entry per line; the report sender wraps the lines into a root element.

## Rotation

A file reaching `max_size` (1 MiB, `debug.log` 10 MiB) is renamed to `name.1`, older ones move up to `name.30`, the
oldest is deleted. `rotate(file)` does the same on demand, e.g. before a log is sent, so sent entries are not sent
again.

## Settings (`LoggerConfig`)

| Field                | Default   | Meaning                                              |
|----------------------|-----------|------------------------------------------------------|
| `log_dir`            | —         | directory of the log files                           |
| `max_size`           | 1 MiB     | rotation size of the XML logs                        |
| `debug_max_size`     | 10 MiB    | rotation size of `debug.log`                         |
| `max_files`          | 30        | rotated files kept                                   |
| `min_debug_severity` | `Debug`   | lower entries are not written to `debug.log` (e.g. `Warning` on ESP32) |
| `mirror`             | none      | every entry also goes here (console, logcat)         |

## Differences to garlic-lib

- Values in the XML logs are escaped. garlic-lib wrote them raw, so e.g. a URL with `&` made the log invalid XML.
- Qt's own messages (`qt.*`, `qtdebug.log`) belong to the Qt frontends now.
- Free text in event categories (`qWarning(MediaControl) << "text"`) becomes a regular event or a debug.log entry
  when the callers are ported.
