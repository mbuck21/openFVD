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

#ifndef RIDESUMMARY_H
#define RIDESUMMARY_H

// The ride sheet's numbers (FR-18, docs/decisions/0012): one function from a
// track to a plain struct of results, in metric units (m, m/s, s, g, degrees).
// The command line prints them (`fvdcore summary`) and the main window's panel
// shows the same struct. Nothing here writes to a node or a track.
//
// Forces come from the measure registry (core/measure.h; roll smoothing is
// included, as in the program's read-out), findings from the check registry
// (core/finding.h). No force is derived again here.
//
// Time. A node's time is the sum of the steps before it. Force, geometric,
// straight and curved sections step 1/F_HZ per node (the last node of a
// straight or curve is a fractional step, ignored, as the plot ignores it).
// Bezier and NoLimits CSV sections space their nodes by distance, so there a
// step is the heartline distance to the previous node divided by the mean of
// the two nodes' speeds. This is why a time here can differ a little from the
// node index / F_HZ the plot and `fvdcore nodes` print for such a track.
//
// Heights, positions and headings are in the world frame (the track's start
// position and yaw applied), the same as the info panel and `fvdcore nodes`.
//
// Sign of the lateral force: positive when the rider is pressed toward his
// right (what a left turn does), negative toward his left (a right turn).
// Worked out from the code: vLat points to the rider's right (a train heading
// -z has vLat = +x, docs/research/r1-heartlines-and-trains.md); the section
// code stores forceLateral = -dot(forceVec, vLat), where forceVec is the force
// the train exerts on the rider (up plus acceleration/g); a left turn has a
// positive heading change, whose lateral term is negative in forceVec, so the
// stored force is positive. tests/ridesummary checks it on a baseline turn.

#include <QList>
#include <QString>
#include <QVector>
#include <glm/glm.hpp>

#include "designforcelimits.h"
#include "finding.h"
#include "measure.h"

class track;

// Where a track point is in the world (copied from MainWindow::updateInfoPanel,
// ui/mainwindow.cpp): translate(startPos) * rotateY(startYaw - 90 degrees).
// With world false everything passes through unchanged (the anchor frame).
// Used by the ride sheet and by the command line (cli/project.h).
struct placement
{
    placement(const track* t, bool world);
    glm::vec3 point(const glm::vec3& p) const;      // a position
    glm::vec3 direction(const glm::vec3& d) const;  // a direction (no shift)
    float heading(float rawHeading) const;          // getDirection() + startYaw

    bool bWorld;
    glm::mat4 base;
    float fYaw;
};

// The thresholds of the ride sheet, in one place. The owner's definitions of
// 2026-11-11 (docs/decisions/0012); in Stage 5 they become ride rules.
struct rideSummaryOptions
{
    float floaterUpperG;        // floater airtime: vertical force at or below this ...
    float ejectorUpperG;        // ... and above this; at or below this is ejector airtime
    double inversionAngleDeg;   // an inversion: the up direction passes more than this from upright
    double inversionReleaseDeg; // ... and must fall this far below it before the next one counts
    double dropHysteresisM;     // a bump of less than this on the way down does not end a drop

    rideSummaryOptions()
        : floaterUpperG(0.1f), ejectorUpperG(-0.5f), inversionAngleDeg(135.0),
          inversionReleaseDeg(5.0), dropHysteresisM(1.0) {}
};

// A value and where it happens.
struct rideSpot
{
    bool valid;         // false when there is no such point (an empty track, only non-finite values)
    int node;           // global node index (measure.h)
    int section;        // 1-based section number
    double time_s;      // see "Time" above
    double dist_m;      // along the centre line (fTotalLength)
    double value;       // the value, in the unit of the field that holds it
    rideSpot() : valid(false), node(0), section(0), time_s(0.0), dist_m(0.0), value(0.0) {}
};

// A fall (or a climb) in height: the amount (positive, metres) and its ends.
struct rideFall
{
    bool valid;
    double amount_m;
    rideSpot start;     // value = height there
    rideSpot end;
    rideFall() : valid(false), amount_m(0.0) {}
};

// One section: the compact description of a track.
struct rideSectionInfo
{
    int number;             // 1-based
    int type;               // enum secType (section.h)
    QString typeName;       // "straight", "curved", "force", "geometric", "bezier", "nolimits-csv"
    QString name;           // the section's own name (may be empty)
    double length_m;        // along the centre line
    double duration_s;
    int firstNode, lastNode;    // global node indices (the first node is shared with the section before)
    glm::vec3 endPosition;  // world position of the centre line at the section's end, m
    double endHeading_deg;  // world heading (includes the track's start yaw)
    double endPitch_deg;
    double endBank_deg;
    double endSpeed_mps;
    rideSectionInfo() : number(0), type(0), length_m(0.0), duration_s(0.0), firstNode(0), lastNode(0),
        endPosition(0.f), endHeading_deg(0.0), endPitch_deg(0.0), endBank_deg(0.0), endSpeed_mps(0.0) {}
};

struct rideSummary
{
    int sectionCount;
    int nodeCount;
    bool empty;             // no sections, so nothing below means anything
    bool nonFinite;         // some node has a speed, force or position that is not a number
                            // (a stalled train); such values are skipped in every sum and extreme

    double length_m;        // along the centre line: fTotalLength of the last node whose length is a number
    double heartLength_m;   // along the heartline: fTotalHeartLength, the same way
    bool lengthStopsAtStall;    // the last node's length is not a number (a stalled train), so the
                            // two lengths above are those of the last node before the stall
    double duration_s;      // first node to last node

    rideSpot highest;       // value = height in the world, m
    rideSpot lowest;
    double heightRange_m;   // highest minus lowest
    rideFall biggestDrop;   // crest to the valley after it
    rideFall biggestClimb;

    rideSpot topSpeed;      // value = m/s
    rideSpot lowSpeed;

    double floaterAirtime_s;    // vertical force above the ejector bound and at or below the floater bound
    double ejectorAirtime_s;    // vertical force at or below the ejector bound
    double totalAirtime_s;      // both

    rideSpot normalMax;     // value = g, vertical (positive: pressed into the seat)
    rideSpot normalMin;
    rideSpot lateralRight;  // highest lateral force, g, positive: rider pressed toward his right
    rideSpot lateralLeft;   // lowest lateral force, g, negative: rider pressed toward his left
    rideSpot alongMax;      // along-track force, g, positive: pressed back into the seat
    rideSpot alongMin;      // negative: thrown forward

    int inversions;
    rideSpot maxTilt;       // value = largest angle of the up direction from upright, degrees

    int findingCount[3];    // indexed by findingSeverity (information, warning, problem)
    QList<finding> findings;
    designLimitsInfo forceLimits;   // what the check "forceLimits" says of itself (its findings are in findings)

    QList<rideSectionInfo> sections;

    rideSummary();
};

// The ride sheet of one track (the track is not changed).
rideSummary computeRideSummary(const track* t, const rideSummaryOptions& options = rideSummaryOptions());

// The time of every node, seconds, one value per global node index; see "Time".
QVector<double> rideNodeTimes(const measureNodes& n);

// Pure helpers, public so that tests can feed them known arrays.

// Floater and ejector seconds: dt[i] seconds are added for node i when its
// force is in the band. Non-finite forces count as neither. Bounds are compared
// as floats: a force of exactly floaterUpperG is a floater, exactly
// ejectorUpperG is an ejector.
void rideAirtime(const QVector<float>& normalForce, const QVector<double>& dt,
                 const rideSummaryOptions& options, double* floaterSeconds, double* ejectorSeconds);

// Counts the times the angle (degrees from upright) rises above angleDeg; the
// next one counts only after it fell below angleDeg - releaseDeg. Non-finite
// values are skipped.
int rideCountInversions(const QVector<double>& angleDeg, double angleLimitDeg, double releaseDeg);

// The largest fall from a crest to the valley after it (the largest rise when
// climb is true), ignoring bumps against the direction of less than hysteresis
// (same unit as heights). Sets *from and *to (indices into heights) and
// returns the amount (>= 0); -1 in *from and *to and 0 when nothing falls.
// Non-finite heights are skipped.
double rideBiggestFall(const QVector<double>& heights, double hysteresis, bool climb, int* from, int* to);

// "straight", "curved", ... for an enum secType value.
QString rideSectionTypeName(int type);

// The words the sheet prints for the thresholds, built from the options so
// that a label cannot disagree with the numbers. The command line and the
// panel both use them.
QString rideFloaterLabel(const rideSummaryOptions& options);    // "Floater airtime (+0.1 g down to -0.5 g)"
QString rideEjectorLabel(const rideSummaryOptions& options);    // "Ejector airtime (below -0.5 g)"
QString rideInversionLabel(const rideSummaryOptions& options);  // "Inversions (more than 135 degrees from upright)"

// One sentence that goes under the forces and airtime: they are for the
// single point the program follows, not for every car (docs/research/r7-train-against-point.md).
QString rideSheetNote();

// The sentence that goes with the force-limits line: a design aid from public
// secondary sources, not a certification (decision record 0014, point 11). It
// says the numbers are unchecked unless the owner has checked them all.
QString rideForceLimitsNote(const designLimitsInfo& info);

#endif // RIDESUMMARY_H
