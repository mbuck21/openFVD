# The command-line program's own sources (not core/: that is core/core.pri).
# One list for both users: cli/fvdcore.pro (the program that is deployed) and
# tests/coreonly/coreonly.pro (the same program, built by tools/run-tests.sh).
# A new command: its cli/cmd_<name>.cpp goes here, plus one line in the table
# in main.cpp and one `extern` line in commands.h.

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/cmd_check.cpp \
    $$PWD/cmd_commands.cpp \
    $$PWD/cmd_compare.cpp \
    $$PWD/cmd_export.cpp \
    $$PWD/cmd_help.cpp \
    $$PWD/cmd_import.cpp \
    $$PWD/cmd_limits.cpp \
    $$PWD/cmd_measures.cpp \
    $$PWD/cmd_nodes.cpp \
    $$PWD/cmd_selftest.cpp \
    $$PWD/cmd_session.cpp \
    $$PWD/cmd_summary.cpp \
    $$PWD/cmd_text.cpp \
    $$PWD/main.cpp \
    $$PWD/project.cpp

HEADERS += \
    $$PWD/commands.h \
    $$PWD/project.h
