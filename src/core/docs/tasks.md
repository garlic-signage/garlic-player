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
| `updateSettings` | loads `sourceURI` as `configuration.xml`, checks, applies the props | `ApplyConfiguration` (load the index again) | `COMPLETED` |
| `firmwareUpdate` | loads `sourceURI` as `garlic-player.apk/.exe/.dmg/.tar.xz/.zip` (by OS), checks | `InstallSoftware{path}` | `PENDING` |
| `shutdownPlayer` | –                                                               | `Reboot`                  | `PENDING`   |
| `applyCommand`   | `clear_all_caches`, `clear_playercache`/`clear_cache`, `clear_webcache` | `ClearCaches{player, web}`, then restart | `PENDING` |

The core restarts no device and installs nothing; the host (launcher, daemon) does it.

- **Done**: the id is stored after success, per kind as a list (`update_settings_task_ids` …, the last 20) and the
  last one under the old key of garlic-lib (`update_settings_task_id`, `firmware_update_task_id`,
  `shutdown_player_task_id`, `apply_command_task_id`), which is still honoured. So an update of the software runs no
  task twice. Restarts and commands count as done when they are handed over, otherwise they would restart the
  player again and again.
- **Failed** (download, checksum, XML): not done, tried again with the next index; `FAILED` in the task log.
- **Checksum**: `contentChecksum` with `checksumMethod` MD5 (also without method, any case). A wrong checksum or an
  unknown method counts as failed (events `CHECKSUM_ERROR`, `CHECKSUM_UNKNOWN_METHOD`). garlic-lib never checked.
- **PENDING**: before handing a restart or an installation over, the id (and the firmware version) is stored as
  pending. At the next start `checkPending()` writes `COMPLETED` - the player runs again - or, after a firmware update
  with another version than the task's, `FAILED`. A restart that never happens stays `PENDING`.
- The states go to `task_execution_log.xml`. Sending it to the hub follows later, the same way as the other logs.
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

## Fixed against garlic-lib

- A task was marked as done before it ran: a failed download of the firmware or the settings was never tried again.
- Only the last id per kind was stored: of two tasks of the same kind the first ran again with every index.
- `applyCommand` reported the id of `shutdownPlayer`.
- Checksums were read but not checked.
