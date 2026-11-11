#ifndef PROJECTTEXT_H
#define PROJECTTEXT_H

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

// The project as text: a JSON document beside the binary .fvd, written from
// the loaded model and read back into it through the same calls a binary load
// uses, so that binary -> text -> binary gives the same bytes. The format is
// described in docs/project-text-format.md. QtCore's JSON classes only.

#include <QByteArray>
#include <QJsonValue>
#include <QList>
#include <QString>

#include "projectfile.h"

class track;

// Writes the project as text into out (UTF-8, ending in a newline). Calls
// client->projectTrackSaving() before each track, as writeProject() does; the
// client may be NULL. Returns "Project Saved!" or a text starting "Error: "
// that names the place (track 1, section 3, ...); out is left empty then.
// A freeform transition cannot be written (the binary file does not hold its
// control points either).
QString writeProjectText(QByteArray& out, const QString& texPath, const QList<track*>& tracks, projectFileClient* client);

// Reads a project from text into new tracks obtained from the client, which
// must not be NULL. The whole text is checked first: if anything is wrong the
// result is a text starting "Error: " naming the place, and the client is not
// called at all. Otherwise the client is called as readProject() calls it and
// the result is "Load Successful." or a text starting "Warning: " (an unknown
// key, a ground texture that could not be loaded).
QString readProjectText(const QByteArray& in, projectFileClient* client);

// The same, through the file fileName ("Error: File is NULL" when it cannot
// be opened).
QString saveProjectTextFile(const QString& fileName, const QString& texPath, const QList<track*>& tracks, projectFileClient* client);
QString loadProjectTextFile(const QString& fileName, projectFileClient* client);

namespace projectText {

// A 32-bit float as text: the shortest decimal (1 to 9 significant digits)
// that reads back to the same float, as a JSON number. Negative zero,
// infinities and NaN, which a JSON number cannot carry, become the strings
// "-0", "inf", "-inf" and "nan".
QJsonValue floatToJson(float f);

// The reverse; returns false when v is neither a number nor one of the four
// strings above.
bool floatFromJson(const QJsonValue& v, float* f);

// The number of floats between a and b (both finite): 0 when equal, 1 for
// neighbours, across zero as well.
qint64 floatSteps(float a, float b);

// How far a later transition's start may lie from the end of the one before,
// in float steps. The one rule both directions use: the writer refuses a model
// beyond it and the reader a text (docs/project-text-format.md, "A transition").
const qint64 FROM_TOLERANCE_STEPS = 64;
bool fromWithinTolerance(float from, float endBefore);

// Whether a transition's shape reads its shape parameter (subfunc::arg1):
// quadratic, quartic, quintic and plateau do. The others never set it either,
// so a new linear, cubic or sinusoidal piece keeps whatever the memory held,
// and the project file saves that. One rule both directions use: where the
// shape reads it, it must be finite; where it does not, any value is kept.
bool shapeUsesParameter(int shape);
bool shapeParameterAccepted(int shape, float value);

}

#endif // PROJECTTEXT_H
