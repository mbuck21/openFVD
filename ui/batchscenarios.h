#ifndef BATCHSCENARIOS_H
#define BATCHSCENARIOS_H

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
#include "section.h"
#include "function.h"

class MainWindow;
class QWidget;
class trackHandler;
class track;
class trackWidget;
class graphWidget;

// Scripted test scenarios for the batch mode (docs/testing.md):
//   --batch-make-fixtures builds test projects by driving the windows,
//   --batch-undo-check edits a project through the windows and undoes and redoes it.
//
// THE RULE FOR THIS CODE: it drives only the surface a person uses.
//   - Widgets are found by the object names in the .ui forms (findChild), and
//     their values are set the way a person's input sets them: setValue(),
//     setCurrentIndex(), setChecked(), click(), selecting a row in a list.
//   - Where a person uses a context menu, the public slot the menu calls is
//     called (trackWidget::addStraightSec(), projectWidget::newEmptyTrack()).
//   - A transition is picked with graphWidget::changeSelection() followed by
//     graphWidget::selectionChanged(), which is what clicking its curve does.
//   - Undo and redo go through the Edit menu's actions.
//   - The model (core/) is only READ: to find which transition to pick, and for
//     the reports. Nothing here runs a command, writes a model field, or
//     touches a private member.
// The refactor of Stage 1 changes the plumbing under the widgets; these
// scenarios must keep passing unchanged. A change that breaks them has changed
// what a person's input does.

// One transition of a function, as the transition editor sets it.
struct transitionSpec
{
    double length;      // seconds, metres or degrees of arc, as the section's argument
    double change;      // g, or degrees per second (per metre with a distance argument)
    int shape;          // transition box index: 0 linear ... 6 plateau; -1 keeps the shape
    int variant;        // quadratic, quartic or quintic box index; -1 keeps it
    double variantValue;// quartic or quintic spin box; 0 keeps it
    double center;      // time-warp centre; 0 keeps it
    double tension;     // time-warp tension; 0 keeps it
};

class batchDriver
{
public:
    explicit batchDriver(MainWindow* w);

    bool ok() const { return sError.isEmpty(); }
    QString error() const { return sError; }
    void clearError() { sError.clear(); }

    // project panel
    void newProject();
    void newTrack();
    void openTrack(int t);
    void renameTrack(int t, const QString& name);
    void trackProperties(int t, double heartline, double friction, double resistance);

    // track panel of the track whose tab is open
    void addSection(secType type);
    void selectSection(int s);          // -1 is the anchor
    void deleteSection(int s);
    void renameSection(int s, const QString& name);
    void setPanelValue(const char* name, double value);
    void setPanelIndex(const char* name, int index);
    void setPanelChecked(const char* name, bool on);
    double panelValue(const char* name);
    bool panelEnabled(const char* name);

    // transition editor of the open track
    void selectTransition(eFunctype f, int index);
    void setTransitionValue(const char* name, double value);
    void setTransitionIndex(const char* name, int index);
    void setTransitionChecked(const char* name, bool on);
    void clickTransitionButton(const char* name);
    double transitionValue(const char* name);
    int transitionIndex(const char* name);
    bool transitionEnabled(const char* name);
    bool transitionChecked(const char* name);
    void setTransition(eFunctype f, int index, const transitionSpec& spec);
    void appendTransition(eFunctype f, int after, const transitionSpec& spec);
    void prependTransition(eFunctype f, const transitionSpec& spec);
    void lockTransition(eFunctype f, int index);

    // roll smoothing dialog: entry 0 is the whole track, n is section n (counted from 1)
    void smoothing(int entry, double lengthSeconds, int iterations, bool on);

    // Edit menu; false when the entry is disabled
    bool undo();
    bool redo();

    // unit factors of the options dialog (the spin boxes show the user's units)
    double len(double metres);
    double spd(double metresPerSecond);
    double perLen(double perMetre);

    // read-only access to the open track, for choosing targets and for reports
    trackHandler* openHandler();
    track* openTrackData();
    int sectionCount();
    secType sectionType(int s);
    int transitionCount(eFunctype f);

private:
    trackWidget* trackPanel();
    graphWidget* graphPanel();
    QWidget* transitionPanel();
    QWidget* projectPanel();
    QWidget* find(QWidget* parent, const char* name);
    func* function(eFunctype f);
    void fail(const QString& what);

    MainWindow* mWindow;
    QString sError;
};

struct historyStep;

namespace batchScenarios
{
    // the fixed list of scenario names; each builds one project
    QStringList names();
    QString description(const QString& name);
    void build(const QString& name, batchDriver& d);

    // statistics of one track for the report; empty error when sane
    QString trackStats(track* t, QString& error);

    // the scripted edits of --batch-undo-check
    int editCount();
    // Applies edit i to the open track. Returns false when the project has no
    // target for it (the reason goes to note). The name of the command
    // (core/command.h) the edit must record goes to expected.
    bool applyEdit(int i, batchDriver& d, QString& description, QString& expected, QString& note);

    // READ ONLY, for the report: how many steps the undo list holds
    int undoDepth(trackHandler* h);
    // The newest steps, oldest of them first, as text: each step is the names
    // of its commands joined by "+". commands gets every name.
    QString recordedCommands(trackHandler* h, int steps, QStringList& commands);
    // How the reports name a step: the name of its first command (its label
    // when it has none).
    QString stepName(const historyStep& step);
}

#endif // BATCHSCENARIOS_H
