# fvdcore: the command-line program, built from core/ and cli/ alone. No
# QtWidgets, no OpenGL, no file from ui/ or renderer/. Built by
# tools/build-windows.sh into build/cli/ and copied to build/dist/ beside
# FVD.exe. If this stops building, something in core/ (or a header it
# includes) needs a window again: fix that, do not add modules here.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = fvdcore
TEMPLATE = app

INCLUDEPATH += ..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(cli.pri)
include(../core/core.pri)
