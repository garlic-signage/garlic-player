include(../defaults.pri)
include(../ext/QtWebApp/httpserver/httpserver.pri)
include(../ext/quazip/quazip.pri) # needed for android build

QT       += core sql network xml widgets
lessThan(QT_MAJOR_VERSION, 6): QT += xmlpatterns

TEMPLATE  = lib
CONFIG   += warn_on c++11 stl
DEFINES  += QUAZIP_STATIC QT_DEPRECATED_WARNINGS
TARGET    = garlic
CONFIG(release, debug|release) {
    message("release")
    DEFINES += QT_NO_DEBUG_OUTPUT
}
INCLUDEPATH += ../ext/zlib/

linux:!android {
    message( "linux compile" )
    DESTDIR = ../lib/
    LIBS += -L../lib -lquazip -lzlib
    #temporary ToDO
    #  warning: template-id not allowed for constructor in C++20 in Arch gcc
    QMAKE_CXXFLAGS += -Wno-template-id-cdtor
    CONFIG(debug, debug|release) {
        message( "debug" )
        CONFIG += gcov
    }
}

ios{
    DESTDIR = ../libios/
    LIBS += -L../libios -lquazip -lzlib
}

android{
    message( "android compile" )
    DESTDIR = ../libandroid/
    LIBS += -L../libandroid
    equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 14) {
        LIBS += -lquazip
        LIBS += -lzlib
    }
    else{
        CONFIG += staticlib # because androiddeploy is not able to put the dyn lib in the required folder
        ANDROID_ABIS=armeabi-v7a arm64-v8a x86 x86_64
        equals(ANDROID_TARGET_ARCH, armeabi-v7a) {
            TARGET = garlic_armeabi-v7a
            LIBS += -lquazip_armeabi-v7a -lzlib_armeabi-v7a
        }
        equals(ANDROID_TARGET_ARCH, arm64-v8a) {
            TARGET = garlic_arm64-v8a
            LIBS += -lquazip_arm64-v8a -lzlib_arm64-v8a
        }
        equals(ANDROID_TARGET_ARCH, x86_64) {
            TARGET = garlic_x86_64
            LIBS += -lquazip_x86_64 -lzlib_x86_64
        }
        equals(ANDROID_TARGET_ARCH, x86) {
            TARGET = garlic_x86
            LIBS += -lquazip_x86 -lzlib_x86
        }
    }
}

win32 {
    message( "windows compile" )
    CONFIG += staticlib
    DESTDIR = ../lib/
    Release:LIBS += -L../lib -lquazip -lzlib
    Debug:LIBS += -L../lib  -lquazipd -lzlib
}

macx {
    message( "macOS compile" )
    # release version build as static lib
    CONFIG += staticlib
    QMAKE_APPLE_DEVICE_ARCHS = arm64 x86_64
    DESTDIR = ../lib/
    LIBS += -L../lib -lquazip -lzlib
    CONFIG(debug, debug|release) {
        message( "debug" )
 #       CONFIG += gcov
    }

    greaterThan(QT_MAJOR_VERSION, 5) {
        INCLUDEPATH += /usr/local/include
        LIBS += -L/usr/local/lib -lxerces-c -lxqilla
    }

}


INCLUDEPATH+=../ext/zlib/includes
INCLUDEPATH+=../ext/quazip/includes

SOURCES += \
    files/base_manager.cpp \
    files/download_factory.cpp \
    files/downloader.cpp \
    files/download_queue.cpp \
    files/free_disc_space.cpp \
    files/index_manager.cpp \
    files/index_model.cpp \
    files/media_manager.cpp \
    files/media_model.cpp \
    files/network_access.cpp \
    files/webdav.cpp \
    files/wgt.cpp \
    rest_api/httpd.cpp \
    rest_api/json_model.cpp \
    rest_api/request_mapper.cpp \
    rest_api/v2/app.cpp \
    rest_api/v2/base_controller.cpp \
    rest_api/v2/files.cpp \
    rest_api/v2/oauth.cpp \
    rest_api/v2/system_info.cpp \
    rest_api/v2/task.cpp \
    smilparser/base_timings.cpp \
    smilparser/body_parser.cpp \
    smilparser/conditional/adapi_wrapper.cpp \
    smilparser/conditional/expr.cpp \
    smilparser/container/excl/priority_queue.cpp \
    smilparser/container/tools/childs.cpp \
    smilparser/current_playing_media.cpp \
    smilparser/element_factory.cpp \
    smilparser/elements_container.cpp \
    smilparser/head/firmware_downloader.cpp \
    smilparser/head/placeholder.cpp \
    smilparser/head/task_scheduler.cpp \
    smilparser/head/xml_configuration.cpp \
    smilparser/head_parser.cpp \
    smilparser/media/audio.cpp \
    smilparser/media/base_media.cpp \
    smilparser/media/brush.cpp \
    smilparser/media/image.cpp \
    smilparser/media/prefetch.cpp \
    smilparser/media/ref_command.cpp \
    smilparser/media/unknown.cpp \
    smilparser/media/video.cpp \
    smilparser/media/web.cpp \
    smilparser/media/widget.cpp \
    smilparser/medialist.cpp \
    smilparser/smil.cpp \
    smilparser/timings/begin_timer.cpp \
    smilparser/timings/end_timer.cpp \
    smilparser/timings/enhanced_timer.cpp \
    smilparser/timings/wrapper.cpp \
    smilparser/trigger/accesskey.cpp \
    smilparser/trigger/base_trigger.cpp \
    smilparser/trigger/clock_value.cpp \
    smilparser/timings/simple_timer.cpp \
    smilparser/trigger/event.cpp \
    smilparser/trigger/iso_date.cpp \
    smilparser/trigger/notify.cpp \
    smilparser/trigger/syncbase.cpp \
    smilparser/trigger/target_trigger.cpp \
    smilparser/trigger/wallclock.cpp \
    smilparser/base.cpp \
    smilparser/container/tools/shuffle.cpp \
    smilparser/container/body.cpp \
    smilparser/container/container.cpp \
    smilparser/container/excl.cpp \
    smilparser/container/par.cpp \
    smilparser/container/priorityclass.cpp \
    smilparser/container/seq.cpp \
    system_infos/disc_space.cpp \
    system_infos/wrapper_storageinfo.cpp \
    tools/logging_categories.cpp \
    smilparser/head/subscription.cpp \
    system_infos/memory.cpp \
    system_infos/network.cpp \
    system_infos/cpu.cpp \
    system_infos/general.cpp \
    lib_facade.cpp \
    reports/create/base_reports.cpp \
    reports/create/system_report.cpp \
    reports/system_report_manager.cpp \
    files/file_downloader.cpp \
    tools/logger.cpp \
    reports/event_logs_manager.cpp \
    reports/create/event_logs.cpp \
    tools/log_file.cpp \
    reports/base_report_manager.cpp \
    reports/play_logs_manager.cpp \
    reports/create/play_logs.cpp \
    reports/inventory_report_manager.cpp \
    db/inventory_table.cpp \
    reports/create/inventory_report.cpp \
    tools/main_configuration.cpp \
    tools/reboot/scheduler.cpp \
    tools/reboot/timer.cpp \
    tools/reboot/weekdayconverter.cpp \
    tools/resource_monitor.cpp \
    tools/wrapper_settings.cpp

HEADERS += \
    files/base_manager.h \
    files/download_factory.h \
    files/downloader.h \
    files/download_queue.h \
    files/file_downloader.h \
    files/free_disc_space.h \
    files/index_manager.h \
    files/index_model.h \
    files/media_manager.h \
    files/media_model.h \
    files/network_access.h \
    files/webdav.h \
    files/wgt.h \
    rest_api/httpd.h \
    rest_api/json_model.h \
    rest_api/request_mapper.h \
    rest_api/v2/app.h \
    rest_api/v2/base_controller.h \
    rest_api/v2/files.h \
    rest_api/v2/oauth.h \
    rest_api/v2/system_info.h \
    rest_api/v2/task.h \
    smilparser/base.hpp \
    smilparser/body_parser.h \
    smilparser/conditional/adapi_wrapper.hpp \
    smilparser/conditional/expr.hpp \
    smilparser/container/excl/priority_queue.h \
    smilparser/container/tools/childs.h \
    smilparser/current_playing_media.h \
    smilparser/element_factory.h \
    smilparser/elements_container.h \
    smilparser/head/firmware_downloader.h \
    smilparser/head/placeholder.h \
    smilparser/head/task_scheduler.h \
    smilparser/head/xml_configuration.h \
    smilparser/head_parser.h \
    smilparser/media/base_media.h \
    smilparser/media/audio.h \
    smilparser/media/brush.h \
    smilparser/media/image.h \
    smilparser/media/prefetch.h \
    smilparser/media/ref_command.h \
    smilparser/media/unknown.h \
    smilparser/media/video.h \
    smilparser/media/web.h \
    smilparser/media/widget.h \
    smilparser/medialist.h \
    smilparser/smil.h \
    smilparser/timings/begin_timer.h \
    smilparser/timings/end_timer.h \
    smilparser/timings/enhanced_timer.h \
    smilparser/timings/simple_timer.hpp \
    smilparser/timings/wrapper.h \
    smilparser/trigger/accesskey.hpp \
    smilparser/trigger/base_trigger.h \
    smilparser/trigger/clock_value.hpp \
    smilparser/trigger/event.h \
    smilparser/trigger/iso_date.h \
    smilparser/trigger/notify.h \
    smilparser/trigger/syncbase.h \
    smilparser/trigger/target_trigger.h \
    smilparser/container/tools/shuffle.h \
    smilparser/container/body.h \
    smilparser/container/par.h \
    smilparser/container/priorityclass.h \
    smilparser/container/seq.h \
    smilparser/container/container.h \
    smilparser/container/excl.h \
    smilparser/base_timings.h \
    smilparser/head/subscription.h \
    smilparser/trigger/wallclock.hpp \
    system_infos/disc_space.hpp \
    system_infos/i_storageinfo.hpp \
    system_infos/memory.h \
    system_infos/network.h \
    system_infos/cpu.h \
    system_infos/general.h \
    system_infos/wrapper_storageinfo.hpp \
    tools/i_main_configuration.hpp \
    tools/i_settings.hpp \
    tools/logging_categories.h \
    tools/logger.h \
    tools/log_file.h \
    tools/main_configuration.hpp \
    tools/reboot/i_weekdayconverter.hpp \
    tools/reboot/scheduler.hpp \
    tools/reboot/timer.hpp \
    tools/reboot/weekdayconverter.hpp \
    tools/resource_monitor.h \
    reports/create/base_reports.h \
    reports/create/system_report.h \
    reports/system_report_manager.h \
    reports/event_logs_manager.h \
    reports/create/event_logs.h \
    reports/base_report_manager.h \
    reports/play_logs_manager.h \
    reports/create/play_logs.h \
    reports/create/inventory_report.h \
    reports/inventory_report_manager.h \
    db/inventory_table.h \
    lib_facade.h \
    tools/wrapper_settings.hpp \
    version.h

DISTFILES +=

