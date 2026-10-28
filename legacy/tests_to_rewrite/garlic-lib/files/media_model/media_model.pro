#-------------------------------------------------
#
# Project created by QtCreator 2017-09-14T21:30:06
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql network testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_media_model
SOURCES += tst_media_model.cpp

include(../../../tests.pri)
RESOURCES = resources.qrc
