# Test for core/trainspec.* and core/trainride.*: the train ridden over the
# finished track (docs/stage-3-plan.md, piece P1, its "Proof"). Built from
# core/core.pri with QtCore and QtGui only (the project loader needs all of
# core/). Built by tools/run-tests.sh, which passes the repository folder as
# the first argument.
QT = core gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = trainride
TEMPLATE = app

SOURCES += main.cpp
HEADERS += r7tables.h
INCLUDEPATH += ../..

# The same two glm switches as fvd.pro
DEFINES += GLM_ENABLE_EXPERIMENTAL
DEFINES += GLM_FORCE_CTOR_INIT

include(../../core/core.pri)
