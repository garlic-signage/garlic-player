#-------------------------------------------------
#
# Project created by QtCreator 2015-12-17T15:22:23
#
#-------------------------------------------------
include(../defaults.pri)
include(../player-common/common.pri)

QT          += core gui sql xml xmlpatterns widgets webenginewidgets
CONFIG      += warn_on c++17
DEFINES     += QT_DEPRECATED_WARNINGS
TARGET       = garlic-player
TEMPLATE     = app
DESTDIR      = ../bin
INCLUDEPATH += ../ext/zlib/includes
INCLUDEPATH += ../ext/QtWebApp/httpserver
INCLUDEPATH += ../garlic-lib/
Release:DEFINES += QT_NO_DEBUG_OUTPUT
INCLUDEPATH += ../ext/zlib/

# defines for using different default content_url per player
# remarks:
# protocol must be seperate from url cause the double slash // cut string and no escape sequence helped
# & and must be escaped => \&
#DEFINES += DEFAULT_CONTENT_URL_NAME=SmilControl
#DEFINES += DEFAULT_CONTENT_URL_PROTOCOL=http
#DEFINES += DEFAULT_CONTENT_URL=indexes.smil-control.com/index.php?site=get_index\&owner_id=12

# possible media backends:
# support_qtmm              Qt Multimedia lib. Slow, laggy and platform dependend
# support_qtavplayer        replacement for support_qtav
# support_libvlc            For Raspberry Pi OS only. libvlc has much better hw acceleration than ffmpeg

linux {
    contains(QT_ARCH, arm64){
        CONFIG   += support_libvlc
    } else {
        CONFIG   += support_qtavplayer
    }
}

win32 {
    CONFIG   += support_qtavplayer
}

macx {
    CONFIG   += support_qtavplayer
}


support_qtavplayer {
    DEFINES += SUPPORT_QTAVPLAYER
    DEFINES+="QT_AVPLAYER_MULTIMEDIA"
    INCLUDEPATH += ../ext/
    include(../ext/QtAVPlayer/QtAVPlayer.pri)

    QT += multimedia multimediawidgets
    HEADERS  += \
        mm_libs/qtavplayer_decoder.h \
        mm_libs/qtavplayer_widget.h
    SOURCES += \
        mm_libs/qtavplayer_decoder.cpp \
        mm_libs/qtavplayer_widget.cpp
    win32 {
        # include ffmpeg  Use your own directories
        # alternatively you can put it in Qt paths.
        INCLUDEPATH += "C:\BuildEssentials\ffmpeg\include"
        LIBS += -LC:\BuildEssentials\ffmpeg\lib
        LIBS += -LC:\BuildEssentials\ffmpeg\bin

        LIBS += -L$$QT.core.libs
    }
    macx {
        # because someone is to silly to find ffmpeg includes even in Qt Dir
        # this is only for the build server. Change it to your values
        INCLUDEPATH += /Users/niko/Deployment/ffmpeg/lib_universal_include
        LIBS += -L/Users/niko/Deployment/ffmpeg/lib_universal
        QMAKE_LFLAGS += -Wl,-rpath,/Users/niko/Deployment/ffmpeg/lib_universal
        LIBS += -ldav1d
    }
}
support_libvlc{
    DEFINES += SUPPORT_LIBVLC
    linux {
        LIBS += -lvlc -lX11
    }
    macx {
        INCLUDEPATH  += $$QT.core.libs/vlcpp
        QMAKE_LFLAGS += -F$$QT.core.libs/vlcpp
        QMAKE_LFLAGS += -F$$QT.core.libs/vlcpp/plugins
        LIBS += -L$$QT.core.libs/vlcpp/plugins
        LIBS += -L$$QT.core.libs/vlcpp -lvlc -lvlccore
    }
    HEADERS  += \
        mm_libs/vlc_decoder.h \
        mm_libs/vlc_widget.h
    SOURCES += \
        mm_libs/vlc_decoder.cpp \
        mm_libs/vlc_widget.cpp
}

support_qtmm {
     DEFINES += SUPPORT_QTMM
     QT += multimedia multimediawidgets
     HEADERS  += \
        mm_libs/qtmm_decoder.h \
        mm_libs/qtmm_widget.h
    SOURCES += \
        mm_libs/qtmm_decoder.cpp \
        mm_libs/qtmm_widget.cpp
}

linux {
    LIBS += -L../lib -lgarlic #quazip and lzlib are compiled as static libs into libgarlic.so
    # hide some annoying GCC compiler warnings
    # Todo check if neccessary from time to time
    #  warning: template-id not allowed for constructor in C++20 in Arch gcc
    QMAKE_CXXFLAGS += -Wno-template-id-cdtor
}

win32 {
    #libgarlic is static compiled in windows
    Release:LIBS += -L../lib -lgarlic -lquazip -lzlib
    Debug:LIBS += -L../lib -lgarlic -lquazipd -lzlib
}

macx {
    QMAKE_APPLE_DEVICE_ARCHS = arm64 x86_64
    debug:CONFIG+=sdk_no_version_check # remove annoying SDK messages on Apple Silicon
    ICON = ../../deployment/macx/garlic-player.icns

    #libgarlic release is static compiled in osx
    LIBS += -L../lib -lgarlic -lquazip -lzlib
}

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
    mediaplayer_wrapper.cpp \
    mediawidget_wrapper.cpp \
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
    mediaplayer_wrapper.h \
    mediawidget_wrapper.h \
    region.h \
    regions_list.h
