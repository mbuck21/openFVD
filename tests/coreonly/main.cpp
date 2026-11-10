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
// QtWidgets, no OpenGL). It loads a project, computes it, exports it and saves
// it again, the way the windowed program's --batch-export does, and writes the
// same files (docs/testing.md). Its existence is the proof that the model does
// not need the windows; tools/check-core.py guards the same thing in the source.
//
//   fvdcore export <project.fvd> <outdir> [--log <file>]
//   fvdcore <repository folder>            quick self-test (tools/run-tests.sh)
//
// Qt's debug output (the exporters write thousands of lines) goes nowhere
// unless --log <file> is given. Nothing is written into the working folder.

#include "../../core/track.h"
#include "../../core/projectfile.h"
#include "../../core/elementfile.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtDebug>
#include <cmath>
#include <cstdio>

using namespace std;

// The export dialog's defaults, as in ui/batchrunner.cpp (ui/exportui.cpp).
static const double EXPORT_SEGMENT_LENGTH = 2.0;
static const double EXPORT_RELROLL_THRESHOLD = 85.0;

static FILE* logFile = NULL;

static void messageHandler(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    if(logFile) {
        fprintf(logFile, "%s\n", msg.toLocal8Bit().constData());
    }
}

// What the windowed program's projectWidget does for the file reader, without
// the windows: it owns the tracks, makes new ones the way trackHandler's
// constructor does (ui/trackhandler.cpp), and keeps the ground texture path.
class coreProject : public projectFileClient
{
public:
    coreProject() : texPath(":/background.png") {}
    ~coreProject() { clear(); }

    QList<track*> tracks;
    QString texPath;

    void clear()
    {
        while(!tracks.isEmpty()) {
            delete tracks.takeFirst();
        }
    }

    void projectCleared() { clear(); }

    // The windowed program loads the image into the view and falls back to the
    // built-in one when that fails. The built-in images are Qt resources, which
    // this program does not link: those are accepted by name.
    bool projectGroundTexture(const QString& path)
    {
        bool loaded = true;
        texPath = path;
        if(!canLoad(texPath)) {
            texPath = QString(":/background.png");
            loaded = false;
        }
        return loaded;
    }

    track* projectNewTrack()
    {
        track* t = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
        t->name = QString("unnamed Track");
        t->display.colors[0] = QColor(20, 20, 130);
        t->display.colors[1] = QColor(255, 51, 51);
        t->display.colors[2] = QColor(51, 255, 51);
        tracks.append(t);
        return t;
    }

    void projectTrackSmoothingUpdated(track*) {}
    void projectTrackLoaded(track*) {}
    void projectLoadFinished() {}
    void projectTrackSaving(track*) {}

private:
    // the image resources of resources.qrc
    static bool canLoad(const QString& path)
    {
        if(path.startsWith(":/")) {
            static const char* resources[] = { ":/background.png", ":/metal.png", ":/sky/negx.jpg", ":/sky/negy.jpg",
                                               ":/sky/negz.jpg", ":/sky/posx.jpg", ":/sky/posy.jpg", ":/sky/posz.jpg" };
            for(unsigned i = 0; i < sizeof(resources)/sizeof(resources[0]); ++i) {
                if(path == resources[i]) return true;
            }
            return false;
        }
        return !QImage(path).isNull();
    }
};

class exporter
{
public:
    QStringList lReport;
    QString sReportFile;

    int runExport(const QString& projectFile, const QString& outDir);

private:
    bool writeProjectFiles(coreProject& project, const QDir& dir, const QString& stem, QString& error);
    bool exportNL2(track* tTrack, const QString& fileName);
    bool exportNL1(track* tTrack, const QString& fileName, bool tangent);
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

    if(!QFileInfo(projectFile).isFile()) {
        return finish(false, QString("project file not found: ") + projectFile);
    }

    coreProject project;
    QString loadResult = loadProjectFile(projectFile, &project);
    if(loadResult.contains("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(loadResult.contains("Warning")) {
        lReport.append(QString("warning: ") + loadResult);
    }

    QString error;
    if(!writeProjectFiles(project, dir, stem, error)) {
        return finish(false, error);
    }
    return finish(true, QString());
}

// The three exports of every track with sections, then the re-saved project
// as <stem>.resaved.fvd; each file written is listed in the report.
bool exporter::writeProjectFiles(coreProject& project, const QDir& dir, const QString& stem, QString& error)
{
    QList<track*>& trackList = project.tracks;
    for(int i = 0; i < trackList.size(); ++i) {
        track* tTrack = trackList[i];
        if(tTrack->lSections.size() == 0) {
            lReport.append(QString("skipped track %1 (%2): no sections").arg(i+1).arg(tTrack->name));
            continue;
        }
        QString base = stem;
        if(i > 0) {
            base += QString(".track%1").arg(i+1);
        }

        QString nl2Name = base + ".nl2elem";
        QString tangentName = base + ".tangent.nlelem";
        QString splineName = base + ".spline.nlelem";

        if(!exportNL2(tTrack, dir.filePath(nl2Name))) {
            error = QString("cannot write ") + nl2Name;
            return false;
        }
        lReport.append(QString("wrote ") + nl2Name);
        if(!exportNL1(tTrack, dir.filePath(tangentName), true)) {
            error = QString("cannot write ") + tangentName;
            return false;
        }
        lReport.append(QString("wrote ") + tangentName);
        if(!exportNL1(tTrack, dir.filePath(splineName), false)) {
            error = QString("cannot write ") + splineName;
            return false;
        }
        lReport.append(QString("wrote ") + splineName);
    }

    QString resavedName = stem + ".resaved.fvd";
    QString saveResult = saveProjectFile(dir.filePath(resavedName), project.texPath, trackList, &project);
    if(saveResult.contains("Error")) {
        error = QString("saving ") + resavedName + ": " + saveResult;
        return false;
    }
    lReport.append(QString("wrote ") + resavedName);
    return true;
}

bool exporter::exportNL2(track* tTrack, const QString& fileName)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    return writeNL2Element(fileName, tTrack, fPerNode, fromIndex, fromIndex+toIndex);
}

bool exporter::exportNL1(track* tTrack, const QString& fileName, bool tangent)
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

static QByteArray readAll(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

static bool checkSame(bool& allOk, const QString& label, const QString& made, const QString& reference)
{
    QByteArray a = readAll(made);
    QByteArray b = readAll(reference);
    bool ok = !b.isEmpty() && a == b;
    printf("%s: %s (%d bytes made, %d bytes in the reference)\n", ok ? "ok" : "FAIL", qPrintable(label), a.size(), b.size());
    if(!ok) allOk = false;
    return ok;
}

// Exports samples/baseline/four-sections.fvd and compares the Spline export
// and the re-saved project with the 2016 binary's files, byte for byte.
static int selfTest(const QString& repo)
{
    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }
    QString baseline = QDir(repo).filePath("samples/baseline");
    exporter e;
    int code = e.runExport(QDir(baseline).filePath("four-sections.fvd"), tmp.path());
    bool allOk = true;
    printf("%s: export of four-sections.fvd (exit code %d)\n", code == 0 ? "ok" : "FAIL", code);
    if(code != 0) allOk = false;
    checkSame(allOk, "spline export", QDir(tmp.path()).filePath("four-sections.spline.nlelem"),
              QDir(baseline).filePath("four-sections.2016.spline.nlelem"));
    checkSame(allOk, "re-saved project", QDir(tmp.path()).filePath("four-sections.resaved.fvd"),
              QDir(baseline).filePath("four-sections.2016.resaved.fvd"));
    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
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
    if(args.size() == 3 && args.at(0) == "export") {
        exporter e;
        result = e.runExport(args.at(1), args.at(2));
    } else if(args.size() == 1 && args.at(0) != "export") {
        result = selfTest(args.at(0));
    } else {
        fprintf(stderr, "usage: fvdcore export <project.fvd> <outdir> [--log <file>]\n"
                        "       fvdcore <repository folder>\n");
        result = 1;
    }
    if(logFile) fclose(logFile);
    return result;
}
