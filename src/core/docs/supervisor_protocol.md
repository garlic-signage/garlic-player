# supervisor protocol

What the player sends to its supervisor and gets from it: the launcher on Android, garlic-daemon on Linux,
garlic-service on Windows. The supervisor starts the player, watches it and does what the player may not: restart the
device, install software, switch the screen, apply device settings. The core reaches restart and installation through
the interface `Supervisor` ([player_runtime](player_runtime.md#supervisor)), the frontend the screen through its
`ScreenControl`; the platform part of the player speaks this protocol. The Java frontend later speaks the same.

Protocol version **1**. In the player: `SupervisorClient` of `src/player-ui/supervisor`, used by the qml and the
qwidget player with `--supervisor`; not yet on Android (intent extra), there launcher 1.x and Philips
([Without protocol](#without-protocol)). garlic-lib: `LauncherInterface` (Java), `Launcher` (C++).

**Test cases**: [supervisor_protocol/player.jsonl](supervisor_protocol/player.jsonl), one per line: what a supervisor
sends (`send`, or `send_raw` for a line that is no JSON) and what the player answers (`expect`, compared as subset;
`null`: no answer). The tests of the player read them; a supervisor can play them against a player.

## Transport

[JSON-RPC 2.0](https://www.jsonrpc.org/specification), one message per line: a JSON object in UTF-8, ended by `\n`
(`\r\n` is accepted), no `\n` inside (JSON escapes it). At most 64 KiB per line, a longer one closes the connection.
No batches.

| Platform | Endpoint                                             |
|----------|------------------------------------------------------|
| Linux    | Unix domain socket, e.g. `/run/garlic-daemon/garlic-daemon.sock` |
| Windows  | named pipe, e.g. `\\.\pipe\garlic-service`           |
| Android  | `LocalSocket` in the abstract namespace, e.g. `garlic-launcher`; the player checks the uid of the other side against the package of the launcher |

The supervisor is the server, the player a client; who may connect is the matter of the supervisor. It hands the
endpoint to the player when it starts it:

- Linux, Windows: command line `--supervisor <endpoint>`, besides `-c <config.xml>` and the index.
- Android: intent extra `supervisor` with the name of the socket.

Without an endpoint the player looks for another counterpart ([Without protocol](#without-protocol)).

The player says in `hello` that it is the player (`client`): a supervisor may have other clients, which get no
requests for the player.

## Flow

```
supervisor                                   player
starts the player with --supervisor <ep>  →
                                          ←  connects
                                          ←  hello            (request, the first message)
result: version, capabilities, locked,    →
        device
                                          ←  heartbeat        (notification, every heartbeat_interval)
                                          ←  reboot, installSoftware, screenOff …   (requests)
result or error                           →
applyConfiguration, setIndex, quit        →  (requests)
                                          ←  result or error
setLocked                                 →  (notification, when the service mode changes)
                                          ←  closing          (notification, before a wanted end)
                                             connection closed, process ends
```

- **Before `hello`** neither side sends anything else. A request to a client without `hello` gets `-32001 not ready`.
- **Both sides send requests**, each with its own `id` (number or string, unique per sender and connection). The
  answer goes back on the same connection.
- **Timeouts**: a request without answer after 10 s counts as failed for the sender; a late answer is dropped.
- **Connection lost** (supervisor restarts): the player keeps playing and connects again every 5 s, starting with
  `hello`. Messages while there is no connection are lost. The supervisor treats a player without connection like one
  without heartbeat.

## Lifecycle

### `hello` - client → supervisor, request

```json
{"jsonrpc": "2.0", "id": 1, "method": "hello", "params": {
  "protocol": 1,
  "client": "player",
  "player": {"name": "garlic-player", "version": "2.0.0", "platform": "linux", "pid": 4711},
  "capabilities": ["applyConfiguration", "setIndex", "quit", "setLocked"]
}}
```

```json
{"jsonrpc": "2.0", "id": 1, "result": {
  "protocol": 1,
  "supervisor": {"name": "garlic-daemon", "version": "2.0.0"},
  "capabilities": ["reboot", "install", "screen", "deep_standby", "volume", "brightness", "configuration"],
  "heartbeat_interval": 10,
  "locked": true,
  "device": {"uuid": "…", "content_url": "https://…", "player_name": "…"}
}}
```

- `protocol`: the highest version the sender speaks. Both use the lower one; a supervisor that does not speak the
  client's version at all answers `-32002 protocol not supported` and closes.
- `client`: always `player` for the player.
- `capabilities`: what the side implements (see the tables below). The other side calls only what is listed; unknown
  names are ignored. `configuration` of the supervisor: it delivers the configuration, see
  [Settings](#settings).
- `heartbeat_interval`: seconds, chosen by the supervisor, default 10. `0`: no hang detection.
- `locked`: the device is in strict mode, see [Lock](#lock). Missing: `false`.
- `device`: optional, every field optional. **Only start values**: the player takes `uuid` only if its ini has none
  yet (the player is the one truth of its identity; `Configuration::load(uuid_seed)`), `content_url` and `player_name`
  only if it has none. Later changes come with `applyConfiguration`. Replaces the ContentProvider queries of launcher
  1.x (`uuid`, `smil_content_url`).

### `heartbeat` - player → supervisor, notification

```json
{"jsonrpc": "2.0", "method": "heartbeat", "params": {"uptime": 3600}}
```

Every `heartbeat_interval` seconds. It must prove that the player works, not only that a thread of it lives: the
player sends it from a timer in the UI thread and only while the core thread runs its loop (`poll()`). The supervisor
counts a player as hung after some intervals without a heartbeat, ends it and starts it again.

### `closing` - player → supervisor, notification

```json
{"jsonrpc": "2.0", "method": "closing", "params": {"reason": "user"}}
```

Before a wanted end (exit in the menu, after `quit`, before a restart of the player itself, e.g. after
`ClearCaches`). `reason`: `user`, `quit`, `restart`. The supervisor does not start the player again after `user`; it
does after `restart`. A process that ends without `closing` crashed and is started again. garlic-lib:
`closePlayerCorrect()`, broadcast `PlayerClosedReceiver`.

### End

- After `quit` the player answers, sends `closing` and ends.
- After `SIGTERM` the player ends within 5 s.
- The player never ends with exit code 127: a start shell reports a missing program with it.

## Player → supervisor

Requests. The result is `{}`: the supervisor has taken the command over and carries it out. For `reboot` and
`installSoftware` that is all the player hears; the core checks the outcome at the next start with `checkPending()`
([tasks](tasks.md)).

| Method            | Capability     | Params                          | Does                                       | Launcher 1.x                     |
|-------------------|----------------|---------------------------------|--------------------------------------------|----------------------------------|
| `reboot`          | `reboot`       | `task_id`                       | restarts the device                        | `CommandReceiver`, `command=reboot`, `task_id` |
| `installSoftware` | `install`      | `path`                          | installs the software at `path` (apk, exe, tar.xz …), starts the player again | `InstallAppReceiver`, `apk_path` |
| `screenOff`       | `screen`       | –                               | switches the screen off                    | `CommandReceiver`, `command=screen_off` |
| `screenOn`        | `screen`       | –                               | switches the screen on                     | `CommandReceiver`, `command=screen_on` |
| `deepStandby`     | `deep_standby` | `seconds_to_wakeup` (integer)   | deep standby, wakes up after the seconds   | `CommandReceiver`, `command=deep_standby`, `seconds_to_wakeup` (string) |
| `setVolume`       | `volume`       | `level` (0…100)                 | master volume of the device                | –                                |
| `setBrightness`   | `brightness`   | `level` (0…100)                 | brightness of the screen                   | –                                |

- `task_id`: of the task, `DailyReboot` or `httpd: <local time>`, as in `Supervisor::reboot()`.
- Before `reboot` and `installSoftware` the supervisor may send `quit` and wait for `closing`, at most 10 s.
- `screenOff`, `screenOn`, `deepStandby`: at `adapi:blankScreen` with the `standby_mode` of the configuration;
  `deepStandby` with `remainingTime()` of the element ([tasks](tasks.md#who-applies-them)).
- `setVolume`, `setBrightness`: from `audio/volume` and `display/brightness` of a `configuration.xml`, see
  [Settings](#settings).

## Supervisor → player

| Method               | Kind         | Capability           | Params   | Does                                                  | Launcher 1.x                        |
|----------------------|--------------|----------------------|----------|-------------------------------------------------------|-------------------------------------|
| `applyConfiguration` | request      | `applyConfiguration` | `path`   | applies a local `configuration.xml` (`applyConfigurationFile()`), the index is loaded again | `ConfigReceiver`, `config_path`     |
| `setIndex`           | request      | `setIndex`           | `uri`    | stores and plays this index, a path or a URL (e.g. from a USB stick) | `SmilIndexReceiver`, `smil_index_path` |
| `quit`               | request      | `quit`               | –        | the player answers, sends `closing` with `quit` and ends | –                                   |
| `setLocked`          | notification | `setLocked`          | `locked` | strict mode on or off, see [Lock](#lock)              | –                                   |

Requests are answered with `{}`. Paths may be on a USB stick that is gone a moment later: a missing file is `-32000`
with `file not found`.

## Settings

Who applies a setting owns it, no matter whether a supervisor is connected. So the player runs alone as it runs with
a supervisor, and no setting exists twice.

| Setting                                                   | The player                                           |
|-----------------------------------------------------------|------------------------------------------------------|
| uuid, player name, content URL, HTTP server, reboot times | stores them in its ini and uses them, with and without supervisor |
| master volume, brightness, rotation, time zone, NTP       | reads them from every `configuration.xml` (task, `-c`, `applyConfiguration`), stores them and passes them on (`setVolume`, `setBrightness`, …) if the supervisor offers it |

- **Supervisor with `configuration`** (the launcher): it is the way name and content URL come in, by
  `applyConfiguration`. The settings dialog of the player shows them but does not change them. With launcher 1.x the
  dialog does the same (`Player.identityLocked`).
- An `updateSettings` task of the hub changes name and content URL also with a supervisor: the hub stands above both.

## Lock

So nobody changes a public screen with a keyboard. The supervisor decides, the player follows: `locked` in `hello`,
`setLocked` when it changes (also while the connection was lost: the next `hello` brings the state).

Locked, the player opens nothing a keyboard or a touch could reach: no settings, no debug infos, no exit, no menu
gesture, no `Ctrl+S`/`Ctrl+D`/`Ctrl+Q`. Accesskeys of the SMIL stay. Name and content URL then change only through
the hub or `applyConfiguration`.

## Errors

The codes of JSON-RPC and three of the range for implementations:

| Code     | Meaning                                                              |
|----------|----------------------------------------------------------------------|
| `-32700` | parse error: not JSON. The line is dropped, the connection stays     |
| `-32600` | invalid request                                                      |
| `-32601` | method not found: unknown, or a capability not offered               |
| `-32602` | invalid params                                                       |
| `-32000` | failed: `data.reason` says why (e.g. `file not found`, `no permission`) |
| `-32001` | not ready: no `hello` yet                                            |
| `-32002` | protocol not supported: answer to `hello`, then the connection closes |

The core logs every error under the category `Supervisor` to `debug.log`.

## Extending

- **New methods and capabilities** need no new version: a side calls only what the other offers.
- **Unknown fields** in `params`, `result` and `device` are ignored, so fields can be added.
- **Unknown notifications** are ignored, unknown requests answered with `-32601`.
- **A new version** only for changes that break: a method that means something else, a field that goes or changes its
  type.

## Without protocol

The player tries in this order:

| Order | Condition                                                   | Counterpart                                  |
|-------|-------------------------------------------------------------|----------------------------------------------|
| 1     | `--supervisor` (desktop); intent extra `supervisor` (Android, not yet) | this protocol                 |
| 2     | Android, package `com.sagiadinos.garlic.launcher` installed | launcher 1.x                                 |
| 3     | Android, package `com.tpv.app.tpvlauncher` installed        | Philips                                      |
| 4     | –                                                           | none: `Supervisor` is `nullptr`, the core only logs, the screen stays on |

Launcher 1.x and Philips are implemented in the qml player for Android (`android_launcher.hpp/.cpp`, the Java classes
in `android/`, see [its README](../../player-ui/qml/README.md#android)); `GarlicActivity` finds them before Qt starts.

- **Launcher 1.x** (frozen): `reboot`, `install`, `screen`, `deep_standby`. uuid, content URL and version from the
  ContentProvider `com.sagiadinos.garlic.launcher.SettingsProvider` (`uuid`, `smil_content_url`, `launcher_version`);
  commands as the broadcasts of the tables above, with the package of the launcher set (its receivers are declared in
  its manifest, Android 8+ delivers no implicit broadcasts to those). No answers, no heartbeat, no `quit`, no lock.
  `ConfigReceiver` and `SmilIndexReceiver` bring configuration and index; the exit in the menu sends
  `PlayerClosedReceiver`. The uuid is a start value as in protocol 1; the content URL wins at every start, as in the
  legacy player.
- **Philips**: `reboot` (`php.intent.action.REBOOT`), `install` (`php.intent.action.UPDATE_APK`), `screen` (SICP
  backlight over `127.0.0.1:5000`). uuid is model number and serial over SICP, the version the firmware build;
  `https://indexes.smil-control.com` only as default without an index. No deep standby, nobody watches the player.

## Decisions

- **JSON-RPC 2.0 instead of an own format.** Ids, answers, errors and notifications are defined, libraries exist for
  C++ and Java; only the methods are ours.
- **One message per line** instead of `Content-Length` headers: simple to read and to test (`socat`), JSON has no raw
  line break.
- **The supervisor is the server and starts the player.** It is there first and stays; the player connects to the
  endpoint it gets. Several clients, a test supervisor or `socat` need no change in the player.
- **The vocabulary of launcher 1.x** (`task_id`, `seconds_to_wakeup`, `screen_off/on`, `config_path`,
  `smil_index_path`), so its adapter only translates. Neutral where 1.x was Android-only: `path` instead of
  `apk_path`, `seconds_to_wakeup` as integer.
- **Capabilities instead of a fixed set.** The device decides what it can (Philips no deep standby, garlic-daemon so
  far only the restart); the core calls only what is offered, a new capability breaks nobody.
- **Answers, but no proof of a restart.** `{}` means taken over. Whether the device really restarted or installed the
  core sees at the next start (`checkPending()`), the only point that works for every supervisor.
- **A heartbeat that proves work**, from the UI thread while the core loop runs. A heartbeat from a socket thread of
  its own would hide exactly the hang it is for.
- **The player is the one truth of its identity.** The uuid of a supervisor is only a start value; garlic-lib took the
  one of the launcher at every start, so a reset launcher gave the device a new identity.
- **Who applies a setting owns it.** No setting is kept in two places, the player runs alone as with a supervisor.
- **The supervisor decides the lock, the player follows.** The protection needs only the state, not the settings.
- **Launcher 1.x stays as it is.** It belongs to the legacy player; the new player reaches it with less: no answer, no
  heartbeat, no lock.

## When something goes wrong

- **The player restarts again and again**: no heartbeat within the timeout (core or UI thread blocked), or it ends
  without `closing`. The log of the supervisor says which.
- **A command is ignored**: the capability is not in the `hello` of the supervisor (`-32601`); with launcher 1.x the
  broadcast was dropped (no answer: the launcher is not installed or older).
- **`pending` stays in the task log**: the supervisor took the restart over, but it never happened.
- **No connection**: the endpoint of `--supervisor` does not exist, or the supervisor does not let the user of the
  player connect; the player keeps playing and tries every 5 s.
