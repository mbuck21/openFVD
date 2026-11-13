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
#include <QJsonObject>
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

    // Puts every transition's start and end through the sums a load makes
    // when it reads a function back (func::loadFunction() appends the pieces
    // one by one, and each append links every earlier piece again, end to
    // start, with (prev.max + cur.max) - cur.min, which in single precision
    // can move an end by a last digit; issue 63, item 7). After it the pieces
    // are those a save and reopen gives; on pieces a load made it changes
    // nothing. One pass of the sums is not always a fixed point (an end can
    // move a last digit again on the second), so the passes repeat until
    // nothing moves, so that a load of the saved track moves nothing more;
    // false, with a qWarning, if eight passes do not settle. Changes no node;
    // run updateTrack() or updateFromAnchor() after it. Every command ends
    // with it (core/command.cpp, decision record 0013).
    bool settleTransitions();

    // Builds the anchor node from the values a save writes for it (its fRoll,
    // fVel, forceNormal and forceLateral, and startPitch and fHeart) exactly
    // as a load does: a fresh level node, the energy, pitch, roll and the
    // change rates from the forces. Used by loadTrackHeader() and
    // legacyLoadTrack(), and by every edit of an anchor value, so the node
    // after an edit is the one saving and reopening gives. Changes no saved
    // value but fRoll, which setRoll() derives again. Does not compute the
    // track; updateFromAnchor() does.
    void rebuildAnchor();

    // rebuildAnchor() until the roll the node is built from is, bit for bit,
    // the roll it then holds (setRoll() derives fRoll again, so 15 can become
    // 15.0000019, then 15.0000029): a save then writes the values the node was
    // built from, and a load builds the same node. A roll that alternates
    // between two neighbouring floats (about 1 in 75) is moved to the nearest
    // float roll a rebuild keeps, at most 256 floats away. Returns the number of
    // rebuilds (1 when the node was already settled), or -1 when no roll was
    // kept after maxPasses rebuilds and that search.
    int settleAnchor(int maxPasses = 8);

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
    // While set, updateTrack() computes nothing (it only marks the track
    // changed). commandRegistry::runSpec() sets it around a command's apply
    // function when the whole track is computed from the anchor right after
    // (decision record 0013): the apply function's own recompute from the
    // edited point would be thrown away. False at every other time; not part
    // of the saved or captured state.
    bool bDeferUpdate;
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

    // The track's additions: what the project file keeps for it in the
    // appendix after "EOP" (core/projectfile.h), one key per feature. Keys this
    // program does not know are kept as read and written back. Empty for a
    // track without additions. Part of the track's state (core/trackstate.h);
    // not touched by saveTrack() / loadTrack().
    QJsonObject additions;
};

#endif // TRACK_H
