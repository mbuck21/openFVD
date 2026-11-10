# Test for core/ as a whole: the console program fvdcore, built from
# core/core.pri alone. No QtWidgets, no OpenGL, no file from ui/ or renderer/.
# Built by tools/run-tests.sh. If this stops building, something in core/ (or a
# header it includes) needs a window again: fix that, do not add modules here.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = fvdcore
TEMPLATE = app

SOURCES += main.cpp
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
