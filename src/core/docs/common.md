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
| `lastAccess(path)`           | last read (atime), for the cache cleanup; without atime the creation |
| `list(path)`                 | names and directory flag of a directory, no `.`/`..`, no order |
| `readFile(path)`             | complete content, for small files (configuration, SMIL)      |
| `openForWrite(path, mode)`   | `Truncate` or `Append` (resume); returns a `WritableFile`    |
| `rename(from, to)`           | file or directory; replaces an existing target file          |
| `remove(path)`               | `NotFound` if missing                                        |
| `removeAll(path)`            | file or directory with everything in it; missing is no error |
| `createDirectories(path)`    | existing directories are no error                            |

`WritableFile::write()` takes data in chunks; `close()` flushes and reports whether everything was stored.


### Implementations

| Implementation     | Platform                                              |
|--------------------|-------------------------------------------------------|
| `StdFileSystem`    | Linux, Windows, macOS, Android, iOS (`std::filesystem` + stdio, UTF-8 paths also on Windows) |
| `MemoryFileSystem` | tests and garlic-hub preview; times come from the injected `Clock`, `readFile` sets `lastAccess`; directories are derived from the paths |
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
- `toLocalTime(time)`: the local wall time of a moment as `std::chrono::local_seconds`, e.g. for SMIL wallclocks

Uses the C library for the time zone, because the time zone database of `std::chrono` is not available on every
platform (ESP32, older Android).

## Decisions

- **`Clock` has two times.** `now()` for everything that is a date (HTTP, logs, SMIL); `monotonic()` for timeouts, so
  a change of the system time (NTP, the user) never fires a timeout by mistake. `FakeClock` lets tests run 30 minutes
  of playlist in milliseconds.
- **An own `FileSystem` interface** instead of writing with `std::filesystem` directly: the ESP32 writes to SD/LittleFS,
  the browser to OPFS, Tizen and webOS through their own APIs. `MemoryFileSystem` serves the tests and the garlic-hub
  preview (nothing to keep there). Every implementation runs through the same tests, so they behave alike.
- **An own MD5**, because OpenSSL is not available on WASM and ESP32. It takes data in chunks, so a large file never
  has to be in RAM.
- **Time zones through the C library** (`localtime_r`), because the time zone database of `std::chrono` is missing on
  the ESP32 and older Android. `toLocalTime` gives `local_seconds`, wall time without zone, so the SMIL wallclocks
  need no tz database either.
- **`lastAccess()`** exists for the cache cleanup (the longest unread file goes first), like `QFileInfo::lastRead` in
  garlic-lib, with the same weakness: see below.

## When something goes wrong

- Errors are `FileSystemError{code, message}`; `NoSpace` comes from the operating system, the cache cleanup works
  with `system::diskSpace` instead.
- `lastAccess()` is the atime: with `relatime` updated at most once a day, with `noatime` never. Then the cache
  cleanup removes in the order of the downloads, not of the last use.
- The Windows branches of `StdFileSystem` (`_wfopen`, `_wstat64`) have not been compiled yet.
- Tests: `tests/common/test_file_system.cpp` (one contract for both file systems), `test_md5.cpp` (values of RFC 1321
  and the block borders 55/56/64), `test_time_format.cpp` (UTC, UTC+1, UTC−5 with the day before, UTC+5:30).
