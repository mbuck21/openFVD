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

class QTreeWidgetItem;
class trackWidget;
class graphWidget;
class undoHandler;
class trackMesh;

// One track's window side. Creates and owns the track (trackData), its panel, graph
// pane, list row, mesh and undo list. Made by projectWidget (and importUi). Implements
// trackListener: when the track says its geometry changed it rebuilds the mesh, and
// it shows the status text and redraws the graphs. Holds no model logic.
class trackHandler : public trackListener
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


    track* trackData;
    QTreeWidgetItem* listItem;
    trackWidget* trackWidgetItem;
    graphWidget* graphWidgetItem;
    int tabId;

    trackMesh* mMesh;
    undoHandler* mUndoHandler;

private:

    int id;
    //mnode* curNode;
    //section* curSection;
    //int j;
};

#endif // TRACKHANDLER_H
