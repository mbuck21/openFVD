#ifndef TRACKHANDLER_H
#define TRACKHANDLER_H

/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
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

#include "track.h"
#include "tracklistener.h"
#include "trackhistory.h"

class QTreeWidgetItem;
class trackWidget;
class graphWidget;
class trackMesh;

// One track's window side. Creates and owns the track (trackData), its panel, graph
// pane, list row, mesh and undo list (mHistory). Made by projectWidget (and importUi).
// Implements trackListener: when the track says its geometry changed it rebuilds the
// mesh, and it shows the status text and redraws the graphs. Holds no model logic.
//
// Undo: every edit runs a named command (core/command.h), which records its own
// step in mHistory (docs/decisions/0011-undo-by-snapshot-commands-on-top.md).
// The Edit menu calls undo() and redo() here, which put the step's selection
// back in the panel. Anything that changes the track without a step (a load,
// an import) calls clearHistory() afterwards, or the first undo would take it back.
class trackHandler : public trackListener, public trackHistoryListener
{
public:
    trackHandler(QString _name, int _id, int _maxUndoChanges);
    ~trackHandler();
    void changeID(int _id);
    int getID();

    void trackGeometryChanged(track* which, int fromNode);
    void trackStatusMessage(const QString& text, int timeoutMs);
    void trackSmoothingApplied(track* which);
    void trackSectionAdded(track* which, int index);
    void trackLoaded(track* which);
    void trackAboutToReset(track* which);
    void trackReset(track* which);

    // The track's history changed (a step, an undo, a redo, a clear): the
    // Undo and Redo menu entries follow it, whoever made the change.
    void historyChanged(trackHistory* which);

    // What trackReset() selects in the panel: the section and transition of
    // this undo step (undone or redone), or with NULL what was selected before.
    void selectAfterReset(const commandTarget* selection);

    // Selects section sec in the panel (-1: the anchor, -2: nothing; clamped
    // to the sections there are) and, when funcType is not -1, its transition
    // sub of that function (clamped).
    void selectInPanel(int sec, int funcType, int sub);

    // Edit, Undo and Edit, Redo: put the newest step (or the next one) back
    // and select its section and transition again. Nothing while a step is
    // being put back or when there is nothing to undo or redo.
    void undo();
    void redo();

    // Empties the history and takes the track as it is now as its start
    // (after a load or an import).
    void clearHistory();

    // The track was changed by an edit that is not an undo step (the track
    // list's show tick, switching on bAutoSmooth when the smoothing dialog is
    // first made): the history takes it into the newest state
    // (trackHistory::foldOutsideChange()), so undoing the newest step keeps it.
    // Undoing an older step still takes it back. Like a recorded edit, it ends Redo.
    void takeOutsideChange();

    // Ends the history's merge run, so the next change is a step of its own:
    // called when a box loses focus and when the selection changes (decision
    // record 0011, point 7).
    void endMergeRun();

    // How long a run of changes to the same value through the same box stays
    // one undo step, in ms (trackHistory::setMergeWindow()); read when a
    // handler is made. 1000 for the windows; the batch mode sets 0 before it
    // makes any, so its scripted checks stay one step per edit.
    static int mergeWindowMs;

    track* trackData;
    trackHistory* mHistory;     // the track's undo steps; made after the track, deleted before it
    QTreeWidgetItem* listItem;
    trackWidget* trackWidgetItem;
    graphWidget* graphWidgetItem;
    int tabId;

    trackMesh* mMesh;

private:
    // what to select after the step is undone or redone: the step's own
    // selection, moved off a section or transition the undo made vanish.
    // False when the step selects nothing (an anchor or track edit); a
    // section of -1 in the result with true means the anchor.
    bool selectionAfter(const historyStep& step, bool undone, commandTarget* out) const;
    void putBack(bool undo);

    int id;
    bool restoring;     // a step is being put back: undo, redo and outside changes wait

    // the selection to bring back after a reset (see selectAfterReset())
    bool resetHasSelection;
    commandTarget resetSelection;
    int resetSection;
    int resetFunc;
    int resetSubfunc;
    int resetArgument;      // the argument (time, distance) of the section shown before, or -1
    //mnode* curNode;
    //section* curSection;
    //int j;
};

#endif // TRACKHANDLER_H
