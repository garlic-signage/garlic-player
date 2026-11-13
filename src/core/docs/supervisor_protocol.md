# supervisor protocol

The protocol between the player and its supervisor: the launcher on Android, garlic-daemon on Linux, garlic-service
on Windows. The supervisor starts the player, watches it and does what the player may not: restart the device,
install software, switch the screen. The core reaches it through the interface `Supervisor`
([player_runtime](player_runtime.md#supervisor)); the platform part of the player speaks this protocol. The Java
frontend later speaks the same.

Protocol version **1**. garlic-lib: `LauncherInterface` (Java), `Launcher` (C++), broadcasts and the ContentProvider
of launcher 1.x.

## Transport

[JSON-RPC 2.0](https://www.jsonrpc.org/specification), one message per line: a JSON object in UTF-8, ended by `\n`,
no `\n` inside (JSON escapes it). No batches.

| Platform | Supervisor                 | Endpoint                                                    |
|----------|----------------------------|-------------------------------------------------------------|
| Linux    | garlic-daemon              | Unix domain socket, e.g. `$XDG_RUNTIME_DIR/garlic-daemon.sock`, mode `0600` |
| Windows  | garlic-service             | named pipe, e.g. `\\.\pipe\garlic-service`, ACL for the user of the player only |
| Android  | launcher (from version 2)  | `LocalSocket`, abstract namespace, e.g. `garlic-launcher`; both sides check the uid of the other (`getPeerCredentials()`) against its package |

The supervisor is the server, the player the client. The supervisor chooses the endpoint and hands it to the player
when it starts it:

- Linux, Windows: command line `--supervisor <endpoint>`, besides `-c <config.xml>` and the index as now.
- Android: intent extra `supervisor` with the name of the socket.

Without an endpoint the player looks for another counterpart (see [Without protocol](#without-protocol)).

## Flow

```
supervisor                                   player
starts the player with --supervisor <ep>  →
                                          ←  connects
                                          ←  hello            (request, the first message)
result: version, capabilities, device     →
                                          ←  heartbeat        (notification, every heartbeat_interval)
                                          ←  reboot, installSoftware, screenOff …   (requests)
result or error                           →
applyConfiguration, setIndex, quit        →  (requests)
                                          ←  result or error
                                          ←  closing          (notification, before a wanted end)
                                             connection closed, process ends
```

- **Before `hello`** both sides send nothing else. A request of the supervisor before it is answered gets
  `-32001 not ready`.
- **Both sides send requests**, each with its own `id` (number or string, unique per sender and connection). The
  answer goes back on the same connection.
- **Timeouts**: a request without answer after 10 s counts as failed for the sender; a late answer is dropped.
- **Connection lost**: the player keeps playing and connects again every 5 s, starting with `hello`. The supervisor
  treats a player without connection like one without heartbeat.

## Lifecycle

### `hello` - player → supervisor, request

```json
{"jsonrpc": "2.0", "id": 1, "method": "hello", "params": {
  "protocol": 1,
  "player": {"name": "garlic-player", "version": "1.0.0", "platform": "linux", "pid": 4711},
  "capabilities": ["applyConfiguration", "setIndex", "quit"]
}}
```

```json
{"jsonrpc": "2.0", "id": 1, "result": {
  "protocol": 1,
  "supervisor": {"name": "garlic-daemon", "version": "2.0.0"},
  "capabilities": ["reboot", "install", "screen", "deep_standby", "volume", "brightness"],
  "heartbeat_interval": 10,
  "device": {"uuid": "…", "content_url": "https://…", "player_name": "…"}
}}
```

- `protocol`: the highest version the sender speaks. Both use the lower one; a supervisor that does not speak the
  player's version at all answers `-32002 protocol not supported` and closes.
- `capabilities`: what the side implements (see the tables below). The other side calls only what is listed; unknown
  names are ignored. The player's list tells the supervisor which requests it may send.
- `heartbeat_interval`: seconds, chosen by the supervisor, default 10.
- `device`: optional, every field optional. `uuid` and `content_url` replace the ContentProvider queries of launcher
  1.x (`uuid`, `smil_content_url`); the core takes them like garlic-lib took them from the launcher. Missing fields:
  the player uses its own.

### `heartbeat` - player → supervisor, notification

```json
{"jsonrpc": "2.0", "method": "heartbeat", "params": {"uptime": 3600}}
```

Every `heartbeat_interval` seconds. It must prove that the player works, not only that its socket thread lives: the
player sends it only while the core thread runs its loop (`poll()`) and the UI thread answers. The supervisor counts a
player as hung after three intervals without a heartbeat, ends the process and starts it again.

### `closing` - player → supervisor, notification

```json
{"jsonrpc": "2.0", "method": "closing", "params": {"reason": "user"}}
```

Before a wanted end (exit in the menu, after `quit`, before a restart of the player itself, e.g. after
`ClearCaches`). `reason`: `user`, `quit`, `restart`. The supervisor does not start the player again after `user`; it
does after `restart`. A process that ends without `closing` crashed and is started again. garlic-lib:
`closePlayerCorrect()`, broadcast `PlayerClosedReceiver`.

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
- `setVolume` and `setBrightness`: from `audio/volume` and `display/brightness` of the `configuration.xml`, which
  nobody applied so far ([tasks](tasks.md#who-applies-them)).

## Supervisor → player

Requests, result `{}`.

| Method               | Capability           | Params  | Does                                                  | Launcher 1.x                        |
|----------------------|----------------------|---------|-------------------------------------------------------|-------------------------------------|
| `applyConfiguration` | `applyConfiguration` | `path`  | applies a local `configuration.xml` (`applyConfigurationFile()`) | `ConfigReceiver`, `config_path`     |
| `setIndex`           | `setIndex`           | `uri`   | plays this index, a path or a URL (e.g. from a USB stick) | `SmilIndexReceiver`, `smil_index_path` |
| `quit`               | `quit`               | –       | the player answers, sends `closing` with `quit` and ends | –                                   |

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

| Order | Condition                                         | Counterpart                                                       |
|-------|---------------------------------------------------|-------------------------------------------------------------------|
| 1     | `--supervisor` / intent extra `supervisor`        | this protocol                                                     |
| 2     | Android, package `com.sagiadinos.garlic.launcher` with version 1.x | adapter launcher 1.x                             |
| 3     | Android on a Philips display (SICP reachable)     | adapter Philips                                                   |
| 4     | –                                                 | none: `Supervisor` is `nullptr`, the core only logs               |

The adapters implement the same interface in the player, the core sees no difference:

- **Launcher 1.x** (from `GarlicLauncher.java`): capabilities `reboot`, `install`, `screen`, `deep_standby`; `device`
  from the ContentProvider `com.sagiadinos.garlic.launcher.SettingsProvider` (`uuid`, `smil_content_url`,
  `launcher_version`); commands as the broadcasts of the tables above. The broadcasts give no answer: the adapter
  answers `{}` at once, logged as unconfirmed. No heartbeat, no `quit`. `applyConfiguration` and `setIndex` come as
  before through `ConfigReceiver` and `SmilIndexReceiver`; `closing` as broadcast `PlayerClosedReceiver`.
- **Philips** (from `PhilipsLauncher.java`): capabilities `reboot` (`php.intent.action.REBOOT`), `install`
  (`php.intent.action.UPDATE_APK`), `screen` (SICP backlight). `uuid` is model number and serial over SICP, the
  version the firmware build. No heartbeat, nobody watches the player.

## Decisions

- **JSON-RPC 2.0 instead of an own format.** Ids, answers, errors and notifications are defined, libraries exist for
  C++, Java and C#; only the methods are ours.
- **One message per line** instead of `Content-Length` headers: simple to read and to test (`socat`, a pipe client),
  JSON has no raw line break.
- **The supervisor is the server and starts the player.** It is there first and stays; the player connects to the
  endpoint it gets and knows no fixed path. So several players or a test supervisor need no change in the player.
- **The vocabulary of launcher 1.x** (`task_id`, `seconds_to_wakeup`, `screen_off/on`, `config_path`,
  `smil_index_path`), so the adapter only translates. Neutral where 1.x was Android-only: `path` instead of
  `apk_path`, `seconds_to_wakeup` as integer.
- **Capabilities instead of a fixed set.** The device decides what it can (Philips no deep standby, garlic-daemon so
  far only the restart); the core calls only what is offered, a new capability breaks nobody.
- **Answers, but no proof of a restart.** `{}` means taken over. Whether the device really restarted or installed the
  core sees at the next start (`checkPending()`), the only point that works for every supervisor.
- **A heartbeat that proves work**, sent only while the core loop and the UI thread run. A heartbeat from a socket
  thread of its own would hide exactly the hang it is for.
- **Launcher 1.x stays as it is.** It belongs to the legacy player; the new player reaches it through an adapter with
  less: no answer, no heartbeat.

## When something goes wrong

- **The player restarts again and again**: no heartbeat within three intervals (core or UI thread blocked), or it ends
  without `closing`. The log of the supervisor says which.
- **A command is ignored**: the capability is not in the `hello` of the supervisor (`-32601`), or launcher 1.x
  dropped the broadcast (no answer, the adapter logs `unconfirmed`).
- **`pending` stays in the task log**: the supervisor took the restart over, but it never happened.
- **No connection**: the endpoint of `--supervisor` does not exist or has the wrong permissions; the player keeps
  playing and tries every 5 s.
