# common

Shared types and interfaces used by all other modules. Namespace `garlic::core::common`.

## Clock

Header: `common/clock.hpp`

| Method        | Use                                                                    |
|---------------|------------------------------------------------------------------------|
| `now()`       | wall-clock time (UTC): HTTP dates, logs, SMIL timings, `adapi-date()`  |
| `monotonic()` | timeouts and durations; not affected when the system time is changed  |

`SystemClock` is the implementation for the operating system. Tests use `FakeClock` (`tests/support`), which only
moves when the test calls `advance()`. This makes timeouts and SMIL timings testable without waiting.

## FileSystem

Header: `common/file_system.hpp`

Paths are UTF-8 strings with `/` as separator. All methods return `std::expected`, errors are
`FileSystemError{code, message}` with the codes `NotFound`, `PermissionDenied`, `NoSpace`, `IoError`.

| Method                       | Note                                                         |
|------------------------------|--------------------------------------------------------------|
| `exists(path)`               |                                                              |
| `fileSize(path)`             |                                                              |
| `lastModified(path)`         | as `system_clock::time_point`                                |
| `readFile(path)`             | complete content, for small files (configuration, SMIL)      |
| `openForWrite(path, mode)`   | `Truncate` or `Append` (resume); returns a `WritableFile`    |
| `rename(from, to)`           | replaces an existing target                                  |
| `remove(path)`               | `NotFound` if missing                                        |
| `createDirectories(path)`    | existing directories are no error                            |

`WritableFile::write()` takes data in chunks; `close()` flushes and reports whether everything was stored.


### Implementations

| Implementation     | Platform                                              |
|--------------------|-------------------------------------------------------|
| `StdFileSystem`    | Linux, Windows, macOS, Android, iOS (`std::filesystem` + stdio, UTF-8 paths also on Windows) |
| `MemoryFileSystem` | tests and garlic-hub preview; `lastModified` comes from the injected `Clock` |
| OPFS, Tizen, webOS, LittleFS/SD | planned                                  |

The tests in `tests/common/test_file_system.cpp` run identically against every implementation.

## Md5

Header: `common/md5.hpp`

```cpp
std::string name = Md5::hex(url);     // cache file names

Md5 md5;                               // large files in chunks
md5.update(chunk);
std::string hex = Md5::toHex(md5.finish());
```

Own implementation of RFC 1321, because OpenSSL is not available on WASM and ESP32. Not for security purposes.

## Time format

Header: `common/time_format.hpp`

- `isoLocalTime(time)`: local time with offset like Qt::ISODate, `2026-10-28T03:30:56+01:00`, `…Z` at UTC
- `logTimestamp(time)`: `2026-10-28 03:30:56,123` for text logs

Uses the C library for the time zone, because the time zone database of `std::chrono` is not available on every
platform (ESP32, older Android).
