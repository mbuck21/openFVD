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

#include "trainspec.h"
#include "traintype.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Stand-in values: what fromType() takes when the catalogue has nothing. All
// of them are assumptions, not data: no manufacturer publishes car lengths,
// row spacings or masses (docs/research/r1-heartlines-and-trains.md, R7
// section 2.3). They are kept here, in one place, so that later research only
// changes these lines.
// ---------------------------------------------------------------------------
static const int   kStandInCars = 6;            // cars per train when the catalogue has no count
static const float kStandInCarPitch = 2.5f;     // m, centre of one car to the next (R7's "short" train)
static const int   kStandInRows = 2;            // rows per car when the catalogue has no count
static const float kStandInRowSpacing = 0.9f;   // m between rows, the rows centred in the car
static const int   kStandInSeats = 2;           // seats per row when the catalogue has no count
static const float kStandInSeatSpacing = 0.55f; // m between seats, the seats centred on the track
static const float kStandInCarMass = 700.f;     // kg, an empty car
static const float kStandInRiderMass = 75.f;    // kg per rider
static const float kStandInLoad = 1.f;          // every seat taken

bool rideResistance::operator==(const rideResistance& o) const
{
    return bFromProject == o.bFromProject && fRolling == o.fRolling && fRollingLoad == o.fRollingLoad
            && fLinear == o.fLinear && fDrag == o.fDrag;
}

// The program's own point: one car, one row, one seat at the heartline, its
// mass at the design point, the project's resistance. The car mass values
// cancel for a single car; they are the stand-ins so that the spec is valid.
trainSpec::trainSpec()
    : iCars(1), fCarPitch(0.f), fSeatUp(0.f), fCarMass(kStandInCarMass), fRiderMass(kStandInRiderMass),
      fLoad(kStandInLoad), bMassAtDesignPoint(true), fMassAboveRail(0.f), iReferenceCar(-1)
{
    lRowOffsets << 0.f;
    lSeatOffsets << 0.f;
}

bool trainSpec::operator==(const trainSpec& o) const
{
    return sTypeId == o.sTypeId && iCars == o.iCars && fCarPitch == o.fCarPitch
            && lRowOffsets == o.lRowOffsets && lSeatOffsets == o.lSeatOffsets && fSeatUp == o.fSeatUp
            && fCarMass == o.fCarMass && fRiderMass == o.fRiderMass && fLoad == o.fLoad
            && lCarMassFactor == o.lCarMassFactor && bMassAtDesignPoint == o.bMassAtDesignPoint
            && fMassAboveRail == o.fMassAboveRail && resistance == o.resistance
            && iReferenceCar == o.iReferenceCar && sRestraint == o.sRestraint;
}

float trainSpec::trainLength() const
{
    return iCars*fCarPitch;
}

double trainSpec::carOffset(int car) const
{
    // car centres relative to the train's centre, front car first (as R7's ride.py)
    double fromCentre = (0.5*(iCars - 1) - car)*(double)fCarPitch;
    if(iReferenceCar < 0 || iReferenceCar >= iCars) return fromCentre;
    double refFromCentre = (0.5*(iCars - 1) - iReferenceCar)*(double)fCarPitch;
    return fromCentre - refFromCentre;
}

double trainSpec::carMass(int car) const
{
    double riders = (double)rows()*seatsPerRow()*fLoad;
    double m = fCarMass + riders*fRiderMass;
    if(car >= 0 && car < lCarMassFactor.size()) m *= lCarMassFactor.at(car);
    return m;
}

double trainSpec::totalMass() const
{
    double m = 0.0;
    for(int i = 0; i < iCars; ++i) m += carMass(i);
    return m;
}

static bool fin(float v) { return std::isfinite(v); }

bool trainSpec::isValid(QString* why) const
{
    QString w;
    if(iCars < 1) w = "the train needs at least one car";
    else if(!fin(fCarPitch) || fCarPitch < 0.f || (iCars > 1 && fCarPitch <= 0.f)) w = "the car pitch must be above 0 m";
    else if(lRowOffsets.isEmpty()) w = "a car needs at least one row";
    else if(lSeatOffsets.isEmpty()) w = "a row needs at least one seat";
    else if(!fin(fSeatUp)) w = "the seat height is not a number";
    else if(!fin(fCarMass) || fCarMass < 0.f) w = "the car mass must be 0 kg or more";
    else if(!fin(fRiderMass) || fRiderMass < 0.f) w = "the rider mass must be 0 kg or more";
    else if(!fin(fLoad) || fLoad < 0.f || fLoad > 1.f) w = "the load must be between 0 and 1";
    else if(!lCarMassFactor.isEmpty() && lCarMassFactor.size() != iCars) w = "the mass factors must be one per car (or none)";
    else if(!fin(fMassAboveRail)) w = "the mass height is not a number";
    else if(iReferenceCar < -1 || iReferenceCar >= iCars) w = "the reference car is not in the train";
    else if(!resistance.bFromProject && (!fin(resistance.fRolling) || !fin(resistance.fRollingLoad) || !fin(resistance.fLinear)
                                         || !fin(resistance.fDrag) || resistance.fRolling < 0.f || resistance.fRollingLoad < 0.f
                                         || resistance.fLinear < 0.f || resistance.fDrag < 0.f))
        w = "the resistance terms must be 0 or more";
    if(w.isEmpty()) {
        for(int i = 0; i < lRowOffsets.size(); ++i) if(!fin(lRowOffsets.at(i))) w = "a row offset is not a number";
        for(int i = 0; i < lSeatOffsets.size(); ++i) if(!fin(lSeatOffsets.at(i))) w = "a seat offset is not a number";
        for(int i = 0; i < lCarMassFactor.size(); ++i)
            if(!fin(lCarMassFactor.at(i)) || lCarMassFactor.at(i) <= 0.f) w = "a mass factor must be above 0";
    }
    if(w.isEmpty() && !(totalMass() > 0.0)) w = "the train has no mass";
    if(why) *why = w;
    return w.isEmpty();
}

trainSpec trainSpec::point()
{
    return trainSpec();
}

float trainSpec::standInCarPitch()
{
    return kStandInCarPitch;
}

// front row first (+ toward the front), the rows centred on the car's centre
void trainSpec::setRows(int count)
{
    setRows(count, kStandInRowSpacing);
}

void trainSpec::setRows(int count, float spacing)
{
    lRowOffsets.clear();
    for(int r = 0; r < count; ++r) lRowOffsets << (0.5f*(count - 1) - r)*spacing;
}

// left to right (+ toward vLat, the rider's right), centred on the track
void trainSpec::setSeats(int count)
{
    setSeats(count, kStandInSeatSpacing);
}

void trainSpec::setSeats(int count, float spacing)
{
    lSeatOffsets.clear();
    for(int s = 0; s < count; ++s) lSeatOffsets << (s - 0.5f*(count - 1))*spacing;
}

trainSpec trainSpec::fromType(const trainType& type, QStringList* assumed)
{
    QStringList a;
    trainSpec t = point();
    t.sTypeId = type.sId;
    t.sRestraint = type.sRestraint;
    if(type.iCarsPerTrain > 0) t.iCars = type.iCarsPerTrain;
    else { t.iCars = kStandInCars; a << "iCars"; }
    // the car pitch (schema 2) first, else the car length, else the stand-in
    if(type.carPitch.bPresent && type.carPitch.fValue > 0.f) t.fCarPitch = type.carPitch.fValue;
    else if(type.carLength.bPresent && type.carLength.fValue > 0.f) t.fCarPitch = type.carLength.fValue;
    else { t.fCarPitch = kStandInCarPitch; a << "fCarPitch"; }
    // rows and seats: their places from the catalogue (schema 2) when it has
    // them, else the counts if known, placed the stand-in distance apart
    if(type.rowOffsets.bPresent && !type.rowOffsets.lValues.isEmpty()) {
        t.lRowOffsets = type.rowOffsets.lValues;
    } else {
        int rows = type.iRowsPerCar > 0 ? type.iRowsPerCar : kStandInRows;
        if(type.iRowsPerCar <= 0) a << "rowCount";
        t.setRows(rows);
        a << "lRowOffsets";
    }
    if(type.seatOffsets.bPresent && !type.seatOffsets.lValues.isEmpty()) {
        t.lSeatOffsets = type.seatOffsets.lValues;
    } else {
        int seats = type.iSeatsAcross > 0 ? type.iSeatsAcross : kStandInSeats;
        if(type.iSeatsAcross <= 0) a << "seatCount";
        t.setSeats(seats);
        a << "lSeatOffsets";
    }
    // friction and drag (schema 2): only both together replace the project's
    // own two numbers, so that the ride never mixes a type's friction with a
    // project's drag
    if(type.friction.bPresent && type.drag.bPresent) {
        t.resistance.bFromProject = false;
        t.resistance.fRolling = type.friction.fValue;
        t.resistance.fDrag = type.drag.fValue;
    }
    t.fCarMass = kStandInCarMass;
    t.fRiderMass = kStandInRiderMass;
    t.fLoad = kStandInLoad;
    a << "fCarMass" << "fRiderMass" << "fLoad";
    if(assumed) *assumed << a;
    return t;
}
