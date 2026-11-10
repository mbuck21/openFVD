#ifndef ELEMENTFILE_H
#define ELEMENTFILE_H

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

#include <QString>

class track;

// The NoLimits element files written by the export dialog and by batch mode:
// the file's header and footer around the track's own export. fromIndex and
// toIndex are handed to the track's exporter as they are (the callers pass
// the first section and the first plus the last section). Each returns false
// when the file could not be opened or written.

// NoLimits 2 (.nl2elem): XML around track::exportNL2Track().
bool writeNL2Element(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex);

// NoLimits 1 (.nlelem), "Tangent" (track::exportTrack4()) and "Spline"
// (track::exportTrack3()). fRollThresh is the sine of the relative roll
// threshold; with bNoHeartline the track's heartline is set to 0 for the
// export and put back afterwards.
bool writeNL1TangentElement(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex, float fRollThresh, bool bNoHeartline);
bool writeNL1SplineElement(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex, float fRollThresh, bool bNoHeartline);

#endif // ELEMENTFILE_H
