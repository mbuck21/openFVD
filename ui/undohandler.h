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
#include "command.h"

class MainWindow;
class trackHandler;
class trackHistory;
struct historyStep;

// The window's side of a track's undo history (core/trackhistory.h, which holds
// the steps and puts states back; trackHandler owns it as mHistory). The edit
// slots hand an undoAction to addAction() once the edit is made; that becomes
// one step of the history. doUndo() and doRedo() are called from MainWindow's
// Edit menu. Owned by trackHandler. docs/decisions/0011-undo-by-snapshot-commands-on-top.md.
//
// Anything that changes the track without a step (a load, an import) must call
// clearActions() afterwards, or the first undo would take it back.
class undoHandler
{
public:
    explicit undoHandler(trackHandler* handler);
    ~undoHandler();
    void doUndo();
    void doRedo();

    // Records an edit that has just been made as a step of the history (the
    // label is the names of the action and the ones chained to it, the
    // selection the action's section and transition) and deletes the action.
    // A step that changed nothing is dropped.
    void addAction(undoAction* _action);

    // Empties the history and takes the track as it is now as its start.
    void clearActions();

    // The track was changed by an edit that is not an undo step (renaming,
    // colours, style, the smoothing dialog): the history takes it into the
    // newest state (trackHistory::foldOutsideChange()), so undoing the newest
    // step keeps it. Undoing an older step still takes it back. Like a
    // recorded edit, a change made after an undo ends Redo.
    void takeOutsideChange();

    bool busy;          // set while a state is put back: nothing is recorded

private:
    // what to select after the step is undone or redone: the step's own
    // selection, moved off a section or transition the undo made vanish
    commandTarget selectionAfter(const historyStep& step, bool undone) const;
    void putBack(bool undo);

    trackHandler* hTrack;
    trackHistory* mHistory;
};

#endif // UNDOHANDLER_H
