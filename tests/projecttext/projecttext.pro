# Test for core/projecttext.*: the project as text. Built from core/core.pri
# (a project needs the whole model) with QtCore and QtGui only, like
# tests/coreonly. Built by tools/run-tests.sh, which passes the repository
# folder as the first argument.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = projecttext
TEMPLATE = app

SOURCES += main.cpp
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
