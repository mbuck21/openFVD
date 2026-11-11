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
#include "trackhandler.h"
#include "graphwidget.h"
#include "trackwidget.h"
#include "mainwindow.h"

extern MainWindow* gloParent;

undoAction::undoAction(trackHandler* _track, eActionType _type)
{
    type = _type;
    hTrack = _track;
    inTrack = _track->trackData;
    nextAction = NULL;
    sectionNumber = -1;
    hasFunction = false;
    inFunction = funcRoll;
    subfuncNumber = -1;
    if(type == newTrack || type == deleteTrack);
    else if(type != changeAnchorPosX && type != changeAnchorPosY && type != changeAnchorPosZ && type != changeAnchorYaw && type != changeAnchorRoll
            && type != changeAnchorPitch && type != changeAnchorNormal && type != changeAnchorLateral && type != changeAnchorSpeed
            && type != changeTrackHeartline && type != changeTrackFriction && type != changeTrackResistance && type != changeAnchorPitchChange && type != changeAnchorYawChange)
    {
        sectionNumber = inTrack->getSectionNumber(inTrack->activeSection);
        if(type != changeSegmentLength && type != changeCurveDirection && type != changeCurveLeadIn && type != changeCurveLeadOut
                && type != changeCurveRadius && type != changeSpeedState && type != changeSegmentSpeed && type != appendSegment
                && type != removeSegment && type != changeSegmentArgument && type != changeSegmentOrientation
                && hTrack->graphWidgetItem->selFunc != NULL)
        {
            hasFunction = true;
            inFunction = hTrack->graphWidgetItem->selFunc->parent->type;
            subfuncNumber = hTrack->graphWidgetItem->selFunc->parent->getSubfuncNumber(hTrack->graphWidgetItem->selFunc);
        }
    }
}

undoAction::~undoAction()
{
    if(nextAction)
    {
        delete nextAction;
    }
}

commandTarget undoAction::target(int trackNumber) const
{
    // anchor and track edits: what was selected stays selected
    if(type == newTrack || type == deleteTrack || sectionNumber < 0) return commandTarget::forTrack(trackNumber);
    if(!hasFunction) return commandTarget::forSection(trackNumber, sectionNumber);
    return commandTarget::forTransition(trackNumber, sectionNumber, (int)inFunction, subfuncNumber);
}

// The names are the ones the reports print; keep them in the order of eActionType.
static const char* ACTION_NAMES[] = {
    "onLengthSpin", "onChangeSpin", "onArg1", "onCenterSpin", "onTensionSpin", "onTransitionBox",
    "appendSubFunction", "removeSubFunction", "changeSegmentLength", "changeCurveDirection",
    "changeCurveLeadIn", "changeCurveLeadOut", "changeCurveRadius", "changeSpeedState",
    "changeSegmentSpeed", "changeAnchorPosX", "changeAnchorPosY", "changeAnchorPosZ",
    "changeAnchorRoll", "changeAnchorPitch", "changeAnchorYaw", "changeAnchorNormal",
    "changeAnchorLateral", "changeAnchorPitchChange", "changeAnchorYawChange", "changeAnchorSpeed",
    "changeTrackHeartline", "changeTrackFriction", "changeTrackResistance", "appendSegment",
    "removeSegment", "changeSegmentArgument", "changeSegmentOrientation", "changeFunctionStatus",
    "newTrack", "deleteTrack"
};

int undoActionTypeCount()
{
    return (int)(sizeof(ACTION_NAMES)/sizeof(ACTION_NAMES[0]));
}

QString undoActionTypeName(int type)
{
    if(type < 0 || type >= undoActionTypeCount()) return QString("type%1").arg(type);
    return QString(ACTION_NAMES[type]);
}

int undoActionTypeFromName(const QString& name)
{
    for(int i = 0; i < undoActionTypeCount(); ++i) {
        if(name == ACTION_NAMES[i]) return i;
    }
    return -1;
}
