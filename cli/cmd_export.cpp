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

// fvdcore export: loads a project, computes it, exports it and saves it again,
// the way the windowed program's --batch-export does, and writes the same
// files (docs/testing.md). Its output is byte for byte what the first
// version of fvdcore (tests/coreonly/main.cpp) wrote.

#include "commands.h"
#include "project.h"

#include "../core/elementfile.h"
#include "../lenassert.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <cmath>
#include <cstdio>

using namespace std;

// The export dialog's defaults, as in ui/batchrunner.cpp (ui/exportui.cpp).
static const double EXPORT_SEGMENT_LENGTH = 2.0;
static const double EXPORT_RELROLL_THRESHOLD = 85.0;

static bool exportNL2File(track* tTrack, const QString& fileName)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    return writeNL2Element(fileName, tTrack, fPerNode, fromIndex, fromIndex+toIndex);
}

static bool exportNL1File(track* tTrack, const QString& fileName, bool tangent)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    float fRollThresh = sin(EXPORT_RELROLL_THRESHOLD*F_PI/180.f);
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    if(tangent) {
        return writeNL1TangentElement(fileName, tTrack, fPerNode, fromIndex, toIndex+fromIndex, fRollThresh, false);
    } else {
        return writeNL1SplineElement(fileName, tTrack, fPerNode, fromIndex, toIndex+fromIndex, fRollThresh, false);
    }
}

// The three exports of every track with sections, then the re-saved project
// as <stem>.resaved.fvd; each file written is listed in report. Shared with
// the session's "export" (cli/cmd_session.cpp).
bool writeProjectExports(coreProject& project, const QDir& dir, const QString& stem, QStringList& report, QString& error)
{
    QList<track*>& trackList = project.tracks;
    for(int i = 0; i < trackList.size(); ++i) {
        track* tTrack = trackList[i];
        if(tTrack->lSections.size() == 0) {
            report.append(QString("skipped track %1 (%2): no sections").arg(i+1).arg(tTrack->name));
            continue;
        }
        QString base = stem;
        if(i > 0) {
            base += QString(".track%1").arg(i+1);
        }

        QString nl2Name = base + ".nl2elem";
        QString tangentName = base + ".tangent.nlelem";
        QString splineName = base + ".spline.nlelem";

        if(!exportNL2File(tTrack, dir.filePath(nl2Name))) {
            error = QString("cannot write ") + nl2Name;
            return false;
        }
        report.append(QString("wrote ") + nl2Name);
        if(!exportNL1File(tTrack, dir.filePath(tangentName), true)) {
            error = QString("cannot write ") + tangentName;
            return false;
        }
        report.append(QString("wrote ") + tangentName);
        if(!exportNL1File(tTrack, dir.filePath(splineName), false)) {
            error = QString("cannot write ") + splineName;
            return false;
        }
        report.append(QString("wrote ") + splineName);
    }

    QString resavedName = stem + ".resaved.fvd";
    QString saveResult = saveProjectFile(dir.filePath(resavedName), project.texPath, trackList, &project);
    if(saveResult.contains("Error")) {
        error = QString("saving ") + resavedName + ": " + saveResult;
        return false;
    }
    report.append(QString("wrote ") + resavedName);
    return true;
}

namespace {

class exporter
{
public:
    QStringList lReport;
    QString sReportFile;

    int runExport(const QString& projectFile, const QString& outDir);

private:
    bool writeProjectFiles(coreProject& project, const QDir& dir, const QString& stem, QString& error);
    int finish(bool ok, const QString& error);
};

// Mirrors batchRunner::runExport() in ui/batchrunner.cpp, step by step.
int exporter::runExport(const QString& projectFile, const QString& outDir)
{
    QString stem = QFileInfo(projectFile).fileName();
    if(stem.endsWith(".fvd", Qt::CaseInsensitive)) {
        stem.chop(4);
    }

    if(!QDir().mkpath(outDir)) {
        fprintf(stderr, "cannot create %s\n", qPrintable(outDir));
        return 1;
    }
    QDir dir(outDir);
    sReportFile = dir.filePath(stem + ".batch.txt");

    coreProject project;
    QString loadError;
    QStringList warnings;
    if(!loadProject(projectFile, project, loadError, &warnings)) {
        return finish(false, loadError);
    }
    for(int i = 0; i < warnings.size(); ++i) {
        lReport.append(QString("warning: ") + warnings.at(i));
    }

    QString error;
    if(!writeProjectFiles(project, dir, stem, error)) {
        return finish(false, error);
    }
    return finish(true, QString());
}

bool exporter::writeProjectFiles(coreProject& project, const QDir& dir, const QString& stem, QString& error)
{
    return writeProjectExports(project, dir, stem, lReport, error);
}

int exporter::finish(bool ok, const QString& error)
{
    lReport.append(ok ? QString("OK") : QString("ERROR: ") + error);

    QFile report(sReportFile);
    if(report.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&report);
        for(int i = 0; i < lReport.size(); ++i) {
            out << lReport.at(i) << "\n";
        }
        out.flush();
        report.close();
    } else {
        fprintf(stderr, "cannot write %s\n", qPrintable(sReportFile));
        return 1;
    }
    return ok ? 0 : 1;
}

} // namespace

static int runExportCommand(const QStringList& args)
{
    if(args.size() != 2 || args.at(0).startsWith("--") || args.at(1).startsWith("--")) {
        fprintf(stderr, "usage: %s\n", cmdExport.usage);
        return 1;
    }
    exporter e;
    return e.runExport(args.at(0), args.at(1));
}

const command cmdExport = {
    "export",
    "fvdcore export <project.fvd> <outdir>",
    "write the NoLimits exports, a re-saved project and a report, as FVD.exe --batch-export does",
    runExportCommand
};
