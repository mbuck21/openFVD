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

#ifndef HEARTLINE_H
#define HEARTLINE_H

// The heartline's motion, read from the finished track's frame: the one rule
// both the ride (core/trainride.*) and the along-track force measure
// (forceAlong, core/measure.*) use, so that the two cannot drift apart
// (issue 96). It reads nodes and writes none, in double precision.
//
// A rider at the heartline sits fHeart from the centre line toward his head
// and moves with the car's frame. Per metre of centre line the heartline runs
// rh metres (rh < 1 on the inside of a loop, > 1 on the outside of a valley),
// so at a centre-line speed v the heartline runs at rh v, and its
// acceleration along the track is
//     a_heart = rh a + v^2 drh/ds
// with a the centre line's acceleration (v dv/ds) and drh/ds the change of rh
// per metre of centre line: the first term is the train speeding up or
// slowing, the second the car pitching, which throws the rider forward or
// back even at a steady speed. What the rider feels along the track, in g,
// gravity included, positive when pressed into the seat back:
//     gx = vDir.y + a_heart / g.

#include <QVector>

struct measureNodes;
class mnode;
class section;

// The standard gravity (F_G in lenassert.h) as a double.
#define HEARTLINE_G 9.80665

// The turning of the frame at an entry is taken over +-2 entries (about 2 ms
// either side at 1 kHz), its rate of change (for a seat in the ride) over +-10
// entries, and rh's rate of change over +-20 (heartlineStretchRate(): two
// averages of rh over 20 entries each, the float rounding of rh counting v^2/g
// times). See heartline.cpp.
#define HEARTLINE_TURN_HALFSPAN 2
#define HEARTLINE_RATE_HALFSPAN 10
#define HEARTLINE_STRETCH_HALFSPAN 20

// The heartline's acceleration along the track, m/s^2, from the centre line's
// speed v and acceleration a and rh and its change drh per centre-line metre.
inline double heartlineAccel(double v, double a, double rh, double drh)
{
    return a*rh + v*v*drh;
}

// What a rider at the heartline feels along the track, g, from vDir.y and the
// heartline's acceleration (heartlineAccel()).
inline double heartlineAlongG(double dirY, double accelHeart)
{
    return dirY + accelHeart/HEARTLINE_G;
}

// The centre-line length of the step from node prev to node m, in double (see
// "Distances" in heartline.cpp).
double heartlineRailStep(const mnode* prev, const mnode* m, const section* sec, double fHeart);

// The frame of a finished track along its centre line. An entry is a node
// whose centre-line place is beyond the one before (a node that repeats a
// place shares the entry before it). A node that is not a number in speed,
// roll rate, position or frame ends the entries: the design stalls there.
struct heartlineFrame
{
    // per global node (core/measure.h numbering)
    QVector<double> nodePlace;  // centre-line distance from node 0, m (summed in double)
    QVector<int> nodeEntry;     // the entry of the node (or the one before it); -1 from where the design stalls

    // per entry
    QVector<int> node;          // its global node index
    QVector<double> s;          // its centre-line distance, m
    QVector<double> wD, wL, wN; // turning of the frame per metre of heartline about vDir (roll),
                                // vLat (pitch: + toward the rider's head) and vNorm, rad/m
    QVector<double> rh;         // heartline metres per centre-line metre
    QVector<double> sh;         // heartline distance from the first entry, m (rh summed over s)
    QVector<double> drh;        // rh's change per centre-line metre (heartlineStretchRate())

    bool truncated;             // the design stalls: the entries stop at the last node that is a number

    heartlineFrame() : truncated(false) {}
    int size() const { return s.size(); }
};

// The frame of the nodes n (with their track's heartline, fHeart).
heartlineFrame heartlineFrameOf(const measureNodes& n);

// rh's change per metre of centre line at entry i, from the heartline length
// sh, rh and the centre-line distance s over +-HEARTLINE_STRETCH_HALFSPAN entries
// (see heartline.cpp; 0 where the entries do not move).
double heartlineStretchRate(const QVector<double>& sh, const QVector<double>& rh, const QVector<double>& s, int i);

#endif // HEARTLINE_H
