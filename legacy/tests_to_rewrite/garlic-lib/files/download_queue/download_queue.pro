#-------------------------------------------------
#
# Project created by QtCreator 2017-09-22T17:36:25
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql network testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_download_queue
SOURCES += tst_download_queue.cpp

include(../../../tests.pri)
