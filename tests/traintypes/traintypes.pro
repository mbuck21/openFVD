# Test for core/traintype.*: QtCore only. Built by tools/run-tests.sh.
QT = core
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = traintypes
TEMPLATE = app

SOURCES += main.cpp ../../core/traintype.cpp ../../core/trainspec.cpp
HEADERS += ../../core/traintype.h ../../core/trainspec.h

# The test gets the repository folder as its first argument (tools/run-tests.sh
# passes it) and finds data/traintypes.json from there.
