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

// Train types as data (data/traintypes.json). QtCore only, on purpose: the
// test in tests/traintypes builds this file on its own, and the future
// window-less core uses it too.

#ifndef TRAINTYPE_H
#define TRAINTYPE_H

#include <QString>
#include <QList>

// A number together with where it came from and how far it can be trusted.
struct sourcedValue
{
    sourcedValue() : bPresent(false), fValue(0.0f) {}

    bool bPresent;      // false when the file has null for this value
    float fValue;
    QString sSource;
    QString sConfidence; // "verified", "likely", "estimate" or "unverified"
};

// A list of numbers together with where it came from (schema 2): the same as
// sourcedValue, for row and seat offsets.
struct sourcedList
{
    sourcedList() : bPresent(false) {}

    bool bPresent;      // false when the file has null (or, schema 1, nothing) for this list
    QList<float> lValues;
    QString sSource;
    QString sConfidence;
};

class trainType
{
public:
    trainType() : iSeatsAcross(-1), iRowsPerCar(-1), iCarsPerTrain(-1) {}

    QString sId;            // unique lower-case slug
    QString sName;          // shown to the user
    QString sManufacturer;  // empty when not known
    QString sNl2Name;       // exact NoLimits 2 train name, or empty
    sourcedValue heartline; // metres; the number that goes into track::fHeart (rails to heartline, as
                            // NoLimits 2 shows it), negative = train hangs below. Datum: docs/research/r1-heartlines-and-trains.md
    int iSeatsAcross;       // -1 when not known
    int iRowsPerCar;        // -1 when not known
    int iCarsPerTrain;      // -1 when not known
    sourcedValue carLength; // metres
    // Schema 2 (all null until Stage 4; a schema 1 file has none of them):
    sourcedValue carPitch;  // metres from one car's centre to the next, along the track (before carLength)
    sourcedList rowOffsets; // metres along the car from its centre, + toward the front, front row first
    sourcedList seatOffsets;// metres across from the car's centre line, + toward the rider's right, left seat first
    sourcedValue friction;  // the share of the weight lost to rolling friction (the meaning of track::fFriction)
    sourcedValue drag;      // 1/m, the air resistance (the meaning of track::fResistance)
    QString sRestraint;     // "lapbar", "otsr", "vest", "prone", "other" or empty (the words of
                            // trainSpec::sRestraint; the catalogue uses none of "prone" yet)
    sourcedValue minRadiusVertical;   // metres
    sourcedValue minRadiusHorizontal; // metres
    QString sNotes;
};

// Loads the train catalogue (data/traintypes.json) into trainType entries. Static
// functions only; the default catalogue is loaded once and kept. QtCore only, so
// tests/traintypes builds it on its own. Holds data; it changes no track.
class trainCatalogue
{
public:
    // Path of the catalogue that ships inside the program.
    static QString defaultPath();

    // Reads and checks a catalogue file (a file path or a Qt resource path).
    // Returns the trains in file order. On the first problem it returns an
    // empty list and, if error is not null, says what is wrong there.
    static QList<trainType> load(const QString& path = defaultPath(), QString* error = nullptr);

    // The catalogue that ships inside the program, loaded once and kept.
    // Empty (and logged with qWarning) if it cannot be read.
    static const QList<trainType>& defaultCatalogue();

    // The train with this id, or nullptr.
    static const trainType* findById(const QList<trainType>& trains, const QString& id);
};

#endif // TRAINTYPE_H
