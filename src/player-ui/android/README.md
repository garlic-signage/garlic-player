# android

The Android frontend of the garlic-player: Java on the core (`PlayerRuntimeApi` in a `ThreadedRuntime`), like the sdl
and the JS player. Video and audio with Media3 (ExoPlayer, the decoders of the device), web pages with the WebView of
the system, images with `BitmapFactory`; all are views of one hierarchy, so they stack by z-index (video in a
`TextureView`, not a `SurfaceView`, which would lie over or under everything). Android 7.1.1 (API 25) or newer.

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
| `app/…/java/Media.java`      | image, video/audio (Media3), web page (WebView), brush                           |
| `app/…/java/FitFrame.java`   | fit and mediaAlign like `place()` of the sdl player                             |
| `app/…/java/Dialogs.java`    | menu, settings dialog, debug dialog (with the running downloads)                 |
| `app/…/java/Paths.java`      | ini, cache and logs at the places of the earlier players, the CA bundle for libcurl |
| `app/…/java/BlankScreen.java` | adapi:blankScreen with the standby_mode, like `../supervisor/blank_screen.cpp` |
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
- `adapi:blankScreen` with the `standby_mode` of the configuration: `partially` screen off until the element ends,
  `deep` the launcher sleeps until its planned end (without one: screen off), otherwise nothing; without launcher the
  screen stays on. Other commands fail.

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
- Master volume and brightness (`deviceSettings`) are not applied: launcher 1.x has no command for them.

## Not done yet

- Launcher tested in the emulator with the garlic-launcher only (uuid, content URL, locked settings, Exit); not
  restart, installation, blankScreen, Philips.
- Tested in the emulator (x86_64, API 31): images, video, web page, brush over web page and video, menu, debug
  dialog. On a device (Rockchip tablet, Android 7.1.2) with armeabi-v7a, without launcher: the same plus settings
  dialog and loading overlay; web pages need a newer system WebView than the 56 of the device (119 works). On an
  RK3399 box (Android 7.1.2) with arm64-v8a: more than 26 hours without a crash, the memory nearly flat
  (about 2 MB/h).
- A background image with repeatX or repeatY stretches the last pixels in the other direction (BitmapDrawable).
