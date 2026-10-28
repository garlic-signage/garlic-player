#-------------------------------------------------
#
# Project created by QtCreator 2017-09-22T16:10:19
#
#-------------------------------------------------
include(../../../../src/defaults.pri)


QT       += sql network testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_base_manager
SOURCES += tst_base_manager.cpp

include(../../../tests.pri)
