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

undoHandler::undoHandler(track* _track, int _stackSize)
{
    busy = false;
    maxStackSize = _stackSize;
    stackIndex = -1;
    mTrack = _track;
    baseState = captureTrackState(mTrack);
}

undoHandler::~undoHandler()
{
    for(int i=0; i < lActions.size(); ++i)
    {
        delete lActions[i];
    }
}

const trackState& undoHandler::stateNow() const
{
    if(stackIndex >= 0 && stackIndex < lActions.size()) return lActions[stackIndex]->after;
    return baseState;
}

const trackState& undoHandler::stateBefore(int index) const
{
    if(index+1 < lActions.size()) return lActions[index+1]->after;
    return baseState;
}

void undoHandler::putBack(undoAction* step, const trackState& state, bool undone)
{
    gloParent->setUpdatesEnabled(false);
    busy = true;
    // the panel selects the step's section and transition again once the
    // track is rebuilt (trackHandler::trackReset())
    step->hTrack->selectAfterReset(step, undone);
    restoreTrackState(mTrack, state);
    step->hTrack->selectAfterReset(NULL, false);
    busy = false;
    gloParent->setUpdatesEnabled(true);
}

// The bounds are checked here, not only by the menu entries (issue 31).
void undoHandler::doUndo()
{
    if(stackIndex < 0 || stackIndex >= lActions.size()) return;

    trackState before = stateBefore(stackIndex);
    putBack(lActions[stackIndex], before, true);
    ++stackIndex;
}

void undoHandler::doRedo()
{
    if(stackIndex <= 0 || stackIndex > lActions.size()) return;

    --stackIndex;
    trackState after = lActions[stackIndex]->after;
    putBack(lActions[stackIndex], after, false);
}

void undoHandler::addAction(undoAction* _action)
{
    if(busy)
    {
        delete _action;
        return;
    }

    trackState before = stateNow();
    _action->after = captureTrackState(mTrack, &before);
    if(_action->after == before)
    {
        delete _action;
        return;
    }

    while(stackIndex > 0)
    {
        delete lActions[0];
        lActions.removeFirst();
        stackIndex--;
    }

    lActions.prepend(_action);
    if(lActions.size() > maxStackSize)
    {
        undoAction* oldest = lActions.takeLast();
        baseState = oldest->after;
        delete oldest;
    }

    stackIndex = 0;
}

void undoHandler::takeOutsideChange()
{
    if(busy) return;   // a restore in progress is not an edit
    if(stackIndex >= 0 && stackIndex < lActions.size()) {
        lActions[stackIndex]->after = captureTrackState(mTrack, &lActions[stackIndex]->after);
    } else {
        baseState = captureTrackState(mTrack, &baseState);
    }
}

void undoHandler::clearActions()
{
    while(lActions.size())
    {
        delete lActions[0];
        lActions.removeFirst();
    }
    stackIndex = -1;
    baseState = captureTrackState(mTrack);
}
