# Test for core/trackhistory.* and core/command.* (with core/commands_*.cpp):
# every registered command run with undo and redo on the baseline projects,
# and the history's scopes, merging, caps and listener; with every file of
# core/ (core/core.pri) and only QtCore and QtGui. Built by
# tools/run-tests.sh; gets the repository folder as its first argument.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = commands
TEMPLATE = app

SOURCES += main.cpp
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
