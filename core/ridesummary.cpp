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

#include "ridesummary.h"
#include "track.h"
#include "section.h"
#include "mnode.h"
#include "finding.h"
#include "measure.h"

#include <QStringList>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "../lenassert.h"

// Nothing in this file writes to a node or a track. As in measure.cpp, the few
// mnode member functions that read but are not declared const are called
// through a pointer taken from measureNodes.

// ---------------------------------------------------------------------------
// World placement (moved here from cli/project.cpp so the ride sheet and the
// command line share one copy; the maths is unchanged)
// ---------------------------------------------------------------------------

placement::placement(const track* t, bool world) : bWorld(world), base(1.f), fYaw(0.f)
{
    if(world) {
        fYaw = t->startYaw;
        base = glm::translate(t->startPos) * glm::rotate(TO_RAD(t->startYaw-90.f), glm::vec3(0.f, 1.f, 0.f));
    }
}

glm::vec3 placement::point(const glm::vec3& p) const
{
    return glm::vec3(base * glm::vec4(p, 1.f));
}

glm::vec3 placement::direction(const glm::vec3& d) const
{
    return glm::vec3(base * glm::vec4(d, 0.f));
}

float placement::heading(float rawHeading) const
{
    return rawHeading + fYaw;
}

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

QString rideSectionTypeName(int type)
{
    switch(type) {
    case anchor:        return "anchor";
    case straight:      return "straight";
    case curved:        return "curved";
    case forced:        return "force";
    case geometric:     return "geometric";
    case bezier:        return "bezier";
    case nolimitscsv:   return "nolimits-csv";
    }
    return "unknown";
}

static QString gText(float g)
{
    QString t = QString::number(g, 'g', 3);
    return (g > 0.f ? "+" : "") + t;
}

QString rideFloaterLabel(const rideSummaryOptions& o)
{
    return QString("Floater airtime (%1 g down to %2 g)").arg(gText(o.floaterUpperG)).arg(gText(o.ejectorUpperG));
}

QString rideEjectorLabel(const rideSummaryOptions& o)
{
    return QString("Ejector airtime (at or below %1 g)").arg(gText(o.ejectorUpperG));
}

QString rideInversionLabel(const rideSummaryOptions& o)
{
    return QString("Inversions (more than %1 degrees from upright)").arg(o.inversionAngleDeg, 0, 'g', 4);
}

QString rideSheetNote()
{
    return "These forces and the airtime are for the single point the program follows along the heartline; "
           "the front and rear cars of a train feel more (docs/research/r7-train-against-point.md).";
}

QString rideForceLimitsNote(const designLimitsInfo& info)
{
    QString text = "The force limits are a design aid taken from public secondary sources, not a certification: "
                   "they judge the single point the program follows, with the base restraint case.";
    if(info.sConfidence != QLatin1String("checked")) {
        text += " The numbers have not all been checked against the standard.";
    }
    return text;
}

void rideAirtime(const QVector<float>& normalForce, const QVector<double>& dt,
                 const rideSummaryOptions& options, double* floaterSeconds, double* ejectorSeconds)
{
    double floater = 0.0, ejector = 0.0;
    int n = qMin(normalForce.size(), dt.size());
    for(int i = 0; i < n; ++i) {
        float g = normalForce.at(i);
        if(!std::isfinite(g) || !std::isfinite(dt.at(i))) continue;
        if(g <= options.ejectorUpperG) ejector += dt.at(i);
        else if(g <= options.floaterUpperG) floater += dt.at(i);
    }
    if(floaterSeconds) *floaterSeconds = floater;
    if(ejectorSeconds) *ejectorSeconds = ejector;
}

int rideCountInversions(const QVector<double>& angleDeg, double angleLimitDeg, double releaseDeg)
{
    int count = 0;
    bool above = false;
    for(int i = 0; i < angleDeg.size(); ++i) {
        double a = angleDeg.at(i);
        if(!std::isfinite(a)) continue;
        if(!above && a > angleLimitDeg) {
            ++count;
            above = true;
        } else if(above && a < angleLimitDeg - releaseDeg) {
            above = false;
        }
    }
    return count;
}

// A fall is tracked as a crest (hi) and the lowest point since it (lo). It
// ends when the height climbs back to the crest, or climbs hysteresis above
// the lowest point; a smaller bump on the way down is part of the same fall.
double rideBiggestFall(const QVector<double>& heights, double hysteresis, bool climb, int* from, int* to)
{
    double sign = climb ? -1.0 : 1.0;
    double best = 0.0;
    int bestFrom = -1, bestTo = -1;
    int hi = -1, lo = -1;
    for(int i = 0; i < heights.size(); ++i) {
        if(!std::isfinite(heights.at(i))) continue;
        if(hi < 0) { hi = lo = i; continue; }
        double h = sign*heights.at(i);
        double crest = sign*heights.at(hi);
        double low = sign*heights.at(lo);
        bool ended = false;
        if(h >= crest) ended = true;
        else if(h < low) lo = i;
        else if(h - low >= hysteresis) ended = true;
        if(ended) {
            if(crest - low > best) { best = crest - low; bestFrom = hi; bestTo = lo; }
            hi = lo = i;
        }
    }
    if(hi >= 0) {
        double crest = sign*heights.at(hi), low = sign*heights.at(lo);
        if(crest - low > best) { best = crest - low; bestFrom = hi; bestTo = lo; }
    }
    if(from) *from = bestFrom;
    if(to) *to = bestTo;
    return best;
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

QVector<double> rideNodeTimes(const measureNodes& n)
{
    QVector<double> t(n.size());
    double now = 0.0;
    for(int i = 0; i < n.size(); ++i) {
        if(i > 0) {
            const section* sec = n.sectionAt(i);
            double step = 1.0/F_HZ;     // time-stepped: force, geometric, straight, curved
            if(sec && (sec->type == bezier || sec->type == nolimitscsv)) {
                // Spaced by distance: heartline distance over the mean speed.
                double d = n.heartDist.at(i) - n.heartDist.at(i-1);
                double v = 0.5*((double)n.nodes.at(i)->fVel + (double)n.nodes.at(i-1)->fVel);
                if(std::isfinite(d) && std::isfinite(v) && v > 0.0) step = d/v;
            }
            now += step;
        }
        t[i] = now;
    }
    return t;
}

// ---------------------------------------------------------------------------
// The ride sheet
// ---------------------------------------------------------------------------

rideSummary::rideSummary()
    : sectionCount(0), nodeCount(0), empty(true), nonFinite(false), length_m(0.0), heartLength_m(0.0),
      lengthStopsAtStall(false), duration_s(0.0), heightRange_m(0.0), floaterAirtime_s(0.0), ejectorAirtime_s(0.0), totalAirtime_s(0.0),
      inversions(0)
{
    findingCount[0] = findingCount[1] = findingCount[2] = 0;
}

static rideSpot spotAt(const measureNodes& n, const QVector<double>& times, int i, double value)
{
    rideSpot s;
    s.valid = true;
    s.node = i;
    s.section = n.sectionOf.at(i) + 1;
    s.time_s = times.at(i);
    s.dist_m = n.nodes.at(i)->fTotalLength;
    s.value = value;
    return s;
}

// The highest (or lowest) finite value; invalid when there is none. The first
// of equal values wins.
static rideSpot extreme(const measureNodes& n, const QVector<double>& times, const QVector<float>& v, bool highest)
{
    int best = -1;
    for(int i = 0; i < v.size(); ++i) {
        if(!std::isfinite(v.at(i))) continue;
        if(best < 0 || (highest ? v.at(i) > v.at(best) : v.at(i) < v.at(best))) best = i;
    }
    if(best < 0) return rideSpot();
    return spotAt(n, times, best, v.at(best));
}

static rideFall fallFrom(const measureNodes& n, const QVector<double>& times, const QVector<double>& heights,
                         double hysteresis, bool climb)
{
    rideFall f;
    int from, to;
    double amount = rideBiggestFall(heights, hysteresis, climb, &from, &to);
    // under a centimetre is level ground (float noise), not a drop
    if(from >= 0 && amount >= 0.01) {
        f.valid = true;
        f.amount_m = amount;
        f.start = spotAt(n, times, from, heights.at(from));
        f.end = spotAt(n, times, to, heights.at(to));
    }
    return f;
}

rideSummary computeRideSummary(const track* t, const rideSummaryOptions& options)
{
    rideSummary r;
    if(!t || t->lSections.isEmpty()) return r;
    measureNodes n = measureRegistry::collect(t);
    if(n.size() == 0) return r;

    r.empty = false;
    r.sectionCount = t->lSections.size();
    r.nodeCount = n.size();
    QVector<double> times = rideNodeTimes(n);
    int last = n.size() - 1;

    // Lengths and duration. On a stalled train the last node's length is not
    // a number; the length is then that of the last node whose length is one
    // (0 when none is), and lengthStopsAtStall says so.
    for(int i = last; i >= 0; --i) {
        if(std::isfinite(n.nodes.at(i)->fTotalLength)) { r.length_m = n.nodes.at(i)->fTotalLength; break; }
    }
    for(int i = last; i >= 0; --i) {
        if(std::isfinite(n.nodes.at(i)->fTotalHeartLength)) { r.heartLength_m = n.nodes.at(i)->fTotalHeartLength; break; }
    }
    r.lengthStopsAtStall = !std::isfinite(n.nodes.at(last)->fTotalLength) || !std::isfinite(n.nodes.at(last)->fTotalHeartLength);
    r.duration_s = times.at(last);

    // The measures, in one pass.
    QStringList ids;
    ids << "height" << "speed" << "forceNormal" << "forceLateral" << "forceAlong";
    QVector<QVector<float> > m = measureRegistry::values(n, ids);
    const QVector<float>& height = m.at(0);
    const QVector<float>& speed = m.at(1);
    const QVector<float>& normal = m.at(2);
    const QVector<float>& lateral = m.at(3);
    const QVector<float>& along = m.at(4);

    // Is anything not a number?
    for(int i = 0; i < n.size() && !r.nonFinite; ++i) {
        const mnode* node = n.nodes.at(i);
        if(!std::isfinite(speed.at(i)) || !std::isfinite(normal.at(i)) || !std::isfinite(lateral.at(i))
                || !std::isfinite(along.at(i)) || !std::isfinite(height.at(i))
                || !std::isfinite(node->fTotalLength)) {
            r.nonFinite = true;
        }
    }

    // Height
    r.highest = extreme(n, times, height, true);
    r.lowest = extreme(n, times, height, false);
    if(r.highest.valid && r.lowest.valid) r.heightRange_m = r.highest.value - r.lowest.value;
    QVector<double> heights(height.size());
    for(int i = 0; i < height.size(); ++i) heights[i] = height.at(i);
    r.biggestDrop = fallFrom(n, times, heights, options.dropHysteresisM, false);
    r.biggestClimb = fallFrom(n, times, heights, options.dropHysteresisM, true);

    // Speed
    r.topSpeed = extreme(n, times, speed, true);
    r.lowSpeed = extreme(n, times, speed, false);

    // Airtime: the time of the step that ends at each node.
    QVector<double> dt(n.size());
    dt[0] = 0.0;
    for(int i = 1; i < n.size(); ++i) dt[i] = times.at(i) - times.at(i-1);
    rideAirtime(normal, dt, options, &r.floaterAirtime_s, &r.ejectorAirtime_s);
    r.totalAirtime_s = r.floaterAirtime_s + r.ejectorAirtime_s;

    // Peak forces
    r.normalMax = extreme(n, times, normal, true);
    r.normalMin = extreme(n, times, normal, false);
    r.lateralRight = extreme(n, times, lateral, true);
    r.lateralLeft = extreme(n, times, lateral, false);
    r.alongMax = extreme(n, times, along, true);
    r.alongMin = extreme(n, times, along, false);

    // Inversions: the up direction is -vNorm, so the angle from upright is
    // acos(-vNorm.y).
    QVector<double> tilt(n.size());
    int tiltBest = -1;
    for(int i = 0; i < n.size(); ++i) {
        double y = -(double)n.nodes.at(i)->vNorm.y;
        if(!std::isfinite(y)) { tilt[i] = y; continue; }
        if(y > 1.0) y = 1.0;
        if(y < -1.0) y = -1.0;
        tilt[i] = std::acos(y)*180.0/F_PI;
        if(tiltBest < 0 || tilt.at(i) > tilt.at(tiltBest)) tiltBest = i;
    }
    r.inversions = rideCountInversions(tilt, options.inversionAngleDeg, options.inversionReleaseDeg);
    if(tiltBest >= 0) r.maxTilt = spotAt(n, times, tiltBest, tilt.at(tiltBest));

    // Findings
    // (the registry's checks in order; "forceLimits" is given the measures and times
    // computed above instead of computing them again)
    const QList<checkDef>& checks = checkRegistry::all();
    for(int c = 0; c < checks.size(); ++c) {
        if(checks.at(c).id == QLatin1String("forceLimits")) {
            r.findings << designPointForceLimits(times, along, lateral, normal, &r.forceLimits);
        } else {
            r.findings << checks.at(c).run(t);
        }
    }
    for(int i = 0; i < r.findings.size(); ++i) {
        int s = (int)r.findings.at(i).severity;
        if(s >= 0 && s <= 2) r.findingCount[s]++;
    }

    // Sections
    placement place(t, true);
    for(int s = 0; s < t->lSections.size(); ++s) {
        const section* sec = t->lSections.at(s);
        rideSectionInfo info;
        info.number = s + 1;
        info.type = (int)sec->type;
        info.typeName = rideSectionTypeName(info.type);
        info.name = sec->sName;
        info.firstNode = n.sectionFirst.at(s);
        info.lastNode = n.sectionLast.at(s);
        double l = n.nodes.at(info.lastNode)->fTotalLength - n.nodes.at(info.firstNode)->fTotalLength;
        info.length_m = std::isfinite(l) ? l : 0.0;
        info.duration_s = times.at(info.lastNode) - times.at(info.firstNode);
        mnode* end = n.nodes.at(info.lastNode);
        info.endPosition = place.point(end->vPosHeart(t->fHeart));
        info.endHeading_deg = place.heading(end->getDirection());
        info.endPitch_deg = end->getPitch();
        info.endBank_deg = end->fRoll;
        info.endSpeed_mps = end->fVel;
        r.sections.append(info);
    }
    return r;
}
