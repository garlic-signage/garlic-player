#-------------------------------------------------
#
# Project created by QtCreator 2017-11-29T18:52:36
#
#-------------------------------------------------
include(../../../../../src/defaults.pri)

QT       += xml sql testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_excl

SOURCES += tst_event_logs.cpp

include(../../../../tests.pri)
RESOURCES     = resources.qrc
