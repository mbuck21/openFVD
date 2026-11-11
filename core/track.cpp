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
#include "exportfuncs.h"
#include "smoothhandler.h"
#include <QElapsedTimer>
#include <QVector>
#include <limits>
#include <cmath>
#include <cstring>

#define RELTHRESH 0.98f

using namespace std;


track::track()
{

}

track::track(glm::vec3 startPos, float startYaw, float heartLine)
{
    this->anchorNode = new mnode(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0, 0, -1), 0., 10.f, 1., 0.);
    this->startPos = startPos;
    this->startYaw = startYaw;
    this->startPitch = 0.f;
    povPos = glm::vec2(0, 0);
    anchorNode->updateNorm();
    anchorNode->fEnergy = 0.5f*anchorNode->fVel*anchorNode->fVel + F_G*anchorNode->fPosHearty(0.9*heartLine);
    this->fHeart = heartLine;
    fFriction = 0.03f;
    fResistance = 2e-5;
    hasChanged = true;
    drawTrack = true;
    drawHeartline = 0;
    mListener = NULL;
    bAutoSmooth = false;
    display.bWireframe = false;
    activeSection = NULL;

    smoothList.append(new smoothHandler(this, -1));

    smoothedUntil = 0;
    style = generic;
}

track::~track()
{
    while(lSections.size() != 0)
    {
        delete lSections.at(0);
        lSections.removeAt(0);
    }
    // Each smoothing entry leaves the list before it is deleted: its
    // destructor tells its listener (the smoothing dialog, which deletes the
    // entry's row), and nothing that runs then may find it in the list.
    while(smoothList.size() != 0)
    {
        smoothHandler* entry = smoothList.takeFirst();
        delete entry;
    }
    delete anchorNode;
}

void track::removeSection(int index)
{
    if(lSections.size() <= index) return;

    // out of the list before it is deleted (see ~track())
    smoothHandler* entry = smoothList.takeAt(index+1);
    delete entry;

    if(index == lSections.size()-1)
    {
        delete this->lSections.at(index);
        this->lSections.removeAt(index);
        if(lSections.size() != 0) activeSection = lSections.at(index-1);

        if(mListener != NULL)
            mListener->trackGeometryChanged(this, getNumPoints()-50 < 0 ? 0 : getNumPoints()-50);

        //updateTrack(index-1, lSections[index-1]->lNodes.size()-2);
    }
    else
    {
        lSections.at(index+1)->lNodes.prepend(lSections.at(index)->lNodes.at(0));
        delete this->lSections.at(index);
        this->lSections.removeAt(index);
        activeSection = lSections.at(index);

        updateTrack(index, 0);//lSections[index]->lNodes.size()-2);
    }
}

void track::removeSection(section* fromSection)
{
    int i = 0;
    if(lSections.size() == 0) return;
    for(; i > lSections.size(); ++i)
    {
        if(lSections.at(i) == fromSection) break;
    }
    removeSection(i);
}

void track::removeSmooth(int fromNode)
{
    if(smoothedUntil == fromNode) return;
    if(fromNode < 0) fromNode = 0;
    smoothedUntil = fromNode;
    mnode* prevNode, *curNode = NULL;
    float temp = 0.f;
    for(int i = 0; i < lSections.size(); ++i)
    {
        section* curSection = lSections[i];
        if(fromNode >= curSection->lNodes.size() && curSection->lNodes.size() > 1)
        {
            fromNode -= curSection->lNodes.size()-1;
            continue;
        }
		if(fromNode != 0) curNode = &curSection->lNodes[fromNode-1];
		else if(i != 0) curNode = &lSections[i-1]->lNodes.last();
        else curNode = this->anchorNode;
        for(int j = fromNode; j < curSection->lNodes.size(); ++j)
        {
            prevNode = curNode;
			curNode = &curSection->lNodes[j];
            if(fabs(curNode->fSmoothSpeed) > 0.)
            {
                temp -= curNode->fSmoothSpeed;
                curNode->setRoll(temp/F_HZ);
                curNode->smoothNormal = 0.f;
                curNode->smoothLateral = 0.f;
                curNode->fSmoothSpeed = 0.f;
                curNode->fDistFromLast = glm::distance(curNode->vPosHeart(fHeart), prevNode->vPosHeart(fHeart));
                curNode->fTotalLength = prevNode->fTotalLength + curNode->fDistFromLast;
            }
        }
        fromNode = 1;
    }
}

void track::applySmooth(int fromNode)
{
    if(fromNode < 0) fromNode = 0;
    if(smoothedUntil != fromNode)
    {
        qWarning("Smoothing state unstable!");
        return;
    }
    mnode* prevNode, *curNode = NULL;
    smoothedUntil = getNumPoints();
    float temp = 0.f;
    for(int i = 0; i < lSections.size(); ++i)
    {
        section* curSection = lSections[i];
        if(fromNode >= curSection->lNodes.size() && curSection->lNodes.size() > 1)
        {
            fromNode -= curSection->lNodes.size()-1;
            continue;
        }
		if(fromNode != 0) curNode = &curSection->lNodes[fromNode-1];
		else if(i != 0) curNode = &lSections[i-1]->lNodes.last();
        else curNode = this->anchorNode;
        for(int j = fromNode; j < curSection->lNodes.size(); ++j)
        {
            prevNode = curNode;
			curNode = &curSection->lNodes[j];
            if(fabs(curNode->fSmoothSpeed) > 0.)
            {
                temp += curNode->fSmoothSpeed;
                curNode->setRoll(temp/F_HZ);
                curNode->calcSmoothForces();
                curNode->fDistFromLast = glm::distance(curNode->vPosHeart(fHeart), prevNode->vPosHeart(fHeart));
                curNode->fTotalLength = prevNode->fTotalLength + curNode->fDistFromLast;
            }
        }
        fromNode = 1;
    }
}

// Moved unchanged from the smoothing dialog's code; only m_track-> became this->
// and the graph redraw goes through the listener.
void track::applyRollSmooth(int fromNode)
{
    this->removeSmooth(fromNode);

    this->anchorNode->fRollSpeed = 0.0;

    int sec, curNode = fromNode < 0 ? 0 : fromNode;
    for(sec = 0; sec < this->lSections.size(); ++sec) {
        if(this->lSections[sec]->lNodes.size() >= curNode) {
            break;
        }
        curNode -= this->lSections[sec]->lNodes.size()-1;
    }

    for(; sec < this->lSections.size(); ++sec) {
        section* curSection = this->lSections[sec];
        for(int i = curNode; i < curSection->lNodes.size(); ++i) {
			curSection->lNodes[i].fSmoothSpeed = 0.f;
        }
        curNode = 0;
    }

    for(int i = 0; i < this->smoothList.size(); ++i) {
        smoothHandler* cur = this->smoothList[i];
        if(cur->active == false) continue;

        if(cur->getTo() > fromNode) {
            applyRollSmoothFilter(cur);
        }
    }

    if(smoothingActive()) {
        this->applySmooth(fromNode);
    }
    if(mListener != NULL)
        mListener->trackSmoothingApplied(this);
    this->hasChanged = true;
    return;
}

void track::applyRollSmoothFilter(smoothHandler* _handler)
{
    const int iter = _handler->getIterations();
    const int length = _handler->getLength()/iter;
    const int fromNode = _handler->getFrom();
    const int toNode = _handler->getTo();

    QVector<double> adjustValues;

    QVector<double> *cur = new QVector<double>();
    QVector<double> *last = new QVector<double>();
    QVector<double> orig;
    QVector<double> *swap;

    mnode* curNode;

    if(toNode - fromNode - length/2*iter < 0) {
        lenAssert(0 && "Smoothing not possible");
        return;
    }

    double lastValue = 0., firstValue = 0.;
    for(int i = 0; i <= length/2*iter; ++i) {
        curNode = this->getPoint(toNode - i);
        lastValue += curNode->fRollSpeed + curNode->fSmoothSpeed;
        curNode =  this->getPoint(fromNode + i);
        firstValue += curNode->fRollSpeed + curNode->fSmoothSpeed;
    }
    lastValue /= length/2*iter + 1;
    firstValue /= length/2*iter + 1;

    for(int i = fromNode; i < toNode; ++i) {
        if(length == 0) {
            curNode = this->getPoint(i);
            cur->append(curNode->fRollSpeed + curNode->fSmoothSpeed);
            last->append(cur->last());
            orig.append(cur->last());
            continue;
        }
        double t1 = (i - fromNode - length/2.*iter)/(length/2. * iter);
        double t2 = (toNode - length/2.*iter - i)/(length/2. * iter);
        if(t1 < 0) t1 = 1.;
        else t1 = exp(-2*t1*t1);
        if(t2 < 0) t2 = 1.;
        else t2 = exp(-2*t2*t2);
        double t = (1. - t1)*(1. - t2);
        if(t != t) t = 0.;

        if(t2 > t1) {
            if(t > t2) { // max = t
                if(fabs(t1+t2) > std::numeric_limits<double>::epsilon()) {
                    t2 = t2/(t1+t2)*(1.-t);
                    t1 = t1/(t1+t2)*(1.-t);
                }
            } else {   // max = t2
                if(fabs(t1+t) > std::numeric_limits<double>::epsilon()) {
                    t = t/(t1+t)*(1.-t2);
                    t1 = t1/(t1+t)*(1.-t2);
                }
            }
        } else {
            if(t > t1) { // max = t
                if(fabs(t1+t2) > std::numeric_limits<double>::epsilon()) {
                    t2 = t2/(t1+t2)*(1.-t);
                    t1 = t1/(t1+t2)*(1.-t);
                }
            } else {   // max = t1
                if(fabs(t+t2) > std::numeric_limits<double>::epsilon()) {
                    t = t/(t2+t)*(1.-t1);
                    t2 = t2/(t2+t)*(1.-t1);
                }
            }
        }
        if(i < fromNode + length/2 * iter) {
            cur->append(firstValue);
        } else if(i > toNode - length/2*iter) {
            cur->append(lastValue);
        } else {
            curNode = this->getPoint(i);
            cur->append(t*(curNode->fRollSpeed + curNode->fSmoothSpeed) + t1*firstValue + t2*lastValue);
        }
        last->append(cur->last());
        orig.append(cur->last());
    }

    for(int iterations = 0; iterations < iter; ++iterations) {
        swap = cur;
        cur = last;
        last = swap;
        for(int i = 0; i < cur->size(); ++i) {
            double temp = 0.0, div = 0.;

            for(int j = -length/2; j <= length/2; ++j) {
                if(i+j < 0) {
                    temp += last->at(0);
                } else if(i+j >= last->size()) {
                    temp += last->last();
                } else {
                    temp += last->at(i+j);
                }
                div = length/2 * 2 + 1;
            }
            cur->replace(i, temp/div);
        }
    }

    for(int i = 0; i < cur->size(); ++i) {
        adjustValues.append(cur->at(i) - orig[i]);
    }

    for(int i = 0; i < adjustValues.size(); ++i) {
        this->getPoint(i+fromNode)->fSmoothSpeed += adjustValues[i];
    }

    delete cur;
    delete last;
}

// Brings every roll smoothing entry up to date from the track (range, label,
// name; see smoothHandler::update()), first to last, labelling the custom
// regions 'a', 'b', ... through *customChar, which ends one past the last
// letter used. Was the model part of the smoothing dialog's updateUi().
void track::updateSmoothEntries(char* customChar)
{
    *customChar = 'a';
    for(int i = 0; i < this->smoothList.size(); ++i) {
        this->smoothList[i]->update(customChar);
    }
}

bool track::smoothingActive()
{
    for(int i = 0; i < this->smoothList.size(); ++i) {
        if(this->smoothList[i]->active) return true;
    }
    return false;
}

void track::updateTrack(int index, int iNode)
{
    //qDebug("called updateTrack(%d, %d)", index, iNode);
    if(index < 0) index = 0;
    if(lSections.size() <= index)
    {
        hasChanged = true;
        return;   // for savety
    }

    QElapsedTimer timer;
    float mSec;
    bool useSmoothing = false;
    timer.start();

    int nodeAt = (lSections[index]->type == straight || lSections[index]->type == curved) ? 0 : iNode;
    for(int i = 0; i < index; ++i)
    {
        nodeAt += lSections[i]->lNodes.size()-1;
    }

    for(int i = 0; i < smoothList.size(); ++i)
    {
        smoothHandler* cur = smoothList[i];
        if(cur->active == false) continue;

        cur->update();
        if(cur->getTo() > nodeAt)
        {
            useSmoothing = true;
            if(cur->getFrom() < nodeAt)
            {
                nodeAt = cur->getFrom();
                i = -1;
            }
        }
    }

    if(useSmoothing)
    {
        removeSmooth(nodeAt);
    }

    int updateFrom = lSections.at(index)->updateSection(iNode);
    for(int i = index+1; i < lSections.size(); i++)
    {
		lSections.at(i)->lNodes.prepend(lSections.at(i-1)->lNodes[lSections.at(i-1)->lNodes.size()-1]);
        lSections.at(i)->updateSection(0);
    }

    if(useSmoothing && bAutoSmooth && smoothingActive())
    {
        // The node ranges taken above are those of the nodes before this
        // update (they say where the old smoothing must come off). The new
        // smoothing goes on over the ranges of the nodes just computed, as a
        // load does; otherwise it would depend on the previous edit.
        for(int i = 0; i < smoothList.size(); ++i)
        {
            if(smoothList[i]->active) smoothList[i]->update();
        }
        applyRollSmooth(nodeAt);
    }

    unsigned int count = getNumPoints() - nodeAt;
    unsigned int count2 = getNumPoints() - iNode - getNumPoints(lSections[index]);

    nodeAt = nodeAt > getNumPoints(lSections[index])+updateFrom ? getNumPoints(lSections[index])+updateFrom : nodeAt;

    if(mListener != NULL)
        mListener->trackGeometryChanged(this, nodeAt);

    mSec = timer.nsecsElapsed()/1000000.;
    if(mListener != NULL)
        mListener->trackStatusMessage(QString::number(mSec).append(QString("ms used to update %1 (%2) points").arg(count2).arg(count)), 3000);

    hasChanged = true;
}

void track::updateTrack(section* fromSection, int iNode)
{
    int i = 0;
    if(lSections.size() == 0) return;
    for(; i < lSections.size(); ++i)
    {
        if(lSections.at(i) == fromSection) break;
    }
    updateTrack(i, iNode);
}

void track::newSection(enum secType type, int index)
{
    mnode* startNode;
    if(!lSections.isEmpty())
    {
        section* temp;
        if(index == -1)
        {
            temp = lSections.at(lSections.size()-1);
			startNode = &temp->lNodes[temp->lNodes.size()-1];
        }
        else if(index == 0)
        {
            startNode = anchorNode;
            lSections.at(0)->lNodes.removeFirst();
        }
        else
        {
            temp = lSections.at(index-1);
			startNode = &temp->lNodes[temp->lNodes.size()-1];
            if(lSections.size() > index)
            {
                lSections.at(index)->lNodes.removeFirst();
            }
        }
    }
    else
    {
        startNode = anchorNode;
    }

    section* newSection;
    switch(type)
    {
    case 1:
        newSection = new secstraight(this, startNode, 10);
        break;
    case 2:
        newSection = new seccurved(this, startNode, 90, 15);
        break;
    case 3:
        newSection = new secforced(this, startNode, 1000);
        break;
    case 4:
        newSection = new secgeometric(this, startNode, 1000);
        break;
    case 5:
        newSection = new secbezier(this, startNode);
        break;
    case 6:
        newSection = new secnlcsv(this, startNode);
        break;
    default:
        newSection = NULL;
        qWarning("Wrong Section type defined!");
    }
    activeSection = newSection;
    if(index == -1)
    {
        lSections.append(newSection);
        newSection->updateSection();

        smoothList.insert(lSections.size(), new smoothHandler(this, lSections.size()-1));
    }
    else if(index == 0)
    {
        lSections.prepend(newSection);
        newSection->updateSection();
        if(lSections.size() > 1)
        {
            lSections.at(1)->lNodes.prepend(newSection->lNodes[newSection->lNodes.size()-1]);
        }
        smoothList.insert(1, new smoothHandler(this, 0));
    }
    else
    {
        lSections.insert(index, newSection);
        newSection->updateSection();
        if(lSections.size() > index+1)
        {
            lSections.at(index+1)->lNodes.prepend(newSection->lNodes[newSection->lNodes.size()-1]);
        }
        smoothList.insert(index+1, new smoothHandler(this, index));
    }
    hasChanged = true;
}

// Adds a section of the given type at the end of the track with the model
// work the track panel's "add section" does (trackWidget::addSection(), the
// sectionHandler constructor): creates it, names it "unnamed" and computes
// the track from it with its default values. Then tells the listener, which
// shows the new section as the panel's "add" does. For sections that come
// from a file (a project being loaded, an imported track); the caller fills
// in the values. The panel passed its row count less the anchor row as the
// index, which is lSections.size() on a track whose rows all came this way.
section* track::appendSectionFromFile(enum secType type)
{
    int index = lSections.size();
    newSection(type, index);
    section* added = lSections[index];
    added->sName = QString("unnamed");
    updateTrack(added, 0);

    if(mListener != NULL)
        mListener->trackSectionAdded(this, index);

    return added;
}

// The anchor's pitch and yaw change rates from its forces and speed.
void track::updateAnchorGeometrics()
{
    glm::vec3 forceVec = glm::vec3(0, 1, 0) + anchorNode->forceNormal*anchorNode->vNorm + anchorNode->forceLateral*anchorNode->vLat;

    glm::vec3 pitchVec = (float)cos(anchorNode->fRoll*F_PI/180)*anchorNode->vNorm - (float)sin(anchorNode->fRoll*F_PI/180)*anchorNode->vLat;
    glm::vec3 yawVec = (float)sin(anchorNode->fRoll*F_PI/180)*anchorNode->vNorm + (float)cos(anchorNode->fRoll*F_PI/180)*anchorNode->vLat;

    anchorNode->fPitchFromLast = glm::dot(forceVec, pitchVec)/anchorNode->fVel*1.8/F_PI;
    anchorNode->fYawFromLast = glm::dot(forceVec, yawVec)/anchorNode->fVel*1.8/F_PI;
}

// See track.h. A load makes the first section from the anchor node after the
// anchor is set up (newSection() copies *anchorNode); the sections only ever
// read that copy (updateNorm() aside, which gives the same bits again), so
// copying the anchor now gives the first section the start a reload gives it.
void track::updateFromAnchor()
{
    if(!lSections.isEmpty())
    {
        lSections[0]->lNodes[0] = *anchorNode;
    }
    updateTrack(0, 0);
}

// See track.h. The node track::track() makes, then the steps loadTrackHeader()
// always made after reading the values, unchanged: on the fresh node of a
// track being loaded this gives what a load always gave, bit for bit.
void track::rebuildAnchor()
{
    float roll = anchorNode->fRoll;
    float vel = anchorNode->fVel;
    float normal = anchorNode->forceNormal;
    float lateral = anchorNode->forceLateral;

    *anchorNode = mnode(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0, 0, -1), 0., 10.f, 1., 0.);
    anchorNode->updateNorm();

    anchorNode->fRoll = roll;
    anchorNode->fVel = vel;
    anchorNode->forceNormal = normal;
    anchorNode->forceLateral = lateral;

    anchorNode->fEnergy = 0.5f*anchorNode->fVel*anchorNode->fVel + F_G*anchorNode->fPosHearty(0.9*fHeart);

    anchorNode->changePitch(startPitch, false);
    anchorNode->setRoll(anchorNode->fRoll);

    updateAnchorGeometrics();

    anchorNode->updateNorm();
}

// See track.h.
int track::settleAnchor(int maxPasses)
{
    int pass = 1;
    for(; pass <= maxPasses; ++pass)
    {
        float roll = anchorNode->fRoll;
        rebuildAnchor();
        if(memcmp(&roll, &anchorNode->fRoll, sizeof(float)) == 0) return pass;
    }

    // Some rolls alternate between two neighbouring floats (-89.25 gives
    // -89.2500076, which gives -89.25 again), so neither is kept. Then the
    // nearest float roll a rebuild keeps is taken, within 256 floats either side
    // of the last one (256 floats are 0.00012 degrees at a roll of 5 degrees).
    const float last = anchorNode->fRoll;
    for(int k = 1; k <= 256; ++k)
    {
        for(int side = 0; side < 2; ++side, ++pass)
        {
            float roll = last;
            for(int i = 0; i < k; ++i)
            {
                roll = nextafterf(roll, side == 0 ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity());
            }
            anchorNode->fRoll = roll;
            rebuildAnchor();
            if(memcmp(&roll, &anchorNode->fRoll, sizeof(float)) == 0) return pass;
        }
    }
    return -1;
}

int track::exportTrack3(fstream *file, float mPerNode, int fromIndex, int toIndex, float fRollThresh)
{
    QList<int> exportPoints;
	mnode* anchor = &lSections.at(fromIndex)->lNodes[0];
    for(int i = fromIndex; i <= toIndex; ++i)
    {
        lSections.at(i)->iFillPointList(exportPoints, mPerNode);
    }

    mnode *lastP = anchor, *curP = getPoint(exportPoints[0]);
    QList<bezier_t*> bezList;

    bezList.append(new bezier_t);
    bezList[0]->P1 = glm::vec3(0.f, 0.f, 0.f);

    for(int i = 0; i < exportPoints.size(); ++i)
    {
        curP = getPoint(exportPoints[i]);

        curP->exportNode(bezList, lastP, NULL, anchor, fHeart, fRollThresh);

        lastP = curP;
    }


    size_t size = bezList.size();
    QVector<float> a = QVector<float>(size);
    QVector<float> b = QVector<float>(size);
    QVector<float> c = QVector<float>(size);
    QVector<glm::vec3> d = QVector<glm::vec3>(size);

    for(size_t i = 0; i < size; ++i)
    {
        d[i] = bezList[i]->P1;
        if(i == 0)
        {
            a[i] = 0.f;
            b[i] = 1.f;
            c[i] = 0.f;
        }
        else if(i == size-1)
        {
            a[i] = 0.f;
            b[i] = 1.f;
            c[i] = 0.f;
        }
        else
        {
            a[i] = 1.f/6.f;
            b[i] = 4.f/6.f;
            c[i] = 1.f/6.f;
       }
    }

    // solve that shit

    c[0] = c[0]/b[0];
    d[0] = d[0]/b[0];

    for(size_t i = 1; i < size; ++i)
    {
        float m = 1.f/(b[i]-a[i]*c[i-1]);
        c[i] = c[i] * m;
        d[i] = m*(d[i] - a[i]*d[i-1]);
    }

    for(size_t i = size-1; i-- > 0;)
    {
        d[i] = d[i] - c[i] * d[i+1];
    }

    int i;
    for(i = 1; i < bezList.size(); ++i)
    {
        bezList[i]->Kp1 = 1.f/3.f*(d[i]+2.f*d[i-1]);
        bezList[i]->Kp2 = 1.f/3.f*(2.f*d[i]+d[i-1]);
        bezList[i-1]->P1 = 0.5f*(bezList[i-1]->Kp2 + bezList[i]->Kp1);
    }
    delete bezList[0];

    bezList.removeFirst();

    writeToExportFile(file, bezList);

    for(int i = 0; i < bezList.size(); ++i)
    {
        delete bezList[i];
    }

    return exportPoints.size();
}

int track::exportTrack4(fstream *file, float mPerNode, int fromIndex, int toIndex, float fRollThresh)
{
    QList<int> exportPoints;
	mnode* anchor = &lSections.at(fromIndex)->lNodes[0];
    for(int i = fromIndex; i <= toIndex; ++i)
    {
        lSections.at(i)->iFillPointList(exportPoints, mPerNode);
    }

    mnode *last = anchor, *current = getPoint(exportPoints[0]), *mid = getPoint(exportPoints[0]/2);
    QList<bezier_t*> bezList;

    for(int i = 0; i < exportPoints.size(); ++i)
    {
        current = getPoint(exportPoints[i]);

        mid = NULL; //i == 0 ? getPoint(exportPoints[0]/2) : getPoint((exportPoints[i]+exportPoints[i-1])/2);

        current->exportNode(bezList, last, mid, anchor, fHeart, fRollThresh);

        last = current;
    }

    writeToExportFile(file, bezList);

    return exportPoints.size();
}

void track::exportNL2Track(FILE *file, float mPerNode, int fromIndex, int toIndex)
{
    QList<int> exportPoints, rollPoints;
	mnode* anchor = &lSections.at(fromIndex)->lNodes[0];
    exportPoints.append(getNumPoints(lSections.at(fromIndex)));
    rollPoints.append(getNumPoints(lSections.at(fromIndex)));
    for(int i = fromIndex; i <= toIndex; ++i)
    {
        lSections.at(i)->fFillPointList(exportPoints, mPerNode);
    }

    for(int i = 0; i < exportPoints.size(); ++i) {
        qDebug("%d\n", exportPoints[i]);
    }

    size_t size = exportPoints.size();
    QVector<float> a = QVector<float>(size);
    QVector<float> b = QVector<float>(size);
    QVector<float> c = QVector<float>(size);
    QVector<glm::vec3> d = QVector<glm::vec3>(size);

    for(size_t i = 0; i < size; ++i)
    {
        int point = exportPoints[i];
        mnode* curNode = getPoint(point < 0 ? -point : point);
        d[i] = curNode->vPos - anchor->vPos;
        if(i == 0 || i == size-1 || point < 0) {
            a[i] = 0.f;
            b[i] = 1.f;
            c[i] = 0.f;
        } else {
            a[i] = 1.f/6.f;
            b[i] = 4.f/6.f;
            c[i] = 1.f/6.f;
       }
    }

    // tridiagonal solver

    c[0] = c[0]/b[0];
    d[0] = d[0]/b[0];

    for(size_t i = 1; i < size; ++i) {
        float m = 1.f/(b[i]-a[i]*c[i-1]);
        c[i] = c[i] * m;
        d[i] = m*(d[i] - a[i]*d[i-1]);
    }

    for(size_t i = size-1; i-- > 0;) {
        d[i] = d[i] - c[i] * d[i+1];
    }

    // resolve strictness
    QList<glm::vec4> e;
    e.append(glm::vec4(d[0], 1.f));
    for(size_t i = 1; i < size-1; ++i) {
        int point = exportPoints[i];
        int ppoint = exportPoints[i-1];
        int npoint = exportPoints[i+1];
        int strict = 0;
        if(ppoint <= 0) {
            strict |= 1;
            ppoint *= -1;
        }
        if(point < 0) {
            strict |= 2;
            point *= -1;
        }
        if(npoint <= 0) {
            strict |= 4;
            npoint *= -1;
        }

        // if strict == 0 -> export normally
        // if strict == 1 -> straight into open section -> resolve point
        // if strict == 2 -> can't happen
        // if strict == 3 -> export normally
        // if strict == 4 -> open section into straight -> resolve point
        // if strict == 5 -> straight into straight -> resolve point into two points
        // if strict == 6 -> export normally
        // if strict == 7 -> can't happen

        if(strict == 1) {
            glm::vec3 dir = getPoint(ppoint)->vDir;
            glm::vec3 nP = getPoint(npoint)->vPos;
            float a = glm::length(nP-d[i-1]);
            float cosa = glm::dot(glm::normalize(nP-d[i-1]), dir);
            e.append(glm::vec4(d[i-1]+dir*a/(2.f*cosa), 0.f));
        } else if (strict == 2) {
            qDebug("bad export status");
        } else if (strict == 3) {
            e.append(glm::vec4(d[i], 1.f));
        } else if (strict == 4) {
            glm::vec3 dir = getPoint(npoint)->vDir;
            glm::vec3 pP = getPoint(ppoint)->vPos;
            float a = glm::length(d[i+1]-pP);
            float cosa = glm::dot(glm::normalize(d[i+1]-pP), dir);
            e.append(glm::vec4(d[i+1]-dir*a/(2.f*cosa), 0.f));
        } else if (strict == 5) {
            glm::vec3 dp = getPoint(npoint)->vPos - getPoint(ppoint)->vPos;
            glm::vec3 dv = getPoint(npoint)->vDir + getPoint(ppoint)->vDir;

            float a = glm::length2(dv)-1.f;
            float b = glm::dot(dv, dp)*2.f;
            float c = glm::length2(dp);

            b /= a;
            c /= a;

            float p = b/2.f;
            float x0 = -p + sqrt(p*p - c);
            //float x1 = -p - sqrt(p*p - c); // second solution (unsused)

            e.append(glm::vec4(getPoint(ppoint)->vPos-x0*getPoint(ppoint)->vDir, 0.f));
            e.append(glm::vec4(getPoint(npoint)->vPos+x0*getPoint(npoint)->vDir, 0.f));

            //qDebug("%f, %f", x0, x1);

        } else if (strict == 6) {
            e.append(glm::vec4(d[i], 1.f));
        } else if (strict == 7) {
            e.append(glm::vec4(d[i], 1.f));
        } else {
            e.append(glm::vec4(d[i], 0.f));
        }
    }
    e.append(glm::vec4(d[size-1], 1.f));

    float temp = glm::length(glm::vec3(anchor->vDir.x, 0.f, anchor->vDir.z));
    glm::mat3 anchorBase = glm::transpose(glm::mat3(-anchor->vDir.z/temp, 0.f, anchor->vDir.x/temp,
                     0.f, 1.f, 0.f,
                     -anchor->vDir.x/temp, 0.f, -anchor->vDir.z/temp));

    for(int i = 0; i < e.size(); ++i) {
        glm::vec3 ex = anchorBase*glm::vec3(e[i]);
        fprintf(file, "\t\t\t<vertex>\n");
        fprintf(file, "\t\t\t\t<x>%e</x>\n", ex.x);
        fprintf(file, "\t\t\t\t<y>%e</y>\n", ex.y);
        fprintf(file, "\t\t\t\t<z>%e</z>\n", ex.z);
        if(e[i].w > 0.5f) {
            fprintf(file, "\t\t\t\t<strict>true</strict>\n");
        }
        fprintf(file, "\t\t\t</vertex>\n");
    }

    float startLen = getPoint(exportPoints[0])->fTotalHeartLength;
    int lp = abs(exportPoints.last());
    float endLen = getPoint(lp)->fTotalHeartLength;

    for(size_t i = 0; i < size; ++i) {
        int point = exportPoints[i];
        mnode* curNode = getPoint(point < 0 ? -point : point);

        glm::vec3 up = anchorBase*(-curNode->vNorm);
        glm::vec3 right = anchorBase*(curNode->vLat);
        float coord = (curNode->fTotalHeartLength-startLen)/(endLen-startLen);

        fprintf(file, "\t\t\t<roll>\n");
        fprintf(file, "\t\t\t\t<ux>%e</ux>\n", up.x);
        fprintf(file, "\t\t\t\t<uy>%e</uy>\n", up.y);
        fprintf(file, "\t\t\t\t<uz>%e</uz>\n", up.z);
        fprintf(file, "\t\t\t\t<rx>%e</rx>\n", right.x);
        fprintf(file, "\t\t\t\t<ry>%e</ry>\n", right.y);
        fprintf(file, "\t\t\t\t<rz>%e</rz>\n", right.z);
        fprintf(file, "\t\t\t\t<coord>%e</coord>\n", coord);
        fprintf(file, "\t\t\t\t<strict>false</strict>\n");
        fprintf(file, "\t\t\t</roll>\n");
    }

    return;
}

QString track::saveTrack(std::iostream& file)
{
    saveTrackHeader(file);

    for(int i = 0; i < lSections.size(); ++i)
    {
        lSections.at(i)->saveSection(file);
    }

    saveTrackSmoothing(file);

    return QString("Save Successful");
}

// The track's saved bytes up to its sections: "TRC", the name, the colours,
// the anchor, the display values and the number of sections. Split out of
// saveTrack() so a trackState (trackstate.h) can keep it apart from the
// sections; the bytes are the same.
void track::saveTrackHeader(std::iostream& file)
{
    file << "TRC";

    // The count is in bytes: the name is written as UTF-8.
    std::string stdName = name.toStdString();
    int namelength = (int)stdName.length();

    writeBytes(&file, (const char*)&namelength, sizeof(int));
    file << stdName;

    // The three colours are written as the raw bytes of three QColor objects,
    // so the project file format depends on the size of QColor (issue 18).
    Q_STATIC_ASSERT_X(sizeof(QColor) == 16, "the .fvd format stores QColor as 16 raw bytes");
    writeBytes(&file, (const char*)&(display.colors), 3*sizeof(QColor));

    // ANCHOR
    writeBytes(&file, (const char*)&startPos, sizeof(glm::vec3));
    writeBytes(&file, (const char*)&anchorNode->fRoll, sizeof(float));
    writeBytes(&file, (const char*)&startPitch, sizeof(float));
    writeBytes(&file, (const char*)&startYaw, sizeof(float));

    writeBytes(&file, (const char*)&anchorNode->fVel, sizeof(float));

    writeBytes(&file, (const char*)&anchorNode->forceNormal, sizeof(float));
    writeBytes(&file, (const char*)&anchorNode->forceLateral, sizeof(float));

    writeBytes(&file, (const char*)&fHeart, sizeof(float));
    writeBytes(&file, (const char*)&fFriction, sizeof(float));
    writeBytes(&file, (const char*)&fResistance, sizeof(float));

    writeBytes(&file, (const char*)&drawTrack, sizeof(bool));
    writeBytes(&file, (const char*)&drawHeartline, sizeof(int));
    writeBytes(&file, (const char*)&style, sizeof(int));
    writeBytes(&file, (const char*)&display.bWireframe, sizeof(bool));

    writeBytes(&file, (const char*)&povPos.x, sizeof(float));
    writeBytes(&file, (const char*)&povPos.y, sizeof(float));

    //float fFriction;
    int size = lSections.size();
    writeBytes(&file, (const char*)&size, sizeof(int));
}

// The track's saved bytes after its sections: the smoothing entries and "EOT".
void track::saveTrackSmoothing(std::iostream& file)
{
    int size = smoothList.size();
    writeBytes(&file, (const char*)&size, sizeof(int));
    for(int i = 0; i < size; ++i)
    {
        smoothList[i]->saveSmooth(file);
    }

    file << "EOT";
}

QString track::loadTrack(std::iostream& file)
{
    loadTrackHeader(file);

    QString error = loadTrackSections(file);
    if(!error.isEmpty())
    {
        return error;
    }

    string temp = readString(&file, 3);
    if(temp == "EOT")
    {
        updateTrack(0, 0);
        if(mListener != NULL)
            mListener->trackLoaded(this);
        return QString("Load Successful");
    }
    else
    {
        return QString("Load not Successful");
    }
}

// The first part of loadTrack(): the name, colours, anchor and display values,
// read after the "TRC" tag, and the anchor node set up from them. Shared with
// restoreTrackState() (trackstate.cpp), which overwrites the anchor afterwards.
void track::loadTrackHeader(std::iostream& file)
{
    int namelength = readInt(&file);
    name = QString(readString(&file, namelength).c_str());

    readBytes(&file, &display.colors, 3*sizeof(QColor));

    startPos = readVec3(&file);
    anchorNode->fRoll = readFloat(&file);
    startPitch = readFloat(&file);
    startYaw = readFloat(&file);
    anchorNode->fVel = readFloat(&file);
    anchorNode->forceNormal = readFloat(&file);
    anchorNode->forceLateral = readFloat(&file);

    fHeart = readFloat(&file);
    fFriction = readFloat(&file);
    fResistance = readFloat(&file);

    drawTrack = readBool(&file);
    drawHeartline = readInt(&file);
    style = (enum trackStyle)readInt(&file);
    display.bWireframe = readBool(&file);

    povPos.x = readFloat(&file);
    povPos.y = readFloat(&file);

    // the anchor node from the values just read; the anchor commands build it
    // with the same function
    rebuildAnchor();
}

// The second part of loadTrack(): the number of sections, each section
// (appended, read and computed), then the smoothing entries; it stops before
// the "EOT" tag. Returns an error text, or an empty string when all was read.
// Shared with restoreTrackState() (trackstate.cpp).
QString track::loadTrackSections(std::iostream& file, bool compute)
{
    string temp;
    int size = readInt(&file);
    for(int i = 0; i < size; ++i)
    {
        temp = readString(&file, 3);
        if(temp == "STR")
        {
            appendSectionFromFile(straight);
            //this->newSection(straight);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else if(temp == "CUR")
        {
            appendSectionFromFile(curved);
            //this->newSection(curved);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else if(temp == "GEO")
        {
            appendSectionFromFile(geometric);
            //this->newSection(geometric);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else if(temp == "FRC")
        {
            appendSectionFromFile(forced);
            //this->newSection(forced);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else if(temp == "BEZ")
        {
            appendSectionFromFile(bezier);
            //this->newSection(forced);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else if(temp == "CSV")
        {
            appendSectionFromFile(nolimitscsv);
            //this->newSection(forced);
            activeSection->loadSection(file);
            if(compute) activeSection->updateSection();
        }
        else
        {
            return QString("Error while Loading: No Such Segment!");
        }
    }

    size = readInt(&file);
    for(int i = 0; i < size; ++i)
    {
        if(i >= smoothList.size()) smoothList.append(new smoothHandler(this, -2));

        smoothList[i]->loadSmooth(file);
    }

    return QString();
}

QString track::legacyLoadTrack(std::iostream& file)
{
    int namelength = readInt(&file);
    name = QString(readString(&file, namelength).c_str());

    readBytes(&file, &display.colors, 3*sizeof(QColor));

    startPos = readVec3(&file);
    anchorNode->fRoll = readFloat(&file);
    startPitch = readFloat(&file);
    startYaw = readFloat(&file);
    anchorNode->fVel = readFloat(&file);
    anchorNode->forceNormal = readFloat(&file);
    anchorNode->forceLateral = readFloat(&file);

    fHeart = readFloat(&file);
    fFriction = readFloat(&file);
    fResistance = readFloat(&file);

    drawTrack = readBool(&file);
    drawHeartline = readInt(&file);
    style = (enum trackStyle)readInt(&file);
    display.bWireframe = readBool(&file);

    povPos.x = readFloat(&file);
    povPos.y = readFloat(&file);

    rebuildAnchor();

    string temp;
    int size = readInt(&file);
    for(int i = 0; i < size; ++i)
    {
        temp = readString(&file, 3);
        if(temp == "STR")
        {
            appendSectionFromFile(straight);
            //this->newSection(straight);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else if(temp == "CUR")
        {
            appendSectionFromFile(curved);
            //this->newSection(curved);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else if(temp == "GEO")
        {
            appendSectionFromFile(geometric);
            //this->newSection(geometric);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else if(temp == "FRC")
        {
            appendSectionFromFile(forced);
            //this->newSection(forced);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else if(temp == "BEZ")
        {
            appendSectionFromFile(bezier);
            //this->newSection(forced);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else if(temp == "CSV")
        {
            appendSectionFromFile(nolimitscsv);
            //this->newSection(forced);
            activeSection->legacyLoadSection(file);
            activeSection->updateSection();
        }
        else
        {
            return QString("Error while Loading: No Such Segment!");
        }
    }

    size = readInt(&file);
    for(int i = 0; i < size; ++i)
    {
        if(i >= smoothList.size()) smoothList.append(new smoothHandler(this, -2));

        smoothList[i]->legacyLoadSmooth(file);
    }

    temp = readString(&file, 3);
    if(temp == "EOT")
    {
        updateTrack(0, 0);
        if(mListener != NULL)
            mListener->trackLoaded(this);
        return QString("Load Successful");
    }
    else
    {
        return QString("Load not Successful");
    }
}

mnode* track::getPoint(int index)
{
    int i = 0;
    if(index < 0) index = 0;
    while(lSections.size() > i && index > lSections.at(i)->lNodes.size()-1)
    {
        index -= lSections.at(i++)->lNodes.size()-1;
    }
    if(lSections.size() == i)
    {
		if(lSections.size())    return &lSections.last()->lNodes.last();
        else return anchorNode;
    }
	return &lSections.at(i)->lNodes[index];
}

int  track::getIndexFromDist(float dist)
{
    int lower = 0;
    int upper = getNumPoints();
    mnode* point = getPoint(upper);
    float cur = point->fTotalLength;
    if(dist > cur)
    {
        return upper;
    }
    else if(dist < 0.f)
    {
        return 0;
    }
    else
    {
        while(lower+2 <= upper-2)
        {
            point = getPoint((upper+lower)/2);
            cur = point->fTotalLength;
            if(cur < dist)
            {
                lower = (upper+lower)/2;
            }
            else
            {
                upper = (upper+lower)/2;
            }
        }
        return (upper+lower)/2;
    }
}

int track::getNumPoints(section* until)
{

    int sum = 0;
    for(int i = 0; i < lSections.size(); ++i)
    {
        if(lSections.at(i) == until) return sum;
        sum += lSections.at(i)->lNodes.size()-1;
    }
    return sum;
}

int track::getSectionNumber(section *_section)
{
    int number = 0;
    while(number < lSections.size() && lSections.at(number) != _section) ++number;
    if(number < lSections.size())
    {
        return number;
    }
    else
    {
        return -1;
    }
}

void track::getSecNode(int index, int *node, int *section)
{
    int i = 0;
    while(lSections.size() > i && index > lSections.at(i)->lNodes.size()-1)
    {
        index -= lSections.at(i++)->lNodes.size()-1;
    }
    if(lSections.size() == i)
    {
        if(lSections.size())
        {
            *node = lSections.last()->lNodes.size()-1;
            *section = lSections.size()-1;
        }
        else
        {
            *node = 0;
            *section = -1;
        }
        return;
    }
    *node = index;
    *section = i;
    return;
}
