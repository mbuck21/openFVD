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

#include "finding.h"
#include "measure.h"
#include "mnode.h"
#include <QStringList>
#include <cmath>

// Like measure.cpp, nothing here writes to a node or a track.

static QString valueText(float v, const QString& unit)
{
    int decimals = (unit == "g") ? 2 : 1;
    return QString::number(v, 'f', decimals) + " " + unit;
}

static QString sentence(const finding& f)
{
    const measureDef* d = measureRegistry::find(f.measureId);
    QString name = d ? d->name : f.measureId;
    QString unit = d ? d->unit : QString();
    QString where;
    if(f.fromNode == f.toNode) {
        where = QString("at %1 s").arg(f.fromNode/F_HZ, 0, 'f', 1);
    } else {
        where = QString("between %1 s and %2 s").arg(f.fromNode/F_HZ, 0, 'f', 1).arg(f.toNode/F_HZ, 0, 'f', 1);
    }
    if(f.nonFinite) {
        // "Speed is not a number (the train stalls) between 3.1 s and 9.4 s"
        return QString("%1 is not a number%2 %3")
                .arg(name)
                .arg(f.measureId == "speed" ? " (the train stalls)" : "")
                .arg(where);
    }
    return QString("%1 %2 %3, %4 %5, %6")
            .arg(name)
            .arg(f.below ? "falls to" : "rises to")
            .arg(valueText(f.worstValue, unit))
            .arg(f.below ? "below" : "above")
            .arg(QString::number(f.limit) + " " + unit)
            .arg(where);
}

// Whether v is outside on this side. A value that is not a finite number
// (NaN or infinite) is outside when countNonFinite is set.
static bool isOutside(float v, float limit, bool below, bool countNonFinite)
{
    if(!std::isfinite(v)) return countNonFinite;
    return below ? (v < limit) : (v > limit);
}

// The stretches on one side of one limit.
static void stretches(QList<finding>& out, const QVector<float>& values, const QString& checkId,
                      const QString& measureId, float limit, bool below, bool countNonFinite,
                      findingSeverity severity, int mergeGap)
{
    if(!std::isfinite(limit)) return;
    int n = values.size();
    int i = 0;
    while(i < n) {
        float v = values.at(i);
        if(!isOutside(v, limit, below, countNonFinite)) { ++i; continue; }

        finding f;
        f.checkId = checkId;
        f.measureId = measureId;
        f.fromNode = i;
        f.toNode = i;
        f.worstValue = v;
        f.worstNode = i;
        f.limit = limit;
        f.below = below;
        f.nonFinite = !std::isfinite(v);
        f.severity = severity;

        int lastOutside = i;
        int j = i + 1;
        while(j < n && j - lastOutside <= mergeGap) {
            float w = values.at(j);
            if(isOutside(w, limit, below, countNonFinite)) {
                lastOutside = j;
                // the first non-finite value is the worst; among finite
                // values, the one furthest past the limit
                if(!std::isfinite(w)) {
                    if(!f.nonFinite) {
                        f.nonFinite = true;
                        f.worstValue = w;
                        f.worstNode = j;
                    }
                } else if(!f.nonFinite && (below ? (w < f.worstValue) : (w > f.worstValue))) {
                    f.worstValue = w;
                    f.worstNode = j;
                }
            }
            ++j;
        }
        f.toNode = lastOutside;
        f.text = sentence(f);
        out.append(f);
        i = lastOutside + 1;
    }
}

QList<finding> findOutside(const QVector<float>& values, const QString& checkId, const QString& measureId,
                           float lo, float hi, findingSeverity severity, int mergeGap)
{
    if(mergeGap < 1) mergeGap = 1;
    QList<finding> low, high;
    // a non-finite value is reported once: with the lower limit if that is
    // checked, else with the upper one
    bool lowChecked = std::isfinite(lo);
    stretches(low, values, checkId, measureId, lo, true, lowChecked, severity, mergeGap);
    stretches(high, values, checkId, measureId, hi, false, !lowChecked, severity, mergeGap);
    // one list in node order
    QList<finding> result;
    int a = 0, b = 0;
    while(a < low.size() || b < high.size()) {
        if(b >= high.size() || (a < low.size() && low.at(a).fromNode <= high.at(b).fromNode)) {
            result.append(low.at(a++));
        } else {
            result.append(high.at(b++));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// The checks
// ---------------------------------------------------------------------------

QList<finding> basicCheck(const track* t, const basicCheckLimits& limits)
{
    QVector<QVector<float> > v = measureRegistry::values(t, QStringList() << "speed" << "forceNormal");
    QList<finding> result;
    result << findOutside(v.at(0), "basic", "speed", limits.minSpeed, INFINITY, findingProblem);
    result << findOutside(v.at(1), "basic", "forceNormal", limits.minNormal, limits.maxNormal, findingProblem);
    return result;
}

static QList<finding> runBasic(const track* t)
{
    return basicCheck(t, basicCheckLimits());
}

static checkDef check(const char* id, const char* name, const char* description, checkRun run)
{
    checkDef c;
    c.id = QString::fromLatin1(id);
    c.name = QString::fromLatin1(name);
    c.description = QString::fromLatin1(description);
    c.run = run;
    return c;
}

static QList<checkDef> buildChecks()
{
    QList<checkDef> table;
    table
        << check("basic", "Basic limits",
                 "The train keeps moving (speed at least 3 m/s) and the vertical force stays between -1.5 g and +5 g.", runBasic);
    return table;
}

const QList<checkDef>& checkRegistry::all()
{
    // built once, on first use (thread-safe in C++11)
    static const QList<checkDef> table = buildChecks();
    return table;
}

const checkDef* checkRegistry::find(const QString& id)
{
    const QList<checkDef>& table = all();
    for(int i = 0; i < table.size(); ++i) {
        if(table.at(i).id == id) return &table.at(i);
    }
    return NULL;
}

QList<finding> checkRegistry::run(const track* t, const QString& id)
{
    const checkDef* c = find(id);
    if(!c) return QList<finding>();
    return c->run(t);
}

QList<finding> checkRegistry::runAll(const track* t)
{
    QList<finding> result;
    const QList<checkDef>& table = all();
    for(int i = 0; i < table.size(); ++i) {
        result << table.at(i).run(t);
    }
    return result;
}
