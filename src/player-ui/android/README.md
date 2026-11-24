# android

The Android frontend of the garlic-player: Java on the core (`PlayerRuntimeApi` in a `ThreadedRuntime`), like the sdl
and the JS player. Video and audio with `android.media.MediaPlayer` (the player of the system, as in the Qt player), web
pages with the WebView of the system, images with `BitmapFactory`; all are views of one hierarchy, so they stack by
z-index. Video in a `SurfaceView`: SurfaceFlinger shows each frame at its time (a `TextureView` judders); it lies behind
the window, which is transparent there, so regions of a higher z-index lie over the video. Overlapping videos have no
reliable order among each other. Android 7.1.1 (API 25) or newer.

| File                         | Content                                                                          |
|------------------------------|----------------------------------------------------------------------------------|
| `native/jni.cpp`             | the bridge: the core in its thread, events and dialog data as JSON for Java, calls back into the core; SMIL colors with `../sdl/color.cpp` |
| `native/CMakeLists.txt`      | `libgarlic-player.so` with the core (`libcore.so`)                               |
| `build-native.sh`            | builds the native part for one ABI with the NDK and vcpkg into `app/src/main/jniLibs/<abi>` |
| `vcpkg/triplets/`            | vcpkg's Android triplets with API 25 instead of 28                               |
| `vcpkg/ports/openssl/`       | vcpkg's OpenSSL, for x86_64 without assembly (the emulator)                       |
| `app/…/NativeCore.java`      | the native methods; the wake of the core thread runs in the UI thread            |
| `app/…/java/GarlicActivity.java` | events of the core, media per region, menu, keys, loading, screenshot, clear_webcache |
| `app/…/java/Stage.java`      | regions by z-index with background color and image, media stacked, the topmost with a picture visible |
| `app/…/java/Media.java`      | image, video/audio (MediaPlayer), web page (WebView), brush                           |
| `app/…/java/FitFrame.java`   | fit and mediaAlign like `place()` of the sdl player                             |
| `app/…/java/Dialogs.java`    | menu, settings dialog, debug dialog (with the running downloads)                 |
| `app/…/java/Paths.java`      | ini, cache and logs at the places of the earlier players, the CA bundle for libcurl |
| `app/…/java/BlankScreen.java` | adapi:blankScreen with the standby_mode, like `../supervisor/blank_screen.cpp` |
| `app/…/java/Watchdog.java`   | notices a hanging UI thread (30 s), then debug.log and a restart of the device through the launcher |
| `app/…/java/LauncherInterface.java`, `GarlicLauncher.java`, `PhilipsLauncher.java`, `SICPDef.java`, `SocketClient.java` | launcher 1.x and the Philips firmware, from the legacy player |
| `app/…/java/ConfigReceiver.java`, `SmilIndexReceiver.java` | configuration and index the launcher sends |

## Build

```
export ANDROID_NDK_HOME=…/Sdk/ndk/26.1.10909125 VCPKG_ROOT=… JAVA_HOME=/usr/lib/jvm/java-17-openjdk
./build-native.sh x86_64          # or x86, arm64-v8a, armeabi-v7a; Release by default
./gradlew assembleDebug           # app/build/outputs/apk/debug/app-debug.apk
```

Version: `app/build.gradle` reads `../VERSION`, `versionName` is it as it is, `versionCode` 20000000 + the build
number the build server appends (`2.0.0.997`, `2.0.0-dev.996`, see the [README](../../../README.md#version)). Android
updates only to a higher `versionCode`, so `assembleRelease` stops without that number; a debug build takes 1.

`local.properties` (not in git) names the SDK: `sdk.dir=…/Android/Sdk`. Gradle 8.13 (wrapper), Android Gradle plugin
8.6 (works with the build tools 34; 8.12 needs 35), compileSdk 34, minSdk 25, targetSdk 29.

What the native build needs on Android, also for a CI:
- **API 25 triplets** (`vcpkg/triplets`): vcpkg's own Android triplets take API 28, so the libraries would use
  functions Android 7 lacks, and libxml2 would miss GNU libiconv (bionic has iconv from API 28).
- **`-DCMAKE_CXX_SCAN_FOR_MODULES=OFF`**: CMake 4 would build the `std` module with the libstdc++ of the build machine.
- **x86_64 only: OpenSSL without assembly** (`vcpkg/ports/openssl`): the assembler of NDK 26 does not know the SM3/SM4
  instructions of its x86_64 assembly. arm64 and armv7 do not need it; a newer NDK neither.
- `ANDROID_STL=c++_shared`: core and bridge are two libraries, `libc++_shared.so` goes with them.

## Files of the player

- **ini**: `files/.config/SmilControl/garlic-player.ini` of the app, where QSettings of the Qt players wrote it (seen
  on a device with an earlier player): an update keeps the uuid and the player stays the same device in the CMS.
- **cache and logs**: `/sdcard/garlic-player/{cache,logs}` with the permission for the shared storage (launcher 1.x
  installs updates from there), otherwise inside the app. `requestLegacyExternalStorage` and targetSdk 29 keep that on
  Android 11+, like the legacy player.
- **CA certificates**: Android has no OpenSSL store; the certificates of the system (`/apex/com.android.conscrypt/cacerts`
  or `/system/etc/security/cacerts`) are joined into `files/ca-bundle.pem` for libcurl.

## Like the other players

- Regions placed by their part of the screen (0..1 of the root layout), by z-index; background color and image
  (repeat, repeatX, repeatY, noRepeat), a background image that comes later replaces the one of the layout.
- fit and mediaAlign for images and videos alike (SMIL standard), the region cuts the rest.
- Only the topmost medium with a picture is visible, audio covers nothing; a paused web page is hidden.
- Only the first region of an instance reports end and errors; a missing file fails once.
- Web pages: local files may read remote data and other files, media play without a gesture, no new windows, no
  scroll bars after loading; white background.
- Ten fast touches (< 500 ms) or one held for 10 s open the menu (Settings, Information, Exit); with a keyboard
  Ctrl+S, Ctrl+D, Ctrl+Q, other keys are access keys; a touch on a region activates its top medium.
- Dialogs and the loading overlay scaled like the sdl player, by max(density, screen height / 900); dialogs slightly
  transparent. Information: one row per value, cut with "…", only a centered Close. CPU is the player process only
  (MediaCodec and the WebView renderer run in processes of the system), the system load stays `-` (Android 8+ does
  not let an app read `/proc/stat`); temperatures where the thermal zones are readable, throttling from Android 11 on.
- "Loading 1 / X" with the bar of the first download until the first medium of a layout, centered.
- Without an index the settings dialog comes first; Cancel ends the player.
- `adapi:blankScreen`: black over the whole screen while the element runs (over all regions and "Loading", below the
  dialogs; it takes the touches, the menu gesture still counts), with every `standby_mode` and without launcher, as in
  the sdl and js players. Switching off only through the launcher, with the `standby_mode` of the configuration:
  `partially` screen off until the element ends, `deep` the launcher sleeps until its planned end (without one: screen
  off). Other commands fail.

## Launcher

Like the legacy player, with its Java classes:
- **Found** at the start: the garlic-launcher 1.x (`com.sagiadinos.garlic.launcher`), else the Philips firmware
  (`com.tpv.app.tpvlauncher`), else none. Its device information is asked for up to 5 s in a thread (Philips answers
  over the SICP socket on 127.0.0.1:5000).
- **uuid** of the launcher only for a new player, a stored one stays. **Content URL**: the one of the garlic-launcher
  wins, settings dialog with name and content URL only shown, and without index no settings dialog (the player waits
  for the configuration of the launcher); Philips: only a default without index.
- **Supervisor of the core**: restarts and installations (`NativeCore.Supervisor`) go to the launcher.
- **Broadcasts** of the launcher (`ConfigReceiver`: `config_path`, `SmilIndexReceiver`: `smil_index_path`): applied,
  also without a found launcher, like the legacy player.
- **Ending on purpose** (menu, Ctrl+Q): `PlayerClosedReceiver` of the garlic-launcher, which then does
  not start the player again. Cancel of the settings dialog at the start does not tell it.
- **User agent**: name and version of the launcher behind the version, `garlic-android/2.0.0.997Lphilips-FB01.09` or
  `…Lgarlic-<version>`, as the legacy player; garlic-hub reads the build number before the `L`. Only in the user agent,
  not in the version of the player (firmware update, REST API).
- **Master volume and brightness** (`deviceSettings`, `audio/volume` and `display/brightness` of the configuration,
  at the start and when a configuration changes them; neither in the legacy player):
  - volume: the player itself, on every device, with or without launcher (`GarlicActivity.setMasterVolume()`): the
    media volume of the system (`AudioManager.setStreamVolume(STREAM_MUSIC)`), no permission needed. Videos, audios
    and web pages play on it, the `soundLevel` of a medium is a factor below it (master 50 % × 80 % = 40 %), web
    pages have none. Devices with a fixed volume (`isVolumeFixed()`, often HDMI boxes) ignore it. On Philips the
    volume of the display itself (menu, SICP `0x44`) stays as it is and limits what Android gives.
  - brightness: only on Philips, as Smart Power of SICP (`0xDD`, chapter 7.2; `PhilipsLauncher.setBrightness()`), a
    power saving that dims the backlight in levels: below 33 % High (the darkest), below 66 % Medium, else Off (Low is
    the same as Off, says the document). Coarse: on the BDL4550D High is only a little darker (tested by hand,
    2026-11-23). Only documented interfaces: the video parameters of SICP (`0x32`/`0x33`, chapter 5.1) answer "not
    available" for an installed apk, as the document says for QL3, and SICP has no backlight level. Not used: the
    backlight of the MStar firmware (`ITvPicture.setBacklight`, 0 clearly darker than 100) is not documented. The
    brightness of Android (`screen_brightness`, also `screenBrightness` of a window) does not reach the panel there.
    Launcher 1.x has no command for it.
- **Deep standby on Philips** (`standby_mode` `deep`, the legacy player did nothing): the display is really off, not
  only dark, and the player sleeps with it; it cannot send anything until the display wakes itself. So
  `PhilipsLauncher.activateDeepStandBy()` first programs the wake time, checks it, then switches off (SICP document
  V2.03):
  1. **Scheduling page 7** (`0x5A`, chapter 8): on at the planned end of the `blankScreen` element
     (`remainingTime()`, the earlier of its `dur` and `end`), no off time (24:60), source Custom (`0x18`, the custom
     app, which is the player), its weekday and every week. Local time of the display (`persist.sys.timezone`, the
     same the core plans with), rounded up to the minute, at least 2 minutes away.
  2. **Read back** (`0x5B` page 7), 1 s later: the answer must be enabled and carry the written times, source and
     days, within 3 s. If not, the display does not go off: only the backlight (`0x72`), on again at the end of the
     element. The log shows both pages in hex.
  3. **Power off** (`0x18 0x01`, chapter 4.1: backlight off, CPU clock low).
  At the wake time the firmware switches on and brings the player to the front. Without a planned end (a trigger,
  only the end of a container) `remainingTime()` is unknown: the backlight goes off instead (`0x72`) and on again at
  the end of the element. A wake time more than a week away comes early: the page fires on its weekday every week.
  Tested on the BDL4550D below (2026-11-23): power off, woken by the page on the minute of the planned end.
- **The scheduling pages belong to the CMS with `deep`.** The firmware switches exactly at the minutes of a page, every
  week, whatever the player plays; it does not hold a state between them. A page left behind switches the display
  beside the SMIL. The BDL4550D does not even take 24:60 as "no off time": page 7 showed an end time of 1:00 in the
  menu and the display went off at midnight of the wake day (2026-11-23), and would have stayed off until the wake
  time a week later. So with `standby_mode` exactly `deep` the player clears all pages 1 to 7 (`0x5A`, disabled,
  24:60, no source, no days; `PhilipsLauncher.takeOverSchedule()`):
  - at the start, after the core read the configuration, before the first `blankScreen`;
  - after waking: when the `blankScreen` element of the deep standby ends (`deepStandbyEnded()`).
  Schedules set up in the menu of the display are lost then; `deep` means the CMS alone switches the display. With
  any other `standby_mode` (`partially`, `no_standby`, none) the player does not touch the pages, a schedule from the
  menu stays. Not cleared: pages left from `deep` when the CMS switches to another mode (the player reads the mode
  only at the start and at `blankScreen`). Whoever switches a display from `deep` to
  another mode has to clear page 7 in the menu (Scheduling) by hand, else it wakes the display once a week and, on the
  BDL4550D, switches it off at midnight of that day. Likewise a mode changed to `deep` clears the pages only at the
  next start. The schedule commands run one after the other in one thread, so a clear after waking cannot overwrite
  the wake time of a `blankScreen` that follows at once.
- **Daily restart:** the core plans it (`schedule/reboot_days`, `schedule/reboot_time` of the configuration, like
  garlic-lib) and hands `Reboot` to the launcher; on Philips the broadcast `php.intent.action.REBOOT`. The Philips
  firmware has a restart schedule of its own (menu Signage Display, `com.tpv.signagesettings`, receiver
  `AutoRestartReceiver` with `tpv.intent.action.AUTO_RESTART_ENABLE_CHANGE`/`…_TIME_CHANGE`, extras not documented;
  SICP V2.03 has only the immediate restart `0x57`). The player leaves it alone: the schedule belongs to the
  configuration from the CMS, the same on every device. Both switched on restart the display twice; switch the one in
  the menu off.
- **Written for one series:** Philips D-Line `43BDL4550D/00` (SICP model number), firmware `02.30` (SICP firmware
  build), Android 8.0 (API 26), MStar platform `m7221` (`ro.product.device` `sugarcane`), build
  `TVOS-04.24.010.05.03` of 2024-10-09. The commands follow the SICP document V2.03 of June 2019, checked against
  V2.13 of September 2025, whose platform table lists `xxBDL4550D` as platform "BDL4550D 3.0". Newer series (other
  platforms, newer Android, newer SICP versions) may lack a command or number the scheduling pages differently. Check
  every new series against the SICP document of its firmware.
- **The uuid** as of the legacy player: model number and serial, `43BDL4550D-AU0A2123004336`, only a start value for
  a new player. Model number and firmware both answer `0xA1` without saying which was asked, so `fetchDeviceInformation()`
  asks them one after the other and waits up to 1 s for each answer (NAV, `0x18` as the only byte, is none). The
  legacy player told them apart by length (15 and 12 bytes); from SICP 2.09 on the model comes with the stroke number
  (`43BDL4550D/00`) and the firmware without "V" (`02.30`), 18 and 10 bytes, so it took neither and the uuid stayed
  empty. The stroke number is cut off for the uuid, so it is the one of the legacy player.
- **The screen in the system report** (`hardwareInfo`, `getDisplayHardware()`): model with stroke number and serial over
  SICP, vendor Philips. On the BDL4550D (read by hand, 2026-11-23): `43BDL4550D/00`, serial `AU0A2123004336`, firmware `02.30`,
  build date `2024/10/09`. Taken once after the start, when the launcher had its 5 s to answer; the uuid stays as
  before.

## Watchdog

The UI thread must not hang: everything on the screen, the dialogs and the keys run in it. On a Philips BDL4550D
(Android 8) a video did not start (the decoders of the firmware failed), and its `MediaPlayer.release()` then waited
for the media server of the firmware, which never answered. The screen stayed black, the information dialog did not
open, Android noticed it only 50 minutes later at the next key (ANR); the players the firmware started again hung
in the same media server. Neither launcher notices it: both only check that the player is in front.

- `Watchdog.java`: a thread of its own posts a beat to the UI thread every 5 s. When none has run for 30 s
  (`SystemClock.uptimeMillis`, deep sleep does not count), it calls `GarlicActivity.uiHangs()` once, in its thread.
- `uiHangs()`: a warning into logcat and, through `NativeCore.logWarning` (the core writes it, waits at most 2 s),
  into `debug.log` with category `Watchdog` and the topmost frames of the UI thread, e.g.
  `UI thread hangs for 30 s, restart of the device: android.media.MediaPlayer._release:-2 < …`. Then the launcher
  restarts the device (`rebootOS("Watchdog")`: garlic-launcher `CommandReceiver` `reboot`, Philips
  `php.intent.action.REBOOT`). Without a launcher only the log.
- A restart of the device and not of the player: the hanging part is the media server of the system, a new player
  would hang again. The sdl player gets the same from outside: its heartbeat goes to the garlic-daemon, which
  restarts it ([supervisor protocol](../../core/docs/supervisor_protocol.md)); there a new process helps, mpv runs
  in it.
- Not a `Service`: its callbacks run in the UI thread. A core thread that hangs is not noticed.

## Not done yet

- Launcher tested in the emulator with the garlic-launcher only (uuid, content URL, locked settings, Exit); not
  restart, installation, blankScreen, Philips.
- Tested in the emulator (x86_64, API 31): images, video, web page, brush over web page and video, menu, debug
  dialog. On a device (Rockchip tablet, Android 7.1.2) with armeabi-v7a, without launcher: the same plus settings
  dialog and loading overlay; web pages need a newer system WebView than the 56 of the device (119 works). On an
  RK3399 box (Android 7.1.2) with arm64-v8a: more than 26 hours without a crash, the memory nearly flat
  (about 2 MB/h).
- Watchdog only built, not yet tested on a device (hang and restart).
- Master volume and brightness on Philips only built, not tested from the player (Smart Power only by hand).
- Philips scheduling pages: read back (`0x5B`) and clearing (`takeOverSchedule()`) only built, not tested on the
  BDL4550D; the format of the `0x5B` answer is the one of SICP V2.03, not seen from this firmware yet.
- A background image with repeatX or repeatY stretches the last pixels in the other direction (BitmapDrawable).
