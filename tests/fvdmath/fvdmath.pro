# Test for core/fvdmath.*: QtCore only. Built by tools/run-tests.sh.
QT = core
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = fvdmath
TEMPLATE = app

SOURCES += main.cpp ../../core/fvdmath.cpp
HEADERS += ../../core/fvdmath.h
