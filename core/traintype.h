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

class trainType
{
public:
    trainType() : iSeatsAcross(-1), iRowsPerCar(-1), iCarsPerTrain(-1) {}

    QString sId;            // unique lower-case slug
    QString sName;          // shown to the user
    QString sManufacturer;  // empty when not known
    QString sNl2Name;       // exact NoLimits 2 train name, or empty
    sourcedValue heartline; // metres above the track centre line, negative below
    int iSeatsAcross;       // -1 when not known
    int iRowsPerCar;        // -1 when not known
    int iCarsPerTrain;      // -1 when not known
    sourcedValue carLength; // metres
    QString sRestraint;     // "lapbar", "otsr", "vest", "other" or empty
    sourcedValue minRadiusVertical;   // metres
    sourcedValue minRadiusHorizontal; // metres
    QString sNotes;
};

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
