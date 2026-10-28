#-------------------------------------------------
#
# Project created by QtCreator 2017-09-19T16:16:13
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_wgt
SOURCES +=tst_wgt.cpp

include(../../../tests.pri)
RESOURCES     = resources.qrc
