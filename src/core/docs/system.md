# system

System information on the native platforms (Linux, Android, Windows, macOS, iOS). Namespace `garlic::core::system`.
garlic-lib: `SystemInfos` (`DiscSpace`, `Memory`, `Network`, `General`), `ResourceMonitor`, start time and time zone
in `MainConfiguration`. Not built for WASM and ESP32; there the host fills `reports::SystemInfo` itself.

```cpp
Reporter reporter(…, [&] { return system::collectSystemInfo(cache_dir, start_time); });
auto disk = system::diskSpace(cache_dir);   // also for cleaning up the cache later
auto self = system::processInfo();          // for the debug display of a frontend
```

| Value                    | Source                                                                  |
|--------------------------|-------------------------------------------------------------------------|
| disk total / free        | `std::filesystem::space()` of the cache directory; 96 % count as total, 4 % stay for the system (like garlic-lib) |
| memory total / used      | Linux/Android `/proc/meminfo`, Windows `GlobalMemoryStatusEx`, macOS/iOS `sysctl` + `host_statistics64` |
| memory and threads of the player | `/proc/self/status`, `GetProcessMemoryInfo` + Toolhelp, `task_info` + `task_threads` |
| network interfaces       | `getifaddrs` (Linux, Apple), `GetAdaptersAddresses` (Windows)           |
| time zone                | IANA name: Linux `TZ`, `/etc/localtime`, `/etc/timezone`; Android `persist.sys.timezone`; Windows through ICU; Apple `CFTimeZone` |
| locale                   | `LC_ALL`/`LC_MESSAGES`/`LANG`; Android `persist.sys.locale`; Windows `GetUserDefaultLocaleName`; Apple `CFLocale` |
| start time               | given by the host (ISO 8601, local time with offset)                   |

- **Memory used = total − available.** Linux and Android use `MemAvailable` (`MemFree` on kernels before 3.14),
  Apple free + inactive + purgeable pages. garlic-lib took `MemFree`, so the file cache of the kernel counted as used
  and a device looked full nearly always.
- **Network**: like garlic-lib (`isRealInterface`) only interfaces with a MAC and at least one address, the loopback
  interface included. MAC in upper case, type names of Qt (`ethernet`, `wifi`, `loopback`, `virtual`, `Ppp` …); on
  Linux `wifi` if `/sys/class/net/<name>/wireless` exists. IPv6 link-local addresses get `%<interface>` like
  `QHostAddress`.
- **Locale** like `QLocale::name()`: `de_DE`; `C` for `C`/`POSIX` or nothing set.
- One `platform_*.cpp` per system, chosen by CMake: `platform_linux.cpp` (Linux, Android), `platform_windows.cpp`,
  `platform_apple.cpp`, `platform_none.cpp` (other systems: empty values). The text parsing (`parseMeminfo`,
  `parseProcStatus`, `normalizeLocale`, `zoneName`) is in `system_info.cpp` and tested on every platform.
- `platform_windows.cpp` and `platform_apple.cpp` have not been compiled yet; they are checked with the first
  Windows and Apple build.
