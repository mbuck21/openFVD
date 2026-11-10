#ifndef BATCHRUNNER_H
#define BATCHRUNNER_H

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

#include <QString>
#include <QStringList>
#include <QList>
#include <QByteArray>

class MainWindow;
class QDir;
class track;

// Runs FVD++ without user interaction for testing (docs/testing.md).
// main.cpp hands over when the first argument starts with "--batch-".
// The full program with its window classes is built, but no window is shown.
//
//   FVD.exe --batch-export <project.fvd> <outdir>
//   FVD.exe --batch-make-fixtures <outdir>
//   FVD.exe --batch-undo-check <project.fvd> <outdir> [--all-edits] [--edits n,n,...] [--each]
//   FVD.exe --batch-import <nltrack|txt|csv> <input file> <outdir>
//
// The result is reported in a text file and the exit code (0 or 1), because
// the program has no console.
class batchRunner
{
public:
    batchRunner(int argc, char *argv[]);

    static bool isBatchCommand(int argc, char *argv[]);

    // runs the command and returns the exit code; the caller leaves with std::exit()
    int run();

private:
    // the files of one state of a project: three exports per track and the re-saved project
    struct batchState
    {
        QStringList lNames;
        QList<QByteArray> lData;
    };

    int runExport();
    int runMakeFixtures();
    int runUndoCheck();
    int runImport();

    bool writeProjectFiles(MainWindow* w, const QDir& dir, const QString& stem, QString& error);
    static QStringList panelState(MainWindow* w);
    static bool writeLines(const QString& fileName, const QStringList& lines);

    bool writeState(const QString& dirName, const QString& base, batchState& state, QString& error);
    static QStringList differences(const batchState& a, const batchState& b);
    static void writeFiles(const QString& dirName, const QString& base, const batchState& state, const QStringList& kinds);
    bool windowsStayedHidden(QString& error);

    MainWindow* createHiddenWindow();
    bool exportNL2(track* tTrack, const QString& fileName);
    bool exportNL1(track* tTrack, const QString& fileName, bool tangent);
    int finish(bool ok, const QString& error);

    QStringList lArgs;
    QStringList lReport;
    QString sReportFile;
};

#endif // BATCHRUNNER_H
