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

#include "measure.h"
#include "track.h"
#include "section.h"
#include "mnode.h"
#include <cmath>

// Nothing in this file writes to a node or a track. Nodes are reached through
// const access only (a non-const QVector::operator[] could detach the
// section's node list); the few mnode member functions that read but are not
// declared const are called through a const_cast pointer.

const section* measureNodes::sectionAt(int index) const
{
    if(index < 0 || index >= sectionOf.size()) return NULL;
    return trk->lSections.at(sectionOf.at(index));
}

bool measureSpeedIsHeld(const section* s)
{
    if(!s) return false;
    switch(s->type) {
    case straight:
    case curved:
    case forced:
    case geometric:
        return !s->bSpeed;
    default:
        // A Bezier section takes its speed from its points (physics where a
        // point's speed is 0); a NoLimits CSV section has one constant speed.
        // The speed-difference form below gives the right answer for both.
        return false;
    }
}

measureNodes measureRegistry::collect(const track* t)
{
    measureNodes n;
    n.trk = t;
    if(!t) return n;
    for(int s = 0; s < t->lSections.size(); ++s) {
        const section* sec = t->lSections.at(s);
        const QVector<mnode>& list = sec->lNodes;
        // As track::getPoint(): a section's node 0 is the previous section's
        // last and belongs to it, so it is counted once.
        int start = (s == 0) ? 0 : 1;
        n.sectionFirst.append(s == 0 ? 0 : n.nodes.size() - 1);
        for(int j = start; j < list.size(); ++j) {
            n.nodes.append(const_cast<mnode*>(&list.at(j)));
            n.sectionOf.append(s);
        }
        n.sectionLast.append(n.nodes.size() - 1);
    }
    n.heartDist.resize(n.nodes.size());
    double d = 0.0;
    for(int i = 0; i < n.nodes.size(); ++i) {
        if(i > 0) {
            const glm::vec3& a = n.nodes.at(i-1)->vPos;
            const glm::vec3& b = n.nodes.at(i)->vPos;
            double dx = (double)b.x - a.x, dy = (double)b.y - a.y, dz = (double)b.z - a.z;
            d += std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        n.heartDist[i] = d;
    }
    return n;
}

// ---------------------------------------------------------------------------
// The fill functions, one per measure
// ---------------------------------------------------------------------------

static void fillSpeed(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->fVel;
}

static void fillHeight(const measureNodes& n, QVector<float>& out)
{
    float fHeart = n.trk->fHeart;
    float y0 = n.trk->startPos.y;
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->vPosHeart(fHeart).y + y0;
}

static void fillForceNormal(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->forceNormal + n.nodes.at(i)->smoothNormal;
}

static void fillForceLateral(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->forceLateral + n.nodes.at(i)->smoothLateral;
}

// The along-track force, in g, positive when the rider is pressed back into
// the seat: forceAlong = vDir.y + a_t / F_G, the along-track part of the same
// "up minus acceleration" the program uses for the normal and lateral force.
// a_t is the acceleration along the direction of travel, a_t = v dv/ds, taken
// as (v_hi^2 - v_lo^2) / (2 (s_hi - s_lo)) with v the node speed (fVel) and s
// the heartline distance (summed in double from vPos, see collect()).
//
// Why a distance form: force, geometric, straight and curved sections step
// 1/F_HZ per node (the last node of a straight or curve is a fractional step),
// Bezier nodes are about 1 ms apart at the local speed and NoLimits CSV nodes
// are evenly spaced, so node index is not time everywhere; v dv/ds is right
// for any spacing.
//
// Why a span of +-10 nodes (20 ms at 1 kHz): the speed is a 32-bit float made
// from an energy of a few hundred J/kg, so a difference over one node is
// noisy; measured on the baseline projects (tests/measures prints it) the
// noise is about 2e-5 to 4e-5 g over one node and 2e-6 to 5e-6 g over this
// span on ordinary track (3e-5 g over all of them, the broken test loops
// included), while 20 ms is far shorter than any change in ride forces (the
// plot's own rates use the same 20-node span). The window is
// clamped to the node's own section, so a jump in speed at a joint (a section
// that sets its speed) never leaks into its neighbours; near a section's ends
// the difference is one-sided. The section's node 0 is left out of the window:
// it is a copy of the previous section's last node (or of the anchor), whose
// stored energy can disagree with the section's own energy by about a
// centimetre of height, which shows as a one-step speed jump worth several
// hundredths of a g; the section's own nodes are consistent with each other.
//
// Held speed (measureSpeedIsHeld): a_t = 0 and forceAlong = vDir.y exactly.
// Zero or tiny spacing (a section of one node, or the train standing): the
// distance gives no acceleration and forceAlong = vDir.y.
void measureForceAlong(const measureNodes& n, QVector<float>& out, int halfSpan)
{
    if(halfSpan < 1) halfSpan = 1;
    for(int i = 0; i < n.size(); ++i) {
        const mnode* node = n.nodes.at(i);
        int s = n.sectionOf.at(i);
        const section* sec = n.trk->lSections.at(s);
        double along = node->vDir.y;
        if(!measureSpeedIsHeld(sec)) {
            int lo = qMax(i - halfSpan, n.sectionFirst.at(s) + 1);
            int hi = qMin(i + halfSpan, n.sectionLast.at(s));
            double ds = n.heartDist.at(hi) - n.heartDist.at(lo);
            if(hi > lo && ds > 1e-6) {
                double vlo = n.nodes.at(lo)->fVel;
                double vhi = n.nodes.at(hi)->fVel;
                double at = (vhi*vhi - vlo*vlo) / (2.0*ds);
                along += at / F_G;
            }
        }
        out[i] = (float)along;
    }
}

static void fillForceAlong(const measureNodes& n, QVector<float>& out)
{
    measureForceAlong(n, out, MEASURE_ALONG_HALFSPAN);
}

// The rate of change over MEASURE_RATE_SPAN nodes, written as the plot writes
// it (ui/graphhandler.cpp, time argument) so the numbers are the same to the
// bit: (x[j] - x[j-20]) * F_HZ / 20. For the first 20 nodes the plot compares
// a node with itself over one step, which gives 0 or a float rounding residue
// (a + b - a - b); this does the same.
static void rateStep(int j, int* prev, unsigned int* diff)
{
    if(j > MEASURE_RATE_SPAN - 1) {
        *prev = j - MEASURE_RATE_SPAN;
        *diff = MEASURE_RATE_SPAN;
    } else {
        *prev = j;
        *diff = 1;
    }
}

static void rateOf(const QVector<float>& x, QVector<float>& out)
{
    for(int j = 0; j < x.size(); ++j) {
        int pj;
        unsigned int diff;
        rateStep(j, &pj, &diff);
        out[j] = (x.at(j) - x.at(pj))*F_HZ/diff;
    }
}

static void fillJerkNormal(const measureNodes& n, QVector<float>& out)
{
    // Same expression and order as the plot's nForceChange case.
    for(int j = 0; j < n.size(); ++j) {
        int pj;
        unsigned int diff;
        rateStep(j, &pj, &diff);
        const mnode* c = n.nodes.at(j);
        const mnode* p = n.nodes.at(pj);
        out[j] = (c->forceNormal + c->smoothNormal - p->forceNormal - p->smoothNormal)*F_HZ/diff;
    }
}

static void fillJerkLateral(const measureNodes& n, QVector<float>& out)
{
    // Same expression and order as the plot's lForceChange case.
    for(int j = 0; j < n.size(); ++j) {
        int pj;
        unsigned int diff;
        rateStep(j, &pj, &diff);
        const mnode* c = n.nodes.at(j);
        const mnode* p = n.nodes.at(pj);
        out[j] = (c->forceLateral + c->smoothLateral - p->forceLateral - p->smoothLateral)*F_HZ/diff;
    }
}

static void fillJerkAlong(const measureNodes& n, QVector<float>& out)
{
    QVector<float> along(n.size());
    fillForceAlong(n, along);
    rateOf(along, out);
}

static void fillRollRate(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->fRollSpeed + n.nodes.at(i)->fSmoothSpeed;
}

static void fillRollAccel(const measureNodes& n, QVector<float>& out)
{
    // Same expression and order as the plot's rollAccel case (quaternion form).
    for(int j = 0; j < n.size(); ++j) {
        int pj;
        unsigned int diff;
        rateStep(j, &pj, &diff);
        const mnode* c = n.nodes.at(j);
        const mnode* p = n.nodes.at(pj);
        out[j] = (c->fRollSpeed + c->fSmoothSpeed - p->fRollSpeed - p->fSmoothSpeed)*F_HZ/diff;
    }
}

static void fillPitchRate(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->getPitchChange();
}

static void fillYawRate(const measureNodes& n, QVector<float>& out)
{
    for(int i = 0; i < n.size(); ++i) out[i] = n.nodes.at(i)->getYawChange();
}

// ---------------------------------------------------------------------------
// The registry
// ---------------------------------------------------------------------------

static measureDef def(const char* id, const char* name, const char* unit, const char* description,
                      float displayMin, float displayMax, measureFill fill)
{
    measureDef d;
    d.id = QString::fromLatin1(id);
    d.name = QString::fromLatin1(name);
    d.unit = QString::fromLatin1(unit);
    d.description = QString::fromLatin1(description);
    d.displayMin = displayMin;
    d.displayMax = displayMax;
    d.fill = fill;
    return d;
}

static QVector<measureDef> buildTable()
{
    QVector<measureDef> table;
    table
        << def("speed", "Speed", "m/s",
               "The train's speed as the program computes it.", 0.f, 40.f, fillSpeed)
        << def("height", "Height", "m",
               "Height of the track's centre line (the rail, not the heartline) above the ground.", 0.f, 100.f, fillHeight)
        << def("forceNormal", "Vertical force", "g",
               "Force pressing the rider into the seat, with roll smoothing included; 1 g when standing level.", -1.5f, 5.f, fillForceNormal)
        << def("forceLateral", "Lateral force", "g",
               "Sideways force on the rider, with roll smoothing included.", -1.5f, 1.5f, fillForceLateral)
        << def("forceAlong", "Along-track force", "g",
               "Force along the track, positive when the rider is pressed back into the seat (a launch, or a climb at held speed), negative when thrown forward (braking).", -1.5f, 1.5f, fillForceAlong)
        << def("jerkNormal", "Vertical force change", "g/s",
               "How fast the vertical force changes, over 20 ms as the plot shows it.", -20.f, 20.f, fillJerkNormal)
        << def("jerkLateral", "Lateral force change", "g/s",
               "How fast the lateral force changes, over 20 ms as the plot shows it.", -10.f, 10.f, fillJerkLateral)
        << def("jerkAlong", "Along-track force change", "g/s",
               "How fast the along-track force changes, over 20 ms.", -10.f, 10.f, fillJerkAlong)
        << def("rollRate", "Roll rate", "deg/s",
               "How fast the track rolls about its own direction, with roll smoothing included.", -300.f, 300.f, fillRollRate)
        << def("rollAccel", "Roll acceleration", "deg/s^2",
               "How fast the roll rate changes, over 20 ms as the plot shows it.", -1000.f, 1000.f, fillRollAccel)
        << def("pitchRate", "Pitch rate", "deg/s",
               "How fast the track turns up or down.", -200.f, 200.f, fillPitchRate)
        << def("yawRate", "Yaw rate", "deg/s",
               "How fast the track turns left or right.", -200.f, 200.f, fillYawRate);
    return table;
}

const QVector<measureDef>& measureRegistry::all()
{
    // built once, on first use (thread-safe in C++11)
    static const QVector<measureDef> table = buildTable();
    return table;
}

const measureDef* measureRegistry::find(const QString& id)
{
    const QVector<measureDef>& table = all();
    for(int i = 0; i < table.size(); ++i) {
        if(table.at(i).id == id) return &table.at(i);
    }
    return NULL;
}

QVector<float> measureRegistry::values(const track* t, const QString& id)
{
    return values(collect(t), QStringList(id)).value(0);
}

QVector<QVector<float> > measureRegistry::values(const track* t, const QStringList& ids)
{
    return values(collect(t), ids);
}

QVector<QVector<float> > measureRegistry::values(const measureNodes& n, const QStringList& ids)
{
    QVector<QVector<float> > result;
    for(int k = 0; k < ids.size(); ++k) {
        const measureDef* d = find(ids.at(k));
        QVector<float> v;
        if(d && n.size() > 0) {
            v.resize(n.size());
            d->fill(n, v);
        }
        result.append(v);
    }
    return result;
}
