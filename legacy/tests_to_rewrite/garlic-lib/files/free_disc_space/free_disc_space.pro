#-------------------------------------------------
#
# Project created by QtCreator 2017-09-18T13:15:12
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql network testlib
QT       -= gui

CONFIG   += console testcase c++17
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_free_disc_space
SOURCES += \
    tst_free_disc_space.cpp


include(../../../tests.pri)
RESOURCES     = resources.qrc
