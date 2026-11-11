#ifndef TRACK_H
#define TRACK_H

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

#include "mnode.h"
#include "secstraight.h"
#include "seccurved.h"
#include "secforced.h"
#include "secgeometric.h"
#include "secbezier.h"
#include "secnlcsv.h"
#include <QList>
#include <fstream>
#include <QString>
#include <QColor>
#include "tracklistener.h"

class smoothHandler;

enum trackStyle {
    generic = 0,        // 0,5m
    genericflat,    // 0,7m
    vekoma,         // 0,6m
    bm,             // 0,6m
    triangle,       // 0,5m
    box,            // 0,5m
    smallflat,           // 0,5m
    doublespine
};

// How the track is drawn. Saved in the project file, so it lives with the
// track rather than with the window that draws it.
struct trackDisplay
{
    QColor colors[3];   // default, active section, transition
    bool bWireframe;
};

// One track: the anchor node, the ordered sections (lSections), the roll smoothing
// entries (smoothList) and how it is drawn (display). Owns all of them and frees
// them. Created by trackHandler (ui/) and by projectFileClient::projectNewTrack().
// Tells its mListener what changed. Must not include or call any window class.
class track
{
public:
    track();
    track(glm::vec3 startPos, float startYaw, float heartLine = 0.0);
    ~track();
    void removeSection(int index);
    void removeSection(section* fromSection);

    void removeSmooth(int fromNode = 0);
    void applySmooth(int fromNode = 0);

    void applyRollSmooth(int fromNode = 0);
    void applyRollSmoothFilter(smoothHandler* _handler);
    void updateSmoothEntries(char* customChar);
    bool smoothingActive();

    void updateTrack(int index, int iNode);
    void updateTrack(section* fromSection, int iNode);
    void newSection(enum secType type, int index = -1);
    section* appendSectionFromFile(enum secType type);
    void updateAnchorGeometrics();

    // After a change to the anchor node: the first section starts again from
    // the anchor as it is now (the whole node, the copy a load makes when it
    // builds the section), then the whole track is computed with
    // updateTrack(0, 0). Without the copy the first section kept the anchor
    // it was made from until the project was saved and reopened.
    void updateFromAnchor();

    // The startPos (the anchor's place in the scene) that puts the heartline
    // point at the start of the track at holdX, holdY, holdZ (metres, as the
    // anchor's X, Y and Z boxes show it), for an anchor turned by yaw a,
    // pitch b and roll y (radians) with the heartline fHeart above the track.
    // The anchor boxes' and Track Properties' expression, unchanged: the hold
    // is a double, as the boxes give it, and each sum is narrowed to the float
    // once. Defined in commands_anchor.cpp.
    static glm::vec3 anchorStartPos(float a, float b, float y, float fHeart, double holdX, double holdY, double holdZ);

    int exportTrack3(std::fstream* file, float mPerNode, int fromIndex, int toIndex, float fRollThresh);
    int exportTrack4(std::fstream* file, float mPerNode, int fromIndex, int toIndex, float fRollThresh);

    void exportNL2Track(FILE *file, float mPerNode, int fromIndex, int toIndex);

    QString saveTrack(std::iostream& file);
    QString loadTrack(std::iostream& file);
    QString legacyLoadTrack(std::iostream& file);

    // The pieces of saveTrack() and loadTrack(), shared with trackState
    // (trackstate.h): saveTrack() is header, each section's saveSection(),
    // smoothing; loadTrack() is header, sections (with the smoothing entries),
    // then the "EOT" tag and a full update.
    void saveTrackHeader(std::iostream& file);
    void saveTrackSmoothing(std::iostream& file);
    void loadTrackHeader(std::iostream& file);
    // compute false: the sections are read but not computed (a restore computes
    // the whole track once afterwards, core/trackstate.cpp)
    QString loadTrackSections(std::iostream& file, bool compute = true);
    mnode* getPoint(int index);
    int getIndexFromDist(float dist);
    int getNumPoints(section* until = NULL);
    int getSectionNumber(section* _section);

    void getSecNode(int index, int *node, int *section);

    bool hasChanged;
    bool drawTrack;
    int drawHeartline;

    mnode* anchorNode;

    glm::vec3 startPos;
    float startYaw;
    float startPitch;

    section* activeSection;
    float fHeart;
    float fFriction;
    float fResistance;
    QList<section*> lSections;

    QString name;

    // Roll smoothing is re-applied by updateTrack only once this is set: the
    // ui sets it when the smoothing dialog first exists for the track or
    // after the track was loaded (it was "a smoothing dialog exists").
    bool bAutoSmooth;

    QList<smoothHandler*> smoothList;

    trackListener* mListener;   // told when the geometry changes; may be NULL

    trackDisplay display;

    int smoothedUntil;
    enum trackStyle style;
    glm::vec2 povPos;
};

#endif // TRACK_H
