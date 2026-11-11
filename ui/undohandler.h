#ifndef UNDOHANDLER_H
#define UNDOHANDLER_H

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

#include "undoaction.h"

class MainWindow;
class track;

// The undo list of one track: a stack of undoActions limited to maxStackSize,
// newest first. Every step holds the state of the track after it; the state
// before the oldest step is the base state. Undo puts back the state before a
// step, redo the state after it, and the panel then selects the step's section
// and transition again. Owned by trackHandler. doUndo() and doRedo() are called
// from MainWindow's Edit menu. docs/decisions/0011-undo-by-snapshot-commands-on-top.md.
//
// Anything that changes the track without a step (a load, an import) must call
// clearActions() afterwards, or the first undo would take it back.
class undoHandler
{
public:
    undoHandler(track* _track, int _stackSize = 15);
    ~undoHandler();
    void doUndo();
    void doRedo();

    // Records an edit that has just been made: takes the state of the track
    // now as the step's after-state. A step that changed nothing is dropped.
    void addAction(undoAction* _action);

    // Empties the list and takes the track as it is now as the base state.
    void clearActions();

    QList<undoAction*> lActions;
    int maxStackSize;

    int stackIndex;     // the step the next undo takes back; -1 with an empty list

    bool busy;          // set while a state is put back: nothing is recorded

private:
    // the state the track is in, as far as the list knows
    const trackState& stateNow() const;
    // the state before the step at index
    const trackState& stateBefore(int index) const;
    void putBack(undoAction* step, const trackState& state, bool undone);

    track* mTrack;
    trackState baseState;
};

#endif // UNDOHANDLER_H
