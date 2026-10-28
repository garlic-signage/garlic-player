#-------------------------------------------------
#
# Project created by QtCreator 2016-02-28T06:11:21
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += xml sql network testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_head
SOURCES += tst_head_parser.cpp

include(../../../tests.pri)
RESOURCES += head_complex.smil head_simple.smil
