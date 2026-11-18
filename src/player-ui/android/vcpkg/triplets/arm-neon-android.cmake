# vcpkg's arm-neon-android with Android 7.1.1 (API 25), the oldest the player supports; vcpkg's own triplet takes API 28,
# so the libraries would use functions Android 7 lacks, and libxml2 would miss GNU libiconv (bionic has iconv from 28)
set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=armv7a-linux-androideabi")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=armeabi-v7a -DANDROID_ARM_NEON=ON)
set(VCPKG_CMAKE_SYSTEM_VERSION 25)
