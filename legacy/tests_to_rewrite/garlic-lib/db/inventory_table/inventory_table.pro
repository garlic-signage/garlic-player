#-------------------------------------------------
#
# Project created by QtCreator 2017-12-20T00:23:17
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_inventorytable
SOURCES += tst_inventorytable.cpp
include(../../../tests.pri)
RESOURCES     = resources.qrc
