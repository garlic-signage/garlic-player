#-------------------------------------------------
#
# Project created by QtCreator 2016-03-26T02:43:55
#
#-------------------------------------------------
include(../../../../../src/defaults.pri)

QT       += xml testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_shuffle
SOURCES += tst_shuffle.cpp

include(../../../../tests.pri)
