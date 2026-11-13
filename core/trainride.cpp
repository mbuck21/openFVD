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

#include "trainride.h"
#include "track.h"
#include "section.h"
#include "mnode.h"
#include "measure.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Nothing in this file writes to a node or a track: nodes are read through
// measureRegistry::collect(), whose pointers are only read here.

// The standard gravity the program uses (F_G in lenassert.h), as a double.
static const double kG = 9.80665;

// The turning of the frame at an entry of the path is the change of the unit
// direction vectors over +-2 entries (about 2 ms either side at the program's
// 1 kHz node step), divided by the centre-line length between them (see
// "Distances" in ridePathFromTrack()). A first difference of unit vectors is quiet (their float
// rounding is 1e-7 against a change of about 1e-3 per node); a wider span was
// tried and blurs tight shapes where the turning changes fast (four-sections'
// tight loop: 0.15 g off over +-10 entries). The rate of change of that
// turning (for a seat's alpha x r term) is taken over +-10 entries of it.
// Near the ends of the track the span is one-sided. The span is not clamped at
// section joins: the shape is continuous there, and a jump in force at a join
// is spread over the span, which is why the comparison with the design leaves
// out the nodes near joins (tests/trainride).
static const int kTurnHalfSpan = 2;
static const int kRateHalfSpan = 10;

static bool finite3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static const char* typeName(int type)
{
    switch(type) {
    case straight: return "straight";
    case curved: return "curved";
    case forced: return "force";
    case geometric: return "geometric";
    case bezier: return "bezier";
    case nolimitscsv: return "nolimits-csv";
    default: return "anchor";
    }
}

ridePath::ridePath()
    : fHeart(0.0), fFriction(0.0), fResistance(0.0), startSpeed(0.0), startEnergyOffset(0.0), truncated(false),
      sectionCount(0)
{
}

// The change of the turning per metre of heartline, and of rh per metre of
// centre line, over +-kRateHalfSpan entries.
void ridePath::deriveRates()
{
    int n = s.size();
    dwD.fill(0.0, n);
    dwL.fill(0.0, n);
    dwN.fill(0.0, n);
    drh.fill(0.0, n);
    for(int i = 0; i < n; ++i) {
        int lo = qMax(0, i - kRateHalfSpan);
        int hi = qMin(n - 1, i + kRateHalfSpan);
        double ds = (hi > lo) ? s.at(hi) - s.at(lo) : 0.0;
        if(ds > 1e-9) drh[i] = (rh.at(hi) - rh.at(lo))/ds;
        double d = (hi > lo) ? sh.at(hi) - sh.at(lo) : 0.0;
        if(!(d > 1e-9)) continue;
        dwD[i] = (wD.at(hi) - wD.at(lo))/d;
        dwL[i] = (wL.at(hi) - wL.at(lo))/d;
        dwN[i] = (wN.at(hi) - wN.at(lo))/d;
    }
}

// The centre-line length of the step from node prev to node m.
//
// Distances. The ride measures the track by its centre line, summed in
// double. It does not use the nodes' fTotalLength: that is a 32-bit float
// summed node by node, which 3 km along can only move in steps of 0.00024 m,
// a few percent of a 1 ms step, and a step length that is a percent off goes
// straight into the turning and the slope. Nor are the nodes' float positions
// good enough for one step (the same rounding). The sections themselves say
// how far each step is: a force, geometric or curved section moves the
// centre line by the previous node's speed for 1/F_HZ s, a straight moves its
// heartline so (and its centre line winds round the heartline at fHeart while
// it rolls). That is used where the positions agree with it to 2 %; anywhere
// else (Bezier and NoLimits CSV sections, the last part-step of a straight or
// a curve) the distance between the two centre-line points, in double.
// The design itself uses fTotalLength only in its friction term (energy
// minus g (height + fTotalLength fFriction) in every section's update): the
// ride's friction runs on its own distance, and tests/trainride shows the
// point still gives the design's speed.
static double railStep(const mnode* prev, const mnode* m, const section* sec, double fHeart)
{
    glm::dvec3 a = glm::dvec3(prev->vPos) + fHeart*glm::dvec3(prev->vNorm);
    glm::dvec3 b = glm::dvec3(m->vPos) + fHeart*glm::dvec3(m->vNorm);
    double posStep = glm::length(b - a);
    if(!sec || !(prev->fVel > 0.f)) return posStep;
    double step = -1.0;
    if(sec->type == forced || sec->type == geometric || sec->type == curved) {
        step = (double)prev->fVel/F_HZ;
    } else if(sec->type == straight) {
        double heart = (double)prev->fVel/F_HZ;
        double heartPos = glm::length(glm::dvec3(m->vPos) - glm::dvec3(prev->vPos));
        if(std::fabs(heartPos - heart) > 0.02*heart) return posStep;
        // the roll per metre of heartline: the node's roll rate over the step's speed
        double w = (double)m->fRollSpeed*M_PI/180.0/(double)prev->fVel*fHeart;
        step = heart*std::sqrt(1.0 + w*w);
    }
    if(step > 0.0 && std::fabs(posStep - step) <= 0.02*step) return step;
    return posStep;
}

ridePath ridePathFromTrack(const track* t)
{
    ridePath p;
    if(!t) return p;
    measureNodes n = measureRegistry::collect(t);
    p.fHeart = t->fHeart;
    p.fFriction = t->fFriction;
    p.fResistance = t->fResistance;
    p.sectionCount = t->lSections.size();
    for(int k = 0; k < t->lSections.size(); ++k) p.sectionTypes << typeName(t->lSections.at(k)->type);
    int count = n.size();
    p.nodePlace.resize(count);
    p.nodeSection.resize(count);
    if(count == 0) return p;
    p.startSpeed = n.nodes.at(0)->fVel;
    {
        // The program's start energy is taken at a level start point (a load
        // builds it so, track.cpp), which can sit off the first node's own
        // energy point when the anchor is rolled or pitched; the design's speed
        // carries that offset along, so the ride does too.
        const mnode* a = n.nodes.at(0);
        double y09 = (double)a->vPos.y + 0.9*(double)t->fHeart*(double)a->vNorm.y;
        double off = ((double)a->fEnergy - 0.5*(double)a->fVel*a->fVel)/kG - y09;
        if(std::isfinite(off)) p.startEnergyOffset = off;
    }

    // The frame vectors of the entries, for the turning; dropped afterwards.
    QVector<glm::dvec3> D, L, N;
    QVector<bool> heldStraight;
    double lastPlace = 0.0, lastNodePlace = 0.0;
    bool stop = false;
    for(int i = 0; i < count; ++i) {
        const mnode* m = n.nodes.at(i);
        p.nodeSection[i] = n.sectionOf.at(i) + 1;
        bool ok = !stop && std::isfinite(m->fVel) && std::isfinite(m->fRollSpeed) && finite3(m->vPos)
                && finite3(m->vDir) && finite3(m->vLat) && finite3(m->vNorm);
        if(!ok) {
            // the design stalls here (or earlier): nothing after it is ridden
            if(!stop && i > 0) p.truncated = true;
            stop = true;
            p.nodePlace[i] = lastPlace;
            continue;
        }
        double place = i == 0 ? 0.0 : lastNodePlace + railStep(n.nodes.at(i - 1), m, n.sectionAt(i), t->fHeart);
        if(!std::isfinite(place) || place < lastNodePlace) place = lastNodePlace;
        lastNodePlace = place;
        p.nodePlace[i] = place;
        if(i > 0 && !(place > lastPlace + 1e-9)) continue;     // a repeated place: not a new entry
        lastPlace = place;
        const section* sec = n.sectionAt(i);
        p.s.append(place);
        p.heartY.append(m->vPos.y);
        p.dirY.append(m->vDir.y);
        p.latY.append(m->vLat.y);
        p.normY.append(m->vNorm.y);
        p.held.append(measureSpeedIsHeld(sec) ? (double)sec->fVel : -1.0);
        heldStraight.append(measureSpeedIsHeld(sec) && sec->type == straight);
        p.node.append(i);
        p.section.append(n.sectionOf.at(i) + 1);
        D.append(glm::dvec3(m->vDir));
        L.append(glm::dvec3(m->vLat));
        N.append(glm::dvec3(m->vNorm));
    }
    if(stop && p.s.size() < 2) {
        p.truncated = true;
    }

    // The turning of the frame per metre of centre line (W), over
    // +-kTurnHalfSpan entries. The heartline sits fHeart from the centre line
    // toward the rider's head; moving with the frame, the centre line runs
    // (1 + fHeart wL, -fHeart wD) per metre of heartline (w per metre of
    // heartline), so the heartline's length per metre of centre line is
    //     rh = sqrt(1 - (fHeart WD)^2) - fHeart WL,
    // and w = W / rh. Taken from the frame, not from float distances.
    int e = p.s.size();
    p.wD.fill(0.0, e);
    p.wL.fill(0.0, e);
    p.wN.fill(0.0, e);
    p.rh.fill(1.0, e);
    p.sh.fill(0.0, e);
    for(int i = 0; i < e; ++i) {
        int lo = qMax(0, i - kTurnHalfSpan);
        int hi = qMin(e - 1, i + kTurnHalfSpan);
        double ds = (hi > lo) ? p.s.at(hi) - p.s.at(lo) : 0.0;
        if(ds > 1e-9) {
            glm::dvec3 dD = D.at(hi) - D.at(lo);
            glm::dvec3 dL = L.at(hi) - L.at(lo);
            double WD = glm::dot(dL, N.at(i))/ds, WL = -glm::dot(dD, N.at(i))/ds, WN = glm::dot(dD, L.at(i))/ds;
            double h = p.fHeart;
            double rh = std::sqrt(qMax(0.0, 1.0 - h*h*WD*WD)) - h*WL;
            // a loop tighter than its heartline height turns the heartline back
            // on itself; keep a small positive length there
            rh = qMax(rh, 0.02);
            p.rh[i] = rh;
            p.wD[i] = WD/rh;
            p.wL[i] = WL/rh;
            p.wN[i] = WN/rh;
        }
        if(i > 0) p.sh[i] = p.sh.at(i - 1) + 0.5*(p.rh.at(i) + p.rh.at(i - 1))*(p.s.at(i) - p.s.at(i - 1));
    }
    // A straight section moves its heartline at its speed (secstraight.cpp);
    // every other kind moves the centre line at it. So a held straight holds
    // the heartline's speed. Where the straight rolls, its centre line winds
    // round the heartline at fHeart and runs sqrt(1 + (fHeart wD)^2) times as
    // far: the ride, which counts speed on the centre line, holds that much
    // more there. (Taken from the frame's roll, not from the nodes' float
    // distances, which are a thousandth off from step to step.) A free rolling
    // straight has the same mix in the program's energy rule; the ride keeps
    // the centre line's speed there, which differs by under 1 % of the speed
    // while the straight rolls.
    for(int i = 0; i < e; ++i) {
        if(heldStraight.at(i)) p.held[i] *= std::sqrt(1.0 + p.fHeart*p.fHeart*p.wD.at(i)*p.wD.at(i));
    }
    p.deriveRates();
    return p;
}

rideOptions::rideOptions()
    : sampleInterval_s(0.01), step_s(1.0/1000.0), maxTime_s(3600.0), floaterUpperG(0.1), ejectorUpperG(-0.5)
{
}

trainRide::trainRide()
    : valid(false), friction(0.0), rollingLoad(0.0), linear(0.0), drag(0.0), trackStart_m(0.0), trackEnd_m(0.0),
      stalled(false), stallTime_s(0.0), stallPlace_m(0.0), stallNode(-1), stallSection(0), nodesOnPath(0)
{
}

// ---------------------------------------------------------------------------
// Looking up the path at any centre-line distance
// ---------------------------------------------------------------------------

namespace {

struct pathPoint
{
    double massY;       // height of the mass point, m
    double slope;       // its rise per centre-line metre
    double dirY, latY, normY;
    double wD, wL, wN, dwD, dwL, dwN;
    double rh;
    double drh;         // rh's change per metre of centre line
    double held;        // held speed, or -1
};

class pathLookup
{
public:
    pathLookup(const ridePath& path, double massAboveRail)
        : p(path), mAbove(massAboveRail)
    {
        int n = p.size();
        massY.resize(n);
        for(int i = 0; i < n; ++i) {
            // rail = heartline + fHeart vNorm; the mass point is massAboveRail
            // toward the rider's head (-vNorm) from the rail. With 0.1 fHeart
            // (the default) it is the program's vPosHeart(0.9 fHeart).
            massY[i] = p.heartY.at(i) + (p.fHeart - massAboveRail)*p.normY.at(i);
        }
    }

    double start() const { return p.s.first(); }
    double end() const { return p.s.last(); }

    // hint: the segment found last time for this caller (cars only move on)
    pathPoint at(double x, int* hint) const
    {
        pathPoint r;
        int n = p.size();
        if(x <= p.s.at(0) || x >= p.s.at(n - 1)) {
            // beyond either end: the track continues straight along its end direction
            int i = (x <= p.s.at(0)) ? 0 : n - 1;
            double out = x - p.s.at(i);
            r.dirY = p.dirY.at(i);
            r.latY = p.latY.at(i);
            r.normY = p.normY.at(i);
            r.slope = r.dirY;
            r.massY = massY.at(i) + r.dirY*out;
            r.wD = r.wL = r.wN = r.dwD = r.dwL = r.dwN = r.drh = 0.0;
            r.rh = 1.0;
            r.held = p.held.at(i);
            if(out == 0.0) {
                // exactly at the end node: its own turning
                r.wD = p.wD.at(i); r.wL = p.wL.at(i); r.wN = p.wN.at(i);
                r.dwD = p.dwD.at(i); r.dwL = p.dwL.at(i); r.dwN = p.dwN.at(i);
                r.rh = p.rh.at(i);
                r.drh = p.drh.at(i);
                r.slope = r.rh*(r.dirY + (p.fHeart - mAbove)*(r.wL*r.dirY - r.wD*r.latY));
            }
            return r;
        }
        int i = hint ? *hint : 0;
        if(i < 0 || i > n - 2 || !(p.s.at(i) <= x && x < p.s.at(i + 1))) {
            if(i >= 0 && i < n - 2 && p.s.at(i + 1) <= x && x < p.s.at(i + 2)) {
                ++i;
            } else {
                i = int(std::upper_bound(p.s.constBegin(), p.s.constEnd(), x) - p.s.constBegin()) - 1;
                i = qBound(0, i, n - 2);
            }
        }
        if(hint) *hint = i;
        double f = (x - p.s.at(i))/(p.s.at(i + 1) - p.s.at(i));
        r.massY = massY.at(i) + f*(massY.at(i + 1) - massY.at(i));
        r.dirY = lerp(p.dirY, i, f);
        r.latY = lerp(p.latY, i, f);
        r.normY = lerp(p.normY, i, f);
        r.wD = lerp(p.wD, i, f);
        r.wL = lerp(p.wL, i, f);
        r.wN = lerp(p.wN, i, f);
        r.dwD = lerp(p.dwD, i, f);
        r.dwL = lerp(p.dwL, i, f);
        r.dwN = lerp(p.dwN, i, f);
        r.rh = lerp(p.rh, i, f);
        r.drh = lerp(p.drh, i, f);
        // The mass point's rise per metre of centre line, from the frame (not
        // from the float heights of two neighbouring nodes): the heartline
        // rises dirY per metre of heartline, and the mass point, (fHeart -
        // massAboveRail) toward the rail from it, turns with the frame:
        // d(vNorm.y)/ds_heart = wL dirY - wD latY.
        r.slope = r.rh*(r.dirY + (p.fHeart - mAbove)*(r.wL*r.dirY - r.wD*r.latY));
        // the stretch between two entries belongs to the section of the one ahead
        r.held = p.held.at(i + 1);
        return r;
    }

    // The last place in (p0, p1] where a held stretch ends (the entry is held,
    // the stretch after it free), or -infinity when there is none.
    double heldEnd(double p0, double p1) const
    {
        double found = -std::numeric_limits<double>::infinity();
        int n = p.size();
        if(p1 <= p0 || p1 < p.s.at(0) || p0 >= p.s.at(n - 1)) return found;
        int i = int(std::upper_bound(p.s.constBegin(), p.s.constEnd(), p0) - p.s.constBegin());
        for(; i < n - 1 && p.s.at(i) <= p1; ++i) {
            if(p.held.at(i) >= 0.0 && p.held.at(i + 1) < 0.0) found = p.s.at(i);
        }
        return found;
    }

private:
    static double lerp(const QVector<double>& a, int i, double f) { return a.at(i) + f*(a.at(i + 1) - a.at(i)); }
    const ridePath& p;
    double mAbove;
    QVector<double> massY;
};

// What a rider feels (gz, gy, gx) at a point of the car at a place: at the
// heartline (y = u = 0) or at a seat y across (+ toward vLat) and u up (+
// toward the rider's head) from it. v is the centre-line speed, a its rate of
// change.
//
// One rule for every point. The car's frame (vDir, vLat, vNorm) turns at
// omega = vh w, with vh = rh v the heartline's speed and w the turning per
// metre of heartline; the heartline point H moves along vDir at vh, so its
// acceleration in the frame is
//     along  d(vh)/dt = rh a + v^2 drh/ds
//     normal vh^2 wL (toward the head), sideways vh v wN,
// and any point at offset r from H has a_H + alpha x r + omega x (omega x r),
// alpha = d(omega)/dt = d(vh)/dt w + vh^2 dw/ds_heart (R3 section 7). The
// heartline is the car's own value (r = 0). This is the same as taking the
// centre line's acceleration (a along, v omega across) and carrying it up by
// fHeart with the rigid-body rule, where the centre line runs along vDir;
// the normal part gives the program's own vh^2 wL there. The sideways part
// keeps the program's vh v wN (secforced::updateSection()), so one car at the
// program's point reproduces the design's lateral force.
// The along part is neither R7's a (the centre line's) nor the Stage 2
// measure forceAlong's a / rh; trainRide::accel_mps2 keeps a for both.
void feltAt(const pathPoint& q, double v, double a, double y, double u, double out[3])
{
    double vh = v*q.rh;
    double ah = a*q.rh + v*v*q.drh;
    double gz = -q.normY + q.wL*vh*vh/kG;
    double gy = -q.latY - q.wN*vh*v/kG;
    double gx = q.dirY + ah/kG;
    if(y != 0.0 || u != 0.0) {
        // in the right-handed frame (vDir, vLat, vNorm); r = (0, y, -u)
        double wx = vh*q.wD, wy = vh*q.wL, wz = vh*q.wN;
        double ax = ah*q.wD + vh*vh*q.dwD, ay = ah*q.wL + vh*vh*q.dwL, az = ah*q.wN + vh*vh*q.dwN;
        double rx = 0.0, ry = y, rz = -u;
        // alpha x r
        double c1x = ay*rz - az*ry, c1y = az*rx - ax*rz, c1z = ax*ry - ay*rx;
        // omega x (omega x r) = omega (omega.r) - r |omega|^2
        double wr = wx*rx + wy*ry + wz*rz, ww = wx*wx + wy*wy + wz*wz;
        double c2x = wx*wr - rx*ww, c2y = wy*wr - ry*ww, c2z = wz*wr - rz*ww;
        double dx = c1x + c2x, dy = c1y + c2y, dz = c1z + c2z;
        gx += dx/kG;
        gy -= dy/kG;
        gz -= dz/kG;
    }
    out[rideGz] = gz;
    out[rideGy] = gy;
    out[rideGx] = gx;
}

int nearestNode(const QVector<double>& places, int onPath, double x)
{
    if(onPath <= 0 || x < places.at(0) || x > places.at(onPath - 1)) return -1;
    int i = int(std::lower_bound(places.constBegin(), places.constBegin() + onPath, x) - places.constBegin());
    if(i >= onPath) return onPath - 1;
    if(i > 0 && x - places.at(i - 1) <= places.at(i) - x) return i - 1;
    return i;
}

QString metres(double v) { return QString::number(v, 'f', 1); }

} // namespace

// ---------------------------------------------------------------------------
// The ride
// ---------------------------------------------------------------------------

trainRide rideTrain(const track* t, const trainSpec& spec, const rideOptions& options)
{
    if(!t) {
        trainRide r;
        r.spec = spec;
        r.options = options;
        r.error = "there is no track";
        return r;
    }
    return rideTrainOnPath(ridePathFromTrack(t), spec, options);
}

// Held speed. A section with a held speed holds the train exactly while every
// car (its centre) is on held-speed track. While the train straddles held and
// free track the held part can push but not hold back: the train is held at
// the held speed only while running free would make it slower, and runs free
// as soon as free running makes it faster (R7's chain rule; decision record
// 0014 point 6). For one car this is the design's own rule. A stand-in until
// lifts and brakes are section kinds of their own (P3). Choices where the rule
// says nothing, made here:
// - Cars on held sections of different speeds: the speed of the rearmost car
//   that is on held track counts (a lift pushes from behind; R7's prototype
//   looked at the rear car's chain).
// - A held section slower than the arriving train (a brake run): the train
//   runs free until its last car is on the brake, then drops to the brake's
//   speed at once. That drop is not smoothed and its deceleration is not in
//   gx (gx is the free-running or held acceleration only); the ride notes it.
// - A held section faster than the train when free (a launch): it lifts the
//   train to its speed as soon as one car is on it, at once, likewise.
// - The track beyond either end has the held state of the end section, so the
//   cars of a train that starts on a lift stand on the lift (R7's layout).
// - A car is on held track when its centre is.
trainRide rideTrainOnPath(const ridePath& path, const trainSpec& spec, const rideOptions& optionsIn)
{
    trainRide r;
    r.spec = spec;
    r.options = optionsIn;
    r.nodePlace = path.nodePlace;
    r.nodeSection = path.nodeSection;
    QString why;
    if(!spec.isValid(&why)) {
        r.error = "the train is not valid: " + why;
        return r;
    }
    if(path.isEmpty()) {
        r.error = path.truncated ? "the track's own speed is not a number from its start (the design stalls)"
                                 : "the track has fewer than two nodes";
        return r;
    }
    if(!(optionsIn.step_s > 0.0) || !(optionsIn.sampleInterval_s > 0.0) || !(optionsIn.maxTime_s > 0.0)) {
        r.error = "the step, the sample interval and the longest time must be above 0";
        return r;
    }
    if(!std::isfinite(path.startSpeed) || path.startSpeed < 0.0) {
        r.error = "the speed at the track's start is not a number of 0 or more";
        return r;
    }

    const int cars = spec.iCars;
    const int rows = spec.rows();
    const int seats = spec.seatsPerRow();
    int every = qMax(1, (int)std::floor(optionsIn.sampleInterval_s/optionsIn.step_s + 0.5));
    const double dt = optionsIn.step_s;
    r.options.sampleInterval_s = every*dt;

    if(spec.resistance.bFromProject) {
        r.friction = path.fFriction;
        r.drag = path.fResistance;
        r.rollingLoad = 0.0;
        r.linear = 0.0;
    } else {
        r.friction = spec.resistance.fRolling;
        r.rollingLoad = spec.resistance.fRollingLoad;
        r.linear = spec.resistance.fLinear;
        r.drag = spec.resistance.fDrag;
    }

    pathLookup look(path, spec.bMassAtDesignPoint ? 0.1*path.fHeart : (double)spec.fMassAboveRail);
    r.trackStart_m = look.start();
    r.trackEnd_m = look.end();
    QVector<double> weight(cars);
    double total = spec.totalMass();
    for(int c = 0; c < cars; ++c) {
        r.carOffset_m.append(spec.carOffset(c));
        weight[c] = spec.carMass(c)/total;
    }
    for(int k = 0; k < rows; ++k) r.rowOffset_m.append(spec.lRowOffsets.at(k));
    double rearmost = 0.0;
    for(int c = 0; c < cars; ++c) {
        for(int k = 0; k < rows; ++k) rearmost = qMin(rearmost, r.carOffset_m.at(c) + r.rowOffset_m.at(k));
    }
    // The reference starts at the track's start; the ride ends once the
    // rearmost row has passed the last node.
    const double xStart = r.trackStart_m;
    const double xStop = r.trackEnd_m - rearmost;
    // the global nodes the path covers (all of them unless the design stalls)
    const int onPath = path.truncated ? path.node.last() + 1 : path.nodePlace.size();
    r.nodesOnPath = onPath;

    QVector<int> hint(cars, 0);
    auto meanHeight = [&](double x, double* slope, int* heldCount, double* heldSpeed) {
        double y = 0.0, sl = 0.0;
        *heldCount = 0;
        *heldSpeed = -1.0;
        for(int c = 0; c < cars; ++c) {
            pathPoint q = look.at(x + r.carOffset_m.at(c), &hint[c]);
            y += weight.at(c)*q.massY;
            sl += weight.at(c)*q.slope;
            if(q.held >= 0.0) {
                ++*heldCount;
                *heldSpeed = q.held;     // cars run front to rear: the rearmost held car wins
            }
        }
        if(slope) *slope = sl;
        return y;
    };

    QVector<double> load(cars, 1.0);        // force pressing each car on the track, g (for fRollingLoad)
    auto carLoads = [&](double x, double v, double a) {
        for(int c = 0; c < cars; ++c) {
            pathPoint q = look.at(x + r.carOffset_m.at(c), &hint[c]);
            double f[3];
            feltAt(q, v, a, 0.0, 0.0, f);
            load[c] = std::sqrt(f[rideGz]*f[rideGz] + f[rideGy]*f[rideGy]);
        }
    };
    auto loadBar = [&]() {
        double b = 0.0;
        for(int c = 0; c < cars; ++c) b += weight.at(c)*load.at(c);
        return b;
    };

    // the acceleration along the track: 0 while held, otherwise from the energy rule
    auto accel = [&](double v, double slope, bool held) {
        if(held) return 0.0;
        return -(kG*r.friction + kG*r.rollingLoad*loadBar() + r.linear*v + r.drag*v*v) - kG*slope;
    };

    double x = xStart;
    double v = path.startSpeed;
    double slope = 0.0;
    int heldCount = 0;
    double heldSpeed = -1.0;
    double ybar = meanHeight(x, &slope, &heldCount, &heldSpeed);
    double energy = 0.5*v*v + kG*(ybar + path.startEnergyOffset + r.friction*(x - xStart));
    bool heldNow = (heldCount == cars);
    bool notedBrake = false, notedLaunch = false, notedStraddle = false;
    double a = accel(v, slope, heldNow);
    if(r.rollingLoad > 0.0) carLoads(x, v, a);

    QVector<int> rowHint(cars*rows, 0);
    auto record = [&](double time) {
        r.time_s.append(time);
        r.position_m.append(x);
        r.speed_mps.append(v);
        r.accel_mps2.append(a);
        for(int c = 0; c < cars; ++c) {
            pathPoint q = look.at(x + r.carOffset_m.at(c), &hint[c]);
            double f[3];
            feltAt(q, v, a, 0.0, 0.0, f);       // a car: at the heartline at its centre
            r.carG << f[0] << f[1] << f[2];
        }
        for(int c = 0; c < cars; ++c) {
            for(int k = 0; k < rows; ++k) {
                pathPoint q = look.at(x + r.carOffset_m.at(c) + r.rowOffset_m.at(k), &rowHint[c*rows + k]);
                for(int s = 0; s < seats; ++s) {
                    double f[3];
                    feltAt(q, v, a, spec.lSeatOffsets.at(s), spec.fSeatUp, f);
                    r.seatG << f[0] << f[1] << f[2];
                }
            }
        }
    };

    const qint64 maxSteps = (qint64)std::ceil(optionsIn.maxTime_s/dt);
    // samples are reserved for the ride as the design times it (its node count)
    int expected = qMin(qMax(16, (int)((path.node.last() + 1)*0.001/r.options.sampleInterval_s) + 16), 2000000);
    r.time_s.reserve(expected);
    r.carG.reserve(expected*cars*3);
    r.seatG.reserve(expected*cars*rows*seats*3);
    record(0.0);
    qint64 step = 0;
    bool done = (x >= xStop);
    while(!done) {
        double vStep = v;
        double xn = x + vStep*dt;
        // the losses over the distance run (the program: fResistance v^3 / F_HZ per millisecond)
        energy -= (kG*r.rollingLoad*loadBar() + r.linear*vStep + r.drag*vStep*vStep)*vStep*dt;
        if(heldNow) {
            // A car leaves held-speed track during this step: the energy is
            // taken where the held stretch ends, as the design takes it at the
            // held section's last node, not at the step's start (on a slope that
            // is worth a few thousandths of a m/s).
            double xb = -std::numeric_limits<double>::infinity();
            for(int c = 0; c < cars; ++c) {
                double o = r.carOffset_m.at(c);
                xb = qMax(xb, look.heldEnd(x + o, xn + o) - o);
            }
            if(xb > x) {
                int hc;
                double hs, sl;
                double yb = meanHeight(xb, &sl, &hc, &hs);
                energy = 0.5*vStep*vStep + kG*(yb + r.friction*(xb - xStart));
            }
        }
        ybar = meanHeight(xn, &slope, &heldCount, &heldSpeed);
        double e = energy - kG*(ybar + r.friction*(xn - xStart));
        double vFree = e > 0.0 ? std::sqrt(2.0*e) : 0.0;
        ++step;
        x = xn;
        bool stop = false;
        if(heldCount == cars || (heldCount > 0 && vFree < heldSpeed)) {
            if(heldCount == cars && vFree > heldSpeed + 1e-9 && !notedBrake && cars > 1) {
                notedBrake = true;
                r.notes << QString("At %1 m the whole train is on a held-speed section slower than free running "
                                   "(a brake): the speed drops to %2 m/s at once.").arg(metres(x)).arg(heldSpeed, 0, 'f', 2);
            }
            if(heldCount < cars && !notedStraddle) {
                notedStraddle = true;
                r.notes << QString("From %1 m the train straddles held and free track and is held at %2 m/s while "
                                   "running free would be slower.").arg(metres(x)).arg(heldSpeed, 0, 'f', 2);
            }
            if(heldSpeed > v + 0.5 && !notedLaunch && step > 1) {
                notedLaunch = true;
                r.notes << QString("At %1 m a held-speed section faster than the train lifts it to %2 m/s at once.")
                           .arg(metres(x)).arg(heldSpeed, 0, 'f', 2);
            }
            v = heldSpeed;
            heldNow = true;
            energy = 0.5*v*v + kG*(ybar + r.friction*(x - xStart));
            if(!(v > 0.0)) stop = true;
        } else {
            heldNow = false;
            if(!(e > 0.0) || !std::isfinite(e)) {
                stop = true;
                v = 0.0;
            } else {
                v = vFree;
            }
        }
        double tNow = step*dt;
        if(stop) {
            a = 0.0;
            r.stalled = true;
            r.stallTime_s = tNow;
            r.stallPlace_m = x;
            r.stallNode = nearestNode(path.nodePlace, onPath, x);
            r.stallSection = r.stallNode >= 0 ? path.nodeSection.at(r.stallNode) : 0;
            QString where = r.stallNode >= 0
                    ? QString("at %1 m along the track (section %2)").arg(metres(x)).arg(r.stallSection)
                    : (x < r.trackStart_m ? QString("before the start of the track (%1 m)").arg(metres(x))
                                          : QString("on the straight beyond the end of the track (%1 m)").arg(metres(x)));
            r.stallText = heldNow
                    ? QString("The train stops %1, %2 s into the ride: it is held at %3 m/s.")
                      .arg(where).arg(tNow, 0, 'f', 2).arg(heldSpeed, 0, 'f', 2)
                    : QString("The train stalls %1, %2 s into the ride: it has no energy left for speed.")
                      .arg(where).arg(tNow, 0, 'f', 2);
            break;
        }
        a = accel(v, slope, heldNow);
        if(r.rollingLoad > 0.0) carLoads(x, v, a);
        bool atSample = (step % every) == 0;
        if(atSample) record(tNow);
        if(x >= xStop && atSample) done = true;
        if(!done && step >= maxSteps) {
            r.stalled = true;
            r.stallTime_s = tNow;
            r.stallPlace_m = x;
            r.stallNode = nearestNode(path.nodePlace, onPath, x);
            r.stallSection = r.stallNode >= 0 ? path.nodeSection.at(r.stallNode) : 0;
            r.stallText = QString("The train has not reached the end of the track after %1 s (it is at %2 m).")
                    .arg(tNow, 0, 'f', 0).arg(metres(x));
            break;
        }
    }
    if(path.truncated) {
        r.notes << QString("The design's own speed stops being a number after %1 m (the design stalls); the ride "
                           "ends there.").arg(metres(r.trackEnd_m));
    }
    r.valid = true;
    for(int c = 0; c < cars; ++c) r.cars.append(trainRideSummarize(r, c));
    return r;
}

// ---------------------------------------------------------------------------
// The summary and the by-place helpers
// ---------------------------------------------------------------------------

// The share of the straight piece from a to b (in time) that lies in lo < g <= hi.
static double bandShare(double a, double b, double lo, double hi)
{
    if(a == b) return (a > lo && a <= hi) ? 1.0 : 0.0;
    double t1 = (lo - a)/(b - a), t2 = (hi - a)/(b - a);
    double from = qMax(0.0, qMin(t1, t2)), to = qMin(1.0, qMax(t1, t2));
    return to > from ? to - from : 0.0;
}

rideCarSummary trainRideSummarize(const trainRide& ride, int car)
{
    rideCarSummary sum;
    if(car < 0 || car >= ride.carCount()) return sum;
    const double inf = std::numeric_limits<double>::infinity();
    double off = ride.carOffset_m.at(car);
    int onPath = ride.nodesOnPath;
    bool prevOn = false;
    double prevGz = 0.0;
    for(int k = 0; k < ride.samples(); ++k) {
        double place = ride.position_m.at(k) + off;
        bool on = place >= ride.trackStart_m && place <= ride.trackEnd_m;
        double gz = ride.car(k, car, rideGz);
        if(on) {
            for(int ax = 0; ax < 3; ++ax) {
                double g = ride.car(k, car, ax);
                rideExtreme* lows[2] = { &sum.low[ax], &sum.high[ax] };
                for(int w = 0; w < 2; ++w) {
                    rideExtreme& e = *lows[w];
                    bool better = !e.valid || (w == 0 ? g < e.value : g > e.value);
                    if(better) {
                        e.valid = true;
                        e.value = g;
                        e.time_s = ride.time_s.at(k);
                        e.place_m = place;
                        e.node = nearestNode(ride.nodePlace, onPath, place);
                        e.section = e.node >= 0 ? ride.nodeSection.at(e.node) : 0;
                    }
                }
            }
            if(prevOn) {
                double dt = ride.time_s.at(k) - ride.time_s.at(k - 1);
                sum.floater_s += dt*bandShare(prevGz, gz, ride.options.ejectorUpperG, ride.options.floaterUpperG);
                sum.ejector_s += dt*bandShare(prevGz, gz, -inf, ride.options.ejectorUpperG);
                sum.belowZero_s += dt*bandShare(prevGz, gz, -inf, 0.0);
            }
        }
        prevOn = on;
        prevGz = gz;
    }
    return sum;
}

// value(k) for a series whose place at sample k is position + offset
template <typename F>
static QVector<double> byPlace(const trainRide& ride, double offset, F value, QVector<bool>* passed)
{
    QVector<double> out;
    if(passed) passed->clear();
    if(!ride.valid || ride.samples() == 0) return out;
    int n = ride.nodePlace.size();
    out.resize(n);
    if(passed) passed->fill(false, n);
    int last = ride.samples() - 1;
    double p0 = ride.position_m.at(0) + offset, pLast = ride.position_m.at(last) + offset;
    int k = 0;
    for(int i = 0; i < n; ++i) {
        double x = ride.nodePlace.at(i);
        if(i >= ride.nodesOnPath) { out[i] = value(last); continue; }     // beyond the design's stall
        if(x < p0) { out[i] = value(0); continue; }
        if(x > pLast) { out[i] = value(last); continue; }
        while(k < last - 1 && ride.position_m.at(k + 1) + offset < x) ++k;
        double pa = ride.position_m.at(k) + offset;
        double pb = (k < last) ? ride.position_m.at(k + 1) + offset : pa;
        if(k >= last || !(pb > pa)) {
            out[i] = value(k);
        } else {
            double f = (x - pa)/(pb - pa);
            f = qBound(0.0, f, 1.0);
            out[i] = value(k) + f*(value(k + 1) - value(k));
        }
        if(passed) (*passed)[i] = true;
    }
    return out;
}

QVector<double> trainRideCarByNode(const trainRide& ride, int car, int axis, QVector<bool>* passed)
{
    if(car < 0 || car >= ride.carCount() || axis < 0 || axis > 2) {
        if(passed) passed->clear();
        return QVector<double>();
    }
    return byPlace(ride, ride.carOffset_m.at(car), [&](int k) { return ride.car(k, car, axis); }, passed);
}

QVector<double> trainRideSeatByNode(const trainRide& ride, int car, int row, int seat, int axis, QVector<bool>* passed)
{
    if(car < 0 || car >= ride.carCount() || row < 0 || row >= ride.rowCount() || seat < 0 || seat >= ride.seatCount()
            || axis < 0 || axis > 2) {
        if(passed) passed->clear();
        return QVector<double>();
    }
    return byPlace(ride, ride.carOffset_m.at(car) + ride.rowOffset_m.at(row),
                   [&](int k) { return ride.seat(k, car, row, seat, axis); }, passed);
}

QVector<double> trainRideSpeedByNode(const trainRide& ride, int car, QVector<bool>* passed)
{
    if(car < 0 || car >= ride.carCount()) {
        if(passed) passed->clear();
        return QVector<double>();
    }
    return byPlace(ride, ride.carOffset_m.at(car), [&](int k) { return ride.speed_mps.at(k); }, passed);
}
