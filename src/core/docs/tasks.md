# tasks

Tasks from the hub, from the subscription `TaskSchedule`. Namespace `garlic::core::tasks`. garlic-lib: `TaskScheduler`,
`XMLConfiguration`, `FirmwareDownloader`, `LibFacade::applyCommand`.

```cpp
TaskScheduler tasks(queue, fs, logger, config, cache_dir, [&](TaskAction action) { std::visit(host, action); });
tasks.checkPending();                         // at start: result of a task that waited for a restart
tasks.subscribe(document.head.subscriptions); // with every new index
// download results: tasks.downloaded(result) returns whether it was one of its files
```

| File                      | Content                                                          |
|---------------------------|------------------------------------------------------------------|
| `task_list.hpp/.cpp`      | `parseTaskList()`, `parseSettings()`: the two XML files as data |
| `task_scheduler.hpp/.cpp` | `TaskScheduler`: loads, checks, applies, hands actions to the host |

## Flow

The task list (`task_scheduler.xml`) is loaded with every new index, no refresh of its own: a new playlist is a new
index. In `<timedTaskList>` every task has an `id` and runs once.

| Task             | The core does                                                   | Action for the host       | Task log    |
|------------------|-----------------------------------------------------------------|---------------------------|-------------|
| `updateSettings` | loads `sourceURI` as `configuration.xml`, checks, applies the props | `ApplyConfiguration` (load the index again) | `completed` |
| `firmwareUpdate` | loads `sourceURI` as `garlic-player.apk/.exe/.dmg/.tar.xz/.zip` (by OS), checks | `InstallSoftware{path}` | `pending` |
| `shutdownPlayer` | –                                                               | `Reboot`                  | `pending`   |
| `applyCommand`   | `clear_all_caches`, `clear_playercache`/`clear_cache`, `clear_webcache` | `ClearCaches{player, web}`, then restart | `pending` |

The core restarts no device and installs nothing; the host (launcher, daemon) does it.

- **Done**: the id is stored after success, per kind as a list (`update_settings_task_ids` …, the last 20) and the
  last one under the old key of garlic-lib (`update_settings_task_id`, `firmware_update_task_id`,
  `shutdown_player_task_id`, `apply_command_task_id`), which is still honoured. So an update of the software runs no
  task twice. Restarts and commands count as done when they are handed over, otherwise they would restart the
  player again and again.
- **Failed** (download, checksum, XML): not done, tried again with the next index; `aborted` in the task log.
- **Checksum**: `contentChecksum` with `checksumMethod` MD5 (also without method, any case). A wrong checksum or an
  unknown method counts as failed (events `CHECKSUM_ERROR`, `CHECKSUM_UNKNOWN_METHOD`). garlic-lib never checked.
- **pending**: before handing a restart or an installation over, the id (and the firmware version) is stored as
  pending. At the next start `checkPending()` writes `completed` - the player runs again - or, after a firmware update
  with another version than the task's, `aborted`. A restart that never happens stays `pending`.
- The states go to `task_execution_log.xml` with the values of the standard: `pending`, `completed`, `aborted`
  (garlic-lib: `completed`, sending commented out). The subscription `TaskExecutionReport` sends them to the hub
  ([reports](reports.md)).
- Events like in garlic-lib: `TASK_SCHEDULER_LOADED`, `TASK_SCHEDULER_XML_ERROR`, `TASK_SCHEDULER_OPEN_ERROR`,
  `CONFIGURATION_LOADED`, `CONFIGURATION_XML_ERROR`, `CONFIGURATION_OPEN_ERROR`.
- `applyConfigurationFile(path)` applies a local `configuration.xml`, e.g. from the launcher (garlic-lib:
  `setConfigFromExternal`).

## Settings (configuration.xml)

Every `<prop name value>` with a value, the same ini keys as garlic-lib:

| prop                                                                 | ini key                  |
|----------------------------------------------------------------------|--------------------------|
| `info.playerName`                                                    | `player_name`            |
| `content.serverUrl`                                                  | `index_uri`              |
| `display.brightness`, `brightness`                                   | `display/brightness`     |
| `hardware.videoOut.0.rotation`                                       | `display/rotation`       |
| `audio.soundlevel`, `volume`, `hardware.audioOut.0.masterSoundLevel` | `audio/volume`           |
| `time.tzDescription`, `time.tzCode`                                  | `time/tzDescription`, `time/tzCode` |
| `time.autoUpdate.protocol`, `time.autoUpdate.server`                 | `time/protocol`, `time/server` |
| `task.scheduledReboot.days`, `schedule.reboot.days`                  | `schedule/reboot_days`   |
| `task.scheduledReboot.time`, `schedule.reboot.time`                  | `schedule/reboot_time`   |
| `standby_mode`                                                       | `standby_mode`           |

### Who applies them

The core stores every prop like garlic-lib and uses only some of them itself. As of garlic-lib, the Android launcher
and garlic-daemon:

| ini key                                   | used by                                                                  |
|-------------------------------------------|--------------------------------------------------------------------------|
| `player_name`, `index_uri`                | the core                                                                 |
| `schedule/reboot_days`, `schedule/reboot_time` | the core plans the restart and hands `Reboot{"DailyReboot"}` to the host |
| `standby_mode`                            | the frontend at `<ref src="adapi:blankScreen">`: `partially` switches the screen off, `deep` starts the deep standby until the next end time, both through the launcher, `no_standby` does nothing. garlic-lib: `PlayerRefCommand::play()`. The new frontends have no launcher yet, the screen stays on |
| `audio/volume`                            | nobody. The player never read it, it plays every medium with its SMIL `soundLevel` (medium before region). The master volume was meant for the launcher |
| `display/brightness`                      | nobody. The Android launcher stores `volume` and `brightness` from the `configuration.xml`, but never calls `SystemSettings.configMasterVolume()`/`configBrightness()` (brightness needs device owner); `MainConfiguration.getBrightness()` reads the key `volume` |
| `display/rotation`                        | nobody, never implemented                                                |
| `time/tzDescription`, `time/tzCode`, `time/protocol`, `time/server` | nobody, never implemented                  |

So for a launcher, daemon or Windows service: master volume, brightness, screen off and deep standby, besides
`Reboot` and `InstallSoftware`. garlic-daemon only does the restart so far.

## Fixed against garlic-lib

- A task was marked as done before it ran: a failed download of the firmware or the settings was never tried again.
- Only the last id per kind was stored: of two tasks of the same kind the first ran again with every index.
- `applyCommand` reported the id of `shutdownPlayer`.
- Checksums were read but not checked.

## Decisions

- **The task list only with a new index,** no refresh of its own: a new playlist is a new index.
- **Checksums are really checked** (garlic-lib read them only). An unknown method counts as failed, because the file
  cannot be checked then.
- **A task is done only after success**, stored per kind as a list, the last id also under the old key of garlic-lib.
  So a failed download is tried again, two tasks of one kind both run once, and an update of the software runs no
  task twice.
- **Restarts and commands count as done when they are handed over**, otherwise they would restart the player again and
  again.
- **The core restarts nothing and installs nothing;** the host (launcher, daemon) does.
- **`pending` and `checkPending()`** instead of an answer of the launcher: the id is stored as pending before a
  restart or an installation is handed over; a player that starts again proves the restart, a firmware update is
  checked by its version. The price: a restart that never happens stays `pending` for good, the core cannot tell it
  from one that is still to come.
- **The same ini keys as garlic-lib**, so the done tasks of an installed player are known after the update.

## When something goes wrong

- **Events** (source `TaskScheduler`): `TASK_SCHEDULER_LOADED`, `TASK_SCHEDULER_OPEN_ERROR`,
  `TASK_SCHEDULER_XML_ERROR`, `CONFIGURATION_LOADED`, `CONFIGURATION_OPEN_ERROR`, `CONFIGURATION_XML_ERROR`,
  `CHECKSUM_ERROR`, `CHECKSUM_UNKNOWN_METHOD`.
- **States** in `task_execution_log.xml`: `pending`, `completed`, `aborted`.
- **A task that does not run**: its id is in `<kind>_task_ids` or under the old `<kind>_task_id` of the ini (done), or
  the index that brings the task list is not new.
- **A pending restart or firmware**: `tasks/pending_id` and `tasks/pending_version` in the ini; `checkPending()` clears
  them at the next start.
- Tests: `tests/tasks/test_task_list.cpp` (the two XML files), `test_task_scheduler.cpp`.
