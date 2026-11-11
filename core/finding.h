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

#ifndef FINDING_H
#define FINDING_H

// Findings and checks (docs/decisions/0012). A finding is one stretch of track
// where a measure (core/measure.h) breaks a limit; a check is a function from a
// track to its findings. Node indices are global node indices as in measure.h;
// times in the sentences are node index / F_HZ seconds, as the plot shows.
//
// Adding a check: one function (usually a few calls of findOutside()) and one
// line in checkRegistry::all().

#include <QString>
#include <QList>
#include <QVector>

class track;

enum findingSeverity
{
    findingInformation = 0,
    findingWarning,
    findingProblem
};

struct finding
{
    QString checkId;        // "basic"
    QString measureId;      // "speed"
    int fromNode;           // first node of the stretch (global index)
    int toNode;             // last node of the stretch, inclusive
    float worstValue;       // the value furthest past the limit
    int worstNode;          // where it is
    float limit;            // the limit that was broken
    bool below;             // true: the value fell below limit; false: rose above it
    findingSeverity severity;
    QString text;           // "Speed falls to 2.4 m/s, below 3 m/s, between 41.2 s and 43.0 s"
};

typedef QList<finding> (*checkRun)(const track* t);

struct checkDef
{
    QString id;
    QString name;
    QString description;
    checkRun run;
};

class checkRegistry
{
public:
    static const QList<checkDef>& all();
    static const checkDef* find(const QString& id);

    // The findings of one check, or of every check, in node order per check.
    static QList<finding> run(const track* t, const QString& id);
    static QList<finding> runAll(const track* t);
};

// Default for findOutside(): stretches separated by fewer nodes than this
// (100 nodes, 0.1 s) are merged into one, so a value hovering at a limit gives
// one finding and not dozens.
#define FINDING_MERGE_GAP 100

// The stretches where values (one per node, as measureRegistry::values gives)
// fall below lo or rise above hi, each side on its own, merged across gaps of
// fewer than mergeGap nodes. A finding gets the measure's name and unit from
// the registry for its sentence. Pass lo = -INFINITY or hi = INFINITY to check
// one side only.
QList<finding> findOutside(const QVector<float>& values, const QString& checkId, const QString& measureId,
                           float lo, float hi, findingSeverity severity, int mergeGap = FINDING_MERGE_GAP);

// The plain check "basic": the limits of batchScenarios::trackStats()
// (ui/batchscenarios.cpp). Its numbers can be changed for tests.
struct basicCheckLimits
{
    float minSpeed;         // m/s, default 3
    float minNormal;        // g, default -1.5
    float maxNormal;        // g, default +5
    basicCheckLimits() : minSpeed(3.f), minNormal(-1.5f), maxNormal(5.f) {}
};

QList<finding> basicCheck(const track* t, const basicCheckLimits& limits);

#endif // FINDING_H
