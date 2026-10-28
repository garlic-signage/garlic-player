#-------------------------------------------------
#
# Project created by QtCreator 2016-02-28T05:47:20
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += xml network sql testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_element_factory
SOURCES += tst_element_factory.cpp

include(../../../tests.pri)
