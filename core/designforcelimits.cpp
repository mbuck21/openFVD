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

#include "designforcelimits.h"
#include "mnode.h"
#include "ridesummary.h"
#include <QRegExp>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>

// Like measure.cpp, nothing here writes to a node or a track.

QString designLimitsInfo::label() const
{
    QString standard = sStandard.isEmpty() ? QStringLiteral("ASTM F2291") : sStandard;
    QString what;
    if(iNumbers > 0 && iChecked >= iNumbers) what = QStringLiteral("numbers checked by the owner");
    else if(iChecked > 0) what = QStringLiteral("%1 of %2 numbers checked").arg(iChecked).arg(iNumbers);
    else what = QStringLiteral("public numbers");
    return QStringLiteral("Force limits (%1, %2)").arg(standard, what);
}

QString designLimitsInfo::value() const
{
    if(!bRan) {
        return sError.isEmpty() ? QStringLiteral("not judged") : QStringLiteral("not judged (%1)").arg(sError);
    }
    if(iFindings == 0) return QStringLiteral("within");
    return iFindings == 1 ? QStringLiteral("1 finding") : QStringLiteral("%1 findings").arg(iFindings);
}

limitOptions designPointLimitOptions()
{
    // The base case; the train piece replaces this (see the header).
    limitOptions o;
    o.sRestraint = QStringLiteral("base");
    o.bExtendedNegZ = false;
    o.bFilter = true;
    return o;
}

forceTrace designPointTrace(const QVector<double>& times, const QVector<float>& along,
                            const QVector<float>& lateral, const QVector<float>& normal, int lastNode)
{
    forceTrace t;
    t.dt = 1.0/(double)F_HZ;
    if(lastNode < 1 || lastNode >= times.size() || along.size() <= lastNode
            || lateral.size() <= lastNode || normal.size() <= lastNode) {
        return t;
    }
    const double end = times.at(lastNode);
    const int count = (int)std::floor(end/t.dt + 1e-6) + 1;
    t.gx.resize(count);
    t.gy.resize(count);
    t.gz.resize(count);
    int j = 0;
    for(int s = 0; s < count; ++s) {
        const double ts = s*t.dt;
        while(j < lastNode - 1 && times.at(j + 1) < ts) ++j;
        const double t0 = times.at(j);
        const double span = times.at(j + 1) - t0;
        double w = span > 0.0 ? (ts - t0)/span : 0.0;
        if(w < 0.0) w = 0.0;
        if(w > 1.0) w = 1.0;
        t.gx[s] = (double)along.at(j) + ((double)along.at(j + 1) - (double)along.at(j))*w;
        t.gy[s] = (double)lateral.at(j) + ((double)lateral.at(j + 1) - (double)lateral.at(j))*w;
        t.gz[s] = (double)normal.at(j) + ((double)normal.at(j + 1) - (double)normal.at(j))*w;
    }
    return t;
}

// The node whose time is nearest to ts, among nodes 0 to lastNode.
static int nearestNode(const QVector<double>& times, int lastNode, double ts)
{
    QVector<double>::const_iterator b = times.constBegin();
    int i = (int)(std::lower_bound(b, b + lastNode + 1, ts) - b);
    if(i > lastNode) return lastNode;
    if(i > 0 && ts - times.at(i - 1) < times.at(i) - ts) return i - 1;
    return i;
}

// The measure a rule's finding is shown against: the axis of the rule; a pair
// of axes shows against the vertical force if it holds z, else the lateral.
static QString measureOf(const limitFinding& f)
{
    const QString a = f.sAxis;
    if(a.contains('z')) return QStringLiteral("forceNormal");
    if(a.contains('y')) return QStringLiteral("forceLateral");
    return QStringLiteral("forceAlong");
}

static QString fmt(double v, int decimals = 2)
{
    QString t = QString::number(v, 'f', decimals);
    if(t.startsWith('-') && t.indexOf(QRegExp("[1-9]")) < 0) t.remove(0, 1);
    return t;
}

// The sentence of a finding, built from the engine finding's fields. The times
// of the stretch are the plot's (node / F_HZ, as every finding says them, core/finding.h);
// the duration is the ride time the engine judged, and the sentence says "of ride time"
// where the two clocks differ on this stretch.
static QString sentenceOf(const limitFinding& lf, const finding& f, const forceLimits& set, bool clocksDiffer)
{
    // (the stretch is the engine's whole group of events; the duration is that of the worst event in it,
    // for which the engine gives no first and last sample of its own)
    const QString span = QStringLiteral("in the stretch from %1 s to %2 s").arg(fmt(f.fromNode/F_HZ), fmt(f.toNode/F_HZ));
    const QString dur = fmt(lf.dDuration) + (clocksDiffer ? QStringLiteral(" s of ride time") : QStringLiteral(" s"));
    const QString conf = QStringLiteral("numbers of %1 confidence").arg(lf.sConfidence);
    const limitCurve* c = (lf.iFamily == limitFamilyEgg || lf.iFamily == limitFamilyReversal) ? 0 : set.curve(lf.sRule);
    if(c) {
        return QStringLiteral("Force limit, %1: %2 g or beyond lasts %3 (%4); the limit for that long is %5 g, a share of %6 (%7).")
                .arg(c->sName, fmt(lf.dLevel, 1), dur, span, fmt(lf.dLimit), fmt(lf.dUtilisation), conf);
    }
    if(lf.iFamily == limitFamilyEgg) {
        return QStringLiteral("Force limit, %1 and %2 together: %3 times the combined limit for %4 (%5, at the worst moment); %6.")
                .arg(lf.sAxis.mid(0, 1), lf.sAxis.mid(1, 1), fmt(lf.dUtilisation), dur, span, conf);
    }
    if(lf.iFamily == limitFamilyReversal) {
        return QStringLiteral("Force limit, fast reversal in %1: the peaks are only %2 apart (%3); the peak of %4 g may reach only %5 g, a share of %6 (%7).")
                .arg(lf.sAxis, dur, span, fmt(lf.dLevel, 1), fmt(lf.dLimit), fmt(lf.dUtilisation), conf);
    }
    // weightless into positive z: the rise time against the shortest allowed one
    return QStringLiteral("Force limit, from weightlessness into positive z (%1): z rises in %2; it must take at least %3 s, a share of %4 (%5).")
            .arg(span, dur, fmt(lf.dLimit, 3), fmt(lf.dUtilisation), conf);
}

QList<finding> designPointForceLimits(const QVector<double>& times, const QVector<float>& along,
                                      const QVector<float>& lateral, const QVector<float>& normal,
                                      designLimitsInfo* info, const forceLimits* limits)
{
    QList<finding> result;
    designLimitsInfo local;
    designLimitsInfo& inf = info ? *info : local;
    inf = designLimitsInfo();
    inf.iNodes = times.size();
    if(times.size() < 2 || along.size() != times.size() || lateral.size() != times.size()
            || normal.size() != times.size()) {
        return result;      // no track: nothing to judge
    }

    // judged up to the last node before the first value that is not a number
    int last = times.size() - 1;
    for(int i = 0; i < times.size(); ++i) {
        if(!std::isfinite(along.at(i)) || !std::isfinite(lateral.at(i)) || !std::isfinite(normal.at(i))
                || !std::isfinite(times.at(i))) {
            last = i - 1;
            break;
        }
    }
    inf.iJudgedNodes = last + 1;
    if(last < 1) {
        inf.sError = QStringLiteral("the forces are not numbers from the start");
        return result;
    }

    const forceLimits& set = limits ? *limits : forceLimits::defaultSet();
    if(!set.isValid()) {
        inf.sError = QStringLiteral("the limit file could not be read");
        return result;
    }
    inf.sStandard = set.sStandard;
    inf.sDisclaimer = set.sDisclaimer;
    // the whole file, every time: what the label and the note may say
    inf.iNumbers = set.lNumbers.size();
    for(int i = 0; i < set.lNumbers.size(); ++i) {
        const limitListed& n = set.lNumbers.at(i);
        const bool checked = !n.sChecked.isEmpty();
        if(checked) ++inf.iChecked;
        inf.sFileConfidence = forceLimits::lowerConfidence(inf.sFileConfidence, checked ? QStringLiteral("checked") : n.sConfidence);
    }

    const forceTrace trace = designPointTrace(times, along, lateral, normal, last);
    const limitResult r = checkForceLimits(trace, set, designPointLimitOptions());
    if(!r.bValid) {
        inf.sError = r.sError;
        return result;
    }
    inf.bRan = true;

    QString confidence;     // of the numbers the findings used
    for(int k = 0; k < r.lFindings.size(); ++k) {
        const limitFinding& lf = r.lFindings.at(k);
        finding f;
        f.checkId = QStringLiteral("forceLimits");
        f.measureId = measureOf(lf);
        f.fromNode = nearestNode(times, last, lf.iFirst*trace.dt);
        f.toNode = nearestNode(times, last, lf.iLast*trace.dt);
        if(f.toNode < f.fromNode) f.toNode = f.fromNode;
        f.worstNode = nearestNode(times, last, lf.iWorstSample*trace.dt);
        if(f.worstNode < f.fromNode) f.worstNode = f.fromNode;
        if(f.worstNode > f.toNode) f.worstNode = f.toNode;
        const QVector<float>& v = f.measureId == QLatin1String("forceNormal") ? normal
                                : (f.measureId == QLatin1String("forceLateral") ? lateral : along);
        f.worstValue = v.at(f.worstNode);
        f.below = lf.iDirection < 0;
        f.nonFinite = false;
        f.severity = findingWarning;
        f.rule = lf.sRule;
        f.judgedValue = (float)lf.dLevel;
        f.judgedLimit = (float)lf.dLimit;
        // the unit of the pair the engine judged: g for a curve and a reversal, the share of
        // the combined limit for an egg, seconds for the weightless-into-positive-z rule
        if(lf.iFamily == limitFamilyEgg) f.judgedUnit = QStringLiteral("ratio");
        else if(lf.sRule == QLatin1String("weightless_to_pos_z")) f.judgedUnit = QStringLiteral("s");
        else f.judgedUnit = QStringLiteral("g");
        f.limitComparable = f.judgedUnit == QLatin1String("g");
        f.limit = f.limitComparable ? (float)lf.dLimit : std::numeric_limits<float>::quiet_NaN();
        // the clocks differ here when a node's ride time is not node / F_HZ
        const bool differ = std::fabs(times.at(f.fromNode) - f.fromNode/(double)F_HZ) > 0.005
                || std::fabs(times.at(f.toNode) - f.toNode/(double)F_HZ) > 0.005;
        f.text = sentenceOf(lf, f, set, differ);
        result.append(f);
        confidence = forceLimits::lowerConfidence(confidence, lf.sConfidence);
    }
    inf.iFindings = result.size();
    inf.sConfidence = confidence;
    return result;
}

QList<finding> designPointForceLimits(const track* t, designLimitsInfo* info)
{
    const measureNodes n = measureRegistry::collect(t);
    if(n.size() < 2) {
        if(info) *info = designLimitsInfo();
        return QList<finding>();
    }
    QVector<QVector<float> > m = measureRegistry::values(n, QStringList() << "forceAlong" << "forceLateral" << "forceNormal");
    if(m.size() != 3) {
        if(info) *info = designLimitsInfo();
        return QList<finding>();
    }
    return designPointForceLimits(rideNodeTimes(n), m.at(0), m.at(1), m.at(2), info);
}
