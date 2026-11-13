# supervisor

What the qml and the qwidget player share with their supervisor (garlic-daemon, garlic-service, later the new
launcher). A static library on Qt Core and Network and the core.

| File                         | Content                                                                    |
|------------------------------|----------------------------------------------------------------------------|
| `supervisor_client.hpp/.cpp` | `SupervisorClient`: the player side of the [supervisor protocol](../../core/docs/supervisor_protocol.md) over `QLocalSocket` (Unix socket, named pipe); `Supervisor` of the core and `ScreenControl` |
| `screen_control.hpp`         | `ScreenControl`: what switches the screen off (the client, the Android launcher) |
| `blank_screen.hpp/.cpp`      | `BlankScreen`: `adapi:blankScreen` with the `standby_mode`                 |
| `runtime_requests.hpp/.cpp`  | `applyConfiguration`, `playIndex`: what a supervisor or launcher asks of the core |
| `tests/`                     | Qt Test: the client against a fake supervisor (`QLocalServer`), the cases of `supervisor_protocol/player.jsonl`, `BlankScreen` |

```
SupervisorClient supervisor(endpoint, PLAYER_VERSION);
supervisor.start(3s);                    // hello before the core: the uuid is a start value
config.supervisor = &supervisor;         // reboot, installSoftware, called in the core thread
supervisor.seed(config);
… runtime.start(); in the core thread: SupervisorClient::applyStartValues(api, supervisor.hello())
supervisor.setCoreProbe([&](auto done) { runtime.post([done](auto &) { done(); }); });
signals: lockedChanged, configurationRequested, indexRequested, quitRequested
before a wanted end: supervisor.closing("user")
```

## Decisions

- **One client for Linux and Windows**: `QLocalSocket` is a Unix domain socket on Linux and a named pipe on Windows.
- **Lives in the UI thread.** Commands of the core (`reboot`, `installSoftware`) come from the core thread and are
  queued into it; the socket is touched by one thread only.
- **The heartbeat proves both threads**: its timer runs in the UI thread, and each tick asks the core thread for a sign
  of life; the next tick sends only if it came.
- **Commands only with their capability.** What the supervisor does not offer is not sent, only logged.
- **The client answers and checks** (`-32602`, `-32000 file not found`, `-32001` before hello), the frontend only
  gets valid requests as signals.
- **Qt Test, not doctest** like the core: the client needs an event loop, and `QTRY_*` and `QSignalSpy` wait for it.

## Tests

`build/<dir>/src/player-ui/supervisor/tests/player_supervisor_tests` or `ctest` in the build directory; about 18 s,
because reconnect (5 s) and heartbeat (1 s) run in real time.
