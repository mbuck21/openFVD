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
#include "batchscenarios.h"
#include "mainwindow.h"
#include "projectwidget.h"
#include "trackhandler.h"
#include "trackwidget.h"
#include "graphwidget.h"
#include "qcustomplot.h"
#include "track.h"
#include "elementfile.h"
#include "exportfuncs.h"
#include "glviewwidget.h"
#include "ridesheet.h"
#include "optionsmenu.h"
#include "lenassert.h"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTreeWidget>
#include <QWidget>
#include <QtDebug>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
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
    // No merging of typed values in batch mode: every scripted edit is a step
    // of its own (decision record 0011, point 7). --batch-merge-check sets it.
    trackHandler::mergeWindowMs = 0;

    // One entry per command. A new command gets its own runXxx() function.
    if(lArgs.at(0) == "--batch-export") {
        return runExport();
    }
    if(lArgs.at(0) == "--batch-make-fixtures") {
        return runMakeFixtures();
    }
    if(lArgs.at(0) == "--batch-undo-check") {
        return runUndoCheck();
    }
    if(lArgs.at(0) == "--batch-merge-check") {
        return runMergeCheck();
    }
    if(lArgs.at(0) == "--batch-import") {
        return runImport();
    }
    if(lArgs.at(0) == "--batch-ride-sheet") {
        return runRideSheet();
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

// Keeps every window off the screen. Qt sends a window its show event just
// before it puts it on the screen; a window marked "don't show on screen" at
// that point is shown to Qt only (it counts as visible, nothing appears, it
// takes no focus). The scenarios open dialogs (Track Properties, smoothing)
// by clicking their buttons, as a person would; this keeps them invisible.
class noShowFilter : public QObject
{
public:
    explicit noShowFilter(QObject* parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event)
    {
        if(event->type() == QEvent::Show && watched->isWidgetType()) {
            QWidget* widget = static_cast<QWidget*>(watched);
            if(widget->isWindow()) {
                widget->setAttribute(Qt::WA_DontShowOnScreen);
                widget->setAttribute(Qt::WA_ShowWithoutActivating);
            }
        }
        return false;
    }
};

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
    qApp->installEventFilter(new noShowFilter(qApp));
    MainWindow* w = new MainWindow(); // never deleted: ~MainWindow() calls exit(0)
    // Batch output is always metric (m/s, m), whatever options.cfg says: the
    // scripted scenarios convert typed values with the options' factors, and a
    // trip to feet and back moves the last digit of a float, which would break
    // the exact comparison with the 2016 binary. Only the value in memory is
    // set; nothing in batch mode saves the options, so options.cfg is untouched.
    // The hidden Options dialog's measureBox still shows the file's value: do
    // not read that combo in batch mode.
    w->mOptions->measures = metricMPS;
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

    // projectWidget::loadProject(), not MainWindow::loadProject(), which shows a message box
    // on any warning and remembers the file name (the .bak autosave uses it).
    QString loadResult = w->project->loadProject(projectFile);
    qDebug("batch mode: loaded %s: %s", qPrintable(projectFile), qPrintable(loadResult));
    if(loadResult.startsWith("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(loadResult.startsWith("Warning")) {
        lReport.append(QString("warning: ") + loadResult);
    }

    // What the windows show after the load, for comparing two builds; not
    // listed in the report, so tools/run-baseline.py does not compare it.
    if(!writeLines(dir.filePath(stem + ".loadstate.txt"), panelState(w))) {
        return finish(false, QString("cannot write ") + stem + ".loadstate.txt");
    }

    QString error;
    if(!writeProjectFiles(w, dir, stem, error)) {
        return finish(false, error);
    }
    return finish(true, QString());
}

// Loads a project, opens track 1 and writes what the ride sheet panel shows
// as text to <outdir>/<stem>.ridesheet.txt (compared with `fvdcore summary`).
int batchRunner::runRideSheet()
{
    bool unitCheck = lArgs.size() == 4 && lArgs.at(3) == "--english-boxes";
    if(lArgs.size() != 3 && !unitCheck) {
        qWarning("usage: FVD.exe --batch-ride-sheet <project.fvd> <outdir> [--english-boxes]");
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
    QString loadResult = w->project->loadProject(projectFile);
    if(loadResult.startsWith("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(w->project->trackList.size() < 1) {
        return finish(false, QString("the project has no track"));
    }
    w->openTab(w->project->trackList[0]);
    w->mRideSheet->setTrack(w->curTrack());
    w->mRideSheet->refreshNow();

    // --english-boxes: with the units in feet and mph, select every straight and
    // curved section of track 1 and write what the length, radius and speed boxes
    // of the track panel show (value, minimum, maximum), for the check of docs/testing.md.
    QStringList boxLines;
    if(unitCheck) {
        w->mOptions->measures = english;
        batchDriver driver(w);
        QList<section*>& secs = w->project->trackList[0]->trackData->lSections;
        for(int i = 0; i < secs.size() && driver.ok(); ++i) {
            const char* names[2] = {NULL, NULL};
            if(secs[i]->type == straight) { names[0] = "straightLengthBox"; names[1] = "straightSpeedBox"; }
            if(secs[i]->type == curved) { names[0] = "curvedRadiusBox"; names[1] = "curvedSpeedBox"; }
            if(!names[0]) continue;
            driver.selectSection(i);
            // what the box holds in the model, in m or m/s (the last column)
            struct modelOf {
                static QString text(section* s, int b) {
                    if(b == 1) return QString("%1 m/s").arg(s->getSpeed(), 0, 'f', 3);
                    return QString("%1 m").arg(s->type == straight ? s->fHLength : s->fRadius, 0, 'f', 3);
                }
            };
            for(int b = 0; b < 2 && driver.ok(); ++b) {
                QDoubleSpinBox* box = driver.panelSpin(names[b]);
                if(!box) { return finish(false, QString("no box ") + names[b]); }
                boxLines.append(QString("section %1 | %2 | value %3 | min %4 | max %5 | model %6")
                                .arg(i+1).arg(names[b]).arg(box->value(), 0, 'f', 3).arg(box->minimum(), 0, 'f', 3)
                                .arg(box->maximum(), 0, 'f', 3).arg(modelOf::text(secs[i], b)));
            }
        }
        if(!driver.ok()) return finish(false, driver.error());
        if(!writeLines(dir.filePath(stem + ".boxes.txt"), boxLines)) {
            return finish(false, QString("cannot write ") + stem + ".boxes.txt");
        }
    }

    if(!writeLines(dir.filePath(stem + ".ridesheet.txt"), w->mRideSheet->textLines())) {
        return finish(false, QString("cannot write ") + stem + ".ridesheet.txt");
    }
    return finish(true, QString());
}

// The three exports of every track with sections, then the re-saved project
// as <stem>.resaved.fvd; each file written is listed in the report.
bool batchRunner::writeProjectFiles(MainWindow* w, const QDir& dir, const QString& stem, QString& error)
{
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

    // After the exports, which leave the track as it was (the heartline is
    // never switched off here, unlike the dialog's "without heartlining").
    QString resavedName = stem + ".resaved.fvd";
    QString saveResult = w->project->saveProject(dir.filePath(resavedName));
    if(saveResult.contains("Error")) {
        error = QString("saving ") + resavedName + ": " + saveResult;
        return false;
    }
    lReport.append(QString("wrote ") + resavedName);
    return true;
}

// FVD.exe --batch-import <nltrack|txt|csv> <input file> <outdir>
// Starts from the default project and runs what the menu entry "Import
// NoLimits 1 Track", "Import from Pointlist" or "Import NoLimits 2 CSV" runs
// once its file dialog has given a file. Then writes the files as
// --batch-export does (<stem> is the input's name without its extension) and
// adds to the report what the windows show (panelState()).
int batchRunner::runImport()
{
    if(lArgs.size() != 4 || (lArgs.at(1) != "nltrack" && lArgs.at(1) != "txt" && lArgs.at(1) != "csv")) {
        qWarning("usage: FVD.exe --batch-import <nltrack|txt|csv> <input file> <outdir>");
        return 1;
    }
    QString kind = lArgs.at(1);
    QString inputFile = lArgs.at(2);
    QString outDir = lArgs.at(3);
    QString stem = QFileInfo(inputFile).completeBaseName();

    if(!QDir().mkpath(outDir)) {
        qWarning("batch mode: cannot create %s", qPrintable(outDir));
        return 1;
    }
    QDir dir(outDir);
    sReportFile = dir.filePath(stem + ".batch.txt");

    // The importers show no message: a file they cannot open leaves a new
    // empty track, which is reported below. A file they can open but do not
    // understand is read as it is, as from the menu (a point list with fewer
    // than two points, or a CSV line with fewer than ten columns, can end the
    // program).
    if(!QFileInfo(inputFile).isFile()) {
        return finish(false, QString("input file not found: ") + inputFile);
    }

    MainWindow* w = createHiddenWindow();
    if(!w) {
        return finish(false, QString("could not set up OpenGL"));
    }

    int tracksBefore = w->project->trackList.size();
    if(kind == "nltrack") {
        w->project->importNLTrackFile(inputFile);
    } else if(kind == "txt") {
        w->project->importPointListFile(inputFile);
    } else {
        w->project->importNoLimitsCSVFile(inputFile);
    }
    if(w->project->trackList.size() != tracksBefore+1) {
        return finish(false, QString("the import did not add one track"));
    }
    if(w->project->trackList.last()->trackData->lSections.isEmpty()) {
        return finish(false, QString("the import added no section: the file could not be read"));
    }

    QString error;
    if(!writeProjectFiles(w, dir, stem, error)) {
        return finish(false, error);
    }
    lReport.append(panelState(w));

    if(!windowsStayedHidden(error)) {
        return finish(false, error);
    }
    return finish(true, QString());
}

static QString onOff(bool on)
{
    return on ? QString("enabled") : QString("disabled");
}

static QString exact(double value)
{
    return QString::number(value, 'g', 17);
}

// What the windows show of every track: the Undo and Redo menu entries, and
// per track its sections and nodes, the active section, the rows of the
// section list with the selected one, the undo list, and the graph's
// horizontal range. Two builds that load or import the same file must give
// the same lines.
QStringList batchRunner::panelState(MainWindow* w)
{
    QStringList lines;
    QAction* undoMenu = w->findChild<QAction*>("actionUndo");
    QAction* redoMenu = w->findChild<QAction*>("actionRedo");
    // an entry that names a step shows its text ("Undo Change start speed")
    QString undoText = undoMenu && undoMenu->text() != "Undo" ? QString(" (\"%1\")").arg(undoMenu->text()) : QString();
    QString redoText = redoMenu && redoMenu->text() != "Redo" ? QString(" (\"%1\")").arg(redoMenu->text()) : QString();
    lines.append(QString("state: menu Undo %1%2, Redo %3%4")
                 .arg(undoMenu ? onOff(undoMenu->isEnabled()) : QString("missing")).arg(undoText)
                 .arg(redoMenu ? onOff(redoMenu->isEnabled()) : QString("missing")).arg(redoText));

    QList<trackHandler*>& trackList = w->project->trackList;
    for(int i = 0; i < trackList.size(); ++i) {
        trackHandler* h = trackList[i];
        track* t = h->trackData;
        QString label = QString("state: track %1").arg(i+1);
        lines.append(QString("%1 (%2): %3 sections, %4 nodes, active section %5")
                     .arg(label).arg(t->name).arg(t->lSections.size()).arg(t->getNumPoints())
                     .arg(t->activeSection ? QString::number(t->getSectionNumber(t->activeSection)) : QString("none")));

        trackWidget* panel = h->trackWidgetItem;
        QTreeWidget* list = panel ? panel->findChild<QTreeWidget*>("sectionListWidget") : NULL;
        if(list) {
            QStringList rows;
            for(int r = 0; r < list->topLevelItemCount(); ++r) {
                QTreeWidgetItem* item = list->topLevelItem(r);
                rows.append(QString("[%1|%2|%3|flags %4%5]").arg(item->text(0)).arg(item->text(1)).arg(item->text(2))
                            .arg((int)item->flags()).arg(item->isSelected() ? QString("|selected") : QString()));
            }
            int selected = -1;
            for(int k = 0; k < panel->sectionList.size(); ++k) {
                if(panel->sectionList[k] == panel->selSection) selected = k;
            }
            lines.append(QString("%1 section list: %2 rows, %3 entries, selected entry %4: %5")
                         .arg(label).arg(list->topLevelItemCount()).arg(panel->sectionList.size())
                         .arg(selected).arg(rows.join(" ")));
        } else {
            lines.append(QString("%1 section list: missing").arg(label));
        }

        QStringList types;
        // newest first; each step by the name of its first command
        const QList<historyStep>& actions = h->mHistory->steps();
        for(int a = actions.size()-1; a >= 0; --a) {
            types.append(batchScenarios::stepName(actions[a]));
        }
        // the position as the window's list counted it: the steps that can be redone, or -1 with none
        int position = actions.isEmpty() ? -1 : actions.size() - h->mHistory->position();
        lines.append(QString("%1 undo list: %2 actions, position %3, newest first: %4")
                     .arg(label).arg(actions.size()).arg(position)
                     .arg(types.isEmpty() ? QString("none") : types.join(" ")));

        QCustomPlot* plot = h->graphWidgetItem ? h->graphWidgetItem->findChild<QCustomPlot*>("plotter") : NULL;
        QDoubleSpinBox* lengthSpin = h->graphWidgetItem ? h->graphWidgetItem->findChild<QDoubleSpinBox*>("lengthSpin") : NULL;
        lines.append(QString("%1 graph: x from %2 to %3, transition length step %4")
                     .arg(label)
                     .arg(plot ? exact(plot->xAxis->range().lower) : QString("missing"))
                     .arg(plot ? exact(plot->xAxis->range().upper) : QString("missing"))
                     .arg(lengthSpin ? exact(lengthSpin->singleStep()) : QString("missing")));
    }
    return lines;
}

bool batchRunner::writeLines(const QString& fileName, const QStringList& lines)
{
    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    for(int i = 0; i < lines.size(); ++i) {
        out << lines.at(i) << "\n";
    }
    out.flush();
    file.close();
    return true;
}

// What exportUi::doNL2Export() (ui/exportui.cpp) writes, for the whole track,
// through the same core function.
bool batchRunner::exportNL2(track* tTrack, const QString& fileName)
{
    float fPerNode = EXPORT_SEGMENT_LENGTH;
    int fromIndex = 0;
    int toIndex = tTrack->lSections.size()-1;

    return writeNL2Element(fileName, tTrack, fPerNode, fromIndex, fromIndex+toIndex);
}

// What exportUi::doExport() (tangent) and exportUi::doExport2() (spline) in
// ui/exportui.cpp write, for the whole track, through the same core functions;
// the heartline is never switched off here.
bool batchRunner::exportNL1(track* tTrack, const QString& fileName, bool tangent)
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

// Fails when a window reached the screen (the filter above should prevent it).
bool batchRunner::windowsStayedHidden(QString& error)
{
    QWidgetList windows = QApplication::topLevelWidgets();
    for(int i = 0; i < windows.size(); ++i) {
        if(windows[i]->isVisible() && !windows[i]->testAttribute(Qt::WA_DontShowOnScreen)) {
            error = QString("a window was shown: ") + windows[i]->metaObject()->className();
            return false;
        }
    }
    return true;
}

// FVD.exe --batch-make-fixtures <outdir>
// Builds every scenario of batchscenarios.cpp in a new project, by driving the
// windows, and saves it as <outdir>/gen-<name>.fvd. The report lists, per
// track, what the model computed.
int batchRunner::runMakeFixtures()
{
    if(lArgs.size() != 2) {
        qWarning("usage: FVD.exe --batch-make-fixtures <outdir>");
        return 1;
    }
    QString outDir = lArgs.at(1);
    if(!QDir().mkpath(outDir)) {
        qWarning("batch mode: cannot create %s", qPrintable(outDir));
        return 1;
    }
    QDir dir(outDir);
    sReportFile = dir.filePath("make-fixtures.batch.txt");

    MainWindow* w = createHiddenWindow();
    if(!w) {
        return finish(false, QString("could not set up OpenGL"));
    }
    // createHiddenWindow() made the options metric; the scenarios type metres.
    lenAssert(w->mOptions->getLengthFactor() == 1.0 && w->mOptions->getSpeedFactor() == 1.0);

    // A scenario that fails is reported and not saved; the others still run.
    QString firstError;
    batchDriver driver(w);
    QStringList names = batchScenarios::names();
    for(int n = 0; n < names.size(); ++n) {
        batchScenarios::build(names[n], driver);
        if(!driver.ok()) {
            lReport.append(QString("FAILED %1: %2").arg(names[n]).arg(driver.error()));
            if(firstError.isEmpty()) firstError = names[n] + ": " + driver.error();
            driver.clearError();
            continue;
        }
        QString fileName = QString("gen-") + names[n] + ".fvd";
        QList<trackHandler*>& trackList = w->project->trackList;
        QString trackError;
        for(int t = 0; t < trackList.size(); ++t) {
            QString error;
            QString stats = batchScenarios::trackStats(trackList[t]->trackData, error);
            lReport.append(QString("%1 track %2: %3").arg(fileName).arg(t+1).arg(stats));
            if(!error.isEmpty() && trackError.isEmpty()) {
                trackError = QString("%1 track %2: %3").arg(fileName).arg(t+1).arg(error);
            }
        }
        if(!trackError.isEmpty()) {
            lReport.append(QString("FAILED ") + trackError);
            if(firstError.isEmpty()) firstError = trackError;
            continue;
        }
        QString saveResult = w->project->saveProject(dir.filePath(fileName));
        if(saveResult.contains("Error")) {
            return finish(false, QString("saving ") + fileName + ": " + saveResult);
        }
        lReport.append(QString("wrote ") + fileName);
    }

    QString error;
    if(!windowsStayedHidden(error)) {
        return finish(false, error);
    }
    if(!firstError.isEmpty()) {
        return finish(false, firstError);
    }
    return finish(true, QString());
}

// Writes the exports of every track and the re-saved project as <base>.<kind>
// into dirName, and reads them back.
bool batchRunner::writeState(const QString& dirName, const QString& base, batchState& state, QString& error)
{
    QDir dir(dirName);
    MainWindow* w = gloParent;
    state.lNames.clear();
    state.lData.clear();
    QStringList files;
    QList<trackHandler*>& trackList = w->project->trackList;
    for(int i = 0; i < trackList.size(); ++i) {
        track* tTrack = trackList[i]->trackData;
        if(tTrack->lSections.size() == 0) continue;
        QString prefix = base;
        QString label;
        if(i > 0) {
            prefix += QString(".track%1").arg(i+1);
            label = QString("track%1.").arg(i+1);
        }
        if(!exportNL2(tTrack, dir.filePath(prefix + ".nl2elem"))
                || !exportNL1(tTrack, dir.filePath(prefix + ".tangent.nlelem"), true)
                || !exportNL1(tTrack, dir.filePath(prefix + ".spline.nlelem"), false)) {
            error = QString("cannot write the exports of ") + prefix;
            return false;
        }
        files << prefix + ".nl2elem" << prefix + ".tangent.nlelem" << prefix + ".spline.nlelem";
        state.lNames << label + "nl2elem" << label + "tangent.nlelem" << label + "spline.nlelem";
    }
    QString saveResult = w->project->saveProject(dir.filePath(base + ".fvd"));
    if(saveResult.contains("Error")) {
        error = QString("saving ") + base + ".fvd: " + saveResult;
        return false;
    }
    files << base + ".fvd";
    state.lNames << "fvd";

    for(int i = 0; i < files.size(); ++i) {
        QFile file(dir.filePath(files[i]));
        if(!file.open(QIODevice::ReadOnly)) {
            error = QString("cannot read ") + files[i];
            return false;
        }
        state.lData.append(file.readAll());
    }
    return true;
}

// the kinds of file that differ between two states
QStringList batchRunner::differences(const batchState& a, const batchState& b)
{
    QStringList diff;
    if(a.lNames != b.lNames) {
        diff.append(QString("the list of files (%1 against %2)").arg(a.lNames.join(" ")).arg(b.lNames.join(" ")));
        return diff;
    }
    for(int i = 0; i < a.lNames.size(); ++i) {
        if(a.lData[i] != b.lData[i]) diff.append(a.lNames[i]);
    }
    return diff;
}

// writes the files of a state whose kind is listed, as <base>.<kind>
void batchRunner::writeFiles(const QString& dirName, const QString& base, const batchState& state, const QStringList& kinds)
{
    QDir dir(dirName);
    for(int i = 0; i < state.lNames.size(); ++i) {
        if(!kinds.contains(state.lNames[i])) continue;
        QFile file(dir.filePath(base + "." + state.lNames[i]));
        if(file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(state.lData[i]);
            file.close();
        }
    }
}

static QString sameOrNot(const QStringList& diff)
{
    return diff.isEmpty() ? QString("identical") : QString("DIFFERENT (") + diff.join(", ") + ")";
}

// FVD.exe --batch-undo-check <project.fvd> <outdir> [--all-edits] [--edits n,n,...] [--each]
// Loads the project and writes its files ("A"); applies the scripted edits of
// batchscenarios.cpp to track 1 through the windows; writes "B"; undoes every
// step through Edit, Undo and writes "A2"; redoes every step and writes "B2".
// Passes when A2 equals A and B2 equals B byte for byte. With --each, every
// edit is in addition undone and redone on its own right after it is made,
// and each must restore the files exactly (decision record 0013: every edit
// ends in the state a save and reopen gives); a failure names the edit.
// --edits runs only the listed edits, plus the first and the last. Each edit
// must record a step made by the command it names; a step of another command,
// or none, fails the check.
int batchRunner::runUndoCheck()
{
    // --all-edits: accepted and ignored; no edit is left out any more (it
    //     included the edits once known to fail, and older command lines use it)
    // --edits 3,17,20: only these edits (numbered as in the report) and the first
    //     and last, which recompute the whole track; to find which edit fails
    // --each: also undo and redo every edit on its own right after making it;
    //     an edit whose undo or redo does not restore the files fails the check
    lArgs.removeAll("--all-edits");
    bool each = lArgs.contains("--each");
    if(each) lArgs.removeAll("--each");
    QList<int> onlyEdits;
    int editsAt = lArgs.indexOf("--edits");
    if(editsAt >= 0 && editsAt+1 < lArgs.size()) {
        QStringList numbers = lArgs.at(editsAt+1).split(",", QString::SkipEmptyParts);
        for(int k = 0; k < numbers.size(); ++k) {
            onlyEdits.append(numbers[k].toInt());
        }
        lArgs.removeAt(editsAt+1);
        lArgs.removeAt(editsAt);
    }
    if(lArgs.size() != 3) {
        qWarning("usage: FVD.exe --batch-undo-check <project.fvd> <outdir> [--all-edits] [--edits n,n,...] [--each]");
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
    sReportFile = dir.filePath(stem + ".undo.batch.txt");
    QString workDir = dir.filePath(stem + ".undo-work");
    if(!QDir().mkpath(workDir)) {
        return finish(false, QString("cannot create ") + workDir);
    }

    if(!QFileInfo(projectFile).isFile()) {
        return finish(false, QString("project file not found: ") + projectFile);
    }
    MainWindow* w = createHiddenWindow();
    if(!w) {
        return finish(false, QString("could not set up OpenGL"));
    }
    QString loadResult = w->project->loadProject(projectFile);
    if(loadResult.startsWith("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(loadResult.startsWith("Warning")) {
        lReport.append(QString("warning: ") + loadResult);
    }
    if(w->project->trackList.isEmpty() || w->project->trackList[0]->trackData->lSections.isEmpty()) {
        return finish(false, QString("track 1 has no sections"));
    }

    batchDriver d(w);
    d.openTrack(0);
    if(!d.ok()) {
        return finish(false, d.error());
    }
    trackHandler* h = d.openHandler();

    QString error;
    batchState stateA, current;
    if(!writeState(outDir, stem + ".undo-A", stateA, error)) return finish(false, error);
    current = stateA;

    bool allOk = true;
    int totalSteps = 0;
    QStringList commandsSeen;
    for(int i = 0; i < batchScenarios::editCount(); ++i) {
        QString desc, expected, note;
        bool bookend = i == 0 || i == batchScenarios::editCount()-1;
        if(!onlyEdits.isEmpty() && !onlyEdits.contains(i+1) && !bookend) {
            continue;
        }
        int before = batchScenarios::undoDepth(h);
        bool applied = batchScenarios::applyEdit(i, d, desc, expected, note);
        if(!d.ok()) {
            return finish(false, QString("edit %1 (%2): %3").arg(i+1).arg(desc).arg(d.error()));
        }
        if(!applied) {
            lReport.append(QString("edit %1: %2: skipped, %3").arg(i+1).arg(desc).arg(note));
            continue;
        }
        int steps = batchScenarios::undoDepth(h) - before;
        QStringList commandNames;
        QString recorded = batchScenarios::recordedCommands(h, steps, commandNames);
        if(steps <= 0) {
            lReport.append(QString("edit %1: %2: FAILED, no undo step was recorded (expected %3)").arg(i+1).arg(desc).arg(expected));
            allOk = false;
            continue;
        }
        commandsSeen += commandNames;
        if(!commandNames.contains(expected)) {
            // the edit went through another command (or none): the control is
            // wired to the wrong one
            lReport.append(QString("edit %1: %2: FAILED, recorded %3, not %4").arg(i+1).arg(desc).arg(recorded).arg(expected));
            allOk = false;
        }
        totalSteps += steps;
        if(!each) {
            lReport.append(QString("edit %1: %2: recorded %3 (expected %4)").arg(i+1).arg(desc).arg(recorded).arg(expected));
            continue;
        }

        batchState after, undone, redone;
        if(!writeState(workDir, "state", after, error)) return finish(false, error);
        QStringList changed = differences(current, after);

        bool stepOk = true;
        for(int k = 0; k < steps; ++k) {
            if(!d.undo()) {
                stepOk = false;
            }
        }
        if(!d.ok()) return finish(false, QString("edit %1 (%2), undo: %3").arg(i+1).arg(desc).arg(d.error()));
        if(!writeState(workDir, "state", undone, error)) return finish(false, error);
        for(int k = 0; k < steps; ++k) {
            if(!d.redo()) {
                stepOk = false;
            }
        }
        if(!d.ok()) return finish(false, QString("edit %1 (%2), redo: %3").arg(i+1).arg(desc).arg(d.error()));
        if(!writeState(workDir, "state", redone, error)) return finish(false, error);

        QStringList undoDiff = differences(current, undone);
        QStringList redoDiff = differences(after, redone);
        if(!stepOk) {
            lReport.append(QString("edit %1: %2: FAILED, Undo or Redo was not available").arg(i+1).arg(desc));
            allOk = false;
        }
        bool editOk = undoDiff.isEmpty() && redoDiff.isEmpty();
        if(!editOk) {
            // every edit ends in the reopened state, so undo and redo of it
            // alone restore the files exactly (decision record 0013)
            allOk = false;
            // the evidence: the files that differ, before and after the round trip
            QString prefix = QString("%1.edit%2.").arg(stem).arg(i+1);
            if(!undoDiff.isEmpty()) {
                writeFiles(outDir, prefix + "before", current, undoDiff);
                writeFiles(outDir, prefix + "undone", undone, undoDiff);
            }
            if(!redoDiff.isEmpty()) {
                writeFiles(outDir, prefix + "after", after, redoDiff);
                writeFiles(outDir, prefix + "redone", redone, redoDiff);
            }
        }
        lReport.append(QString("edit %1: %2: recorded %3 (expected %4); changed %5; undo %6; redo %7%8")
                       .arg(i+1).arg(desc).arg(recorded).arg(expected)
                       .arg(changed.isEmpty() ? QString("nothing in the files") : changed.join(", "))
                       .arg(undoDiff.isEmpty() ? QString("restores") : QString("DOES NOT RESTORE ") + undoDiff.join(", "))
                       .arg(redoDiff.isEmpty() ? QString("restores") : QString("DOES NOT RESTORE ") + redoDiff.join(", "))
                       .arg(editOk ? QString() : QString(" (on its own)")));
        current = redone;
    }

    batchState stateB, stateA2, stateB2;
    if(!writeState(outDir, stem + ".undo-B", stateB, error)) return finish(false, error);
    QStringList aToB = differences(stateA, stateB);
    lReport.append(QString("B against A: %1").arg(aToB.isEmpty() ? QString("IDENTICAL, the edits changed nothing") : QString("different (") + aToB.join(", ") + ")"));
    if(aToB.isEmpty()) allOk = false;

    for(int k = 0; k < totalSteps; ++k) {
        if(!d.undo()) {
            allOk = false;
            lReport.append(QString("Undo was not available after %1 of %2 steps").arg(k).arg(totalSteps));
            break;
        }
    }
    if(!d.ok()) return finish(false, d.error());
    if(!writeState(outDir, stem + ".undo-A2", stateA2, error)) return finish(false, error);
    QStringList a2 = differences(stateA, stateA2);
    lReport.append(QString("A2 against A, after undoing %1 steps: %2").arg(totalSteps).arg(sameOrNot(a2)));

    for(int k = 0; k < totalSteps; ++k) {
        if(!d.redo()) {
            allOk = false;
            lReport.append(QString("Redo was not available after %1 of %2 steps").arg(k).arg(totalSteps));
            break;
        }
    }
    if(!d.ok()) return finish(false, d.error());
    if(!writeState(outDir, stem + ".undo-B2", stateB2, error)) return finish(false, error);
    QStringList b2 = differences(stateB, stateB2);
    lReport.append(QString("B2 against B, after redoing %1 steps: %2").arg(totalSteps).arg(sameOrNot(b2)));
    allOk = allOk && a2.isEmpty() && b2.isEmpty();

    // coverage of the named commands (core/command.h): those that made a step
    QStringList cmdExercised, cmdNot;
    const QList<commandSpec>& specs = commandRegistry::all();
    for(int c = 0; c < specs.size(); ++c) {
        (commandsSeen.contains(specs[c].name) ? cmdExercised : cmdNot).append(specs[c].name);
    }
    lReport.append(QString("commands exercised (%1): %2").arg(cmdExercised.size()).arg(cmdExercised.join(", ")));
    lReport.append(QString("commands not exercised (%1): %2").arg(cmdNot.size()).arg(cmdNot.join(", ")));

    QDir(workDir).removeRecursively();
    if(!windowsStayedHidden(error)) {
        return finish(false, error);
    }
    if(!allOk) {
        return finish(false, "the undo and redo round trip failed (see the lines above)");
    }
    return finish(true, QString());
}

// FVD.exe --batch-merge-check <project.fvd> <outdir>
// Merging as the windows have it (trackHandler::mergeWindowMs, one second;
// decision record 0011, point 7), on track 1, through the boxes a person
// types into. A value typed digit by digit ("12.5": the box goes 1, 12, 12.5)
// into a transition's length, the start speed and a section's own box must
// each be one undo step, whose undo gives the files of before the first digit
// exactly and whose redo the files after the last. Two values of the same
// section, the same box on two sections, and the same box before and after
// another selection must stay separate steps. At the end every step is
// undone and the files must be those the load gave. Focus loss, which also
// ends a run, is not checked here: the batch mode never takes the focus.
int batchRunner::runMergeCheck()
{
    if(lArgs.size() != 3) {
        qWarning("usage: FVD.exe --batch-merge-check <project.fvd> <outdir>");
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
    sReportFile = dir.filePath(stem + ".merge.batch.txt");
    QString workDir = dir.filePath(stem + ".merge-work");
    if(!QDir().mkpath(workDir)) {
        return finish(false, QString("cannot create ") + workDir);
    }
    if(!QFileInfo(projectFile).isFile()) {
        return finish(false, QString("project file not found: ") + projectFile);
    }

    // the windows' setting, before any track handler is made
    trackHandler::mergeWindowMs = 1000;
    MainWindow* w = createHiddenWindow();
    if(!w) {
        return finish(false, QString("could not set up OpenGL"));
    }
    QString loadResult = w->project->loadProject(projectFile);
    if(loadResult.startsWith("Error")) {
        return finish(false, QString("loading ") + projectFile + ": " + loadResult);
    }
    if(w->project->trackList.isEmpty() || w->project->trackList[0]->trackData->lSections.isEmpty()) {
        return finish(false, QString("track 1 has no sections"));
    }
    batchDriver d(w);
    d.openTrack(0);
    if(!d.ok()) return finish(false, d.error());
    trackHandler* h = d.openHandler();
    if(h->mHistory->mergeWindow() != 1000) {
        return finish(false, QString("the track's history does not merge (window %1 ms)").arg(h->mHistory->mergeWindow()));
    }

    QString error;
    bool allOk = true;
    batchState stateA;
    if(!writeState(workDir, "A", stateA, error)) return finish(false, error);
    int depthA = batchScenarios::undoDepth(h);

    // the force (else geometric) sections, and a transition of the first whose length can be typed
    QList<int> transitionSections;
    for(int s = 0; s < d.sectionCount(); ++s) {
        if(d.sectionType(s) == forced || d.sectionType(s) == geometric) transitionSections.append(s);
    }
    const eFunctype forceOrder[] = { funcNormal, funcRoll, funcLateral };
    const eFunctype geoOrder[] = { funcPitch, funcYaw, funcRoll };
    // selects section s and the first of its transitions whose length can be typed
    auto pickLength = [&](int s) -> bool {
        d.selectSection(s);
        const eFunctype* order = d.sectionType(s) == forced ? forceOrder : geoOrder;
        for(int k = 0; k < 3; ++k) {
            d.selectTransition(order[k], 0);
            if(d.transitionEnabled("lengthSpin")) return true;
        }
        return false;
    };

    // One typed run: the values the box shows digit by digit. Must make one
    // step that undoes to the files before and redoes to the files after.
    auto typedRun = [&](const QString& what, std::function<void(double)> type, std::function<double()> shown,
                        const QList<double>& typed) {
        batchState before, after, undone, redone;
        if(!writeState(workDir, "state", before, error)) return false;
        int depth = batchScenarios::undoDepth(h);
        QList<double> values = typed;
        if(values.contains(shown())) {
            // the first digit must change the box (and typing back to the
            // value the run started from would take the step away)
            for(int k = 0; k < values.size(); ++k) values[k] += 1;
        }
        for(int k = 0; k < values.size(); ++k) type(values[k]);
        if(!d.ok()) return false;
        int steps = batchScenarios::undoDepth(h) - depth;
        int calls = steps > 0 ? h->mHistory->steps().last().calls.size() : 0;
        if(!writeState(workDir, "state", after, error)) return false;
        d.undo();
        if(!writeState(workDir, "state", undone, error)) return false;
        d.redo();
        if(!writeState(workDir, "state", redone, error)) return false;
        if(!d.ok()) return false;
        QStringList undoDiff = differences(before, undone);
        QStringList redoDiff = differences(after, redone);
        bool ok = steps == 1 && calls >= 2 && undoDiff.isEmpty() && redoDiff.isEmpty();
        lReport.append(QString("%1: typed %2 values, recorded %3 step(s) of %4 commands; undo %5; redo %6%7")
                       .arg(what).arg(values.size()).arg(steps).arg(calls)
                       .arg(undoDiff.isEmpty() ? QString("restores the files before the first digit") : QString("DOES NOT RESTORE ") + undoDiff.join(", "))
                       .arg(redoDiff.isEmpty() ? QString("restores") : QString("DOES NOT RESTORE ") + redoDiff.join(", "))
                       .arg(ok ? QString() : QString(": FAILED")));
        allOk = allOk && ok;
        return true;
    };
    // Two edits that must stay two steps.
    auto twoSteps = [&](const QString& what, std::function<void()> edits) {
        int depth = batchScenarios::undoDepth(h);
        edits();
        if(!d.ok()) return false;
        int steps = batchScenarios::undoDepth(h) - depth;
        lReport.append(QString("%1: recorded %2 steps%3").arg(what).arg(steps).arg(steps == 2 ? QString() : QString(", not 2: FAILED")));
        allOk = allOk && steps == 2;
        return true;
    };
    auto typeTransition = [&](const char* box) {
        return std::function<void(double)>([&d, box](double v) { d.setTransitionValue(box, v); });
    };
    auto shownTransition = [&](const char* box) {
        return std::function<double()>([&d, box]() { return d.transitionValue(box); });
    };
    auto typePanel = [&](const char* box) {
        return std::function<void(double)>([&d, box](double v) { d.setPanelValue(box, v); });
    };
    auto shownPanel = [&](const char* box) {
        return std::function<double()>([&d, box]() { return d.panelValue(box); });
    };

    // 1. a transition's length, typed "12.5"
    int s1 = -1;
    for(int k = 0; k < transitionSections.size() && s1 < 0; ++k) {
        if(pickLength(transitionSections[k])) s1 = transitionSections[k];
    }
    if(s1 < 0) {
        lReport.append("transition length: skipped, no force or geometric section with a transition whose length can be typed");
    } else {
        pickLength(s1);
        if(!typedRun(QString("transition length (section %1)").arg(s1+1), typeTransition("lengthSpin"),
                     shownTransition("lengthSpin"), QList<double>() << 1 << 12 << 12.5)) {
            return finish(false, d.ok() ? error : d.error());
        }
    }

    // 2. the start speed, typed "35" (in the box's unit)
    d.selectSection(-1);
    if(!typedRun("start speed", typePanel("anchorSpeedBox"), shownPanel("anchorSpeedBox"), QList<double>() << 3 << 35)) {
        return finish(false, d.ok() ? error : d.error());
    }

    // 3. a section's own box, typed "45": a straight's length or a curve's radius
    int s3 = -1;
    const char* box3 = NULL;
    for(int s = 0; s < d.sectionCount() && s3 < 0; ++s) {
        if(d.sectionType(s) == straight) { s3 = s; box3 = "straightLengthBox"; }
        else if(d.sectionType(s) == curved) { s3 = s; box3 = "curvedRadiusBox"; }
    }
    if(s3 < 0) {
        lReport.append("section box: skipped, no straight or curved section");
    } else {
        d.selectSection(s3);
        if(!typedRun(QString("%1 (section %2)").arg(box3).arg(s3+1), typePanel(box3), shownPanel(box3),
                     QList<double>() << d.len(4) << d.len(45))) {
            return finish(false, d.ok() ? error : d.error());
        }
    }

    if(s1 >= 0) {
        // 4. two values of the same transition: length, then change
        pickLength(s1);
        double length = d.transitionValue("lengthSpin");
        double change = d.transitionValue("changeSpin");
        if(!twoSteps(QString("length then change of one transition (section %1)").arg(s1+1), [&]() {
                d.setTransitionValue("lengthSpin", length + 0.5);
                d.setTransitionValue("changeSpin", change + 0.25);
            })) return finish(false, d.error());

        // 5. the same box before and after another selection
        int other = s1 == 0 ? (d.sectionCount() > 1 ? 1 : -1) : 0;
        if(!twoSteps(QString("length of one transition, another section selected in between (section %1)").arg(s1+1), [&]() {
                d.setTransitionValue("lengthSpin", d.transitionValue("lengthSpin") + 0.5);
                d.selectSection(other);
                pickLength(s1);
                d.setTransitionValue("lengthSpin", d.transitionValue("lengthSpin") + 0.5);
            })) return finish(false, d.error());

        // 6. the same box on two sections
        int s2 = -1;
        for(int k = 0; k < transitionSections.size() && s2 < 0; ++k) {
            if(transitionSections[k] != s1) s2 = transitionSections[k];
        }
        if(s2 < 0) {
            lReport.append("length on two sections: skipped, only one force or geometric section");
        } else {
            if(!pickLength(s2)) {
                lReport.append("length on two sections: skipped, no transition of the second section can be typed");
            } else {
                // from one section's box to the other's, as fast as a script
                // can (a person selects the other section in between, which
                // ends the run as well; the targets differ too)
                pickLength(s1);
                if(!twoSteps(QString("length on sections %1 and %2").arg(s1+1).arg(s2+1), [&]() {
                        d.setTransitionValue("lengthSpin", d.transitionValue("lengthSpin") + 0.5);
                        pickLength(s2);
                        d.setTransitionValue("lengthSpin", d.transitionValue("lengthSpin") + 0.5);
                    })) return finish(false, d.error());
            }
        }
    }

    // every step undone: the files the load gave
    int extra = batchScenarios::undoDepth(h) - depthA;
    for(int k = 0; k < extra; ++k) d.undo();
    if(!d.ok()) return finish(false, d.error());
    batchState stateA2;
    if(!writeState(workDir, "A2", stateA2, error)) return finish(false, error);
    QStringList a2 = differences(stateA, stateA2);
    lReport.append(QString("after undoing all %1 steps: %2").arg(extra).arg(sameOrNot(a2)));
    allOk = allOk && a2.isEmpty() && extra > 0;

    QDir(workDir).removeRecursively();
    if(!windowsStayedHidden(error)) {
        return finish(false, error);
    }
    if(!allOk) {
        return finish(false, "merging of typed values failed (see the lines above)");
    }
    return finish(true, QString());
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
