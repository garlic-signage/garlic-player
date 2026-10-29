# configuration

Configuration of the player in `garlic-player.ini`. Namespace `garlic::core::configuration`.

## Configuration

Header: `configuration/configuration.hpp`

```cpp
Configuration config{fs, clock, ini_path};    // path given by the frontend
config.setCacheDir(cache_dir);                // platform paths, given by the frontend
config.setLogDir(log_dir);
config.setVersion(version);     // version() returns it, e.g. to check a firmware update
if (auto loaded = config.load(); !loaded)
    log(loaded.error());

config.determineIndexUri(command_line_uri);
std::string ua = config.userAgent();
```

- `load()` reads the file; a missing file is no error. If `uuid` or `player_name` are missing they are created and
  stored (the name is the last part of the uuid, like garlic-lib).
- Every change is written immediately, via a temporary file and rename, so a power cut never leaves a broken ini.
  Write errors are kept in `lastError()` and never stop the player.
- Keys the core does not know (e.g. `firmware_update_task_id`) are kept unchanged.

| Method                                   | ini key                                   |
|------------------------------------------|-------------------------------------------|
| `uuid()`                                 | `uuid`                                    |
| `createUuid()` (static)                  | a new random UUID v4, e.g. for upload names |
| `playerName()`, `setPlayerName()`        | `player_name`                             |
| `indexUri()`, `setIndexUri()`, `determineIndexUri()` | `index_uri`                   |
| `lastPlayedIndexPath()`, `set…`          | `last_played_index_path`                  |
| `standbyMode()`, `set…`                  | `standby_mode` (`no_standby`, `partially`, `deep`) |
| `rebootDays()`, `rebootTime()`, `set…`   | `schedule/reboot_days`, `schedule/reboot_time` |
| `determineApiAccessToken(user, pass)`    | `api_access_token`, `api_access_token_expire` |
| `value(key)`, `setValue(key, value)`     | any other key, e.g. `audio/volume`        |

`httpd()` returns the `HttpdSettings` of the HTTP server, see [httpd](httpd.md).

Not stored, derived or given by the frontend: `indexPath()`, `validateIndexUri()`, `os()`, `userAgent()`,
`cacheDir()`, `logDir()`.

The settings of the other modules (e.g. `HttpdSettings`) are structs in those modules; `Configuration` fills them from
the ini. Defaults apply only when a key is missing.

## IniFile

Header: `configuration/ini_file.hpp`

Reads and writes the INI format of QSettings, so the existing files of garlic-lib players keep working:

- keys without group in `[General]`; `httpd/port` is `port` in `[httpd]`; deeper keys as `sub\level`
- keys and groups sorted, an empty line between groups
- values: `,` `;` `=` and leading/trailing spaces cause quotes; `\\`, `\"`, `\n`, `\t`, control characters as
  `\xHH`; a leading `@` is written as `@@`
- reading also understands `\xHHHH` (non-ASCII as written by Qt 5), `%XX` / `%UXXXX` in keys, `;` comments and CRLF
- non-ASCII is written as UTF-8, like Qt 6

The test `tests/configuration/test_ini_file.cpp` reads a real garlic-player.ini of a garlic-lib player and checks that
writing it again gives the identical file.

## Differences to garlic-lib

- The ini path and the cache/log directories come from the frontend instead of QStandardPaths.
- `api_access_token_expire` is written in UTC (garlic-lib: local time). Old values stay valid until they expire.
- Time zone and start time move to the system information module.

## Decisions

- **One central module, the structs stay with their modules.** garlic is an application with a fixed set of modules,
  no plugin system, so one place reads the ini and hands ready values to the others. The struct (e.g. `HttpdSettings`)
  lives in the module that uses it, so one sees there what it can set, and the module knows nothing about the ini and
  is tested with plain structs. The price: a new setting touches two places, the struct and the mapping here.
- **Compatible with the existing `garlic-player.ini`** (same keys, same QSettings format). An installed player keeps
  its settings, above all its `uuid`, which identifies it in the hub. The test reads a real ini of a garlic-lib player
  and writes it back byte for byte.
- **The frontend gives the path.** QSettings wrote to `SmilControl/` (the name of 2016) at a different place per OS;
  the core does not decide paths any more. Moving an old ini to a new place is the business of the Qt frontends, the
  only ones with old paths: a new path without the old file creates a new `uuid`, and the player shows up in the hub
  as a new device.
- **Defaults only for missing keys, never written.** garlic-lib's `initDefaultSettings()` wrote the defaults on every
  start, so a port changed in the ini was overwritten again.
- **Every change is written at once, through a temporary file and rename,** so a power cut never leaves a half
  written ini. A failing write never stops the player.

## When something goes wrong

- Write errors are only in `lastError()`; the player goes on with the values in memory.
- A player that suddenly appears as a new device in the hub has lost its `uuid`: wrong ini path or an unreadable ini.
- Keys the core does not know stay in the file, so an old QtWebApp key in `[httpd]` is no error.
- Tests: `tests/configuration/test_ini_file.cpp` (QSettings format, round trip of a real ini),
  `test_configuration.cpp`.
