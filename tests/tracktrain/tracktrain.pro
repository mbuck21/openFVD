# Test for the train in the project (core/tracktrain.*, core/commands_train.cpp
# and the import's reader of another project's additions in core/projectfile.*),
# with every file of core/ (core/core.pri) and only QtCore and QtGui. Built by
# tools/run-tests.sh; gets the repository folder as its first argument.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = tracktrain
TEMPLATE = app

SOURCES += main.cpp
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
