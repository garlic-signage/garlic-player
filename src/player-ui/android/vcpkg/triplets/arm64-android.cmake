# vcpkg's arm64-android with Android 7.1.1 (API 25), the oldest the player supports; vcpkg's own triplet takes API 28,
# so the libraries would use functions Android 7 lacks, and libxml2 would miss GNU libiconv (bionic has iconv from 28)
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_MAKE_BUILD_TRIPLET "--host=aarch64-linux-android")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a)
set(VCPKG_CMAKE_SYSTEM_VERSION 25)
