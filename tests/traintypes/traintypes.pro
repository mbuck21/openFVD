# Test for core/traintype.*: QtCore only. Built by tools/run-tests.sh.
QT = core
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = traintypes
TEMPLATE = app

SOURCES += main.cpp ../../core/traintype.cpp
HEADERS += ../../core/traintype.h

# Where data/traintypes.json is, found from this folder and not typed by hand.
DEFINES += TRAINTYPES_JSON=\\\"$$PWD/../../data/traintypes.json\\\"
