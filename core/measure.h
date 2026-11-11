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

#ifndef MEASURE_H
#define MEASURE_H

// Measures: a number at every node of a track (speed, height, the felt forces,
// their rates of change, roll rate ...), computed beside the nodes and never
// written into them (docs/decisions/0012). The windows, the command line and
// the ride sheet read them from here and hold no measure maths of their own.
//
// Node index: the track's global node index, the one track::getPoint() takes,
// from 0 to track::getNumPoints() inclusive, so a track has getNumPoints()+1
// values. A section's first node repeats the previous section's last and is
// counted once, as part of the earlier section (as getPoint does). A track
// without sections has no values. Time, where a measure needs it, is the
// program's own: node index / F_HZ seconds, as the plot uses.
//
// Adding a measure: one fill function in measure.cpp and one line in the table
// in measureRegistry::all().

#include <QString>
#include <QStringList>
#include <QVector>

class track;
class section;
class mnode;

// The nodes of one track in global order, gathered once so that several
// measures can be computed in one pass. Pointers into the track: valid until
// the track changes. Read-only by contract: nothing here or in a fill function
// writes to a node.
struct measureNodes
{
    const track* trk;
    QVector<mnode*> nodes;          // index = global node index
    QVector<int> sectionOf;         // the section each node belongs to
    QVector<int> sectionFirst;      // per section: global index of its first node
    QVector<int> sectionLast;       // per section: global index of its last node
    QVector<double> heartDist;      // heartline distance from node 0, metres (summed in double)

    int size() const { return nodes.size(); }
    const section* sectionAt(int index) const;
};

// Fills out (already sized to n.size()) with one value per node.
typedef void (*measureFill)(const measureNodes& n, QVector<float>& out);

struct measureDef
{
    QString id;             // stable name for code, files and the command line: "forceAlong"
    QString name;           // shown to the owner: "Along-track force"
    QString unit;           // "g", "m/s", "deg/s" ...
    QString description;    // one sentence in plain words
    float displayMin;       // suggested range for colouring and plot axes
    float displayMax;
    measureFill fill;
};

class measureRegistry
{
public:
    // Every measure, in display order.
    static const QVector<measureDef>& all();

    // The measure with this id, or NULL.
    static const measureDef* find(const QString& id);

    // Gathers a track's nodes once (see measureNodes).
    static measureNodes collect(const track* t);

    // One measure: a value per global node index. Empty for an unknown id or
    // a track without sections.
    static QVector<float> values(const track* t, const QString& id);

    // Several measures in one pass over the nodes, in the order of ids
    // (an unknown id gives an empty vector in its place).
    static QVector<QVector<float> > values(const track* t, const QStringList& ids);
    static QVector<QVector<float> > values(const measureNodes& n, const QStringList& ids);
};

// The node span of the rates of change (jerk, roll acceleration): the value
// at node j is (x[j] - x[j-20]) * F_HZ / 20, which is what the plot's
// "force change" and "roll acceleration" curves show for the time argument
// (ui/graphhandler.cpp), to the bit. Like the plot, nodes 0 to 19 compare
// the node with itself and get 0 (or a float rounding residue).
#define MEASURE_RATE_SPAN 20

// The along-track force's differencing half-span in nodes (see measure.cpp).
#define MEASURE_ALONG_HALFSPAN 10

// The along-track force with a chosen half-span; the measure uses
// MEASURE_ALONG_HALFSPAN. For tests that measure the float noise.
void measureForceAlong(const measureNodes& n, QVector<float>& out, int halfSpan);

// Whether a section holds its speed (bSpeed off on a straight, curved, force
// or geometric section): the along-track force there is just vDir.y.
bool measureSpeedIsHeld(const section* s);

#endif // MEASURE_H
