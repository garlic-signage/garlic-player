# Build scripts for Android, Linux, and Windows

put this scripts in the root directory of garlic-player

## Android and Linux builds

in env.sh set QT_BASE_PATH to the base of you Qt directory.

e.g. /home/username/Qt/

Use full path and not ~/ cause sometime (Android) it did not work

set the QT_VERSION e.g. to 5.12.6

if you do not use Jenkins set GARLIC_DIR to the full path of your

garlic-player installatation

e.g. /home/username/source/garlic-player/

Linux Qt builds regulary use GCC. If you want to use other compiler like clang you had to build Qt first.

I primary develop with Linux, so I split the process in 4 tasks. Run tests, build, deploy and coverage.
You can skip tests and coverage.

### Android builds with Linux

If you want to build android apk's also with Linux and must edit 2.1_buildAndroid.sh

Set	ANDROID_ARCH, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT and ANDROID_API_VERSION

e.g. for an Armv7 Android 4.4 Devices like 2013 Nexus with clang

export ANDROID_ARCH=android_armv7

export ANDROID_SDK_ROOT=/home/username/Android/Sdk 

export ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk-bundle

export ANDROID_API_VERSION=android-19 

export QT_MKSPEC=android-clang 

Again! Use ANDROID_SDK_ROOT full path to your Android-SDK do not use ~/

#### Build with GCC e.g. for Qt 5.9.9

You need to set the Qt Specs to android-g++ e.g. export QT_MKSPEC=android-g++ and uncomment the following lines to set the neccessary SDK eviroment variables

export ANDROID_NDK_TOOLCHAIN_PREFIX=arm-linux-androideabi

export ANDROID_NDK_TOOLCHAIN_VERSION=4.9

export ANDROID_NDK_HOST=linux-x86_64

export ANDROID_NDK_PLATFORM=android-10

#### Remarks for Android
for Qt < 5.12 Qt build using GCC 4.9 from ndk

It is important to download Android NDK 10e from, especially when you want to build Qt for Android, too.
Google break compatibility and do not support higher versions of GCC in later NDKs.

Beginning from Qt 5.12 and above the ANdroid lib is build with clang

I Currently (January 2020)  recommend to use Qt 5.12.6 with clang for builds

## Windows builds
We need to copy to garlic root-dir and edit 3.1.buildWindows.bat

description follows later

### Remarks for Windows
Visual Studio 2015 or later is needed, cause the ming64-compiler cannot use QWebengine. 
It is also neccessary to execute this script in a VS Native CMD
