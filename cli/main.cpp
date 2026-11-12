/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// fvdcore: a console program built from core/ alone (no ui/, no renderer/, no
// QtWidgets, no OpenGL). It loads a project, computes it and reports on it or
// writes files. Its existence is the proof that the model does not need the
// windows; tools/check-core.py guards the same thing in the source.
// cli/README.md lists the commands.
//
// Qt's debug output (the exporters write thousands of lines) goes nowhere
// unless --log <file> is given. Nothing is written into the working folder.

#include "commands.h"

#include <QCoreApplication>
#include <QDir>
#include <QtDebug>
#include <cstdio>

static FILE* logFile = NULL;

// Without --log, debug output and warnings are dropped (the exporters log
// thousands of lines, and every project logs a harmless "parameter out of
// bounds" warning at its last node). A failed model check (lenAssert, which
// is qCritical in a release build) still goes to stderr, so it leaves a trace.
static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(logFile) {
        fprintf(logFile, "%s\n", msg.toLocal8Bit().constData());
    } else if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
}

// THE TABLE OF COMMANDS. A new command costs its own file,
// cli/cmd_<name>.cpp (defining `const command cmd<Name>`), plus one `extern`
// line in commands.h, one line here and one line in cli/cli.pri. `help` lists
// whatever is in this table.
static const command* const table[] = {
    &cmdExport,
    &cmdNodes,
    &cmdCompare,
    &cmdSummary,
    &cmdMeasures,
    &cmdCheck,
    &cmdToText,
    &cmdFromText,
    &cmdImport,
    &cmdSession,
    &cmdCommands,
    &cmdSelftest,
    &cmdHelp,
};
static const int tableSize = sizeof(table)/sizeof(table[0]);

const command* const* commandTable(int* count)
{
    *count = tableSize;
    return table;
}

static void printUsage(FILE* out)
{
    fprintf(out, "usage: fvdcore <command> [arguments]\n");
    for(int i = 0; i < tableSize; ++i) {
        fprintf(out, "       %s\n", table[i]->usage);
    }
    fprintf(out, "       fvdcore <repository folder>      same as selftest\n"
                 "       fvdcore help                     describe the commands\n");
}

int runCommand(const QStringList& args, FILE* errOut)
{
    if(!args.isEmpty()) {
        for(int i = 0; i < tableSize; ++i) {
            if(args.at(0) == table[i]->name) {
                return table[i]->run(args.mid(1));
            }
        }
    }
    if(errOut) {
        if(!args.isEmpty()) {
            fprintf(errOut, "unknown command: %s\n", qPrintable(args.at(0)));
        }
        printUsage(errOut);
    }
    return 1;
}

static bool isCommandName(const QString& word)
{
    for(int i = 0; i < tableSize; ++i) {
        if(word == table[i]->name) return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // --log <file> anywhere: write Qt's debug output there.
    QStringList args;
    QString logName;
    for(int i = 1; i < argc; ++i) {
        QString a = QString::fromLocal8Bit(argv[i]);
        if(a == "--log" && i+1 < argc) {
            logName = QString::fromLocal8Bit(argv[++i]);
        } else {
            args.append(a);
        }
    }
    if(!logName.isEmpty()) {
        logFile = fopen(logName.toLocal8Bit().constData(), "w");
    }
    qInstallMessageHandler(messageHandler);

    int result;
    if(args.isEmpty()) {
        result = cmdHelp.run(QStringList());
    } else if(args.at(0) == "-h" || args.at(0) == "--help") {
        result = cmdHelp.run(QStringList());
    } else if(args.size() == 1 && !isCommandName(args.at(0)) && QDir(args.at(0)).exists()) {
        // tools/run-tests.sh runs every test with the repository folder alone
        result = cmdSelftest.run(args);
    } else {
        result = runCommand(args, stderr);
    }
    if(logFile) fclose(logFile);
    return result;
}
