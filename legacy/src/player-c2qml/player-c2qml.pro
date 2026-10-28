include(../defaults.pri)

QT          += core sql widgets gui xml xmlpatterns quick qml quickcontrols2 multimedia multimediawidgets
CONFIG      += warn_on c++11
TARGET       = garlic-player
TEMPLATE     = app
!ios:DESTDIR      = ../bin
INCLUDEPATH +=../ext/zlib/includes
INCLUDEPATH += ../ext/QtWebApp/httpserver
INCLUDEPATH +=../ext/quazip/includes
INCLUDEPATH += ../garlic-lib/
INCLUDEPATH += ../ext/zlib/
DEFINES     += QT_DEPRECATED_WARNINGS

#message(check variable: $$[ANDROID_VERSION_NAME])

Release:DEFINES += QT_NO_DEBUG_OUTPUT

macx {
    QT += webview
    #libgarlic is static compiled in osx
    ICON = ../../deployment/macx/garlic-player.icns
    LIBS += -L../lib -lgarlic -lquazip -lzlib
}
ios {
    QT += webview
    LIBS += -L../libios -lgarlic -lquazip -lzlib
    LIBS += -L$$[QT_INSTALL_PLUGINS]/mediaservice -lqavfmediaplayer -lqavfcamera
    LIBS += -framework CoreMedia -framework CoreVideo -framework AVFoundation -framework Metal
    QMAKE_ASSET_CATALOGS = ../../deployment/ios/Assets.xcassets

#    XCODEBUILD_FLAGS += -allowProvisioningUpdates

    QMAKE_INFO_PLIST = ios/Info.plist
    QMAKE_TARGET_BUNDLE_PREFIX = com.sagiadinos
    QMAKE_BUNDLE = garlic-player
    QMAKE_IOS_DEPLOYMENT_TARGET = 13.0
#    QMAKE_APPLE_TARGETED_DEVICE_FAMILY = 1,2
}

linux:!android {
    QT += webview
    LIBS += -L../lib -lgarlic #quazip and lzlib are compiled as static libs into libgarlic.so
    # -Wno-deprecated-copy is against the warnings floading with gcc 9 and Qt < 5.13
    # -Wno-deprecated-declarations is against the warnings floading with gcc 9 and Qt < 5.13
    QMAKE_CXXFLAGS += -Wno-deprecated-declarations -Wno-deprecated-copy
}

android {
    QT        += androidextras webview
    DISTFILES += \
       android_brandings/GarlicPlayer/android/AndroidManifest.xml

    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android_brandings/GarlicPlayer/android/

    # release build crashs when start debugging
    # see https://github.com/KDAB/android_openssl/issues/10
    include(../ext/android_openssl/openssl.pri)

    HEADERS  += \
        Java2Cpp.h \
        android_manager.h
    SOURCES += android_manager.cpp

    LIBS      += -L../libandroid
    GARLIC_LIB = $$OUT_PWD/../libandroid
    equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 14) {
        LIBS      += -lgarlic
        ANDROID_EXTRA_LIBS += $$OUT_PWD/../libandroid/libgarlic.so
    }
    else {

        # 5.15.2 fails to build arm64-v8a 5.15.11 succeed
        #ANDROID_ABIS=x86_64 arm64-v8a armeabi-v7a x86
        equals(ANDROID_TARGET_ARCH, armeabi-v7a) {
            LIBS += -lgarlic_armeabi-v7a -lquazip_armeabi-v7a -lzlib_armeabi-v7a
        }
        equals(ANDROID_TARGET_ARCH, arm64-v8a) {
            LIBS += -lgarlic_arm64-v8a -lquazip_arm64-v8a -lzlib_arm64-v8a
        }
        equals(ANDROID_TARGET_ARCH, x86_64) {
            LIBS += -lgarlic_x86_64 -lquazip_x86_64 -lzlib_x86_64
        }
        equals(ANDROID_TARGET_ARCH, x86) {
            LIBS += -lgarlic_x86 -lquazip_x86 -lzlib_x86
        }

        # sometimes neccessary to include here
      #  ANDROID_EXTRA_LIBS += $$OUT_PWD/../libandroid/libgarlic_armeabi-v7a.so
      # ANDROID_EXTRA_LIBS += $$OUT_PWD/../libandroid/libgarlic_arm64-v8a.so
     #   ANDROID_EXTRA_LIBS += $$OUT_PWD/../libandroid/libgarlic_x86_64.so
     #   ANDROID_EXTRA_LIBS += $$OUT_PWD/../libandroid/libgarlic_x86.so
    }
}
win32 {
    Release:LIBS += -L../lib -lgarlic -lquazip -lzlib
    Debug:LIBS += -L../lib -lgarlic -lquazipd -lzlib
}

include(../player-common/common.pri)

SOURCES += \
    media/brush.cpp \
    media/image.cpp \
    media/player_base_media.cpp \
    media/ref_command.cpp \
    media/video.cpp \
    media/web.cpp \
    media/audio.cpp \
    media/media_factory.cpp \
    main.cpp \
    mainwindow.cpp \
    region.cpp \
    regions_list.cpp

HEADERS  += \
    media/brush.h \
    media/image.h \
    media/player_base_media.h \
    media/ref_command.h \
    media/video.h \
    media/web.h \
    media/audio.h \
    media/media_factory.h \
    mainwindow.h \
    region.h \
    regions_list.h
RESOURCES += qml.qrc

DISTFILES += \
    android/src/com/sagiadinos/garlic/player/java/ConfigReceiver.java \
    android/src/com/sagiadinos/garlic/player/java/GarlicActivity.java \
    android/src/com/sagiadinos/garlic/player/java/GarlicLauncher.java \
    android/src/com/sagiadinos/garlic/player/java/LauncherInterface.java \
    android/src/com/sagiadinos/garlic/player/java/PhilipsLauncher.java \
    android/src/com/sagiadinos/garlic/player/java/SICPDef.java \
    android/src/com/sagiadinos/garlic/player/java/SmilIndexReceiver.java \
    android/src/com/sagiadinos/garlic/player/java/SocketClient.java \
    android_brandings/GarlicPlayer/android/build.gradle \
    android_brandings/GarlicPlayer/android/gradle.properties \
    android_brandings/GarlicPlayer/android/gradle/wrapper/gradle-wrapper.jar \
    android_brandings/GarlicPlayer/android/gradle/wrapper/gradle-wrapper.properties \
    android_brandings/GarlicPlayer/android/gradlew \
    android_brandings/GarlicPlayer/android/gradlew.bat \
    android_brandings/GarlicPlayer/android/res/values/libs.xml
