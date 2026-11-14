# The model: every source file under core/, and nothing that needs a window.
# One list for both users: fvd.pro (the program) and tests/coreonly/coreonly.pro
# (the console program that proves core/ builds on its own). A new file in
# core/ goes here and nowhere else. core/secbezier.cpp.orig is a leftover and
# stays out. Needs QT += core gui (QColor, QImage) and the two glm defines of
# fvd.pro (GLM_ENABLE_EXPERIMENTAL, GLM_FORCE_CTOR_INIT); no widgets, no OpenGL.
# tools/check-core.py fails when a file in core/ includes a header from ui/ or
# renderer/.

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/command.cpp \
    $$PWD/commands_anchor.cpp \
    $$PWD/commands_section.cpp \
    $$PWD/commands_smoothing.cpp \
    $$PWD/commands_track.cpp \
    $$PWD/commands_transition.cpp \
    $$PWD/elementfile.cpp \
    $$PWD/exportfuncs.cpp \
    $$PWD/finding.cpp \
    $$PWD/forcelimits.cpp \
    $$PWD/function.cpp \
    $$PWD/fvdmath.cpp \
    $$PWD/measure.cpp \
    $$PWD/mnode.cpp \
    $$PWD/nolimitsimporter.cpp \
    $$PWD/projectfile.cpp \
    $$PWD/projecttext.cpp \
    $$PWD/ridesummary.cpp \
    $$PWD/secbezier.cpp \
    $$PWD/seccurved.cpp \
    $$PWD/secforced.cpp \
    $$PWD/secgeometric.cpp \
    $$PWD/secnlcsv.cpp \
    $$PWD/secstraight.cpp \
    $$PWD/section.cpp \
    $$PWD/smoothhandler.cpp \
    $$PWD/subfunction.cpp \
    $$PWD/track.cpp \
    $$PWD/trackhistory.cpp \
    $$PWD/trackstate.cpp \
    $$PWD/trainride.cpp \
    $$PWD/trainspec.cpp \
    $$PWD/traintype.cpp

HEADERS += \
    $$PWD/command.h \
    $$PWD/elementfile.h \
    $$PWD/exportfuncs.h \
    $$PWD/finding.h \
    $$PWD/forcelimits.h \
    $$PWD/function.h \
    $$PWD/fvdmath.h \
    $$PWD/measure.h \
    $$PWD/mnode.h \
    $$PWD/nolimitsimporter.h \
    $$PWD/projectfile.h \
    $$PWD/projecttext.h \
    $$PWD/ridesummary.h \
    $$PWD/secbezier.h \
    $$PWD/seccurved.h \
    $$PWD/secforced.h \
    $$PWD/secgeometric.h \
    $$PWD/secnlcsv.h \
    $$PWD/secstraight.h \
    $$PWD/section.h \
    $$PWD/smoothhandler.h \
    $$PWD/subfunction.h \
    $$PWD/track.h \
    $$PWD/trackhistory.h \
    $$PWD/tracklistener.h \
    $$PWD/trackstate.h \
    $$PWD/trainride.h \
    $$PWD/trainspec.h \
    $$PWD/traintype.h

# The force-limit data (core/forcelimits.h), embedded in every program built from core/, fvdcore included.
RESOURCES += $$PWD/../data/limits/limits.qrc
