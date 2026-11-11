# Test for core/ as a whole: the console program fvdcore, built from
# core/core.pri alone plus the program's own sources in cli/cli.pri (the same
# sources as cli/fvdcore.pro, which is the deployed build). No QtWidgets, no
# OpenGL, no file from ui/ or renderer/.
# Built by tools/run-tests.sh, which runs it with the repository folder as its
# only argument (that means: selftest). If this stops building, something in
# core/ (or a header it includes) needs a window again: fix that, do not add
# modules here.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = fvdcore
TEMPLATE = app

INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../cli/cli.pri)
include(../../core/core.pri)
