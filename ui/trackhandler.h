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
class undoHandler;
class trackMesh;
class undoAction;

// One track's window side. Creates and owns the track (trackData), its panel, graph
// pane, list row, mesh and undo list. Made by projectWidget (and importUi). Implements
// trackListener: when the track says its geometry changed it rebuilds the mesh, and
// it shows the status text and redraws the graphs. Holds no model logic.
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


    track* trackData;
    trackHistory* mHistory;     // the track's undo steps; made after the track, deleted before it
    QTreeWidgetItem* listItem;
    trackWidget* trackWidgetItem;
    graphWidget* graphWidgetItem;
    int tabId;

    trackMesh* mMesh;
    undoHandler* mUndoHandler;

private:

    int id;

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
