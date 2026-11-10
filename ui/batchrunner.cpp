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

#include "batchrunner.h"
#include "mainwindow.h"
#include "projectwidget.h"
#include "trackhandler.h"
#include "track.h"
#include "saver.h"
#include "exportfuncs.h"
#include "glviewwidget.h"
#include "lenassert.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtDebug>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace std;

extern MainWindow* gloParent;
extern glViewWidget* glView;

// The export dialog's defaults (ui/exportui.ui, ui/exportui.cpp). The spin boxes
// hold doubles; the dialog stores the segment length in a float and computes
// the threshold in double arithmetic before storing it in a float. Same here.
static const double EXPORT_SEGMENT_LENGTH = 2.0;
static const double EXPORT_RELROLL_THRESHOLD = 85.0;

batchRunner::batchRunner(int argc, char *argv[])
{
    for(int i = 1; i < argc; ++i) {
        lArgs.append(QString::fromLocal8Bit(argv[i]));
    }
}

bool batchRunner::isBatchCommand(int argc, char *argv[])
{
    return argc > 1 && QString::fromLocal8Bit(argv[1]).startsWith("--batch-");
}

int batchRunner::run()
{
    // One entry per command. A new command gets its own runXxx() function.
    if(lArgs.at(0) == "--batch-export") {
        return runExport();
    }
    qWarning("batch mode: unknown command %s", qPrintable(lArgs.at(0)));
    return 1;
}

#ifdef Q_OS_WIN
static void unlockForeground()
{
    LockSetForegroundWindow(LSFW_UNLOCK);
}
#endif

// Builds the main window and initialises OpenGL without showing anything.
// Loading a project builds the track meshes, which needs OpenGL; the GL view
// also creates the default project when it is initialised.
MainWindow* batchRunner::createHiddenWindow()
{
#ifdef Q_OS_WIN
    // Creating the GL view briefly makes its hidden window the foreground
    // window (measured: about 50 ms), which would take keystrokes away from
    // whatever the user is typing into. This stops it. The unlock is also
    // registered for exit, because initializeGL() ends the program with
    // exit(4) when GLEW cannot start, before the unlock below is reached.
    atexit(unlockForeground);
    LockSetForegroundWindow(LSFW_LOCK);
#endif
    MainWindow* w = new MainWindow(); // never deleted: ~MainWindow() calls exit(0)
    glView->initializeWithoutShowing();
#ifdef Q_OS_WIN
    LockSetForegroundWindow(LSFW_UNLOCK);
#endif
    if(!glView->isValid()) {
        return NULL;
    }
    return w;
}

int batchRunner::runExport()
{
    if(lArgs.size() != 3) {
        qWarning("usage: FVD.exe --batch-export <project.fvd> <outdir>");
        return 1;
    }
    QString projectFile = lArgs.at(1);
    QString outDir = lArgs.at(2);

    QString stem = QFileInfo(projectFile).fileName();
    if(stem.endsWith(".fvd", Qt::CaseInsensitive)) {
        stem.chop(4);
    }

    if(!QDir().mkpath(outDir)) {
        qWarning("batch mode: cannot create %s", qPrintable(outDir));
        return 1;
    }
    QDir dir(outDir);
    sReportFile = dir.filePath(stem + ".batch.txt");

    if(!QFileInfo(projectFile).isFile()) {
        return finish(false, QString("project file not found: ") + projectFile);
    }

    MainWindow* w = createHiddenWindow();
    if(!w) {
        return finish(false, QString("could not set up OpenGL"));
    }

    // saver() directly, not MainWindow::loadProject(), which shows a message box
    // on any warning and remembers the file name (the .bak autosave uses it).
    QString loadResult = saver(projectFile, w->project, w).doLoad();
    qDebug("batch mode: loaded %s: %s", qPrintable(projectFile), qPrintable(loadResult));
    if(loadResult.contains("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(loadResult.contains("Warning")) {
        lReport.append(QString("warning: ") + loadResult);
    }

    QList<trackHandler*>& trackList = w->project->trackList;
    for(int i = 0; i < trackList.size(); ++i) {
        track* tTrack = trackList[i]->trackData;
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
            return finish(false, QString("cannot write ") + nl2Name);
        }
        lReport.append(QString("wrote ") + nl2Name);
        if(!exportNL1(tTrack, dir.filePath(tangentName), true)) {
            return finish(false, QString("cannot write ") + tangentName);
        }
        lReport.append(QString("wrote ") + tangentName);
        if(!exportNL1(tTrack, dir.filePath(splineName), false)) {
            return finish(false, QString("cannot write ") + splineName);
        }
        lReport.append(QString("wrote ") + splineName);
    }

    // After the exports, which leave the track as it was (the heartline is
    // never switched off here, unlike the dialog's "without heartlining").
    QString resavedName = stem + ".resaved.fvd";
    QString saveResult = saver(dir.filePath(resavedName), w->project, w).doSave();
    if(saveResult.contains("Error")) {
        return finish(false, QString("saving ") + resavedName + ": " + saveResult);
    }
    lReport.append(QString("wrote ") + resavedName);

    return finish(true, QString());
}

// Same framing as exportUi::doNL2Export() (ui/exportui.cpp), for the whole track.
bool batchRunner::exportNL2(track* tTrack, const QString& fileName)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    QByteArray localFileName = fileName.toLocal8Bit();
    FILE* fout = fopen(localFileName.constData(), "w");
    if(fout == NULL) {
        return false;
    }

    fprintf(fout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fout, "<root>\n");
    fprintf(fout, "\t<element>\n");
    fprintf(fout, "\t\t<description>FVD++ Export Data</description>\n");

    tTrack->exportNL2Track(fout, fPerNode, fromIndex, fromIndex+toIndex);

    fprintf(fout, "\t</element>\n");
    fprintf(fout, "</root>\n");
    fclose(fout);
    return true;
}

// Same framing as exportUi::doExport() (tangent) and exportUi::doExport2()
// (spline) in ui/exportui.cpp, for the whole track.
bool batchRunner::exportNL1(track* tTrack, const QString& fileName, bool tangent)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    float fRollThresh = sin(EXPORT_RELROLL_THRESHOLD*F_PI/180.f);
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    QByteArray localFileName = fileName.toLocal8Bit();
    fstream* fout = new fstream(localFileName.constData(), ios::out | ios::binary);
    if(!fout->is_open()) {
        delete fout;
        return false;
    }

    writeBytes(fout, (const char*)"MELE", 4);
    writeNulls(fout, 4); // will be replaced with length of data
    writeNulls(fout, 64);
    writeNulls(fout, 4); // will be replaced with No of NL beziers

    int iNodes;
    if(tangent) {
        iNodes = tTrack->exportTrack4(fout, fPerNode, fromIndex, toIndex+fromIndex, fRollThresh);
    } else {
        iNodes = tTrack->exportTrack3(fout, fPerNode, fromIndex, toIndex+fromIndex, fRollThresh);
    }

    int iDataLength = iNodes*50+132;

    writeNulls(fout, 69);

    fout->seekp(4);
    writeBytes(fout, (const char*)&iDataLength, 4); // replaced with length of data
    fout->seekp(72);
    writeBytes(fout, (const char*)&iNodes, 4); // replaced with no of NL beziers

    bool ok = !fout->fail();
    fout->close();
    delete fout;
    return ok;
}

// Writes <outdir>/<stem>.batch.txt: what was written, any warning, then OK or ERROR.
int batchRunner::finish(bool ok, const QString& error)
{
    lReport.append(ok ? QString("OK") : QString("ERROR: ") + error);
    qDebug("batch mode: %s", qPrintable(lReport.last()));

    QFile report(sReportFile);
    if(report.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&report);
        for(int i = 0; i < lReport.size(); ++i) {
            out << lReport.at(i) << "\n";
        }
        out.flush();
        report.close();
    } else {
        qWarning("batch mode: cannot write %s", qPrintable(sReportFile));
        return 1;
    }
    return ok ? 0 : 1;
}
