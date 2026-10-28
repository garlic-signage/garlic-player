#-------------------------------------------------
#
# Project created by QtCreator 2016-02-27T20:27:37
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += xml sql testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_excl
SOURCES += tst_excl.cpp

include(../../../tests.pri)
RESOURCES += priorityClass.smil
