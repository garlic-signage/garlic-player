#-------------------------------------------------
#
# Project created by QtCreator 2016-03-04T23:33:06
#
#-------------------------------------------------
include(../../../../../src/defaults.pri)

QT       += xml testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_container
SOURCES += tst_container.cpp

include(../../../../tests.pri)
