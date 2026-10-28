#-------------------------------------------------
#
# Project created by QtCreator 2016-03-21T22:47:38
#
#-------------------------------------------------
include(../../../../src/defaults.pri)

QT       += xml testlib
QT       -= gui

CONFIG   += console testcase c++11
CONFIG   -= app_bundle
TEMPLATE = app

TARGET = tst_tseq
SOURCES += tst_seq.cpp

include(../../../tests.pri)
