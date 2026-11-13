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

#include "forcelimits.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QPair>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <vector>

static const int SCHEMA_VERSION = 1;

// Comparisons of computed durations and levels with the file's numbers allow
// this much, so that 20 samples of 0.01 s count as 0.2 s and a plateau at
// 4.0 g reaches the 4.0 g slice.
static const double EPS = 1e-9;
static const double PI = 3.14159265358979323846;

// ============================================================ the loader

namespace {

int confidenceRank(const QString& c)
{
    if (c == "checked") return 3;
    if (c == "high") return 2;
    if (c == "medium") return 1;
    if (c == "low") return 0;
    return -1;
}

QString numText(double v)
{
    return QString::number(v, 'g', 6);
}

// Reads the file's objects with the place of each kept for the messages.
class fileReader
{
public:
    fileReader(QString* error, QList<limitListed>* listed) : mError(error), mListed(listed) {}

    bool fail(const QString& place, const QString& what)
    {
        *mError = place + ": " + what;
        return false;
    }

    bool object(const QJsonObject& parent, const QString& key, const QString& place, QJsonObject& out)
    {
        const QJsonValue v = parent.value(key);
        if (!v.isObject())
            return fail(place + key, v.isUndefined() ? "is missing" : "must be an object");
        out = v.toObject();
        return true;
    }

    bool string(const QJsonObject& parent, const QString& key, const QString& place, QString& out, bool required = true)
    {
        const QJsonValue v = parent.value(key);
        if ((v.isUndefined() || v.isNull()) && !required)
        {
            out.clear();
            return true;
        }
        if (!v.isString())
            return fail(place + key, v.isUndefined() ? "is missing" : "must be a text");
        out = v.toString();
        return true;
    }

    bool integer(const QJsonObject& parent, const QString& key, const QString& place, int& out)
    {
        const QJsonValue v = parent.value(key);
        const double d = v.toDouble(-1.0);
        if (!v.isDouble() || d != std::floor(d) || d < 0.0 || d > 1000.0)
            return fail(place + key, v.isUndefined() ? "is missing" : "must be a whole number");
        out = (int)d;
        return true;
    }

    bool confidence(const QJsonObject& o, const QString& place, QString& out)
    {
        if (!string(o, "confidence", place, out))
            return false;
        if (out != "high" && out != "medium" && out != "low")
            return fail(place + "confidence", "must be high, medium or low, not \"" + out + "\"");
        return true;
    }

    bool checked(const QJsonObject& o, const QString& place, QString& out)
    {
        const QJsonValue v = o.value("checked");
        if (v.isUndefined() || v.isNull())
        {
            out.clear();
            return true;
        }
        if (!v.isString())
            return fail(place + "checked", "must be null or a text saying where the number was checked");
        out = v.toString().trimmed();
        return true;
    }

    bool uniqueId(const QString& id, const QString& place)
    {
        if (id.isEmpty())
            return fail(place + "id", "is empty");
        if (mIds.contains(id))
            return fail(place + "id", "\"" + id + "\" is used twice");
        mIds.append(id);
        return true;
    }

    // A number object {id, value, source, confidence, checked} at parent[key].
    bool number(const QJsonObject& parent, const QString& key, const QString& place, limitNumber& out, bool allowNull = false)
    {
        QJsonObject o;
        if (!object(parent, key, place, o))
            return false;
        const QString here = place + key + ".";
        out = limitNumber();
        if (!string(o, "id", here, out.sId) || !uniqueId(out.sId, here))
            return false;
        const QJsonValue v = o.value("value");
        if (v.isNull() && allowNull)
            out.bPresent = false;
        else if (!v.isDouble() || !std::isfinite(v.toDouble()))
            return fail(here + "value", v.isUndefined() ? "is missing" : "must be a number");
        else
        {
            out.bPresent = true;
            out.dValue = v.toDouble();
        }
        if (!string(o, "source", here, out.sSource) || !confidence(o, here, out.sConfidence)
         || !checked(o, here, out.sChecked))
            return false;
        limitListed row;
        row.sId = out.sId;
        row.sValue = out.bPresent ? numText(out.dValue) : QString("null (open)");
        row.sSource = out.sSource;
        row.sConfidence = out.sConfidence;
        row.sChecked = out.sChecked;
        mListed->append(row);
        return true;
    }

    // A number that must be present and above zero (and at most max when given).
    bool positive(const QJsonObject& parent, const QString& key, const QString& place, limitNumber& out, double max = -1.0)
    {
        if (!number(parent, key, place, out))
            return false;
        if (!(out.dValue > 0.0))
            return fail(place + key + ".value", "must be above zero");
        if (max > 0.0 && out.dValue > max)
            return fail(place + key + ".value", "must be at most " + numText(max));
        return true;
    }

    void listRow(const limitListed& row) { mListed->append(row); }

private:
    QString* mError;
    QList<limitListed>* mListed;
    QStringList mIds;
};

bool readFilter(fileReader& rd, const QJsonObject& root, const QString& key, int& order, limitNumber& corner)
{
    QJsonObject f;
    if (!rd.object(root, key, "", f))
        return false;
    const QString here = key + ".";
    QString kind;
    if (!rd.string(f, "kind", here, kind))
        return false;
    if (kind != "butterworth")
        return rd.fail(here + "kind", "must be butterworth");
    int passes = 0;
    if (!rd.integer(f, "order", here, order) || !rd.integer(f, "passes", here, passes))
        return false;
    if (order < 2 || order > 8 || order % 2 != 0)
        return rd.fail(here + "order", "must be 2, 4, 6 or 8");
    if (passes != 1)
        return rd.fail(here + "passes", "must be 1 (the standard's filter is single-pass)");
    return rd.positive(f, "corner_hz", here, corner);
}

bool readCurve(fileReader& rd, const QString& id, const QJsonObject& o, limitCurve& c)
{
    const QString here = "curves." + id + ".";
    c = limitCurve();
    c.sId = id;
    QString direction, after;
    if (!rd.string(o, "name", here, c.sName) || !rd.string(o, "axis", here, c.sAxis)
     || !rd.string(o, "direction", here, direction) || !rd.string(o, "after_last", here, after))
        return false;
    if (c.sAxis != "x" && c.sAxis != "y" && c.sAxis != "z")
        return rd.fail(here + "axis", "must be x, y or z");
    if (direction == "positive") c.iSign = 1;
    else if (direction == "negative") c.iSign = -1;
    else if (direction == "both") c.iSign = 0;
    else return rd.fail(here + "direction", "must be positive, negative or both");
    if (after == "then")
    {
        if (!rd.string(o, "then", here, c.sThen))
            return false;
        if (c.sThen.isEmpty() || c.sThen == id)
            return rd.fail(here + "then", "must name another curve");
    }
    else if (after != "hold")
        return rd.fail(here + "after_last", "must be hold or then");

    const QJsonValue pv = o.value("points");
    if (!pv.isArray() || pv.toArray().isEmpty())
        return rd.fail(here + "points", "must be a list of at least one point");
    const QJsonArray arr = pv.toArray();
    for (int i = 0; i < arr.size(); ++i)
    {
        const QString ph = QString("%1points[%2].").arg(here).arg(i);
        if (!arr.at(i).isObject())
            return rd.fail(ph.left(ph.size() - 1), "must be an object");
        const QJsonObject po = arr.at(i).toObject();
        limitPoint p;
        if (!rd.string(po, "id", ph, p.sId) || !rd.uniqueId(p.sId, ph))
            return false;
        const QJsonValue s = po.value("s"), g = po.value("g");
        if (!s.isDouble() || !std::isfinite(s.toDouble()) || !(s.toDouble() > 0.0))
            return rd.fail(ph + "s", s.isUndefined() ? "is missing" : "must be a time in seconds above zero");
        if (!g.isDouble() || !std::isfinite(g.toDouble()))
            return rd.fail(ph + "g", g.isUndefined() ? "is missing" : "must be a number");
        p.dTime = s.toDouble();
        p.dLimit = g.toDouble();
        if (c.iSign >= 0 && !(p.dLimit > 0.0))
            return rd.fail(ph + "g", "must be above zero on a curve whose direction is " + direction);
        if (c.iSign < 0 && !(p.dLimit < 0.0))
            return rd.fail(ph + "g", "must be below zero on a negative curve");
        if (!c.lPoints.isEmpty() && !(p.dTime > c.lPoints.last().dTime))
            return rd.fail(ph + "s", QString("must be later than the point before it (%1 s)").arg(numText(c.lPoints.last().dTime)));
        if (!rd.string(po, "source", ph, p.sSource) || !rd.confidence(po, ph, p.sConfidence)
         || !rd.checked(po, ph, p.sChecked))
            return false;
        c.lPoints.append(p);
        limitListed row;
        row.sId = p.sId;
        row.sValue = QString("%1 g at %2 s").arg(numText(p.dLimit), numText(p.dTime));
        row.sSource = p.sSource;
        row.sConfidence = p.sConfidence;
        row.sChecked = p.sChecked;
        rd.listRow(row);
    }
    // An open number beside the curve (listed, not used).
    if (o.contains("beyond_last"))
    {
        limitNumber open;
        if (!rd.number(o, "beyond_last", here, open, true))
            return false;
    }
    return true;
}

bool readRegion(fileReader& rd, const QJsonValue& v, int index, limitRegion& out)
{
    const QString here = QString("restraint_areas.regions[%1].").arg(index);
    if (!v.isObject())
        return rd.fail(here.left(here.size() - 1), "must be an object");
    const QJsonObject o = v.toObject();
    out = limitRegion();
    QString checkedNote;
    if (!rd.string(o, "id", here, out.sId) || !rd.uniqueId(out.sId, here) || !rd.integer(o, "area", here, out.iArea)
     || !rd.string(o, "class", here, out.sClass) || !rd.checked(o, here, checkedNote))
        return false;
    if (out.iArea != index + 1)
        return rd.fail(here + "area", QString("must be %1 (areas 1 to 4 in order)").arg(index + 1));
    const QJsonValue pv = o.value("points");
    if (!pv.isArray() || pv.toArray().size() < 3)
        return rd.fail(here + "points", "must be a list of at least three corners");
    const QJsonArray arr = pv.toArray();
    for (int i = 0; i < arr.size(); ++i)
    {
        const QString ph = QString("%1points[%2].").arg(here).arg(i);
        const QJsonObject po = arr.at(i).toObject();
        const QJsonValue ax = po.value("ax"), az = po.value("az");
        if (!ax.isDouble() || !std::isfinite(ax.toDouble()))
            return rd.fail(ph + "ax", "must be a number");
        if (!az.isDouble() || !std::isfinite(az.toDouble()))
            return rd.fail(ph + "az", "must be a number");
        limitPolygonPoint p;
        p.dAx = ax.toDouble();
        p.dAz = az.toDouble();
        out.lPoints.append(p);
    }
    limitListed row;
    row.sId = out.sId;
    QStringList corners;
    for (int i = 0; i < out.lPoints.size(); ++i)
        corners << QString("(%1, %2)").arg(numText(out.lPoints.at(i).dAx), numText(out.lPoints.at(i).dAz));
    row.sValue = "corners (ax, az) " + corners.join(" ");
    row.sConfidence = "high";
    row.sSource = "restraint_areas.source; corners marked approximate are read to +-0.05 g";
    row.sChecked = checkedNote;
    rd.listRow(row);
    return true;
}

} // namespace

forceLimits::forceLimits()
    : bValid(false), iFilterOrder(0), iRestraintFilterOrder(0)
{
}

QString forceLimits::defaultPath()
{
    return QStringLiteral(":/data/limits/astm-f2291-public.json");
}

QString forceLimits::lowerConfidence(const QString& a, const QString& b)
{
    if (a.isEmpty()) return b;
    if (b.isEmpty()) return a;
    return confidenceRank(a) <= confidenceRank(b) ? a : b;
}

const limitCurve* forceLimits::curve(const QString& id) const
{
    QMap<QString, limitCurve>::const_iterator it = lCurves.constFind(id);
    return it == lCurves.constEnd() ? nullptr : &it.value();
}

const limitRestraintKind* forceLimits::restraintKind(const QString& word) const
{
    QString id = word;
    if (!lRestraintKinds.contains(id) && lRestraintAliases.contains(id))
        id = lRestraintAliases.value(id);
    QMap<QString, limitRestraintKind>::const_iterator it = lRestraintKinds.constFind(id);
    return it == lRestraintKinds.constEnd() ? nullptr : &it.value();
}

double forceLimits::limitAt(const limitCurve& c, double duration, QString* confidence) const
{
    const limitCurve* cur = &c;
    double d = duration;
    if (d > maxDuration.dValue)
        d = maxDuration.dValue;
    // a "then" chain is checked for loops when the file is read
    for (int hop = 0; hop < 16; ++hop)
    {
        const QList<limitPoint>& pts = cur->lPoints;
        if (d <= pts.first().dTime)
        {
            if (confidence) *confidence = pts.first().confidence();
            return pts.first().dLimit;
        }
        for (int i = 0; i + 1 < pts.size(); ++i)
        {
            const limitPoint& a = pts.at(i);
            const limitPoint& b = pts.at(i + 1);
            if (d <= b.dTime)
            {
                if (confidence) *confidence = lowerConfidence(a.confidence(), b.confidence());
                return a.dLimit + (d - a.dTime) / (b.dTime - a.dTime) * (b.dLimit - a.dLimit);
            }
        }
        if (cur->sThen.isEmpty())
        {
            if (confidence) *confidence = pts.last().confidence();
            return pts.last().dLimit;
        }
        const limitCurve* next = curve(cur->sThen);
        if (!next)
            break;
        cur = next;
    }
    if (confidence) *confidence = cur->lPoints.last().confidence();
    return cur->lPoints.last().dLimit;
}

bool forceLimits::load(const QString& path, forceLimits& out, QString* error)
{
    QString dummy;
    QString* err = error ? error : &dummy;
    err->clear();
    out = forceLimits();
    out.sPath = path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *err = QString("cannot open %1").arg(path);
        return false;
    }
    const QByteArray bytes = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (doc.isNull() || !doc.isObject())
    {
        int line = 1, column = 1;
        for (int i = 0; i < parseError.offset && i < bytes.size(); ++i)
        {
            if (bytes.at(i) == '\n') { ++line; column = 1; }
            else ++column;
        }
        *err = doc.isNull()
             ? QString("%1, line %2, column %3: not valid JSON: %4").arg(path).arg(line).arg(column).arg(parseError.errorString())
             : QString("%1: the file must hold one JSON object").arg(path);
        return false;
    }

    fileReader rd(err, &out.lNumbers);
    const QJsonObject root = doc.object();
    const QJsonValue ver = root.value("schemaVersion");
    if (!ver.isDouble() || ver.toDouble() != (double)SCHEMA_VERSION)
        return rd.fail("schemaVersion", QString("must be %1").arg(SCHEMA_VERSION));
    if (!rd.string(root, "id", "", out.sId) || !rd.string(root, "name", "", out.sName)
     || !rd.string(root, "standard", "", out.sStandard) || !rd.string(root, "edition", "", out.sEdition)
     || !rd.string(root, "edition_note", "", out.sEditionNote) || !rd.string(root, "disclaimer", "", out.sDisclaimer))
        return false;
    QJsonObject sources;
    if (!rd.object(root, "sources", "", sources))
        return false;
    for (QJsonObject::const_iterator it = sources.constBegin(); it != sources.constEnd(); ++it)
        out.lSources.insert(it.key(), it.value().toString());

    // the filters and the general numbers
    if (!readFilter(rd, root, "filter", out.iFilterOrder, out.filterCorner))
        return false;
    if (!readFilter(rd, root, "restraint_filter", out.iRestraintFilterOrder, out.restraintFilterCorner))
        return false;
    {
        QJsonObject rf = root.value("restraint_filter").toObject();
        if (!rd.number(rf, "line_tolerance_g", "restraint_filter.", out.restraintLineTolerance))
            return false;
        if (out.restraintLineTolerance.dValue < 0.0)
            return rd.fail("restraint_filter.line_tolerance_g.value", "must not be below zero");
    }
    if (!rd.positive(root, "min_event_s", "", out.minEvent) || !rd.positive(root, "max_duration_s", "", out.maxDuration)
     || !rd.positive(root, "slice_step_g", "", out.sliceStep, 1.0))
        return false;
    if (!(out.maxDuration.dValue > out.minEvent.dValue))
        return rd.fail("max_duration_s.value", "must be longer than min_event_s");

    // the curves
    QJsonObject curves;
    if (!rd.object(root, "curves", "", curves))
        return false;
    for (QJsonObject::const_iterator it = curves.constBegin(); it != curves.constEnd(); ++it)
    {
        if (!it.value().isObject())
            return rd.fail("curves." + it.key(), "must be an object");
        limitCurve c;
        if (!readCurve(rd, it.key(), it.value().toObject(), c))
            return false;
        out.lCurves.insert(c.sId, c);
    }
    for (QMap<QString, limitCurve>::const_iterator it = out.lCurves.constBegin(); it != out.lCurves.constEnd(); ++it)
    {
        // a "then" names a curve of the same axis and direction, without a loop
        const limitCurve* c = &it.value();
        for (int hop = 0; !c->sThen.isEmpty(); ++hop)
        {
            const limitCurve* next = out.curve(c->sThen);
            if (!next)
                return rd.fail("curves." + c->sId + ".then", "names no curve: \"" + c->sThen + "\"");
            if (next->sAxis != c->sAxis || next->iSign != c->iSign)
                return rd.fail("curves." + c->sId + ".then", "must name a curve of the same axis and direction");
            if (hop > 8)
                return rd.fail("curves." + it.key() + ".then", "the curves name each other in a loop");
            c = next;
        }
    }
    struct required { const char* id; const char* axis; int sign; };
    static const required needed[] = {
        {"z_pos", "z", 1}, {"z_pos_after_neg", "z", 1}, {"z_neg", "z", -1}, {"z_neg_extended", "z", -1},
        {"y", "y", 0}, {"x_pos", "x", 1},
    };
    for (unsigned i = 0; i < sizeof(needed) / sizeof(needed[0]); ++i)
    {
        const limitCurve* c = out.curve(needed[i].id);
        if (!c)
            return rd.fail(QString("curves.") + needed[i].id, "is missing");
        if (c->sAxis != needed[i].axis || c->iSign != needed[i].sign)
            return rd.fail(QString("curves.") + needed[i].id, "has the wrong axis or direction");
    }
    {
        const QJsonObject pp = curves.value("z_pos_after_neg").toObject();
        const QString here = "curves.z_pos_after_neg.";
        if (!rd.positive(pp, "trigger_s", here, out.pushPullTrigger) || !rd.number(pp, "neg_below_g", here, out.pushPullBelow)
         || !rd.positive(pp, "window_s", here, out.pushPullWindow))
            return false;
        if (out.pushPullBelow.dValue > 0.0)
            return rd.fail(here + "neg_below_g.value", "must be zero or below");
    }

    // the restraint kinds
    {
        QJsonObject rk, kinds, aliases;
        if (!rd.object(root, "restraint_kinds", "", rk) || !rd.object(rk, "kinds", "restraint_kinds.", kinds)
         || !rd.object(rk, "aliases", "restraint_kinds.", aliases) || !rd.confidence(rk, "restraint_kinds.", out.sRestraintKindsConfidence))
            return false;
        if (kinds.isEmpty())
            return rd.fail("restraint_kinds.kinds", "must name at least one kind");
        for (QJsonObject::const_iterator it = kinds.constBegin(); it != kinds.constEnd(); ++it)
        {
            const QString here = "restraint_kinds.kinds." + it.key() + ".";
            if (!it.value().isObject())
                return rd.fail(here.left(here.size() - 1), "must be an object");
            const QJsonObject ko = it.value().toObject();
            limitRestraintKind k;
            k.sId = it.key();
            if (!rd.string(ko, "name", here, k.sName) || !rd.string(ko, "x_neg_curve", here, k.sNegXCurve)
             || !rd.string(ko, "egg_x_neg_curve", here, k.sEggNegXCurve))
                return false;
            const limitCurve* a = out.curve(k.sNegXCurve);
            if (!a || a->sAxis != "x" || a->iSign != -1)
                return rd.fail(here + "x_neg_curve", "must name a negative x curve");
            const limitCurve* b = out.curve(k.sEggNegXCurve);
            if (!b || b->sAxis != "x" || b->iSign != -1)
                return rd.fail(here + "egg_x_neg_curve", "must name a negative x curve");
            out.lRestraintKinds.insert(k.sId, k);
        }
        for (QJsonObject::const_iterator it = aliases.constBegin(); it != aliases.constEnd(); ++it)
        {
            const QString target = it.value().toString();
            if (!out.lRestraintKinds.contains(target))
                return rd.fail("restraint_kinds.aliases." + it.key(), "must name a kind, not \"" + target + "\"");
            out.lRestraintAliases.insert(it.key(), target);
        }
    }

    // the combination ("eggs")
    {
        QJsonObject comb, radii;
        if (!rd.object(root, "combination", "", comb) || !rd.object(comb, "radii", "combination.", radii))
            return false;
        const QString rh = "combination.radii.";
        if (!rd.string(radii, "x_pos", rh, out.sEggXPos) || !rd.string(radii, "y", rh, out.sEggY)
         || !rd.string(radii, "z_pos", rh, out.sEggZPos) || !rd.string(radii, "z_neg", rh, out.sEggZNeg)
         || !rd.string(radii, "z_neg_extended", rh, out.sEggZNegExtended) || !rd.confidence(radii, rh, out.sEggRadiiConfidence))
            return false;
        struct radius { const QString* id; const char* key; const char* axis; int sign; };
        const radius rs[] = {
            {&out.sEggXPos, "x_pos", "x", 1}, {&out.sEggY, "y", "y", 0}, {&out.sEggZPos, "z_pos", "z", 1},
            {&out.sEggZNeg, "z_neg", "z", -1}, {&out.sEggZNegExtended, "z_neg_extended", "z", -1},
        };
        for (unsigned i = 0; i < sizeof(rs) / sizeof(rs[0]); ++i)
        {
            const limitCurve* c = out.curve(*rs[i].id);
            if (!c || c->sAxis != rs[i].axis || c->iSign != rs[i].sign)
                return rd.fail(rh + rs[i].key, "must name a curve of that axis and direction");
        }
        const QJsonValue pv = comb.value("pairs");
        if (!pv.isArray())
            return rd.fail("combination.pairs", "must be a list");
        const QJsonArray pairs = pv.toArray();
        for (int i = 0; i < pairs.size(); ++i)
        {
            const QString ph = QString("combination.pairs[%1].").arg(i);
            const QJsonObject po = pairs.at(i).toObject();
            QString axes, conf;
            if (!rd.string(po, "axes", ph, axes) || !rd.confidence(po, ph, conf))
                return false;
            if (axes != "xz" && axes != "xy" && axes != "yz")
                return rd.fail(ph + "axes", "must be xz, xy or yz");
            if (out.lEggPairs.contains(axes))
                return rd.fail(ph + "axes", "is given twice");
            out.lEggPairs.append(axes);
            out.lEggPairConfidence.insert(axes, conf);
        }
        if (!rd.positive(comb, "radius_factor", "combination.", out.eggRadiusFactor, 2.0)
         || !rd.number(comb, "sustained_s", "combination.", out.eggSustained))
            return false;
        if (out.eggSustained.dValue < 0.0)
            return rd.fail("combination.sustained_s.value", "must not be below zero");
    }

    // the reversal rule
    {
        QJsonObject rv;
        if (!rd.object(root, "reversal_xy", "", rv))
            return false;
        const QString here = "reversal_xy.";
        const QJsonValue av = rv.value("axes");
        if (!av.isArray())
            return rd.fail(here + "axes", "must be a list");
        const QJsonArray axes = av.toArray();
        for (int i = 0; i < axes.size(); ++i)
        {
            const QString a = axes.at(i).toString();
            if (a != "x" && a != "y")
                return rd.fail(QString("%1axes[%2]").arg(here).arg(i), "must be x or y");
            out.lReversalAxes.append(a);
        }
        if (!rd.positive(rv, "max_peak_gap_s", here, out.reversalGap) || !rd.positive(rv, "factor", here, out.reversalFactor, 1.0)
         || !rd.positive(rv, "min_event_s", here, out.reversalMinEvent) || !rd.positive(rv, "event_threshold_g", here, out.reversalThreshold))
            return false;
    }

    // weightless into positive z
    {
        QJsonObject wl;
        if (!rd.object(root, "weightless_to_pos_z", "", wl))
            return false;
        const QString here = "weightless_to_pos_z.";
        if (!rd.number(wl, "weightless_at_or_below_g", here, out.weightlessAtOrBelow)
         || !rd.positive(wl, "min_weightless_s", here, out.weightlessMin)
         || !rd.number(wl, "threshold_g", here, out.weightlessThreshold)
         || !rd.positive(wl, "min_rise_s", here, out.weightlessMinRise))
            return false;
        if (!(out.weightlessThreshold.dValue > out.weightlessAtOrBelow.dValue))
            return rd.fail(here + "threshold_g.value", "must be above weightless_at_or_below_g");
    }

    // the children's multiplier: listed, not applied
    {
        const QJsonObject ch = root.value("children").toObject();
        const QJsonArray pts = ch.value("points").toArray();
        for (int i = 0; i < pts.size(); ++i)
        {
            const QString ph = QString("children.points[%1].").arg(i);
            const QJsonObject po = pts.at(i).toObject();
            limitListed row;
            if (!rd.string(po, "id", ph, row.sId) || !rd.uniqueId(row.sId, ph) || !rd.string(po, "source", ph, row.sSource)
             || !rd.confidence(po, ph, row.sConfidence) || !rd.checked(po, ph, row.sChecked))
                return false;
            row.sValue = QString("multiplier %1 at %2 in (informative, not applied)")
                         .arg(numText(po.value("multiplier").toDouble()), numText(po.value("height").toDouble()));
            rd.listRow(row);
        }
    }

    // the restraint areas
    {
        QJsonObject ra;
        if (!rd.object(root, "restraint_areas", "", ra))
            return false;
        if (!rd.string(ra, "confidence", "restraint_areas.", out.sRestraintAreasConfidence))
            return false;
        const QJsonValue rv = ra.value("regions");
        if (!rv.isArray() || rv.toArray().size() != 4)
            return rd.fail("restraint_areas.regions", "must be a list of four regions (areas 1 to 4)");
        const QJsonArray regions = rv.toArray();
        for (int i = 0; i < regions.size(); ++i)
        {
            limitRegion region;
            if (!readRegion(rd, regions.at(i), i, region))
                return false;
            out.lRegions.append(region);
        }
    }

    out.bValid = true;
    return true;
}

const forceLimits& forceLimits::defaultSet()
{
    static forceLimits cache;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        QString error;
        if (!load(defaultPath(), cache, &error))
            qWarning() << "force limits:" << error;
        bLoaded = true;
    }
    return cache;
}

// ============================================================ the result

int limitResult::ruleIndex(const QString& id)
{
    int i = lRuleIds.indexOf(id);
    if (i < 0)
    {
        lRuleIds.append(id);
        i = lRuleIds.size() - 1;
    }
    return i;
}

QString limitFamilyName(int family)
{
    static const char* names[LIMIT_FAMILIES] = { "x", "y", "z", "egg", "reversal", "airtime" };
    return family >= 0 && family < LIMIT_FAMILIES ? QString(names[family]) : QString();
}

void limitResultInit(limitResult& r, int n)
{
    for (int f = 0; f < LIMIT_FAMILIES; ++f)
    {
        r.lUtil[f] = QVector<float>(n, 0.0f);
        r.lRule[f] = QVector<qint8>(n, -1);
        r.worstFamily[f] = limitWorst();
    }
    r.lUtilMax = QVector<float>(n, 0.0f);
    r.lRuleMax = QVector<qint8>(n, -1);
    r.lRestraintArea = QVector<qint8>(n, 0);
    r.lFindings.clear();
    r.worst = limitWorst();
}

void limitResultFinish(limitResult& r)
{
    const int n = r.lUtilMax.size();
    float* um = r.lUtilMax.data();
    qint8* rm = r.lRuleMax.data();
    for (int f = 0; f < LIMIT_FAMILIES; ++f)
    {
        const float* u = r.lUtil[f].constData();
        const qint8* ru = r.lRule[f].constData();
        limitWorst& w = r.worstFamily[f];
        for (int i = 0; i < n; ++i)
        {
            if (u[i] > um[i])
            {
                um[i] = u[i];
                rm[i] = ru[i];
            }
            if (u[i] > w.dUtil)
            {
                w.dUtil = u[i];
                w.iSample = i;
                w.iRule = ru[i];
            }
        }
        if (w.dUtil > r.worst.dUtil)
            r.worst = w;
    }
    std::stable_sort(r.lFindings.begin(), r.lFindings.end(), [](const limitFinding& a, const limitFinding& b) {
        if (a.iFirst != b.iFirst) return a.iFirst < b.iFirst;
        return a.iFamily < b.iFamily;
    });
    r.bPass = r.lFindings.isEmpty();
}

// ============================================================ the filter

limitFilter limitFilterDesign(int order, double cornerHz, double dt)
{
    limitFilter f;
    f.dt = dt;
    if (order < 2 || order % 2 != 0 || !(dt > 0.0) || !(cornerHz > 0.0) || !(cornerHz * dt < 0.5))
        return f;
    const double k = std::tan(PI * cornerHz * dt);    // the corner pre-warped
    const double k2 = k * k;
    for (int i = 0; i < order / 2; ++i)
    {
        // analog prototype s^2 + q s + 1, q = 2 sin((2i+1) pi / (2 order))
        const double q = 2.0 * std::sin((2.0 * i + 1.0) * PI / (2.0 * order));
        const double norm = 1.0 / (1.0 + q * k + k2);
        limitBiquad b;
        b.b0 = k2 * norm;
        b.b1 = 2.0 * b.b0;
        b.b2 = b.b0;
        b.a1 = 2.0 * (k2 - 1.0) * norm;
        b.a2 = (1.0 - q * k + k2) * norm;
        f.lSections.append(b);
    }
    f.bValid = true;
    return f;
}

double limitFilterDelay(const limitFilter& f)
{
    // group delay at 0 Hz of B(z)/A(z): sum(k b_k)/sum(b_k) - sum(k a_k)/sum(a_k)
    double total = 0.0;
    for (int i = 0; i < f.lSections.size(); ++i)
    {
        const limitBiquad& b = f.lSections.at(i);
        const double num = (b.b1 + 2.0 * b.b2) / (b.b0 + b.b1 + b.b2);
        const double den = (b.a1 + 2.0 * b.a2) / (1.0 + b.a1 + b.a2);
        total += num - den;
    }
    return total;
}

QVector<double> limitFilterApply(const limitFilter& f, const QVector<double>& x, bool removeDelay, int* shift)
{
    QVector<double> y;
    const QVector<double>* in[1] = { &x };
    QVector<double>* out[1] = { &y };
    limitFilterApplyMany(f, in, out, 1, removeDelay, shift);
    return y;
}

void limitFilterApplyMany(const limitFilter& f, const QVector<double>* const* in, QVector<double>* const* out,
                          int count, bool removeDelay, int* shift)
{
    int sh = 0;
    if (removeDelay && f.bValid)
        sh = (int)std::floor(limitFilterDelay(f) + 0.5);
    if (shift) *shift = sh;
    if (count < 1)
        return;
    const int n = in[0]->size();
    if (n == 0 || !f.bValid)
    {
        for (int k = 0; k < count; ++k)
            *out[k] = *in[k];
        return;
    }
    // The traces side by side in one loop: each is a chain where every value
    // waits for the one before, so running them together lets the processor
    // overlap them. Transposed direct form II, started at rest at the first value.
    const int ns = f.lSections.size();
    std::vector<limitBiquad> b(f.lSections.begin(), f.lSections.end());
    std::vector<double> z1(ns * count), z2(ns * count);
    std::vector<const double*> src(count);
    std::vector<double*> dst(count);
    for (int k = 0; k < count; ++k)
    {
        src[k] = in[k]->constData();
        *out[k] = QVector<double>(n);
        dst[k] = out[k]->data();
        const double x0 = src[k][0];
        for (int s = 0; s < ns; ++s)
        {
            // at rest every section gives out what comes in (a gain of 1 at 0 Hz)
            z2[s * count + k] = (b[s].b2 - b[s].a2) * x0;
            z1[s * count + k] = (b[s].b1 - b[s].a1) * x0 + z2[s * count + k];
        }
    }
    // the input continued with its last value for sh samples; the first sh
    // outputs are dropped, which takes the delay out
    for (int i = 0; i < n + sh; ++i)
    {
        const int at = i < n ? i : n - 1;
        for (int k = 0; k < count; ++k)
        {
            double v = src[k][at];
            for (int s = 0; s < ns; ++s)
            {
                const limitBiquad& q = b[s];
                double& a1 = z1[s * count + k];
                double& a2 = z2[s * count + k];
                const double r = q.b0 * v + a1;
                a1 = q.b1 * v - q.a1 * r + a2;
                a2 = q.b2 * v - q.a2 * r;
                v = r;
            }
            if (i >= sh)
                dst[k][i - sh] = v;
        }
    }
}

// ============================================================ slicing

QVector<limitSliceEvent> limitSlice(const QVector<double>& values, int sign, double step, QVector<int>* deepest)
{
    QVector<limitSliceEvent> events;
    const int n = values.size();
    if (deepest) deepest->resize(n);    // every sample is written below
    if (!(step > 0.0))
    {
        if (deepest) deepest->fill(-1);
        return events;
    }
    events.reserve(256);
    const double* v = values.constData();
    int* deep = deepest ? deepest->data() : nullptr;
    QVector<int> open;              // open[j-1]: the open event at level j
    int kPrev = 0;
    for (int i = 0; i < n; ++i)
    {
        const double m = sign * v[i] / step;
        int k = 0;
        if (m > 0.0)
            k = (int)std::floor(std::min(m, 100000.0) + EPS);
        if (k > kPrev)
        {
            for (int j = kPrev + 1; j <= k; ++j)
            {
                limitSliceEvent e;
                e.iLevel = j;
                e.iFirst = i;
                e.iLast = n - 1;
                e.iParent = open.isEmpty() ? -1 : open.last();
                events.append(e);
                open.append(events.size() - 1);
            }
        }
        else if (k < kPrev)
        {
            for (int j = kPrev; j > k; --j)
            {
                events[open.last()].iLast = i - 1;
                open.removeLast();
            }
        }
        if (deep) deep[i] = open.isEmpty() ? -1 : open.last();
        kPrev = k;
    }
    return events;
}

// ============================================================ the steps

namespace {

QString fmt(double v, int decimals = 2)
{
    return QString::number(v, 'f', decimals);
}

QString timeSpan(int first, int last, double dt)
{
    return QString("from %1 s to %2 s").arg(fmt(first * dt), fmt((last + 1) * dt));
}

void raiseUtil(limitResult& r, int family, int i, double u, int rule)
{
    float& cur = r.lUtil[family][i];
    if (u > cur)
    {
        cur = (float)u;
        r.lRule[family][i] = rule;
    }
}

const QVector<double>* axisValues(const forceTrace& t, QChar axis)
{
    if (axis == 'x') return &t.gx;
    if (axis == 'y') return &t.gy;
    return &t.gz;
}

// Which slice events a judgement takes: all, only those starting in the
// push-pull windows, or only those starting outside them.
enum sliceFilter { slicesAll, slicesInWindows, slicesOutsideWindows };

bool startsInWindow(int first, const QList<QPair<int, int> >& windows)
{
    for (int w = 0; w < windows.size(); ++w)
        if (first >= windows.at(w).first && first <= windows.at(w).second)
            return true;
    return false;
}

// Slices one direction of values and judges every event of min_event_s or
// longer against curve c: fills the family's shares (each sample gets the
// largest share of the events containing it) and adds one finding per group
// of failing events that nest in one another. deepest is scratch space,
// shared between calls so that it is not made anew each time.
void judgeSlices(const QVector<double>& values, int sign, const limitCurve& c, int family, const QString& axis,
                 const forceTrace& t, const forceLimits& s, limitResult& r,
                 sliceFilter which, const QList<QPair<int, int> >& windows, QVector<int>& deepest)
{
    const double step = s.sliceStep.dValue;
    const QVector<limitSliceEvent> ev = limitSlice(values, sign, step, &deepest);
    const int ne = ev.size();
    if (ne == 0)
        return;
    const int rule = r.ruleIndex(c.sId);
    std::vector<double> share(ne, 0.0), limit(ne, 0.0), best(ne, 0.0);
    std::vector<int> top(ne, -1);
    const double minEvent = s.minEvent.dValue - EPS;
    for (int e = 0; e < ne; ++e)
    {
        const limitSliceEvent& x = ev.at(e);
        const double duration = (x.iLast - x.iFirst + 1) * t.dt;
        bool judged = duration >= minEvent;
        if (judged && which != slicesAll)
        {
            const bool in = startsInWindow(x.iFirst, windows);
            judged = (which == slicesInWindows) ? in : !in;
        }
        if (judged)
        {
            limit[e] = s.limitAt(c, duration);
            share[e] = x.iLevel * step / std::fabs(limit[e]);
        }
        // parents come first, so their best and top are known
        const int p = x.iParent;
        best[e] = std::max(share[e], p >= 0 ? best[p] : 0.0);
        const bool fails = share[e] > 1.0 + EPS;
        top[e] = (p >= 0 && top[p] >= 0) ? top[p] : (fails ? e : -1);
    }
    const int n = values.size();
    const int* deep = deepest.constData();
    float* util = r.lUtil[family].data();
    qint8* rules = r.lRule[family].data();
    for (int i = 0; i < n; ++i)
    {
        if (deep[i] < 0)
            continue;
        const double u = best[deep[i]];
        if (u > util[i])
        {
            util[i] = u;
            rules[i] = rule;
        }
    }

    // one finding per group: the topmost failing event and the worst one in it
    QMap<int, int> worstOf;     // top event -> worst failing event under it
    for (int e = 0; e < ne; ++e)
    {
        if (!(share[e] > 1.0 + EPS) || top[e] < 0)
            continue;
        QMap<int, int>::iterator it = worstOf.find(top[e]);
        if (it == worstOf.end())
            worstOf.insert(top[e], e);
        else if (share[e] > share[it.value()])
            it.value() = e;
    }
    const double* v = values.constData();
    for (QMap<int, int>::const_iterator it = worstOf.constBegin(); it != worstOf.constEnd(); ++it)
    {
        const limitSliceEvent& span = ev.at(it.key());
        const int w = it.value();
        const limitSliceEvent& x = ev.at(w);
        limitFinding f;
        f.sRule = c.sId;
        f.iFamily = family;
        f.sAxis = axis;
        f.iDirection = sign;
        f.iFirst = span.iFirst;
        f.iLast = span.iLast;
        f.iWorstSample = x.iFirst;
        for (int i = x.iFirst; i <= x.iLast; ++i)
            if (sign * v[i] > sign * v[f.iWorstSample])
                f.iWorstSample = i;
        f.dLevel = sign * x.iLevel * step;
        f.dLimit = (c.iSign == 0) ? sign * std::fabs(limit[w]) : limit[w];
        f.dDuration = (x.iLast - x.iFirst + 1) * t.dt;
        f.dUtilisation = share[w];
        s.limitAt(c, f.dDuration, &f.sConfidence);
        f.sText = QString("%1: %2 g or beyond lasts %3 s (%4); the limit for that long is %5 g, a share of %6 "
                          "(numbers of %7 confidence).")
                  .arg(c.sName, fmt(f.dLevel, 1), fmt(f.dDuration), timeSpan(x.iFirst, x.iLast, t.dt),
                       fmt(f.dLimit), fmt(f.dUtilisation), f.sConfidence);
        r.lFindings.append(f);
    }
}

const limitCurve* negZCurve(const forceLimits& s, const limitOptions& o)
{
    return s.curve(o.bExtendedNegZ ? QString("z_neg_extended") : QString("z_neg"));
}

bool kindFor(const forceLimits& s, const limitOptions& o, limitResult& r, const limitRestraintKind** kind)
{
    *kind = s.restraintKind(o.sRestraint);
    if (!*kind)
    {
        r.sError = QString("unknown restraint kind \"%1\" (the limit file knows: %2)")
                   .arg(o.sRestraint, QStringList(s.lRestraintKinds.keys()).join(", "));
        return false;
    }
    return true;
}

// The smallest (or with wantMax the largest) value of each window of m
// samples: out[j] = min(in[j .. j+m-1]) for j = 0 .. n-m (van Herk's method:
// running extremes forward and backward within blocks of m samples).
// work must hold 2n values. Returns the number of windows.
template <bool wantMax>
int slidingExtreme(const double* in, int n, int m, double* out, double* work)
{
    if (m < 1 || n < m)
        return 0;
    double* fwd = work;
    double* bwd = work + n;
    for (int b = 0; b < n; b += m)
    {
        // the forward and the backward run side by side (two chains the
        // processor can overlap)
        const int end = std::min(n, b + m);
        fwd[b] = in[b];
        bwd[end - 1] = in[end - 1];
        for (int t = 1; t < end - b; ++t)
        {
            const int i = b + t, k = end - 1 - t;
            fwd[i] = wantMax ? std::max(fwd[i - 1], in[i]) : std::min(fwd[i - 1], in[i]);
            bwd[k] = wantMax ? std::max(bwd[k + 1], in[k]) : std::min(bwd[k + 1], in[k]);
        }
    }
    for (int j = 0; j + m <= n; ++j)
        out[j] = wantMax ? std::max(bwd[j], fwd[j + m - 1]) : std::min(bwd[j], fwd[j + m - 1]);
    return n - m + 1;
}

} // namespace

bool limitStepDuration(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r)
{
    const limitRestraintKind* kind;
    if (!kindFor(s, o, r, &kind))
        return false;
    const QList<QPair<int, int> > windows = limitPushPullWindows(t.gz, t.dt, s);
    QVector<int> deepest;
    judgeSlices(t.gx, 1, *s.curve("x_pos"), limitFamilyX, "x", t, s, r, slicesAll, windows, deepest);
    judgeSlices(t.gx, -1, *s.curve(kind->sNegXCurve), limitFamilyX, "x", t, s, r, slicesAll, windows, deepest);
    judgeSlices(t.gy, 1, *s.curve("y"), limitFamilyY, "y", t, s, r, slicesAll, windows, deepest);
    judgeSlices(t.gy, -1, *s.curve("y"), limitFamilyY, "y", t, s, r, slicesAll, windows, deepest);
    // +Gz events after airtime are judged by the push-pull curve instead (limitStepAirtime)
    judgeSlices(t.gz, 1, *s.curve("z_pos"), limitFamilyZ, "z", t, s, r, slicesOutsideWindows, windows, deepest);
    judgeSlices(t.gz, -1, *negZCurve(s, o), limitFamilyZ, "z", t, s, r, slicesAll, windows, deepest);
    return true;
}

bool limitStepEggs(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r)
{
    const limitRestraintKind* kind;
    if (!kindFor(s, o, r, &kind))
        return false;
    const double factor = s.eggRadiusFactor.dValue;
    const double at = s.minEvent.dValue;
    // radii: [axis][0 positive, 1 negative], magnitudes; with the confidence of each
    double radius[3][2];
    QString conf = s.lowerConfidence(s.sEggRadiiConfidence, s.eggRadiusFactor.confidence());
    conf = s.lowerConfidence(conf, s.eggSustained.confidence());
    conf = s.lowerConfidence(conf, s.sRestraintKindsConfidence);
    const QString ids[3][2] = {
        { s.sEggXPos, kind->sEggNegXCurve },
        { s.sEggY, s.sEggY },
        { s.sEggZPos, o.bExtendedNegZ ? s.sEggZNegExtended : s.sEggZNeg },
    };
    for (int a = 0; a < 3; ++a)
        for (int d = 0; d < 2; ++d)
        {
            QString c;
            radius[a][d] = std::fabs(s.limitAt(*s.curve(ids[a][d]), at, &c)) * factor;
            conf = s.lowerConfidence(conf, c);
        }

    const int n = t.size();
    int m = (int)std::ceil(s.eggSustained.dValue / t.dt - EPS);
    if (m < 1) m = 1;
    // buffers shared by the pairs; padded keeps zeros outside [m-1, n-1]
    std::vector<double> e2(n), held(n, 0.0), padded(n + m - 1, 0.0), work(2 * (n + m - 1));
    for (int p = 0; p < s.lEggPairs.size(); ++p)
    {
        const QString pair = s.lEggPairs.at(p);
        const int ia = pair.at(0).toLatin1() - 'x';
        const int ib = pair.at(1).toLatin1() - 'x';
        const QVector<double>& va = *axisValues(t, pair.at(0));
        const QVector<double>& vb = *axisValues(t, pair.at(1));
        const double* pa = va.constData();
        const double* pb = vb.constData();
        const double raPos = 1.0 / radius[ia][0], raNeg = 1.0 / radius[ia][1];
        const double rbPos = 1.0 / radius[ib][0], rbNeg = 1.0 / radius[ib][1];
        // the square of the ellipse's share at every sample
        for (int i = 0; i < n; ++i)
        {
            const double a = pa[i] * (pa[i] >= 0.0 ? raPos : raNeg);
            const double b = pb[i] * (pb[i] >= 0.0 ? rbPos : rbNeg);
            e2[i] = a * a + b * b;
        }
        // the share held for m samples: the best of the smallest values of
        // every window of m samples that contains the sample (on squares,
        // which keep the order)
        if (n >= m)
        {
            // padded[k] = lows[k - (m-1)] where that is a window, else 0;
            // held[i] = max(padded[i .. i+m-1]) = max(lows[j]) for j in [i-m+1, i]
            slidingExtreme<false>(e2.data(), n, m, padded.data() + m - 1, work.data());
            slidingExtreme<true>(padded.data(), n + m - 1, m, held.data(), work.data());
        }
        const QString ruleId = "egg_" + pair;
        const int rule = r.ruleIndex(ruleId);
        const QString pairConf = s.lowerConfidence(conf, s.lEggPairConfidence.value(pair));
        float* util = r.lUtil[limitFamilyEgg].data();
        qint8* rules = r.lRule[limitFamilyEgg].data();
        for (int i = 0; i < n; ++i)
        {
            held[i] = std::sqrt(held[i]);
            if (held[i] > util[i])
            {
                util[i] = held[i];
                rules[i] = rule;
            }
        }
        // findings: the stretches held beyond the ellipse
        for (int i = 0; i < n; )
        {
            if (!(held.at(i) > 1.0 + EPS)) { ++i; continue; }
            int j = i;
            int worst = i;
            while (j + 1 < n && held.at(j + 1) > 1.0 + EPS)
            {
                ++j;
                if (held.at(j) > held.at(worst)) worst = j;
            }
            limitFinding f;
            f.sRule = ruleId;
            f.iFamily = limitFamilyEgg;
            f.sAxis = pair;
            f.iDirection = 0;
            f.iFirst = i;
            f.iLast = j;
            f.iWorstSample = worst;
            f.dLevel = held.at(worst);
            f.dLimit = 1.0;
            f.dDuration = (j - i + 1) * t.dt;
            f.dUtilisation = held.at(worst);
            f.sConfidence = pairConf;
            f.sText = QString("%1 and %2 together (%3 g and %4 g) are %5 times the combined limit for %6 s (%7); "
                              "the ellipse's radii are the %8 s limits times %9 (numbers of %10 confidence).")
                      .arg(pair.mid(0, 1), pair.mid(1, 1), fmt(va.at(worst)), fmt(vb.at(worst)), fmt(f.dUtilisation),
                           fmt(f.dDuration), timeSpan(i, j, t.dt), fmt(at, 1), fmt(factor, 2), f.sConfidence);
            r.lFindings.append(f);
            i = j + 1;
        }
    }
    return true;
}

bool limitStepReversal(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r)
{
    const limitRestraintKind* kind;
    if (!kindFor(s, o, r, &kind))
        return false;
    const double thr = s.reversalThreshold.dValue;
    const double factor = s.reversalFactor.dValue;
    const int n = t.size();
    for (int ax = 0; ax < s.lReversalAxes.size(); ++ax)
    {
        const QString axis = s.lReversalAxes.at(ax);
        const QVector<double>& v = *axisValues(t, axis.at(0));
        // the halved limit of each direction: [0] positive, [1] negative
        double half[2];
        QString conf = s.lowerConfidence(s.reversalGap.confidence(), s.reversalFactor.confidence());
        conf = s.lowerConfidence(conf, s.reversalMinEvent.confidence());
        conf = s.lowerConfidence(conf, s.reversalThreshold.confidence());
        const QString curves[2] = { axis == "x" ? QString("x_pos") : QString("y"),
                                    axis == "x" ? kind->sNegXCurve : QString("y") };
        for (int d = 0; d < 2; ++d)
        {
            QString c;
            half[d] = std::fabs(s.limitAt(*s.curve(curves[d]), s.minEvent.dValue, &c)) * factor;
            conf = s.lowerConfidence(conf, c);
        }
        // the sustained events of each direction, in time order
        struct event { int sign, first, last, peakFirst, peakLast; };
        QList<event> events;
        for (int i = 0; i < n; )
        {
            const int sign = v.at(i) >= thr - EPS ? 1 : (v.at(i) <= -thr + EPS ? -1 : 0);
            if (sign == 0) { ++i; continue; }
            int j = i;
            while (j + 1 < n && sign * v.at(j + 1) >= thr - EPS)
                ++j;
            if ((j - i + 1) * t.dt >= s.reversalMinEvent.dValue - EPS)
            {
                event e = { sign, i, j, i, i };
                for (int k = i; k <= j; ++k)
                {
                    if (sign * v.at(k) > sign * v.at(e.peakFirst)) { e.peakFirst = k; e.peakLast = k; }
                    else if (sign * v.at(k) == sign * v.at(e.peakFirst)) e.peakLast = k;
                }
                events.append(e);
            }
            i = j + 1;
        }
        const QString ruleId = "reversal_" + axis;
        const int rule = r.ruleIndex(ruleId);
        for (int k = 0; k + 1 < events.size(); ++k)
        {
            const event& a = events.at(k);
            const event& b = events.at(k + 1);
            if (a.sign == b.sign)
                continue;
            // of equal peaks, the ones nearest each other (the stricter reading)
            const int pa = a.peakLast, pb = b.peakFirst;
            const double gap = (pb - pa) * t.dt;
            if (gap > s.reversalGap.dValue + EPS)
                continue;
            const double ha = half[a.sign > 0 ? 0 : 1], hb = half[b.sign > 0 ? 0 : 1];
            for (int i = a.first; i <= a.last; ++i)
                raiseUtil(r, limitFamilyReversal, i, std::fabs(v.at(i)) / ha, rule);
            for (int i = b.first; i <= b.last; ++i)
                raiseUtil(r, limitFamilyReversal, i, std::fabs(v.at(i)) / hb, rule);
            const double sa = std::fabs(v.at(pa)) / ha, sb = std::fabs(v.at(pb)) / hb;
            if (!(std::max(sa, sb) > 1.0 + EPS))
                continue;
            const bool aWorse = sa >= sb;
            limitFinding f;
            f.sRule = ruleId;
            f.iFamily = limitFamilyReversal;
            f.sAxis = axis;
            f.iDirection = aWorse ? a.sign : b.sign;
            f.iFirst = a.first;
            f.iLast = b.last;
            f.iWorstSample = aWorse ? pa : pb;
            f.dLevel = v.at(f.iWorstSample);
            f.dLimit = f.iDirection * (aWorse ? ha : hb);
            f.dDuration = gap;
            f.dUtilisation = std::max(sa, sb);
            f.sConfidence = conf;
            f.sText = QString("Fast reversal in %1: peaks of %2 g and %3 g only %4 s apart (%5), so each may reach only "
                              "%6 of its limit: %7 g, a share of %8 (numbers of %9 confidence).")
                      .arg(axis, fmt(v.at(pa)), fmt(v.at(pb)), fmt(gap), timeSpan(a.first, b.last, t.dt),
                           fmt(factor), fmt(f.dLimit), fmt(f.dUtilisation), conf);
            r.lFindings.append(f);
        }
    }
    return true;
}

QList<QPair<int, int> > limitPushPullWindows(const QVector<double>& gz, double dt, const forceLimits& s)
{
    QList<QPair<int, int> > windows;
    const int n = gz.size();
    const double below = s.pushPullBelow.dValue;
    const int length = (int)std::floor(s.pushPullWindow.dValue / dt + EPS);
    for (int i = 0; i < n; )
    {
        if (!(gz.at(i) < below)) { ++i; continue; }
        int j = i;
        while (j + 1 < n && gz.at(j + 1) < below)
            ++j;
        if ((j - i + 1) * dt >= s.pushPullTrigger.dValue - EPS && j + 1 < n)
            windows.append(qMakePair(j + 1, std::min(n - 1, j + length)));
        i = j + 1;
    }
    return windows;
}

bool limitStepAirtime(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r)
{
    Q_UNUSED(o);
    // 1. push-pull: +Gz events that start within the window after airtime
    const QList<QPair<int, int> > windows = limitPushPullWindows(t.gz, t.dt, s);
    if (!windows.isEmpty())
    {
        QVector<int> deepest;
        judgeSlices(t.gz, 1, *s.curve("z_pos_after_neg"), limitFamilyAirtime, "z", t, s, r, slicesInWindows, windows, deepest);
    }

    // 2. from sustained weightlessness to threshold_g: not faster than min_rise_s
    const double w = s.weightlessAtOrBelow.dValue;
    const double thr = s.weightlessThreshold.dValue;
    const double minRise = s.weightlessMinRise.dValue;
    QString conf = s.lowerConfidence(s.weightlessAtOrBelow.confidence(), s.weightlessMin.confidence());
    conf = s.lowerConfidence(conf, s.weightlessThreshold.confidence());
    conf = s.lowerConfidence(conf, s.weightlessMinRise.confidence());
    const int rule = r.ruleIndex("weightless_to_pos_z");
    const QVector<double>& z = t.gz;
    const int n = z.size();
    for (int i = 0; i < n; )
    {
        if (!(z.at(i) <= w)) { ++i; continue; }
        int e = i;
        while (e + 1 < n && z.at(e + 1) <= w)
            ++e;
        const int start = i;
        i = e + 1;
        if ((e - start + 1) * t.dt < s.weightlessMin.dValue - EPS || e + 1 >= n)
            continue;
        // the first sample at or above the threshold before z is weightless again
        int j = e + 1;
        while (j < n && z.at(j) < thr && z.at(j) > w)
            ++j;
        if (j >= n || !(z.at(j) >= thr))
            continue;
        // crossing times between samples, in samples
        const double t0 = e + (w - z.at(e)) / (z.at(e + 1) - z.at(e));
        const double t2 = (j - 1) + (thr - z.at(j - 1)) / (z.at(j) - z.at(j - 1));
        const double rise = std::max((t2 - t0) * t.dt, 1e-6);
        const double share = minRise / rise;
        for (int k = e; k <= j; ++k)
            raiseUtil(r, limitFamilyAirtime, k, share, rule);
        if (!(share > 1.0 + EPS))
            continue;
        limitFinding f;
        f.sRule = "weightless_to_pos_z";
        f.iFamily = limitFamilyAirtime;
        f.sAxis = "z";
        f.iDirection = 1;
        f.iFirst = e;
        f.iLast = j;
        f.iWorstSample = j;
        f.dLevel = rise;
        f.dLimit = minRise;
        f.dDuration = rise;
        f.dUtilisation = share;
        f.sConfidence = conf;
        f.sText = QString("From %1 s of weightlessness, z rises from %2 g to %3 g in %4 s (%5); it must take at least "
                          "%6 s, a share of %7 (numbers of %8 confidence).")
                  .arg(fmt((e - start + 1) * t.dt), fmt(w, 1), fmt(thr, 1), fmt(rise, 3), timeSpan(e, j, t.dt),
                       fmt(minRise, 3), fmt(share), conf);
        r.lFindings.append(f);
    }
    return true;
}

// ============================================================ restraint areas

namespace {

// A region's corners as plain arrays, with the lower corner of the box
// around it (to pass over the region quickly for a point far below or left).
struct flatRegion
{
    int iArea;
    std::vector<double> x, y;
    double dMinX, dMinY;
};

std::vector<flatRegion> flatRegions(const forceLimits& s)
{
    std::vector<flatRegion> out;
    for (int k = 0; k < s.lRegions.size(); ++k)
    {
        const limitRegion& region = s.lRegions.at(k);
        flatRegion f;
        f.iArea = region.iArea;
        f.dMinX = f.dMinY = 1e300;
        for (int i = 0; i < region.lPoints.size(); ++i)
        {
            f.x.push_back(region.lPoints.at(i).dAx);
            f.y.push_back(region.lPoints.at(i).dAz);
            f.dMinX = std::min(f.dMinX, region.lPoints.at(i).dAx);
            f.dMinY = std::min(f.dMinY, region.lPoints.at(i).dAz);
        }
        out.push_back(f);
    }
    return out;
}

bool insidePolygon(double x, double y, const flatRegion& p)
{
    bool in = false;
    const int n = (int)p.x.size();
    for (int i = 0, j = n - 1; i < n; j = i++)
    {
        const double xi = p.x[i], yi = p.y[i], xj = p.x[j], yj = p.y[j];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            in = !in;
    }
    return in;
}

// Whether (x, y) is within tol of an edge of the polygon.
bool nearPolygon(double x, double y, const flatRegion& p, double tol)
{
    const double tol2 = tol * tol;
    const int n = (int)p.x.size();
    for (int i = 0, j = n - 1; i < n; j = i++)
    {
        const double ax = p.x[j], ay = p.y[j], bx = p.x[i], by = p.y[i];
        const double dx = bx - ax, dy = by - ay;
        const double len2 = dx * dx + dy * dy;
        double u = len2 > 0.0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0.0;
        u = std::max(0.0, std::min(1.0, u));
        const double ex = ax + u * dx - x, ey = ay + u * dy - y;
        if (ex * ex + ey * ey <= tol2)
            return true;
    }
    return false;
}

// The lowest area whose region holds the point or lies within tol of it.
int areaOf(double ax, double az, const std::vector<flatRegion>& regions, double tol)
{
    for (size_t k = 0; k < regions.size(); ++k)
    {
        const flatRegion& region = regions[k];
        if (ax < region.dMinX - tol || az < region.dMinY - tol)
            continue;
        if (insidePolygon(ax, az, region) || nearPolygon(ax, az, region, tol))
            return region.iArea;
    }
    return 5;
}

} // namespace

int limitRestraintArea(double ax, double az, const forceLimits& s)
{
    return areaOf(ax, az, flatRegions(s), s.restraintLineTolerance.dValue + EPS);
}

bool limitStepRestraint(const forceTrace& raw, const forceLimits& s, limitResult& r)
{
    const limitFilter f = limitFilterDesign(s.iRestraintFilterOrder, s.restraintFilterCorner.dValue, raw.dt);
    if (!f.bValid)
    {
        r.lNotes << QString("The restraint area was not found: samples %1 s apart are too far apart for the %2 Hz filter.")
                    .arg(raw.dt).arg(s.restraintFilterCorner.dValue);
        return true;
    }
    QVector<double> ax, az;
    const QVector<double>* in[2] = { &raw.gx, &raw.gz };
    QVector<double>* out[2] = { &ax, &az };
    limitFilterApplyMany(f, in, out, 2, true);
    const std::vector<flatRegion> regions = flatRegions(s);
    const double tol = s.restraintLineTolerance.dValue + EPS;
    const double* px = ax.constData();
    const double* pz = az.constData();
    qint8* areas = r.lRestraintArea.data();
    r.iRestraintArea = 0;
    r.iRestraintSample = -1;
    for (int i = 0; i < raw.size(); ++i)
    {
        const int area = areaOf(px[i], pz[i], regions, tol);
        areas[i] = area;
        if (area > r.iRestraintArea)
        {
            r.iRestraintArea = area;
            r.iRestraintSample = i;
        }
    }
    return true;
}

// ============================================================ the whole check

limitResult checkForceLimits(const forceTrace& trace, const forceLimits& limits, const limitOptions& options)
{
    limitResult r;
    if (!limits.isValid())
    {
        r.sError = "the limit set is not loaded";
        return r;
    }
    const int n = trace.size();
    if (!(trace.dt > 0.0) || !std::isfinite(trace.dt))
    {
        r.sError = "the time step dt must be above zero";
        return r;
    }
    if (n == 0 || trace.gx.size() != n || trace.gy.size() != n)
    {
        r.sError = "the trace needs the same number of samples, at least one, in gx, gy and gz";
        return r;
    }
    for (int i = 0; i < n; ++i)
    {
        if (!std::isfinite(trace.gx.at(i)) || !std::isfinite(trace.gy.at(i)) || !std::isfinite(trace.gz.at(i)))
        {
            r.sError = QString("sample %1 (%2 s) is not a number").arg(i).arg(i * trace.dt);
            return r;
        }
    }
    if (!limits.restraintKind(options.sRestraint))
    {
        const limitRestraintKind* dummy;
        kindFor(limits, options, r, &dummy);
        return r;
    }

    limitResultInit(r, n);
    forceTrace f = trace;
    if (options.bFilter)
    {
        const limitFilter filter = limitFilterDesign(limits.iFilterOrder, limits.filterCorner.dValue, trace.dt);
        if (!filter.bValid)
        {
            r.sError = QString("samples %1 s apart are too far apart for the %2 Hz filter").arg(trace.dt).arg(limits.filterCorner.dValue);
            return r;
        }
        const QVector<double>* in[3] = { &trace.gx, &trace.gy, &trace.gz };
        QVector<double>* out[3] = { &f.gx, &f.gy, &f.gz };
        limitFilterApplyMany(filter, in, out, 3, true, &r.iFilterShift);
        r.dFilterDelay = limitFilterDelay(filter) * trace.dt;
    }
    else
        r.lNotes << "The trace was judged as given, without the filter.";

    if (!limitStepDuration(f, limits, options, r) || !limitStepEggs(f, limits, options, r)
     || !limitStepReversal(f, limits, options, r) || !limitStepAirtime(f, limits, options, r)
     || !limitStepRestraint(trace, limits, r))
        return r;
    limitResultFinish(r);
    r.bValid = true;
    return r;
}
