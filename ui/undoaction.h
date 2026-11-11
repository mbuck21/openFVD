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
#include "command.h"

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

// The names of the action types ("onLengthSpin", ...): the labels of the steps
// the edit slots record, and the words of the batch reports.
int undoActionTypeCount();
QString undoActionTypeName(int type);
int undoActionTypeFromName(const QString& name);    // -1 when it is none of them

// What an edit slot hands to undoHandler::addAction() once the edit is made:
// its type (eActionType) and what was selected (the section and, for a
// transition edit, the function and transition), which becomes the step's
// label and selection in the track's trackHistory. Made by the window code
// that makes the edit; addAction() reads it and deletes it.
class undoAction
{
public:
    undoAction(trackHandler* _track, eActionType _type);
    ~undoAction();

    // The step's selection: the section and, for a transition edit, the
    // function and transition (-1: an edit of the anchor or the whole track,
    // which selects nothing).
    commandTarget target(int trackNumber) const;

    trackHandler* hTrack;

    eActionType type;

    // Other actions recorded with this one as the same step (the anchor
    // position that follows a roll, pitch, yaw or heartline change); they
    // make the step's label ("changeAnchorRoll+changeAnchorPosX").
    undoAction* nextAction;

    track* inTrack;
    int sectionNumber;      // -1: an edit of the anchor or the whole track
    bool hasFunction;       // a transition edit: inFunction and subfuncNumber are set
    eFunctype inFunction;
    int subfuncNumber;
};

#endif // UNDOACTION_H
