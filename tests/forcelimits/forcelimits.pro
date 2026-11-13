# Test for core/forcelimits.* and data/limits/astm-f2291-public.json: QtCore
# only. Built by tools/run-tests.sh.
QT = core
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = forcelimits
TEMPLATE = app

SOURCES += main.cpp ../../core/forcelimits.cpp
HEADERS += ../../core/forcelimits.h
RESOURCES += ../../data/limits/limits.qrc

# The test gets the repository folder as its first argument (tools/run-tests.sh
# passes it) and finds data/limits/astm-f2291-public.json from there.
