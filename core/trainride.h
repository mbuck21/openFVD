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

#ifndef TRAINRIDE_H
#define TRAINRIDE_H

// The ride: a train (core/trainspec.h) ridden over the finished track, in
// double precision, beside the design (docs/decisions/0014, points 1 to 7;
// docs/stage-3-plan.md, piece P1). It reads the nodes and writes none: no
// project computes or saves differently because a ride was made.
//
// The model (R7, docs/research/r7-train-against-point.md, section 2.2):
// - The cars are rigidly coupled along the track's centre line and share one
//   speed v (the speed of the centre line, the program's fVel).
// - Energy per kilogram of train: E = v^2/2 + g (Ybar + fRolling s), with
//   Ybar the mass-weighted mean height of the cars' mass points, each at its
//   own place, and s the distance the train has run. With the default mass
//   point (0.9 of the way from the heartline to the rail, R8 section 1.3)
//   and one car this is the program's own rule.
// - Losses besides fRolling (rideResistance, trainspec.h), taken out of E per
//   metre run: fRollingLoad g x (mean force pressing the cars on the track, in
//   g), fLinear v and fDrag v^2. fDrag is the program's fResistance
//   (v^3 / F_HZ per millisecond is fDrag v^2 per metre).
// - Before the first node and after the last the track continues straight
//   along its end direction, with the held-speed state of the end section.
// - Held speed (a section with "speed carried" off: lift, station, brake;
//   measureSpeedIsHeld() in core/measure.h): see "Held speed" in trainride.cpp.
// - A stall (no energy left for speed on free track) ends the ride; the
//   result says where. No value that is not a number leaves rideTrain().
//
// What a rider feels, in g, gravity included, in the program's axes
// (docs/research/r3-force-limits.md, section 1):
//   gz = -f.vNorm   pressed into the seat (the program's forceNormal)
//   gy = -f.vLat    the sign of the program's forceLateral: positive when the
//                   rider is pressed toward his right (a left turn)
//   gx =  f.vDir    pressed into the seat back (the sign of forceAlong; the
//                   heartline's own acceleration, see feltAt() in trainride.cpp)
// where f = (acceleration + g up) / g at that point. A car's forces are those
// at the heartline at the car's centre; each row rides the track at its own
// place; a seat adds the turning of the frame at its offset across the car and
// up from the heartline (R3 section 7: alpha x r + omega x (omega x r)), by
// the same rule that carries the centre line's acceleration to the heartline.
// The turning of the frame comes from the nodes' direction vectors over a span
// of nodes (never from second differences of float positions, which are far
// too noisy: docs/stage-3-plan.md section 3).

#include <QString>
#include <QStringList>
#include <QVector>

#include "trainspec.h"

class track;

// The three components of what a rider feels.
enum rideAxis
{
    rideGz = 0,     // pressed into the seat (vertical)
    rideGy = 1,     // sideways, + = rider pressed toward his right
    rideGx = 2      // along the track, + = pressed into the seat back
};

// The finished track as the ride reads it: one entry per node with a distance
// along the centre line greater than the one before (nodes that repeat a
// distance are left out of the lookup), plus the place of every global node
// for the by-place helpers. Built once from a track, or by hand in a test.
struct ridePath
{
    // per entry
    QVector<double> s;          // centre-line distance from the track's start, m (summed in double; not fTotalLength)
    QVector<double> sh;         // heartline distance from the start, m (the centre line's times rh)
    QVector<double> heartY;     // height of the heartline (vPos.y), m
    QVector<double> dirY, latY, normY;  // the frame's y components (vDir, vLat, vNorm)
    QVector<double> wD, wL, wN; // turning of the frame per metre of heartline about vDir (roll),
                                // vLat (pitch: + toward the rider's head) and vNorm, rad/m
    QVector<double> dwD, dwL, dwN;  // their change per metre of heartline, rad/m^2
    QVector<double> rh;         // heartline metres per centre-line metre (from the frame's turning)
    QVector<double> drh;        // its change per centre-line metre
    QVector<double> held;       // the held speed there, m/s, or -1 on free track
    QVector<int> node;          // global node index of the entry
    QVector<int> section;       // 1-based section number of the entry

    // per global node (core/measure.h numbering: getNumPoints()+1 values)
    QVector<double> nodePlace;  // centre-line distance of the node, m
    QVector<int> nodeSection;   // 1-based section number

    double fHeart;              // the track's heartline, m
    double fFriction;           // the track's own two numbers
    double fResistance;
    double startSpeed;          // the speed at the first node, m/s
    double startEnergyOffset;   // m: the height the program's start energy is taken at, less the first
                                // node's energy point (0 unless the anchor is rolled or pitched)
    bool truncated;             // the design itself stalls: the path stops at the last node that is a number
    int sectionCount;
    QStringList sectionTypes;   // per section: "straight", "curved", "force", "geometric", "bezier", "nolimits-csv"

    ridePath();
    int size() const { return s.size(); }
    bool isEmpty() const { return s.size() < 2; }
    // Fills dwD, dwL, dwN and drh from wD, wL, wN, rh, s and sh; called by
    // ridePathFromTrack(), and by a test that fills the arrays by hand.
    void deriveRates();
};

// The path of a finished track (nothing in the track is changed).
ridePath ridePathFromTrack(const track* t);

// How a ride is made.
struct rideOptions
{
    double sampleInterval_s;    // time between samples (default 0.01 s)
    double step_s;              // integration step (default 1/F_HZ, the program's node step)
    double maxTime_s;           // a ride that has not reached the end by then stops (default 3600 s)
    double floaterUpperG;       // the ride sheet's airtime bands (core/ridesummary.h)
    double ejectorUpperG;
    rideOptions();
};

// A value and where it happens, for the summary.
struct rideExtreme
{
    bool valid;
    double value;       // g
    double time_s;
    double place_m;     // centre-line distance where the car (its centre) is
    int node;           // the global node nearest that place, -1 off the track
    int section;        // 1-based, 0 off the track
    rideExtreme() : valid(false), value(0.0), time_s(0.0), place_m(0.0), node(-1), section(0) {}
};

// One car over the ride, counted only while the car's centre is on the track.
struct rideCarSummary
{
    rideExtreme low[3];     // indexed by rideAxis
    rideExtreme high[3];
    double floater_s;       // gz at or below floaterUpperG and above ejectorUpperG
    double ejector_s;       // gz at or below ejectorUpperG
    double belowZero_s;     // gz below 0
    rideCarSummary() : floater_s(0.0), ejector_s(0.0), belowZero_s(0.0) {}
};

// The result of a ride.
struct trainRide
{
    bool valid;             // false: nothing was ridden; error says why
    QString error;

    trainSpec spec;
    rideOptions options;
    double friction;        // the resistance actually used (spec's, or the project's when bFromProject)
    double rollingLoad;
    double linear;
    double drag;

    double trackStart_m, trackEnd_m;    // the centre-line distances of the first and last node
    QVector<double> carOffset_m;        // per car: its centre ahead (+) of the reference, m
    QVector<double> rowOffset_m;        // per row: ahead (+) of the car's centre, m

    // The samples: every sampleInterval_s from time 0, while the reference
    // runs from the track's start until the rearmost row has passed the last
    // node (so every row passes every place it starts behind).
    QVector<double> time_s;
    QVector<double> position_m;     // the reference's centre-line distance
    QVector<double> speed_mps;      // the train's speed (centre line)
    QVector<double> accel_mps2;     // its rate of change: the acceleration of the centre line along the
                                    // track, the same for every car (R7's along-track force is this / g
                                    // plus vDir.y where the car is)
    QVector<double> carG;           // [sample][car][axis]
    QVector<double> seatG;          // [sample][car][row][seat][axis]

    bool stalled;           // the train stopped (or did not reach the end in maxTime_s)
    double stallTime_s;
    double stallPlace_m;    // the reference's place
    int stallNode;          // the global node nearest it, -1 beyond either end of the track
    int stallSection;       // 1-based, 0 beyond either end
    QString stallText;      // one sentence, empty when it did not stall

    QStringList notes;      // what the ride assumed that a reader should know (held-speed cases met ...)

    QList<rideCarSummary> cars;     // per car, front first

    // node places, copied from the path for the by-place helpers
    QVector<double> nodePlace;
    QVector<int> nodeSection;
    int nodesOnPath;        // the leading nodes the ride covers (fewer than all when the design stalls)

    trainRide();
    int samples() const { return time_s.size(); }
    int carCount() const { return carOffset_m.size(); }
    int rowCount() const { return rowOffset_m.size(); }
    int seatCount() const { return spec.lSeatOffsets.size(); }
    double car(int sample, int c, int axis) const { return carG.at((sample*carCount() + c)*3 + axis); }
    double seat(int sample, int c, int row, int seat, int axis) const
    {
        return seatG.at((((sample*carCount() + c)*rowCount() + row)*seatCount() + seat)*3 + axis);
    }
};

// Rides the train over a track's finished nodes. Nothing in the track changes.
trainRide rideTrain(const track* t, const trainSpec& spec, const rideOptions& options = rideOptions());
// The same over a path (a test builds one by hand).
trainRide rideTrainOnPath(const ridePath& path, const trainSpec& spec, const rideOptions& options = rideOptions());

// By place: one value per global node, what the car (its centre) or the seat
// (its row's place) feels as it passes the place of that node, interpolated
// between the two samples either side. Where the car never passes a node (the
// front of the train starts ahead of the track's first metres; a ride that
// stalled) the value is that of the nearest sample it did reach and
// passed[node] is false. Empty when the ride is not valid.
QVector<double> trainRideCarByNode(const trainRide& ride, int car, int axis, QVector<bool>* passed = nullptr);
QVector<double> trainRideSeatByNode(const trainRide& ride, int car, int row, int seat, int axis,
                                    QVector<bool>* passed = nullptr);
// The train's speed when that car passes each node's place.
QVector<double> trainRideSpeedByNode(const trainRide& ride, int car, QVector<bool>* passed = nullptr);

// The per-car summary made from given samples (public for the tests).
rideCarSummary trainRideSummarize(const trainRide& ride, int car);

#endif // TRAINRIDE_H
