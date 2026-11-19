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
| CPU time of the player   | with the processes below it (CEF): Linux `/proc/<pid>/stat` of all processes, Windows Toolhelp + `GetProcessTimes`, macOS `getrusage` + `proc_listchildpids` + `proc_pid_rusage`, iOS `getrusage` |
| temperatures             | °C of CPU, RAM and SSD: Linux and Android `/sys/class/hwmon/*/temp*_input` and `/sys/class/thermal/thermal_zone*`; not on Windows and Apple |
| throttling               | Raspberry Pi `vcgencmd get_throttled`, Intel `thermal_throttle/*_throttle_count`, Android `AThermal_getCurrentThermalStatus` (11+), Windows `CallNtPowerInformation`, Apple `NSProcessInfo.thermalState`; not on AMD |
| CPU time of the system   | busy and total of all cores: Linux first line of `/proc/stat`, Windows `GetSystemTimes`, macOS/iOS `host_statistics` (`HOST_CPU_LOAD_INFO`); not on Android |
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
- **CPU time** (`ProcessInfo::cpu_time`): user and system time of the player and of every process below it, so the
  renderer and GPU processes of CEF count too; the core makes the load in percent out of two of them. Linux adds the
  time of ended children (`cutime`, `cstime`), Windows and macOS lose that of ended processes. Not in it: Android shows
  an app only its own processes, so the decoders of MediaCodec and the renderer of the WebView (processes of the
  system) are missing; macOS decodes with VideoToolbox partly in a service of the system.
- **CPU time of the system** (`ProcessInfo::system_cpu`): busy and total time of all cores since the boot, for the load
  of the whole device beside that of the player. Linux: idle and iowait are idle, the rest busy (guest is already in
  user). Android from version 8 on does not let an app read `/proc/stat`, the value stays empty there. The hardware
  decoders (NVDEC, rpi-hevc-dec, MediaCodec) are not CPU time and in neither value.
- **Temperatures** (`thermal()`): every sensor of the kernel, put to a part by its name (`thermalPart`): `k10temp`,
  `coretemp`, `zenpower`, `x86_pkg_temp`, `cpu…`, `soc…` the CPU, `spd5118` and `jc42` the RAM, `nvme` and `drivetemp`
  the SSD; of each part the highest value. GPU, network card and ACPI zones count not; on the Pi and on Android boxes
  the GPU sits in the chip of the CPU anyway. 0 °C or below is a sensor that does not work. DDR4 without a sensor on the
  module, SATA without the module `drivetemp` and the SD card of the Pi have none. Newer Android versions often
  do not let an app read the sensors. Windows (WMI needs admin rights) and Apple (only the private SMC) give none.
- **Throttling**: whether the device slows down now.
  - Raspberry Pi: `vcgencmd get_throttled` (`/dev/vcio` needs root), bits 1 to 3: frequency capped, throttled, soft
    temperature limit; the under-voltage alone (bit 0) not, it throttles through bit 1 or 2 anyway.
  - Intel: the throttle counters of the kernel, true if they grew since the previous measurement. AMD tells nothing.
  - Android 11+: the thermal status of the NDK, loaded at run time (the player runs from 7.1 on), from LIGHT on.
  - Windows: a core whose frequency limit lies below its maximum, also when a power plan saves energy.
  - macOS/iOS: `thermalState` serious or critical; fair is only "slightly elevated".
- **Locale** like `QLocale::name()`: `de_DE`; `C` for `C`/`POSIX` or nothing set.
- One `platform_*.cpp` per system, chosen by CMake: `platform_linux.cpp` (Linux, Android), `platform_windows.cpp`,
  `platform_apple.cpp`, `platform_none.cpp` (other systems: empty values). The text parsing (`parseMeminfo`,
  `parseProcStatus`, `parseProcStat`, `parseStatCpu`, `thermalPart`, `parseThrottled`, `normalizeLocale`, `zoneName`) is in `system_info.cpp` and tested on every platform.
  `ifaddrs_network.cpp` reads the interfaces with `getifaddrs` for Linux and Apple; the platform file only adds
  type and MAC from the link layer (`AF_PACKET` or `AF_LINK`).
- `platform_windows.cpp` and `platform_apple.cpp` have not been compiled yet; they are checked with the first
  Windows and Apple build.

## Decisions

- **Memory used from `MemAvailable`**, not `MemFree`: with `MemFree` the file cache of the kernel counts as used and a
  device looks nearly full all the time. The hub sees lower values than with garlic-lib.
- **96 % of the disk as total**, like garlic-lib: 4 % stay for the operating system, also for the cache cleanup.
- **One file per platform, chosen by CMake**, instead of `#ifdef` chains; the text parsing is in pure functions that
  are tested on every platform with fixed texts.
- **Not built for WASM and ESP32**: there the values are very different, the host fills `reports::SystemInfo`.
- Network interfaces like garlic-lib (`isRealInterface`, loopback included, the type names of Qt), so the system
  report looks the same to the hub.

## When something goes wrong

- Empty values in the system report: the platform file has no source for them (`platform_none.cpp` gives nothing),
  or the host gives its own `system_info`.
- `platform_windows.cpp` and `platform_apple.cpp` have not been compiled yet.
- Tests: `tests/system/test_system_info.cpp` (`parseMeminfo`, `parseProcStatus`, `normalizeLocale`, `zoneName` with
  fixed texts, and real values on Linux).
