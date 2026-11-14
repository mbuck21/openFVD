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

#ifndef TRAINSPEC_H
#define TRAINSPEC_H

// A train as a plain value: everything about a train that a ride over the
// finished track (core/trainride.h) needs. Copyable, comparable, no pointers.
// It describes; it computes nothing about a track. A later piece saves it in
// the project and gives it boxes, and later research fills in real numbers:
// both only fill in these fields.
//
// Directions, as in the program's nodes (docs/research/r3-force-limits.md,
// section 1): along the track is vDir, toward the rider's right is vLat,
// toward the rider's head is -vNorm. Lengths in metres, masses in kg.

#include <QList>
#include <QString>
#include <QStringList>

class trainType;

// How the train loses energy (decision record 0014, point 4). All four terms
// are summed. R12 (docs/research/r12-nolimits2-train-physics.md) found that
// NoLimits 2 uses the same two numbers as the program, a constant friction and
// an air resistance, so the default is the project's own two and the other two
// terms are 0; they are here for what the owner's NoLimits 2 session may show.
struct rideResistance
{
    bool bFromProject;      // true (the default): fRolling and fDrag are the track's own
                            // fFriction and fResistance, and the other two terms are 0
    float fRolling;         // retarding force = this x the train's total weight (the meaning of fFriction)
    float fRollingLoad;     // retarding force = this x the sum over cars of the force pressing that car
                            // on the track (size of its normal-plus-lateral specific force x its weight)
    float fLinear;          // deceleration = this x speed, 1/s
    float fDrag;            // deceleration = this x speed^2, 1/m (the meaning of fResistance)

    rideResistance() : bFromProject(true), fRolling(0.03f), fRollingLoad(0.f), fLinear(0.f), fDrag(2e-5f) {}
    bool operator==(const rideResistance& o) const;
    bool operator!=(const rideResistance& o) const { return !(*this == o); }
};

struct trainSpec
{
    QString sTypeId;            // catalogue id (core/traintype.h), may be empty
    int iCars;                  // >= 1
    float fCarPitch;            // metres from one car's centre to the next, along the track centre line

    QList<float> lRowOffsets;   // metres along the car from its centre, + toward the front; one per row
    QList<float> lSeatOffsets;  // metres sideways from the car's centre line, + toward the rider's right
                                // (vLat); one per seat of a row
    float fSeatUp;              // metres from the heartline to where a rider's forces are evaluated,
                                // + toward the rider's head; 0 = at the heartline

    float fCarMass;             // kg, empty car
    float fRiderMass;           // kg per rider
    float fLoad;                // 0 to 1, share of seats taken
    QList<float> lCarMassFactor;    // per car, front first; empty = all 1 (a lighter or heavier lead car)

    bool bMassAtDesignPoint;    // true: a car's mass sits where the program's own design puts its point,
                                // 0.9 of the way from the heartline toward the rail (R8 section 1.3)
    float fMassAboveRail;       // otherwise: metres above the rail centre line (toward the rider's head)

    rideResistance resistance;

    int iReferenceCar;          // "the place of the train": the car at the track's start at time 0, whose
                                // position is where the train is. -1 = the train's centre (midway between
                                // the first and the last car's centre), otherwise a car index, 0 = front
    QString sRestraint;         // "lapbar", "otsr", "vest", "prone", "other" or empty; not used by the
                                // ride, carried for the force check

    trainSpec();                // the same as point()
    bool operator==(const trainSpec& o) const;
    bool operator!=(const trainSpec& o) const { return !(*this == o); }

    int rows() const { return lRowOffsets.size(); }
    int seatsPerRow() const { return lSeatOffsets.size(); }
    // count rows (seats), centred on the car's centre (centre line), the
    // stand-in distance apart (see "Stand-in values" in trainspec.cpp)
    void setRows(int count);
    void setSeats(int count);
    static float standInCarPitch();         // m, what fromType() takes when the catalogue has no car length
    float trainLength() const;              // iCars x fCarPitch (front of the first car to the back of the last)
    double carOffset(int car) const;        // metres the car's centre is ahead (+) or behind (-) the reference
    double carMass(int car) const;          // kg: (empty car + riders x load) x the car's factor
    double totalMass() const;
    bool isValid(QString* why = nullptr) const;

    // One car, one row, one seat at the heartline, mass at the design point,
    // resistance from the project: the program's own point.
    static trainSpec point();

    // What the catalogue entry has, and generic stand-ins for what it lacks
    // (see "Stand-in values" in trainspec.cpp). The name of every field that
    // got a stand-in is appended to *assumed, so the windows can say "assumed".
    static trainSpec fromType(const trainType& type, QStringList* assumed = nullptr);
};

#endif // TRAINSPEC_H
