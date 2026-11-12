#ifndef NOLIMITSIMPORTER_H
#define NOLIMITSIMPORTER_H

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

#include "section.h"
#include <QByteArray>
#include <QString>

class track;

class noLimitsImporter
{
public:
    noLimitsImporter(track* _track, QString _fileName);

    // Each reads the file into a new bezier section (or, for the CSV, a
    // NoLimits 2 CSV section) of the track, with the heartline at 0, and
    // computes the track. False when the file cannot be used; error() then
    // says why in plain words. A file that is refused is checked before the
    // track is touched, except that the point list is read as it is checked.
    bool importAsNlTrack();
    bool importAsTxt();
    bool importAsCsv();

    QString error() const { return mError; }

private:
    bool checkNlTrack(const QByteArray& data);
    bool fail(const QString& text);

    track* inTrack;
    QString fileName;
    QString mError;
};

#endif // NOLIMITSIMPORTER_H
