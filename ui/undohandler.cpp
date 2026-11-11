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

#include "undohandler.h"
#include "trackhandler.h"
#include "mainwindow.h"
#include "track.h"

extern MainWindow* gloParent;

undoHandler::undoHandler(trackHandler* handler)
{
    busy = false;
    hTrack = handler;
    mHistory = handler->mHistory;
}

undoHandler::~undoHandler()
{
}

// An undone append or a redone delete has one section (or transition) fewer,
// so the one before it is selected. The step's label starts with the name of
// its main action (undoAction.cpp); a step made some other way has no such
// name and is selected as it says.
commandTarget undoHandler::selectionAfter(const historyStep& step, bool undone) const
{
    commandTarget sel = step.selection;
    if(sel.section < 0) return sel;

    int type = undoActionTypeFromName(step.label.section('+', 0, 0));
    int sec = sel.section;
    int sub = sel.transition;
    if(type == appendSegment && undone) --sec;
    if(type == removeSegment && !undone) --sec;
    if(type == appendSubFunction && undone && sub > 0) --sub;
    if(type == removeSubFunction && !undone && sub > 0) --sub;

    // a transition is only kept on the section it was edited on
    int func = sec == sel.section ? sel.function : -1;
    return commandTarget(sel.track, sec, func, sub);
}

void undoHandler::putBack(bool undo)
{
    // the step that is undone is the newest applied one, the one redone the
    // first after it
    int index = undo ? mHistory->position()-1 : mHistory->position();
    commandTarget sel = selectionAfter(mHistory->steps()[index], undo);

    gloParent->setUpdatesEnabled(false);
    busy = true;
    // the panel selects the step's section and transition again once the
    // track is rebuilt (trackHandler::trackReset())
    hTrack->selectAfterReset(&sel);
    if(undo) mHistory->undo();
    else mHistory->redo();
    hTrack->selectAfterReset(NULL);
    busy = false;
    gloParent->setUpdatesEnabled(true);
}

// The bounds are checked here, not only by the menu entries (issue 31).
void undoHandler::doUndo()
{
    if(busy || !mHistory->canUndo()) return;
    putBack(true);
}

void undoHandler::doRedo()
{
    if(busy || !mHistory->canRedo()) return;
    putBack(false);
}

void undoHandler::addAction(undoAction* _action)
{
    if(busy)
    {
        delete _action;
        return;
    }

    QStringList names;
    for(undoAction* a = _action; a; a = a->nextAction)
    {
        names.append(undoActionTypeName(a->type));
    }
    mHistory->noteEdit(names.join("+"), _action->target(hTrack->getID()-1));
    delete _action;
}

void undoHandler::takeOutsideChange()
{
    if(busy) return;   // a restore in progress is not an edit
    mHistory->foldOutsideChange();
}

void undoHandler::clearActions()
{
    mHistory->clear();
}
