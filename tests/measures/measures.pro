# Test for core/measure.* and core/finding.*: the measures and checks on every
# baseline project. Built from core/core.pri with QtCore and QtGui only (the
# project loader needs all of core/). Built by tools/run-tests.sh, which passes
# the repository folder as the first argument.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = measures
TEMPLATE = app

SOURCES += main.cpp
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
