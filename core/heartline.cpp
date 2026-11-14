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

#include "heartline.h"
#include "track.h"
#include "section.h"
#include "mnode.h"
#include "measure.h"

#include <cmath>

// Nothing in this file writes to a node or a track: nodes are read through a
// measureNodes (core/measure.h), whose pointers are only read here.

static bool finite3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Distances. The track is measured by its centre line, summed in double. Not
// by the nodes' fTotalLength: that is a 32-bit float summed node by node,
// which 3 km along can only move in steps of 0.00024 m, a few percent of a
// 1 ms step, and a step length that is a percent off goes straight into the
// turning and the slope. Nor are the nodes' float positions good enough for
// one step (the same rounding). The sections themselves say how far each step
// is: a force, geometric or curved section moves the centre line by the
// previous node's speed for 1/F_HZ s, a straight moves its heartline so (and
// its centre line winds round the heartline at fHeart while it rolls). That is
// used where the positions agree with it to 2 %; anywhere else (Bezier and
// NoLimits CSV sections, the last part-step of a straight or a curve) the
// distance between the two centre-line points, in double.
// The design itself uses fTotalLength only in its friction term (energy minus
// g (height + fTotalLength fFriction) in every section's update): the ride's
// friction runs on its own distance, and tests/trainride shows the point still
// gives the design's speed.
double heartlineRailStep(const mnode* prev, const mnode* m, const section* sec, double fHeart)
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

// rh's change per metre of centre line. rh carries the float rounding of the
// frame vectors (about 2e-6 from entry to entry), and at speed v its change
// counts v^2 / g times in the force, so it is not taken from two single values
// of rh but from the heartline's length: rh averaged over the
// HEARTLINE_STRETCH_HALFSPAN entries after the entry, (sh_hi - sh_i)/(s_hi - s_i),
// less rh averaged over those before it, over the distance between the two
// averages' middles. The heartline length sums rh, so the rounding of the
// single values mostly cancels in the averages; the result is a second
// difference of the heartline length, d^2 sh / ds^2. Near either end of the
// track, where one side is missing, the difference of rh across the span.
double heartlineStretchRate(const QVector<double>& sh, const QVector<double>& rh, const QVector<double>& s, int i)
{
    int n = s.size();
    int lo = qMax(0, i - HEARTLINE_STRETCH_HALFSPAN);
    int hi = qMin(n - 1, i + HEARTLINE_STRETCH_HALFSPAN);
    if(lo < i && i < hi) {
        double before = s.at(i) - s.at(lo), after = s.at(hi) - s.at(i);
        if(before > 1e-9 && after > 1e-9) {
            double r0 = (sh.at(i) - sh.at(lo))/before, r1 = (sh.at(hi) - sh.at(i))/after;
            return (r1 - r0)/(0.5*(s.at(hi) - s.at(lo)));
        }
    }
    double ds = (hi > lo) ? s.at(hi) - s.at(lo) : 0.0;
    if(ds > 1e-9) return (rh.at(hi) - rh.at(lo))/ds;
    return 0.0;
}

// The turning of the frame at an entry is the change of the unit direction
// vectors over +-HEARTLINE_TURN_HALFSPAN entries, divided by the centre-line
// length between them. A first difference of unit vectors is quiet (their
// float rounding is 1e-7 against a change of about 1e-3 per node); a wider
// span was tried and blurs tight shapes where the turning changes fast
// (four-sections' tight loop: 0.15 g off over +-10 entries). The rate of
// change of rh is taken over +-HEARTLINE_STRETCH_HALFSPAN entries
// (heartlineStretchRate()). Near the
// ends of the track the span is one-sided. The span is not clamped at section
// joins: the shape is continuous there, and a jump in force at a join is
// spread over the span.
//
// The heartline sits fHeart from the centre line toward the rider's head;
// moving with the frame, the centre line runs (1 + fHeart wL, -fHeart wD) per
// metre of heartline (w per metre of heartline), so the heartline's length per
// metre of centre line is
//     rh = sqrt(1 - (fHeart WD)^2) - fHeart WL
// with W the turning per metre of centre line, and w = W / rh. Taken from the
// frame, not from float distances.
heartlineFrame heartlineFrameOf(const measureNodes& n)
{
    heartlineFrame f;
    int count = n.size();
    f.nodePlace.resize(count);
    f.nodeEntry.resize(count);
    if(count == 0 || !n.trk) return f;
    const double fHeart = n.trk->fHeart;

    f.node.reserve(count);
    f.s.reserve(count);
    double lastPlace = 0.0, lastNodePlace = 0.0;
    bool stop = false;
    for(int i = 0; i < count; ++i) {
        const mnode* m = n.nodes.at(i);
        bool ok = !stop && std::isfinite(m->fVel) && std::isfinite(m->fRollSpeed) && finite3(m->vPos)
                && finite3(m->vDir) && finite3(m->vLat) && finite3(m->vNorm);
        if(!ok) {
            // the design stalls here (or earlier): nothing after it has an entry
            if(!stop && i > 0) f.truncated = true;
            stop = true;
            f.nodePlace[i] = lastPlace;
            f.nodeEntry[i] = -1;
            continue;
        }
        double place = i == 0 ? 0.0 : lastNodePlace + heartlineRailStep(n.nodes.at(i - 1), m, n.sectionAt(i), fHeart);
        if(!std::isfinite(place) || place < lastNodePlace) place = lastNodePlace;
        lastNodePlace = place;
        f.nodePlace[i] = place;
        if(i > 0 && !(place > lastPlace + 1e-9)) {     // a repeated place: not a new entry
            f.nodeEntry[i] = f.s.size() - 1;
            continue;
        }
        lastPlace = place;
        f.nodeEntry[i] = f.s.size();
        f.node.append(i);
        f.s.append(place);
    }
    if(stop && f.s.size() < 2) f.truncated = true;

    int e = f.s.size();
    f.wD.fill(0.0, e);
    f.wL.fill(0.0, e);
    f.wN.fill(0.0, e);
    f.rh.fill(1.0, e);
    for(int i = 0; i < e; ++i) {
        int lo = qMax(0, i - HEARTLINE_TURN_HALFSPAN);
        int hi = qMin(e - 1, i + HEARTLINE_TURN_HALFSPAN);
        double ds = (hi > lo) ? f.s.at(hi) - f.s.at(lo) : 0.0;
        if(ds > 1e-9) {
            const mnode* a = n.nodes.at(f.node.at(lo));
            const mnode* b = n.nodes.at(f.node.at(hi));
            const mnode* m = n.nodes.at(f.node.at(i));
            glm::dvec3 dD = glm::dvec3(b->vDir) - glm::dvec3(a->vDir);
            glm::dvec3 dL = glm::dvec3(b->vLat) - glm::dvec3(a->vLat);
            glm::dvec3 N(m->vNorm), L(m->vLat);
            double WD = glm::dot(dL, N)/ds, WL = -glm::dot(dD, N)/ds, WN = glm::dot(dD, L)/ds;
            double rh = std::sqrt(qMax(0.0, 1.0 - fHeart*fHeart*WD*WD)) - fHeart*WL;
            // a loop tighter than its heartline height turns the heartline back
            // on itself; keep a small positive length there
            rh = qMax(rh, 0.02);
            f.rh[i] = rh;
            f.wD[i] = WD/rh;
            f.wL[i] = WL/rh;
            f.wN[i] = WN/rh;
        }
    }
    // the heartline's length, summed from rh (trapezoids), and rh's change
    f.sh.fill(0.0, e);
    for(int i = 1; i < e; ++i) f.sh[i] = f.sh.at(i - 1) + 0.5*(f.rh.at(i) + f.rh.at(i - 1))*(f.s.at(i) - f.s.at(i - 1));
    f.drh.resize(e);
    for(int i = 0; i < e; ++i) f.drh[i] = heartlineStretchRate(f.sh, f.rh, f.s, i);
    return f;
}
