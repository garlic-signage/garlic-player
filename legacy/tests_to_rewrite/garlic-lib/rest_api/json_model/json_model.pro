include(../../../../src/defaults.pri)

QT       += testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_json_model
SOURCES += tst_json_model.cpp

include(../../../tests.pri)
