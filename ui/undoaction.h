#ifndef UNDOACTION_H
#define UNDOACTION_H

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

#include "function.h"
#include "trackstate.h"

class MainWindow;
class trackHandler;
class track;

enum eActionType
{
    onLengthSpin,
    onChangeSpin,
    onArg1,
    onCenterSpin,
    onTensionSpin,
    onTransitionBox,
    appendSubFunction,
    removeSubFunction,
    changeSegmentLength,
    changeCurveDirection,
    changeCurveLeadIn,
    changeCurveLeadOut,
    changeCurveRadius,
    changeSpeedState,
    changeSegmentSpeed,
    changeAnchorPosX,
    changeAnchorPosY,
    changeAnchorPosZ,
    changeAnchorRoll,
    changeAnchorPitch,
    changeAnchorYaw,
    changeAnchorNormal,
    changeAnchorLateral,
    changeAnchorPitchChange,
    changeAnchorYawChange,
    changeAnchorSpeed,
    changeTrackHeartline,
    changeTrackFriction,
    changeTrackResistance,
    appendSegment,
    removeSegment,
    changeSegmentArgument, //<-----
    changeSegmentOrientation,
    changeFunctionStatus,

    newTrack,
    deleteTrack
};

// One recorded edit of a track: its type (eActionType, for the reports), what to
// select again after undo or redo (the section and, for a transition edit, the
// function and transition), and the state of the track after the edit
// (core/trackstate.h). The state before it is the state after the step before,
// or the undo list's base state; undo and redo put those states back. Made by
// the window code that makes the edit, once the edit is finished; owned by the
// track's undoHandler. docs/decisions/0011-undo-by-snapshot-commands-on-top.md.
class undoAction
{
public:
    undoAction(trackHandler* _track, eActionType _type);
    ~undoAction();

    // After the track was put back to the state before this step (undone) or
    // after it: selects its section and transition again in the panel. False
    // for an edit of the anchor or the whole track, which selects nothing.
    bool reselect(bool undone);

    trackHandler* hTrack;

    eActionType type;

    // Other actions recorded with this one as the same step (the anchor
    // position that follows a roll, pitch, yaw or heartline change); for the
    // reports only.
    undoAction* nextAction;

    track* inTrack;
    int sectionNumber;      // -1: an edit of the anchor or the whole track
    bool hasFunction;       // a transition edit: inFunction and subfuncNumber are set
    eFunctype inFunction;
    int subfuncNumber;

    trackState after;       // the track once the edit was made
};

#endif // UNDOACTION_H
