/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
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

#ifndef DESIGNFORCELIMITS_H
#define DESIGNFORCELIMITS_H

// The check "forceLimits": the force limits of core/forcelimits.h applied to
// the forces at the design point, the single point the program follows (the
// measures forceNormal, forceLateral and forceAlong, as the program shows
// them, roll smoothing included). It works on any project, with no train
// (decision record 0014, points 8 to 11). A design aid from public secondary
// sources, not a certification.
//
// The steps: the three measures are resampled to a uniform trace, in the time
// of the ride sheet (rideNodeTimes(): nodes of Bezier and NoLimits CSV
// sections are not evenly spaced in time); the engine judges the trace; each
// finding of the engine is mapped back to the nodes at its times. A track that
// stalls (a force that is not a number) is judged up to the last node before
// the first such value. Nothing here writes to a track.

#include <QList>
#include <QString>
#include <QVector>

#include "finding.h"
#include "forcelimits.h"
#include "measure.h"

class track;

// What the check says about itself, for the ride sheet and the command line.
struct designLimitsInfo
{
    designLimitsInfo() : bRan(false), iFindings(0), iJudgedNodes(0), iNodes(0) {}
    bool bRan;              // false: not judged (no track, a problem with the limit file; sError says which)
    QString sError;
    int iFindings;
    QString sStandard;      // "ASTM F2291"
    QString sConfidence;    // the lowest confidence of the numbers behind the answer (findings, else the whole file)
    QString sDisclaimer;    // the file's sentence: a design aid, not a certification
    int iJudgedNodes;       // the nodes judged (fewer than iNodes when the track stalls)
    int iNodes;

    // "Force limits (ASTM F2291, public numbers)": says "numbers checked by the owner"
    // only when every number of the file has been checked.
    QString label() const;
    // "within", "3 findings", "not judged (...)"
    QString value() const;
};

// The options the check runs with. THE ONE PLACE that names the restraint and
// the extended-airtime choice: until the train is part of the project, the
// engine's base case (eyes-front line of the lap bar or over-the-shoulder
// restraint of the standard curves, no extended negative-z line). The train
// pieces (D5, A4) replace this function with the choice of the track's train.
limitOptions designPointLimitOptions();

// The three forces of the design point as a uniform trace, 1/F_HZ seconds
// apart, from the node values and the node times (strictly rising). Linear
// between nodes. Only nodes 0 to lastNode are used. gx is the along-track
// force, gy the lateral, gz the vertical, as the measures give them.
forceTrace designPointTrace(const QVector<double>& times, const QVector<float>& along,
                            const QVector<float>& lateral, const QVector<float>& normal, int lastNode);

// The check on measures already computed (the ride sheet has them), or from
// the track. times as rideNodeTimes() gives. info may be null.
QList<finding> designPointForceLimits(const QVector<double>& times, const QVector<float>& along,
                                      const QVector<float>& lateral, const QVector<float>& normal,
                                      designLimitsInfo* info);
QList<finding> designPointForceLimits(const track* t, designLimitsInfo* info = 0);

#endif // DESIGNFORCELIMITS_H
