#-------------------------------------------------
#
# Project created by QtCreator 2017-09-22T22:15:43
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += sql network xml testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_index_model
SOURCES += tst_index_model.cpp

include(../../../tests.pri)
RESOURCES = resources.qrc
