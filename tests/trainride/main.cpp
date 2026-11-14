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

// tests/trainride: core/trainspec.* and core/trainride.*, the train ridden
// over the finished track. The checks follow the "Proof" of piece P1 in
// docs/stage-3-plan.md:
//   (1) trainSpec::point() reproduces the design's speed and forces on every
//       baseline project (and the two import samples, whose Bezier and
//       NoLimits CSV sections are named and left out), and its along-track
//       force is the measure forceAlong's (issue 96);
//   (2) R7's tables (docs/research/r7-prototype/results.txt) with R7's three
//       trains on R7's projects;
//   (3) the closed form of R7 section 6;
//   (4) a seat r from the heartline of a straight rolling at a constant rate
//       feels omega^2 r, and the per-seat noise on a smooth track;
//   (5) a ride leaves the track's saved bytes unchanged;
// and the timing, the stall, the by-place helpers and trainSpec itself.

#include "../../core/track.h"
#include "../../core/section.h"
#include "../../core/function.h"
#include "../../core/subfunction.h"
#include "../../core/projectfile.h"
#include "../../core/measure.h"
#include "../../core/traintype.h"
#include "../../core/trainspec.h"
#include "../../core/trainride.h"
#include "../../lenassert.h"
#include "r7tables.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStringList>
#include <QtDebug>
#include <cmath>
#include <cstdio>
#include <sstream>

static bool allOk = true;
static const double kG = 9.80665;

static void report(bool ok, const QString& text)
{
    printf("%s: %s\n", ok ? "ok" : "FAIL", qPrintable(text));
    fflush(stdout);
    if(!ok) allOk = false;
}

// A stalled design makes the model's own NaN checks (lenAssert, a qCritical in
// a release build) fire; that is the case under test there.
static int expectedCriticals = 0;

static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg && expectedCriticals > 0 && msg.startsWith("Assertion")) return;
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
}

// Copied from tests/ridesummary/main.cpp (class coreProject): what the windowed
// program's projectWidget does for the file reader, without the windows.
class coreProject : public projectFileClient
{
public:
    coreProject() : texPath(":/background.png") {}
    ~coreProject() { clear(); }

    QList<track*> tracks;
    QString texPath;

    void clear()
    {
        while(!tracks.isEmpty()) {
            delete tracks.takeFirst();
        }
    }

    void projectCleared() { clear(); }

    bool projectGroundTexture(const QString& path)
    {
        texPath = path;
        return true;
    }

    track* projectNewTrack()
    {
        track* t = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
        t->name = QString("unnamed Track");
        t->display.colors[0] = QColor(20, 20, 130);
        t->display.colors[1] = QColor(255, 51, 51);
        t->display.colors[2] = QColor(51, 255, 51);
        tracks.append(t);
        return t;
    }

    void projectTrackSmoothingUpdated(track*) {}
    void projectTrackLoaded(track*) {}
    void projectLoadFinished() {}
    void projectTrackSaving(track*) {}
};

static bool load(const QString& file, coreProject& p)
{
    QString loaded = loadProjectFile(file, &p);
    return !loaded.startsWith("Error") && !p.tracks.isEmpty();
}

// Every byte of every node of every track (anchor included).
static QByteArray nodeBytes(coreProject& p)
{
    QByteArray b;
    for(int i = 0; i < p.tracks.size(); ++i) {
        track* t = p.tracks[i];
        b.append((const char*)t->anchorNode, sizeof(mnode));
        for(int s = 0; s < t->lSections.size(); ++s) {
            const QVector<mnode>& l = t->lSections.at(s)->lNodes;
            b.append((const char*)l.constData(), l.size()*sizeof(mnode));
        }
    }
    return b;
}

static QByteArray savedBytes(coreProject& p)
{
    std::stringstream file(std::ios::in | std::ios::out | std::ios::binary);
    writeProject(file, p.texPath, p.tracks, &p);
    std::string s = file.str();
    return QByteArray(s.data(), (int)s.size());
}

static bool allFinite(const QVector<double>& v)
{
    for(int i = 0; i < v.size(); ++i) if(!std::isfinite(v.at(i))) return false;
    return true;
}

// Every number in a ride is a number (nothing that is not leaves rideTrain()).
static bool rideFinite(const trainRide& r)
{
    bool ok = allFinite(r.time_s) && allFinite(r.position_m) && allFinite(r.speed_mps) && allFinite(r.accel_mps2)
            && allFinite(r.carG) && allFinite(r.seatG) && std::isfinite(r.stallTime_s) && std::isfinite(r.stallPlace_m)
            && std::isfinite(r.trackStart_m) && std::isfinite(r.trackEnd_m);
    for(int c = 0; c < r.cars.size(); ++c) {
        const rideCarSummary& s = r.cars.at(c);
        ok = ok && std::isfinite(s.floater_s) && std::isfinite(s.ejector_s) && std::isfinite(s.belowZero_s);
        for(int a = 0; a < 3; ++a) ok = ok && std::isfinite(s.low[a].value) && std::isfinite(s.high[a].value);
    }
    for(int c = 0; c < r.carCount(); ++c) {
        for(int a = 0; a < 3; ++a) ok = ok && allFinite(trainRideCarByNode(r, c, a));
        ok = ok && allFinite(trainRideSpeedByNode(r, c));
    }
    return ok;
}

static trainSpec makeTrain(int cars, double pitch)
{
    trainSpec t = trainSpec::point();
    t.iCars = cars;
    t.fCarPitch = (float)pitch;
    return t;
}

// The train's state when its reference is at place x (samples interpolated).
static double refInterp(const trainRide& r, double x, const QVector<double>& series)
{
    int n = r.samples();
    if(n == 0) return 0.0;
    if(x <= r.position_m.first()) return series.first();
    for(int k = 1; k < n; ++k) {
        if(r.position_m.at(k) >= x) {
            double a = r.position_m.at(k - 1), b = r.position_m.at(k);
            double f = b > a ? (x - a)/(b - a) : 0.0;
            return series.at(k - 1) + f*(series.at(k) - series.at(k - 1));
        }
    }
    return series.last();
}

// What a car feels (axis) when it passes place x.
static double carAt(const trainRide& r, int car, double x, int axis)
{
    double off = r.carOffset_m.at(car);
    int n = r.samples();
    for(int k = 1; k < n; ++k) {
        double a = r.position_m.at(k - 1) + off, b = r.position_m.at(k) + off;
        if(a <= x && x <= b) {
            double f = b > a ? (x - a)/(b - a) : 0.0;
            return r.car(k - 1, car, axis) + f*(r.car(k, car, axis) - r.car(k - 1, car, axis));
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// The train's acceleration along the centre line when a car passes place x.
static double accelWhenCarAt(const trainRide& r, int car, double x)
{
    double off = r.carOffset_m.at(car);
    for(int k = 1; k < r.samples(); ++k) {
        double a = r.position_m.at(k - 1) + off, b = r.position_m.at(k) + off;
        if(a <= x && x <= b) {
            double f = b > a ? (x - a)/(b - a) : 0.0;
            return r.accel_mps2.at(k - 1) + f*(r.accel_mps2.at(k) - r.accel_mps2.at(k - 1));
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// A path's vDir.y at centre-line place x.
static double dirYAt(const ridePath& p, double x)
{
    for(int i = 1; i < p.size(); ++i) {
        if(p.s.at(i) >= x) {
            double f = (x - p.s.at(i - 1))/(p.s.at(i) - p.s.at(i - 1));
            return p.dirY.at(i - 1) + f*(p.dirY.at(i) - p.dirY.at(i - 1));
        }
    }
    return p.dirY.last();
}

// The share of the straight piece a..b (in time) with lo < g <= hi.
static double share(double a, double b, double lo, double hi)
{
    if(a == b) return (a > lo && a <= hi) ? 1.0 : 0.0;
    double t1 = (lo - a)/(b - a), t2 = (hi - a)/(b - a);
    double from = qMax(0.0, qMin(t1, t2)), to = qMin(1.0, qMax(t1, t2));
    return to > from ? to - from : 0.0;
}

// Seconds a car spends with gz in lo < gz <= hi while it is between places from and to,
// and the lowest gz there.
static double secondsIn(const trainRide& r, int car, double from, double to, double lo, double hi, double* lowest = nullptr)
{
    double off = r.carOffset_m.at(car), sum = 0.0, low = 1e30;
    for(int k = 0; k < r.samples(); ++k) {
        double p = r.position_m.at(k) + off;
        if(p < from || p > to) continue;
        low = qMin(low, r.car(k, car, rideGz));
        if(k == 0) continue;
        double q = r.position_m.at(k - 1) + off;
        if(q < from) continue;
        sum += (r.time_s.at(k) - r.time_s.at(k - 1))*share(r.car(k - 1, car, rideGz), r.car(k, car, rideGz), lo, hi);
    }
    if(lowest) *lowest = low;
    return sum;
}

// ---------------------------------------------------------------------------
// trainSpec
// ---------------------------------------------------------------------------

static void checkSpec(const QString& repo)
{
    trainSpec p = trainSpec::point();
    QString why;
    report(p.iCars == 1 && p.rows() == 1 && p.seatsPerRow() == 1 && p.lRowOffsets.at(0) == 0.f
           && p.lSeatOffsets.at(0) == 0.f && p.fSeatUp == 0.f && p.bMassAtDesignPoint && p.resistance.bFromProject
           && p.iReferenceCar == -1 && p.isValid(&why) && p == trainSpec() && p.carOffset(0) == 0.0,
           "trainSpec::point() is one car, one row, one seat at the heartline, mass at the design point, the project's "
           "resistance, valid " + why);
    trainSpec t = makeTrain(4, 2.5);
    report(t.carOffset(0) == 3.75 && t.carOffset(3) == -3.75 && t.carOffset(1) == 1.25 && t.trainLength() == 10.f,
           QString("a 4 x 2.5 m train centred: car offsets %1, %2, %3, %4 m").arg(t.carOffset(0)).arg(t.carOffset(1))
           .arg(t.carOffset(2)).arg(t.carOffset(3)));
    t.iReferenceCar = 0;
    report(t.carOffset(0) == 0.0 && t.carOffset(3) == -7.5, "with the front car as the reference, it is at 0 and the rear car 7.5 m behind");
    t.iReferenceCar = 3;
    report(t.carOffset(3) == 0.0 && t.carOffset(0) == 7.5, "with the rear car as the reference, the front car is 7.5 m ahead");
    report(t != p, "two different trains are not equal");

    // refusals
    struct bad { const char* what; trainSpec spec; };
    QList<bad> bads;
    trainSpec b = p; b.iCars = 0; bads.append({ "no car", b });
    b = p; b.iCars = 2; b.fCarPitch = 0.f; bads.append({ "two cars 0 m apart", b });
    b = p; b.lRowOffsets.clear(); bads.append({ "no row", b });
    b = p; b.lSeatOffsets.clear(); bads.append({ "no seat", b });
    b = p; b.lSeatOffsets[0] = std::numeric_limits<float>::quiet_NaN(); bads.append({ "a seat offset not a number", b });
    b = p; b.iReferenceCar = 1; bads.append({ "a reference car not in the train", b });
    b = p; b.fLoad = 1.5f; bads.append({ "a load above 1", b });
    b = p; b.lCarMassFactor << 1.f << 1.f; bads.append({ "two mass factors for one car", b });
    b = p; b.resistance.bFromProject = false; b.resistance.fDrag = -1.f; bads.append({ "a negative drag", b });
    b = p; b.fCarMass = 0.f; b.fRiderMass = 0.f; bads.append({ "no mass", b });
    for(int i = 0; i < bads.size(); ++i) {
        QString w;
        bool valid = bads[i].spec.isValid(&w);
        report(!valid && !w.isEmpty(), QString("refused: %1 (\"%2\")").arg(bads[i].what).arg(w));
    }
    // a refused train gives no ride and no crash
    track none(glm::vec3(0.f, 5.f, 0.f), 0.f, 1.1f);
    trainRide r = rideTrain(&none, bads[0].spec);
    report(!r.valid && !r.error.isEmpty() && r.samples() == 0, "a ride with a refused train is not valid and says why: " + r.error);
    r = rideTrain(&none, p);
    report(!r.valid && !r.error.isEmpty(), "a ride over a track without sections is not valid and says why: " + r.error);
    r = rideTrain(nullptr, p);
    report(!r.valid, "a ride without a track is not valid");

    // fromType on the whole catalogue
    QString error;
    QList<trainType> types = trainCatalogue::load(QDir(repo).filePath("data/traintypes.json"), &error);
    report(!types.isEmpty() && error.isEmpty(), QString("the train catalogue loads (%1 types) %2").arg(types.size()).arg(error));
    int valid = 0, pitchAssumed = 0, countsKept = 0;
    bool standIns = true;
    for(int i = 0; i < types.size(); ++i) {
        QStringList assumed;
        trainSpec s = trainSpec::fromType(types[i], &assumed);
        if(s.isValid()) ++valid;
        if(assumed.contains("fCarPitch")) {
            ++pitchAssumed;
            if(s.fCarPitch != 2.5f) standIns = false;
        }
        if(types[i].iCarsPerTrain > 0 && s.iCars != types[i].iCarsPerTrain) standIns = false;
        if(types[i].iRowsPerCar > 0 && s.rows() != types[i].iRowsPerCar) standIns = false;
        if(types[i].iSeatsAcross > 0 && s.seatsPerRow() != types[i].iSeatsAcross) standIns = false;
        if(types[i].iCarsPerTrain > 0 && types[i].iRowsPerCar > 0 && types[i].iSeatsAcross > 0) ++countsKept;
        if(s.rows() > 1 && std::fabs(s.lRowOffsets.at(0) - s.lRowOffsets.at(1) - 0.9f) > 1e-6f) standIns = false;
        if(s.seatsPerRow() > 1 && std::fabs(s.lSeatOffsets.at(1) - s.lSeatOffsets.at(0) - 0.55f) > 1e-6f) standIns = false;
        if(!assumed.contains("lRowOffsets") || !assumed.contains("lSeatOffsets")) standIns = false;
        if(s.sTypeId != types[i].sId || s.sRestraint != types[i].sRestraint) standIns = false;
    }
    report(valid == types.size() && standIns,
           QString("fromType() gives a valid train for every one of the %1 catalogue types; it keeps the catalogue's counts "
                   "(all three known for %2), takes the stand-ins 2.5 m car pitch (%3 types), rows 0.9 m and seats 0.55 m "
                   "apart, and names every stand-in field").arg(types.size()).arg(countsKept).arg(pitchAssumed));
}

// ---------------------------------------------------------------------------
// (1) the point against the design on every baseline project
// ---------------------------------------------------------------------------

// Nodes this close to a section join are left out of the force comparison:
// the ride takes the frame's turning over +-10 nodes, so a jump in force at a
// join is spread over 10 nodes either side of it.
static const int kJoinMargin = 20;
static const double kSpeedTol = 0.002;      // m/s, the plan's number
static const double kForceTol = 0.02;       // g, gz and gy away from joins
static const double kAlongTol = 0.02;       // g, gx against the measure forceAlong away from joins, held track included

struct pointStats
{
    double dv, dz, dy, dx, dvSmoothed, dvSmoothedDesign, dvRaw, heldOffWorst, measureOffSum;
    int nodes, speedNodes, forceNodes, alongNodes, heldNodes;
    QStringList excluded;
};

static pointStats comparePoint(track* t, const QString& label)
{
    pointStats st = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0, 0, QStringList() };
    measureNodes n = measureRegistry::collect(t);
    QVector<QVector<float> > m = measureRegistry::values(n, QStringList() << "speed" << "forceNormal" << "forceLateral" << "forceAlong");
    rideOptions o;
    o.sampleInterval_s = 0.001;
    trainRide r = rideTrain(t, trainSpec::point(), o);
    st.nodes = n.size();
    if(!r.valid) {
        st.excluded << "the ride is not valid: " + r.error;
        return st;
    }
    QVector<bool> passed;
    QVector<double> v = trainRideSpeedByNode(r, 0, &passed);
    QVector<double> gz = trainRideCarByNode(r, 0, rideGz);
    QVector<double> gy = trainRideCarByNode(r, 0, rideGy);
    QVector<double> gx = trainRideCarByNode(r, 0, rideGx);
    // the first Bezier or NoLimits CSV section: it and everything after it are left out
    int firstImported = -1;
    for(int s = 0; s < t->lSections.size(); ++s) {
        int type = t->lSections.at(s)->type;
        if(type == bezier || type == nolimitscsv) {
            firstImported = s;
            st.excluded << QString("section %1 (%2) and after").arg(s + 1).arg(type == bezier ? "bezier" : "nolimits-csv");
            break;
        }
    }
    int vertical = 0, heldJoins = 0, geometryJumps = 0, smoothedNodes = 0, cusp = 0, roundedNodes = 0;
    double lostWorst = 0.0;
    // Nodes left out of the force comparison:
    // - a loop tighter than twice its heartline: where the heartline moves less
    //   than half as far as the rail it nearly turns back on itself, and the
    //   design's own force jumps from node to node (four-sections' tight loop:
    //   1.8 g, 5.3 g, 2.4 g on three nodes in a row);
    // - a step where the design's energy point moves further than the track runs
    //   (pitch through vertical in such a loop): the design's along-track force
    //   differences its speed over +-10 nodes across the jump.
    QVector<bool> tight(n.size(), false), jumpZone(n.size(), false), jumpStep(n.size() + 1, false);
    for(int i = 1; i < n.size(); ++i) {
        const mnode* q = n.nodes.at(i);
        const mnode* q0 = n.nodes.at(i - 1);
        if(q->fDistFromLast > 0.f && q->fHeartDistFromLast < 0.5f*q->fDistFromLast) {
            for(int j = qMax(0, i - kJoinMargin); j < qMin(n.size(), i + kJoinMargin + 1); ++j) tight[j] = true;
        }
        double y0 = q0->vPos.y + 0.9*t->fHeart*q0->vNorm.y, y1 = q->vPos.y + 0.9*t->fHeart*q->vNorm.y;
        if(std::fabs(y1 - y0) > 1.01*std::fabs((double)q->fTotalLength - q0->fTotalLength) + 1e-6) {
            jumpStep[i] = true;
            for(int j = qMax(0, i - kJoinMargin); j < qMin(n.size(), i + kJoinMargin + 1); ++j) jumpZone[j] = true;
        }
    }
    // The design's own float rounding of its energy: each node takes
    // fResistance v^3 / F_HZ out of a float energy, and where the energy is
    // large next to that step (n1-tension-force-chain, 360 m below its start:
    // -3262 J/kg against steps of 1e-4) the step is lost. The ride, in double,
    // keeps it. "lost" is the energy the design kept that its rule takes away,
    // summed from the last held-speed node; the ride is compared with the speed
    // the design's rule gives without that rounding.
    double lost = 0.0;
    int beyondTen = 0;
    for(int i = 0; i < n.size(); ++i) {
        int s = n.sectionOf.at(i);
        const mnode* q = n.nodes.at(i);
        bool held = measureSpeedIsHeld(t->lSections.at(s));
        if(i == 0 || held) {
            lost = 0.0;
        } else {
            const mnode* qp = n.nodes.at(i - 1);
            double expected = (double)qp->fVel*qp->fVel*qp->fVel/1000.0*t->fResistance;
            lost += expected - ((double)qp->fEnergy - q->fEnergy);
        }
        if(firstImported >= 0 && s >= firstImported) continue;
        if(!passed.at(i) || !std::isfinite(m[0][i])) continue;
        // The speed: every node, except the two either side of a jump in the
        // design's speed, where the ride's samples (1 ms apart, not on the
        // nodes) straddle the jump: a join into or out of a held-speed section,
        // and a jump of the energy point (above).
        bool nearHeldJoin = false, nearGeometryJump = false;
        for(int j = i; j <= i + 1; ++j) {
            if(j < 1 || j >= n.size()) continue;
            int a = n.sectionOf.at(j - 1), b = n.sectionOf.at(j);
            if(a != b && (measureSpeedIsHeld(t->lSections.at(a)) || measureSpeedIsHeld(t->lSections.at(b)))) nearHeldJoin = true;
            if(j == 1 && measureSpeedIsHeld(t->lSections.at(0))) nearHeldJoin = true;   // the anchor's speed, then the held one
            if(jumpStep[j]) nearGeometryJump = true;
        }
        // Roll smoothing turns the frame after a section computed its speed,
        // which moves the energy point: there the design's speed is no longer
        // what its own energy gives on the finished track, and the ride (which
        // rides the finished track) is compared with that instead.
        double ownE = q->fEnergy - lost - kG*(q->vPos.y + 0.9*t->fHeart*q->vNorm.y + q->fTotalLength*t->fFriction);
        double ownV = ownE > 0.0 ? std::sqrt(2.0*ownE) : 0.0;
        double design = held ? (double)m[0][i] : std::sqrt(qMax(0.0, (double)m[0][i]*m[0][i] - 2.0*lost));
        // a held straight holds its heartline's speed: on the centre line, which
        // winds round the heartline at fHeart while the straight rolls, that is
        // sqrt(1 + (fHeart x roll rate / speed)^2) times as much
        if(held && t->lSections.at(s)->type == straight && q->fVel > 0.f) {
            double w = (double)q->fRollSpeed*M_PI/180.0/q->fVel*t->fHeart;
            design *= std::sqrt(1.0 + w*w);
        }
        bool smoothed = i > 0 && !held && std::fabs(ownV - design) > 2e-4;
        if(!held && std::fabs(design - m[0][i]) > 1e-4) {
            ++roundedNodes;
            lostWorst = qMax(lostWorst, std::fabs(design - m[0][i]));
        }
        if(nearHeldJoin || nearGeometryJump) {
            if(nearHeldJoin) ++heldJoins;
            else ++geometryJumps;
        } else if(smoothed) {
            ++smoothedNodes;
            st.dvSmoothed = qMax(st.dvSmoothed, std::fabs(v.at(i) - ownV));
            st.dvSmoothedDesign = qMax(st.dvSmoothedDesign, std::fabs(ownV - design));
        } else {
            st.dv = qMax(st.dv, std::fabs(v.at(i) - design));
            st.dvRaw = qMax(st.dvRaw, std::fabs(v.at(i) - m[0][i]));
            ++st.speedNodes;
        }
        // the forces, away from joins, and not where the design itself is vertical
        bool nearJoin = (i - n.sectionFirst.at(s) < kJoinMargin && s > 0) || (n.sectionLast.at(s) - i < kJoinMargin
                && s + 1 < t->lSections.size()) || i < kJoinMargin || n.size() - 1 - i < kJoinMargin;
        if(nearJoin) continue;
        if(std::fabs(n.nodes.at(i)->getPitch()) > 89.5f) { ++vertical; continue; }
        if(tight.at(i) || jumpZone.at(i)) { ++cusp; continue; }
        // the n1 test loops reach hundreds of g where their shape folds up; no
        // ride has more than 10 g, and there the design's own numbers are not a reference
        if(std::fabs(m[1][i]) > 10.f || std::fabs(m[2][i]) > 10.f) { ++beyondTen; continue; }
        st.dz = qMax(st.dz, std::fabs(gz.at(i) - m[1][i]));
        st.dy = qMax(st.dy, std::fabs(gy.at(i) - m[2][i]));
        // The along-track force: since issue 96 the measure forceAlong is the
        // force at the heartline by the ride's own rule (core/heartline.h),
        // from the design's speed; compared directly, held-speed track
        // included (a = 0 there, the pitching term stays). The worst on held
        // track alone is kept for the record.
        double off = std::fabs(gx.at(i) - m[3][i]);
        st.dx = qMax(st.dx, off);
        st.measureOffSum += off;
        ++st.alongNodes;
        if(held) {
            st.heldOffWorst = qMax(st.heldOffWorst, off);
            ++st.heldNodes;
        }
        ++st.forceNodes;
    }
    if(heldJoins) st.excluded << QString("%1 node(s) at joins into or out of held-speed sections (speed only)").arg(heldJoins);
    if(geometryJumps) st.excluded << QString("%1 node(s) beside a step where the design's energy point jumps further than "
                                             "the track runs (speed only)").arg(geometryJumps);
    if(smoothedNodes) st.excluded << QString("%1 node(s) where roll smoothing moved the track after its speed was computed: "
                                             "the design's speed is up to %2 m/s off its own energy there, the ride is "
                                             "within %3 m/s of that energy's speed")
                                     .arg(smoothedNodes).arg(st.dvSmoothedDesign, 0, 'g', 2).arg(st.dvSmoothed, 0, 'g', 2);
    if(roundedNodes) st.excluded << QString("at %1 node(s) the design's float energy lost drag steps to rounding, worth up "
                                            "to %2 m/s (compared without it; raw worst %3 m/s)")
                                    .arg(roundedNodes).arg(lostWorst, 0, 'g', 2).arg(st.dvRaw, 0, 'g', 3);
    if(vertical) st.excluded << QString("%1 node(s) within 0.5 degrees of vertical (forces only)").arg(vertical);
    if(cusp) st.excluded << QString("%1 node(s) near a loop tighter than twice its heartline or a jump of the energy point "
                                    "(forces only)").arg(cusp);
    if(beyondTen) st.excluded << QString("%1 node(s) where the design's own force is beyond 10 g (forces only)").arg(beyondTen);
    if(!rideFinite(r)) st.excluded << "NOT FINITE";
    Q_UNUSED(label);
    return st;
}

static void checkPointOnBaseline(const QString& repo)
{
    QStringList files;
    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList names = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    for(int i = 0; i < names.size(); ++i) {
        if(!names[i].contains(".2016.")) files << baseline.filePath(names[i]);
    }
    files << QDir(repo).filePath("samples/import/nl1.ref.resaved.fvd") << QDir(repo).filePath("samples/import/nl2.ref.resaved.fvd");
    double worstV = 0.0, worstZ = 0.0, worstY = 0.0, worstX = 0.0, heldWorst = 0.0, offSum = 0.0;
    int tracks = 0, offNodes = 0;
    for(int f = 0; f < files.size(); ++f) {
        coreProject p;
        QString name = QFileInfo(files[f]).fileName();
        if(!load(files[f], p)) {
            report(false, name + " loads");
            continue;
        }
        for(int k = 0; k < p.tracks.size(); ++k) {
            track* t = p.tracks[k];
            if(t->lSections.isEmpty()) continue;
            ++tracks;
            QString label = QString("%1 track %2").arg(name).arg(k + 1);
            pointStats st = comparePoint(t, label);
            worstV = qMax(worstV, st.dv);
            worstZ = qMax(worstZ, st.dz);
            worstY = qMax(worstY, st.dy);
            worstX = qMax(worstX, st.dx);
            heldWorst = qMax(heldWorst, st.heldOffWorst);
            offSum += st.measureOffSum;
            offNodes += st.alongNodes;
            bool ok = st.dv <= kSpeedTol && st.dvSmoothed <= kSpeedTol && st.dz <= kForceTol && st.dy <= kForceTol && st.dx <= kAlongTol
                    && !st.excluded.contains("NOT FINITE");
            report(ok, QString("(1) %1: the point reproduces the design's speed within %2 m/s at %3 of %4 nodes (worst %5), "
                               "gz %6, gy %7 g at %8 nodes away from joins, gx against the measure forceAlong %9 g at worst, "
                               "%10 g on average (%11 nodes, of them on held-speed track %12 nodes, worst there %13 g)%14")
                   .arg(label).arg(kSpeedTol).arg(st.speedNodes).arg(st.nodes).arg(st.dv, 0, 'g', 3)
                   .arg(st.dz, 0, 'f', 4).arg(st.dy, 0, 'f', 4).arg(st.forceNodes).arg(st.dx, 0, 'f', 4)
                   .arg(st.alongNodes ? st.measureOffSum/st.alongNodes : 0.0, 0, 'f', 5).arg(st.alongNodes)
                   .arg(st.heldNodes).arg(st.heldOffWorst, 0, 'f', 4)
                   .arg(st.excluded.isEmpty() ? QString() : "; left out: " + st.excluded.join(", ")));
        }
    }
    report(tracks >= 10, QString("(1) worst over %1 tracks: speed %2 m/s (limit %3), gz %4 g, gy %5 g (limit %6 g), gx against "
                                 "the measure forceAlong %7 g, %8 g on average over %9 nodes (limit %10 g, held-speed "
                                 "track included; worst on held-speed track alone %11 g); nodes within %12 of a section join left out")
           .arg(tracks).arg(worstV, 0, 'g', 3).arg(kSpeedTol).arg(worstZ, 0, 'f', 4).arg(worstY, 0, 'f', 4).arg(kForceTol)
           .arg(worstX, 0, 'f', 4).arg(offNodes ? offSum/offNodes : 0.0, 0, 'f', 5).arg(offNodes).arg(kAlongTol)
           .arg(heldWorst, 0, 'f', 4).arg(kJoinMargin));
}

// ---------------------------------------------------------------------------
// (2) R7's tables
// ---------------------------------------------------------------------------

static const double kR7G = 0.02, kR7V = 0.02, kR7T = 0.05;
static const int r7Cars[3] = { 4, 9, 10 };
static const double r7Pitch[3] = { 2.5, 2.2, 3.0 };
static const char* r7Names[3] = { "short", "long", "extreme" };
static const char* carNames[3] = { "front", "middle", "rear" };

struct shapeData
{
    QVector<double> s, hy, pitch, roll, normal;
};

static shapeData shapeOf(track* t)
{
    shapeData d;
    measureNodes n = measureRegistry::collect(t);
    QVector<float> normal = measureRegistry::values(n, QStringList() << "forceNormal").at(0);
    for(int i = 0; i < n.size(); ++i) {
        mnode* m = n.nodes.at(i);
        d.s << (i == 0 ? 0.0 : (double)m->fTotalLength);
        d.hy << m->vPos.y;
        d.pitch << m->getPitch();
        d.roll << m->fRoll;
        d.normal << normal.at(i);
    }
    return d;
}

static int argExt(const shapeData& d, const QVector<double>& a, double lo, double hi, bool max)
{
    int best = -1;
    for(int i = 0; i < d.s.size(); ++i) {
        if(d.s[i] < lo || d.s[i] > hi) continue;
        if(best < 0 || (max ? a[i] > a[best] : a[i] < a[best])) best = i;
    }
    return best;
}

struct worst
{
    double g, v, t;
    int count;
    QStringList over;
    worst() : g(0.0), v(0.0), t(0.0), count(0) {}
    void add(double got, double want, double tol, double* w, const QString& what)
    {
        double d = std::fabs(got - want);
        *w = qMax(*w, d);
        ++count;
        if(!(d <= tol)) over << QString("%1: %2 against %3").arg(what).arg(got, 0, 'f', 3).arg(want, 0, 'f', 2);
    }
};

// the middle car of the "normal force at a place" table: the centre car, or
// the mean of the two centre cars for an even number of cars
static double middleAt(const trainRide& r, int cars, double x, int axis)
{
    if(cars % 2) return carAt(r, cars/2, x, axis);
    return 0.5*(carAt(r, cars/2 - 1, x, axis) + carAt(r, cars/2, x, axis));
}

static void checkR7Shape(const QString& repo, int shape, worst& w)
{
    static const char* files[3] = { "camelback", "loop", "layout" };
    coreProject p;
    QString file = QDir(repo).filePath(QString("docs/research/r7-prototype/projects/%1.fvd").arg(files[shape]));
    if(!load(file, p)) {
        report(false, QString("(2) R7's %1 loads").arg(files[shape]));
        return;
    }
    track* t = p.tracks.last();
    // the places R7 used (r7tables.h, made with R7's own code from its 0.1 m dumps)
    QList<double> places;
    for(int k = 0; k < (shape < 2 ? 3 : 5); ++k) places << r7Places[shape][k];
    if(shape == 2) {
        // per element, seconds below 0 g (r7Elements)
        const double* bounds = r7ElementBounds;
        trainRide pt = rideTrain(t, trainSpec::point());
        for(int e = 0; e < 3; ++e) {
            w.add(secondsIn(pt, 0, bounds[e], bounds[e + 1], -1e30, 0.0), r7Elements[e][0], kR7T, &w.t,
                  QString("layout element %1, point, below 0 g").arg(e));
        }
        for(int k = 0; k < 3; ++k) {
            trainRide r = rideTrain(t, makeTrain(r7Cars[k], r7Pitch[k]));
            int idx[3] = { 0, r7Cars[k]/2, r7Cars[k] - 1 };
            for(int e = 0; e < 3; ++e) {
                for(int c = 0; c < 3; ++c) {
                    w.add(secondsIn(r, idx[c], bounds[e], bounds[e + 1], -1e30, 0.0), r7Elements[e][1 + 3*k + c], kR7T, &w.t,
                          QString("layout element %1, %2 %3 car, below 0 g").arg(e).arg(r7Names[k]).arg(carNames[c]));
                }
            }
        }
    }
    trainRide pt = rideTrain(t, trainSpec::point());
    trainRide rides[3];
    for(int k = 0; k < 3; ++k) {
        rides[k] = rideTrain(t, makeTrain(r7Cars[k], r7Pitch[k]));
        report(rides[k].valid && !rides[k].stalled && rideFinite(rides[k]),
               QString("(2) the %1 train rides R7's %2 to the end").arg(r7Names[k]).arg(files[shape]));
    }
    for(unsigned i = 0; i < sizeof(r7SpeedDiff)/sizeof(r7SpeedDiff[0]); ++i) {
        const r7Speed& e = r7SpeedDiff[i];
        if(e.shape != shape) continue;
        double x = places[e.place];
        double got = refInterp(rides[e.train], x, rides[e.train].speed_mps) - refInterp(pt, x, pt.speed_mps);
        w.add(got, e.diff, kR7V, &w.v, QString("%1 speed difference, %2 train at place %3").arg(files[shape]).arg(r7Names[e.train]).arg(e.place));
    }
    for(unsigned i = 0; i < sizeof(r7Normal)/sizeof(r7Normal[0]); ++i) {
        const r7AtPlace& e = r7Normal[i];
        if(e.shape != shape) continue;
        const trainRide& r = rides[e.train];
        int n = r7Cars[e.train];
        double x = places[e.place];
        double got = e.car == 1 ? middleAt(r, n, x, rideGz) : carAt(r, e.car == 0 ? 0 : n - 1, x, rideGz);
        w.add(got, e.g, kR7G, &w.g, QString("%1 normal force, %2 %3 car at place %4").arg(files[shape]).arg(r7Names[e.train])
              .arg(carNames[e.car]).arg(e.place));
    }
    // R7's along-track force is vDir.y plus the centre line's acceleration over
    // g (ride.py: a/G + dir_y), not the heartline's: compared with the ride's
    // centre-line acceleration (trainRide::accel_mps2) and the track's vDir.y there
    ridePath path = ridePathFromTrack(t);
    for(unsigned i = 0; i < sizeof(r7Along)/sizeof(r7Along[0]); ++i) {
        const r7AtPlace& e = r7Along[i];
        if(e.shape != shape) continue;
        const trainRide& r = rides[e.train];
        int n = r7Cars[e.train];
        double x = places[e.place];
        double got = accelWhenCarAt(r, e.car == 0 ? 0 : n - 1, x)/kG + dirYAt(path, x);
        w.add(got, e.g, kR7G, &w.g, QString("%1 along-track force, %2 %3 car at place %4").arg(files[shape]).arg(r7Names[e.train])
              .arg(carNames[e.car]).arg(e.place));
    }
    for(unsigned i = 0; i < sizeof(r7Extremes)/sizeof(r7Extremes[0]); ++i) {
        const r7Extreme& e = r7Extremes[i];
        if(e.shape != shape) continue;
        const trainRide& r = e.train < 0 ? pt : rides[e.train];
        int n = e.train < 0 ? 1 : r7Cars[e.train];
        int car = e.car == 0 ? 0 : (e.car == 1 ? n/2 : n - 1);
        const rideCarSummary& s = r.cars.at(car);
        QString who = e.train < 0 ? QString("point") : QString("%1 %2 car").arg(r7Names[e.train]).arg(carNames[e.car]);
        w.add(s.low[rideGz].value, e.low, kR7G, &w.g, QString("%1 lowest gz, %2").arg(files[shape]).arg(who));
        w.add(s.high[rideGz].value, e.high, kR7G, &w.g, QString("%1 highest gz, %2").arg(files[shape]).arg(who));
        w.add(s.belowZero_s, e.belowZero, kR7T, &w.t, QString("%1 seconds below 0 g, %2").arg(files[shape]).arg(who));
        w.add(s.floater_s, e.floater, kR7T, &w.t, QString("%1 floater seconds, %2").arg(files[shape]).arg(who));
        w.add(s.ejector_s, e.ejector, kR7T, &w.t, QString("%1 ejector seconds, %2").arg(files[shape]).arg(who));
    }
}

static void checkR7Lift(const QString& repo, double crest, worst& w)
{
    coreProject p;
    QString name = QString("liftcrest-%1").arg(crest, 0, 'f', 2);
    if(!load(QDir(repo).filePath("docs/research/r7-prototype/projects/" + name + ".fvd"), p)) {
        report(false, "(2) R7's " + name + " loads");
        return;
    }
    track* t = p.tracks.last();
    // the places R7 used: the centre passes -30 degrees, the tightest place of
    // the crest (R7's smallest pitch turning), the bottom of the drop
    int which = crest < 0.45 ? 0 : (crest < 0.7 ? 1 : 2);
    double x30 = r7LiftPlaces[which][0], xt = r7LiftPlaces[which][1], hi = r7LiftPlaces[which][2];
    trainRide pt = rideTrain(t, trainSpec::point());
    for(unsigned i = 0; i < sizeof(r7LiftCrest)/sizeof(r7LiftCrest[0]); ++i) {
        const r7Lift& e = r7LiftCrest[i];
        if(std::fabs(e.crest - crest) > 1e-6) continue;
        trainRide r = e.train < 0 ? pt : rideTrain(t, makeTrain(r7Cars[e.train], r7Pitch[e.train]));
        int n = e.train < 0 ? 1 : r7Cars[e.train];
        int car = e.car == 0 ? 0 : (e.car == 1 ? n/2 : n - 1);
        QString who = QString("%1, %2").arg(name).arg(e.train < 0 ? QString("point")
                                                                   : QString("%1 %2 car").arg(r7Names[e.train]).arg(carNames[e.car]));
        if(e.speed30 >= 0.0) {
            w.add(refInterp(r, x30, r.speed_mps), e.speed30, kR7V, &w.v, who + ", train speed when its centre passes -30 degrees");
        }
        // the speed when this car passes the tightest place
        w.add(refInterp(r, xt - r.carOffset_m.at(car), r.speed_mps), e.speedTight, kR7V, &w.v, who + ", speed at the tightest place");
        double lowest = 0.0;
        double below = secondsIn(r, car, 0.0, hi, -1e30, 0.0, &lowest);
        double ej = secondsIn(r, car, 0.0, hi, -1e30, -0.5);
        w.add(lowest, e.low, kR7G, &w.g, who + ", lowest gz");
        w.add(below, e.belowZero, kR7T, &w.t, who + ", seconds below 0 g");
        w.add(ej, e.ejector, kR7T, &w.t, who + ", ejector seconds");
    }
}

static void checkR7(const QString& repo)
{
    worst w;
    for(int shape = 0; shape < 3; ++shape) checkR7Shape(repo, shape, w);
    checkR7Lift(repo, 0.30, w);
    checkR7Lift(repo, 0.60, w);
    checkR7Lift(repo, 0.75, w);
    for(int i = 0; i < w.over.size(); ++i) printf("    over: %s\n", qPrintable(w.over.at(i)));
    // (R7's values are rounded to two decimals; the limits are taken against
    // the rounded values as printed)
    printf("    (R7's along-track tables are of the centre line's acceleration, as R7 computed them; the ride's own gx is "
           "the heartline's, which differs by the heartline's length per metre of centre line)\n");
    report(w.over.isEmpty() && w.count > 300,
           QString("(2) R7's tables, %1 numbers with R7's three trains: worst %2 g (limit %3), %4 m/s (limit %5), "
                   "%6 s (limit %7), against R7's values as printed; %8 over")
           .arg(w.count).arg(w.g, 0, 'f', 4).arg(kR7G).arg(w.v, 0, 'f', 4).arg(kR7V).arg(w.t, 0, 'f', 4).arg(kR7T)
           .arg(w.over.size()));
}

// ---------------------------------------------------------------------------
// (3) the closed form of R7 section 6
// ---------------------------------------------------------------------------

// A crest made of a straight climb at a0 radians, a circular arc of radius R
// over the top and a straight descent; heartline on the rail; no friction.
// Built as R7 built it (run_r7.py, synthetic_circle(): points ds apart, the
// height summed step by step with the angle at the step's end, the pitch
// turning from the direction vectors of the two neighbours), so that R7's own
// numbers can be compared to four decimals as well as the formula.
static ridePath circlePath(double R, double a0, double straightLen, double ds)
{
    ridePath p;
    double arc = 2.0*a0*R, total = 2.0*straightLen + arc;
    int n = (int)(total/ds);
    QVector<double> th;
    double y = 0.0;
    for(int i = 0; i <= n; ++i) {
        double s = i*ds, t;
        if(s < straightLen) t = a0;
        else if(s < straightLen + arc) t = a0 - (s - straightLen)/R;
        else t = -a0;
        if(i) y += ds*std::sin(t);
        th << t;
        p.s << s;
        p.sh << s;
        p.heartY << y;
        p.dirY << std::sin(t);
        p.latY << 0.0;
        p.normY << -std::cos(t);
        p.wD << 0.0;
        p.wN << 0.0;
        p.rh << 1.0;
        p.held << -1.0;
        p.node << i;
        p.section << 1;
        p.nodePlace << s;
        p.nodeSection << 1;
    }
    for(int i = 0; i <= n; ++i) {
        int lo = qMax(i - 1, 0), hi = qMin(i + 1, n);
        double ddx = std::cos(th[hi]) - std::cos(th[lo]), ddy = std::sin(th[hi]) - std::sin(th[lo]);
        // wL = -(dDir . vNorm) / dsh, vNorm = (sin, -cos)
        p.wL << -(ddx*std::sin(th[i]) - ddy*std::cos(th[i]))/(p.s[hi] - p.s[lo]);
    }
    p.deriveRates();
    p.fHeart = 0.0;
    p.fFriction = 0.0;
    p.fResistance = 0.0;
    p.startSpeed = 25.0;
    p.sectionCount = 1;
    p.sectionTypes << "synthetic";
    return p;
}

// equal to four decimals: the same when rounded, or one apart in the last place
static bool fourDecimals(double a, double b)
{
    return std::fabs(std::floor(a*1e4 + 0.5) - std::floor(b*1e4 + 0.5)) <= 1.0;
}

static void checkClosedForm()
{
    const double R = 20.0, a0 = 1.0, straightLen = 20.0;
    ridePath path = circlePath(R, a0, straightLen, 0.05);
    double top = straightLen + a0*R;
    // the height of the top as the table has it
    int it = 0;
    while(it + 1 < path.size() && path.s[it + 1] <= top) ++it;
    double yTop = path.heartY[it] + (top - path.s[it])/(path.s[it + 1] - path.s[it])*(path.heartY[it + 1] - path.heartY[it]);
    struct expected { int length, cars; double train, force; };
    // results.txt, "Check against the closed form"
    expected rows[3] = { { 10, 4, 10.9009, 0.3922 }, { 20, 8, 11.4414, 0.3306 }, { 30, 10, 12.2649, 0.2302 } };
    rideOptions o;
    o.sampleInterval_s = 0.001;
    trainRide pt = rideTrainOnPath(path, trainSpec::point(), o);
    double v2p = 25.0*25.0 - 2.0*kG*yTop;
    double vp = refInterp(pt, top, pt.speed_mps);
    report(fourDecimals(vp, std::sqrt(v2p)) && fourDecimals(vp, 10.7242),
           QString("(3) the point over a 20 m circular crest: %1 m/s, the formula %2, R7 10.7242").arg(vp, 0, 'f', 5).arg(std::sqrt(v2p), 0, 'f', 5));
    for(int k = 0; k < 3; ++k) {
        trainSpec spec = makeTrain(rows[k].cars, (double)rows[k].length/rows[k].cars);
        trainRide r = rideTrainOnPath(path, spec, o);
        double meanCos = 0.0;
        for(int c = 0; c < rows[k].cars; ++c) meanCos += std::cos(spec.carOffset(c)/R);
        meanCos /= rows[k].cars;
        double v2t = v2p + 2.0*kG*R*(1.0 - meanCos);
        int idx = rows[k].cars/2;
        double nForm = std::cos(spec.carOffset(idx)/R) - v2t/(kG*R);
        double vt = refInterp(r, top, r.speed_mps);
        // the centre car's force when the train is centred on the top
        QVector<double> force;
        for(int s = 0; s < r.samples(); ++s) force << r.car(s, idx, rideGz);
        double nt = refInterp(r, top, force);
        bool ok = fourDecimals(vt, std::sqrt(v2t)) && fourDecimals(nt, nForm) && fourDecimals(vt, rows[k].train)
                && fourDecimals(nt, rows[k].force) && r.valid && !r.stalled;
        report(ok, QString("(3) %1 m train of %2 cars centred on the crest: %3 m/s (formula %4, R7 %5), centre car %6 g "
                           "(formula %7, R7 %8), to four decimals")
               .arg(rows[k].length).arg(rows[k].cars).arg(vt, 0, 'f', 5).arg(std::sqrt(v2t), 0, 'f', 5).arg(rows[k].train, 0, 'f', 4)
               .arg(nt, 0, 'f', 5).arg(nForm, 0, 'f', 5).arg(rows[k].force, 0, 'f', 4));
    }
}

// The along-track force at the heartline on a vertical circle: rail radius R,
// heartline h toward the centre, frictionless, from the bottom at 20 m/s.
// The mass point (0.1 h above the rail) sits at R - 0.1 h from the centre, so
//   v^2 = v0^2 - 2 g (R - 0.1 h)(1 - cos theta),  a = -g (R - 0.1 h) sin theta / R,
// the heartline runs at vh = v (R - h) / R, and the rider at the heartline is
// pressed back by gx = sin theta + (R - h)/R a / g.
static void checkAlongClosedForm()
{
    const double R = 10.0, h = 1.1, ds = 0.01, end = 2.2*R;
    const double rh = (R - h)/R;
    ridePath p;
    for(int i = 0; i*ds <= end + 1e-9; ++i) {
        double s = i*ds, th = s/R;
        p.s << s;
        p.sh << s*rh;
        p.heartY << R*(1.0 - std::cos(th)) + h*std::cos(th);
        p.dirY << std::sin(th);
        p.latY << 0.0;
        p.normY << -std::cos(th);
        p.wD << 0.0;
        p.wL << 1.0/(R - h);
        p.wN << 0.0;
        p.rh << rh;
        p.held << -1.0;
        p.node << i;
        p.section << 1;
        p.nodePlace << s;
        p.nodeSection << 1;
    }
    p.deriveRates();
    p.fHeart = h;
    p.startSpeed = 20.0;
    p.sectionCount = 1;
    rideOptions o;
    o.sampleInterval_s = 0.001;
    trainRide r = rideTrainOnPath(p, trainSpec::point(), o);
    double err = 0.0, errV = 0.0, errZ = 0.0, at90[4] = { 0.0, 0.0, 0.0, 0.0 }, best90 = 1e9;
    int used = 0;
    for(int k = 0; k < r.samples(); ++k) {
        double th = r.position_m.at(k)/R;
        if(th < 0.1 || th > 2.1) continue;
        double v2 = 400.0 - 2.0*kG*(R - 0.1*h)*(1.0 - std::cos(th));
        double a = -kG*(R - 0.1*h)*std::sin(th)/R;
        double gx = std::sin(th) + rh*a/kG;
        double vh = std::sqrt(v2)*rh;
        double gz = std::cos(th) + vh*vh/(R - h)/kG;
        err = qMax(err, std::fabs(r.car(k, 0, rideGx) - gx));
        errZ = qMax(errZ, std::fabs(r.car(k, 0, rideGz) - gz));
        errV = qMax(errV, std::fabs(r.speed_mps.at(k) - std::sqrt(v2)));
        if(std::fabs(th - M_PI/2) < best90) {
            best90 = std::fabs(th - M_PI/2);
            at90[0] = gx;
            at90[1] = r.car(k, 0, rideGx);
            at90[2] = std::sin(th) + a/kG;          // R7's form, the centre line's acceleration
            at90[3] = std::sin(th) + a/(rh*kG);     // the Stage 2 measure's form before issue 96, a / rh
        }
        ++used;
    }
    report(r.valid && used > 500 && err < 0.001 && errZ < 0.001 && errV < 1e-4,
           QString("the along-track force at the heartline on a vertical circle (rail radius %1 m, heartline %2 m, "
                   "frictionless): worst %3 g against the closed form (limit 0.001 g; gz %4 g, speed %5 m/s); with the "
                   "track vertical the closed form gives %6 g, the ride %7 g, the centre line's acceleration (R7's form) "
                   "would give %8 g and the measure forceAlong's form before issue 96 %9 g (tests/measures checks the "
                   "measure itself on such a circle)")
           .arg(R).arg(h).arg(err, 0, 'g', 2).arg(errZ, 0, 'g', 2).arg(errV, 0, 'g', 2).arg(at90[0], 0, 'f', 4)
           .arg(at90[1], 0, 'f', 4).arg(at90[2], 0, 'f', 4).arg(at90[3], 0, 'f', 4));
}

// ---------------------------------------------------------------------------
// (4) the seats
// ---------------------------------------------------------------------------

// A level straight at a held 10 m/s rolling at a constant rate (as tests/ridesummary builds one).
static void rolledTrack(track& t, float rate)
{
    t.newSection(straight);
    section* s = t.lSections[0];
    s->bSpeed = false;
    s->fVel = 10.f;
    s->rollFunc->changeLength(100.f, 0);
    subfunc* sf = s->rollFunc->funcList[0];
    sf->symArg = rate;
    sf->parent->translateValues(sf);
    t.updateTrack(0, 0);
}

static void checkSeats(const QString& repo)
{
    // (a) omega^2 r on a straight rolling at a constant rate
    track t(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f);
    rolledTrack(t, 72.f);
    measureNodes n = measureRegistry::collect(&t);
    // the roll rate the program gives the nodes (fRollSpeed, degrees per
    // second), taken at the node nearest each sample's place: the roll
    // function of the straight is a transition, so the rate changes along it
    trainSpec spec = trainSpec::point();
    spec.lSeatOffsets.clear();
    spec.lSeatOffsets << -0.75f << 0.f << 0.75f;
    trainRide r = rideTrain(&t, spec);
    trainSpec up = trainSpec::point();
    up.fSeatUp = 0.5f;
    trainRide ru = rideTrain(&t, up);
    double errY = 0.0, errZ = 0.0, errCar = 0.0, omegaMax = 0.0, expectY = 0.0, expectZ = 0.0;
    int used = 0;
    for(int k = 0; k < r.samples(); ++k) {
        double x = r.position_m.at(k);
        if(x < 5.0 || x > r.trackEnd_m - 5.0) continue;
        int node = 0;
        while(node + 1 < n.size() && r.nodePlace.at(node + 1) <= x) ++node;
        if(node + 1 < n.size() && x - r.nodePlace.at(node) > r.nodePlace.at(node + 1) - x) ++node;
        double omega = n.nodes.at(node)->fRollSpeed*M_PI/180.0;
        double wantY = omega*omega*0.75/kG, wantZ = omega*omega*0.5/kG;
        if(std::fabs(omega) > omegaMax) {
            omegaMax = std::fabs(omega);
            expectY = wantY;
            expectZ = wantZ;
        }
        double y0 = r.seat(k, 0, 0, 1, rideGy);
        errY = qMax(errY, std::fabs((r.seat(k, 0, 0, 2, rideGy) - y0) - wantY));
        errY = qMax(errY, std::fabs((r.seat(k, 0, 0, 0, rideGy) - y0) + wantY));
        errZ = qMax(errZ, std::fabs((ru.seat(k, 0, 0, 0, rideGz) - r.seat(k, 0, 0, 1, rideGz)) + wantZ));
        for(int ax = 0; ax < 3; ++ax) errCar = qMax(errCar, std::fabs(r.seat(k, 0, 0, 1, ax) - r.car(k, 0, ax)));
        ++used;
    }
    double omega = omegaMax;
    report(used > 100 && errY < 1e-3 && errZ < 1e-3 && errCar < 1e-9 && expectY > 0.05,
           QString("(4) a straight rolling at up to %1 deg/s (the nodes' own roll rate): a seat 0.75 m to the side feels omega^2 r, %2 g at the most, "
                   "sideways (worst error %3 g), one 0.5 m up %4 g less into the seat (worst error %5 g), a seat at the "
                   "heartline what the car feels (%6 samples)")
           .arg(omega*180.0/M_PI, 0, 'f', 2).arg(expectY, 0, 'f', 4).arg(errY, 0, 'g', 2).arg(expectZ, 0, 'f', 4)
           .arg(errZ, 0, 'g', 2).arg(used));

    // (b) alpha x r: a straight whose roll rate grows evenly with distance, at a held speed
    {
        ridePath p;
        const double c = 0.01, v = 10.0;   // roll rate per metre grows 0.01 rad/m per metre
        for(int i = 0; i <= 10000; ++i) {
            double s = i*0.01, phi = 0.5*c*s*s;
            p.s << s; p.sh << s; p.heartY << 0.0; p.dirY << 0.0;
            p.latY << std::sin(phi); p.normY << -std::cos(phi);
            p.wD << c*s; p.wL << 0.0; p.wN << 0.0; p.rh << 1.0; p.held << v;
            p.node << i; p.section << 1; p.nodePlace << s; p.nodeSection << 1;
        }
        p.deriveRates();
        p.fHeart = 0.0;
        p.startSpeed = v;
        p.sectionCount = 1;
        trainSpec sp = trainSpec::point();
        sp.lSeatOffsets.clear();
        sp.lSeatOffsets << 0.f << 0.6f;
        trainRide rs = rideTrainOnPath(p, sp);
        double expect = v*v*c*0.6/kG, err = 0.0;
        for(int k = 0; k < rs.samples(); ++k) {
            double x = rs.position_m.at(k);
            if(x < 1.0 || x > 99.0) continue;
            err = qMax(err, std::fabs((rs.seat(k, 0, 0, 1, rideGz) - rs.seat(k, 0, 0, 0, rideGz)) + expect));
        }
        report(rs.valid && err < 1e-6, QString("(4) a roll that speeds up evenly (alpha = %1 rad/s^2): a seat 0.6 m to the side "
                                               "feels alpha r = %2 g into or out of the seat (worst error %3 g)")
               .arg(v*v*c).arg(expect, 0, 'f', 4).arg(err, 0, 'g', 2));
    }

    // (c) per-seat noise on smooth tracks: R7's camelback and R7's layout (with
    // its banked turn), 9 cars of 2 rows of 4. Noise is what changes from one
    // sample to the next that the shape does not explain: a sample against the
    // mean of its two neighbours, with samples 1 ms apart, so that the shape's
    // own bend between samples (a few ten-thousandths of a g even at the end of
    // a transition) stays far below the limit and what remains is the jitter
    // of the float node data.
    trainSpec wide = makeTrain(9, 2.2);
    wide.lRowOffsets.clear();
    wide.lRowOffsets << 0.45f << -0.45f;
    wide.lSeatOffsets.clear();
    wide.lSeatOffsets << -0.825f << -0.275f << 0.275f << 0.825f;
    rideOptions fine;
    fine.sampleInterval_s = 0.001;
    const char* shapes[2] = { "camelback", "layout" };
    for(int sh = 0; sh < 2; ++sh) {
        coreProject cp;
        if(!load(QDir(repo).filePath(QString("docs/research/r7-prototype/projects/%1.fvd").arg(shapes[sh])), cp)) {
            report(false, QString("(4) R7's %1 loads").arg(shapes[sh]));
            continue;
        }
        trainRide rw = rideTrain(cp.tracks.last(), wide, fine);
        double noise = 0.0, where = 0.0;
        int worstAxis = 0;
        for(int k = 1; k + 1 < rw.samples(); ++k) {
            for(int c = 0; c < 9; ++c) {
                for(int row = 0; row < 2; ++row) {
                    double place = rw.position_m.at(k) + rw.carOffset_m.at(c) + rw.rowOffset_m.at(row);
                    if(place < rw.trackStart_m + 1.0 || place > rw.trackEnd_m - 1.0) continue;
                    for(int st = 0; st < 4; ++st) {
                        for(int ax = 0; ax < 3; ++ax) {
                            double d = std::fabs(rw.seat(k, c, row, st, ax)
                                                 - 0.5*(rw.seat(k - 1, c, row, st, ax) + rw.seat(k + 1, c, row, st, ax)));
                            if(d > noise) {
                                noise = d;
                                where = place;
                                worstAxis = ax;
                            }
                        }
                    }
                }
            }
        }
        static const char* axes[3] = { "gz", "gy", "gx" };
        report(rw.valid && !rw.stalled && noise < 0.01,
               QString("(4) per-seat noise on R7's %1, 9 cars of 2 rows of 4 (72 seats), samples 1 ms apart: worst %2 g "
                       "(%3 at %4 m; limit 0.01 g)").arg(shapes[sh]).arg(noise, 0, 'g', 2).arg(axes[worstAxis])
               .arg(where, 0, 'f', 1));
    }
}

// ---------------------------------------------------------------------------
// (5) a ride changes nothing; the stall; the by-place helpers; the timing
// ---------------------------------------------------------------------------

static void checkUnchanged(const QString& repo)
{
    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList names = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    int projects = 0, same = 0;
    for(int i = 0; i < names.size(); ++i) {
        if(names[i].contains(".2016.")) continue;
        coreProject p;
        if(!load(baseline.filePath(names[i]), p)) continue;
        ++projects;
        QByteArray before = savedBytes(p), nodesBefore = nodeBytes(p);
        trainSpec big = makeTrain(10, 2.5);
        big.lRowOffsets.clear();
        big.lRowOffsets << 0.45f << -0.45f;
        big.lSeatOffsets.clear();
        big.lSeatOffsets << -0.825f << -0.275f << 0.275f << 0.825f;
        bool finite = true;
        for(int k = 0; k < p.tracks.size(); ++k) {
            if(p.tracks[k]->lSections.isEmpty()) continue;
            trainRide a = rideTrain(p.tracks[k], trainSpec::point());
            trainRide b = rideTrain(p.tracks[k], big);
            finite = finite && rideFinite(a) && rideFinite(b);
        }
        if(savedBytes(p) == before && nodeBytes(p) == nodesBefore && finite) ++same;
        else report(false, "(5) " + names[i] + ": a ride changed the project or gave a value that is not a number");
    }
    report(projects >= 8 && same == projects,
           QString("(5) a ride of the point and of a 10-car train of 2 x 4 seats leaves the saved bytes and every node byte "
                   "unchanged, and every value is a number, on all %1 baseline projects").arg(projects));
}

static void checkStall(const QString& repo)
{
    // the design itself stalls (as tests/ridesummary makes it): the path stops there
    coreProject p;
    if(!load(QDir(repo).filePath("samples/baseline/four-sections.fvd"), p)) {
        report(false, "four-sections.fvd loads for the stall test");
        return;
    }
    track* t = p.tracks[0];
    section* held = t->lSections[1];
    bool stalled = false;
    float tries[] = { 5.f, 3.f, 2.f, 1.f, 0.5f };
    expectedCriticals = 1;
    for(unsigned k = 0; k < sizeof(tries)/sizeof(tries[0]) && !stalled; ++k) {
        held->fVel = tries[k];
        t->updateTrack(1, 0);
        for(int i = 0; i <= t->getNumPoints(); ++i) {
            if(!std::isfinite(t->getPoint(i)->fVel)) { stalled = true; break; }
        }
    }
    expectedCriticals = 0;
    trainRide r = rideTrain(t, trainSpec::point());
    trainRide r9 = rideTrain(t, makeTrain(9, 2.2));
    report(stalled && r.valid && r9.valid && rideFinite(r) && rideFinite(r9) && r.nodesOnPath < t->getNumPoints() + 1
           && trainRideCarByNode(r, 0, rideGz).size() == t->getNumPoints() + 1 && !r.notes.isEmpty()
           && r.notes.last().contains("stalls"),
           QString("a design that stalls: the ride follows it to its last node that is a number (%1 of %2 nodes), says so "
                   "(\"%3\"), and every value is a number").arg(r.nodesOnPath).arg(t->getNumPoints() + 1)
           .arg(r.notes.isEmpty() ? QString() : r.notes.last()));

    // the ride stalls where the design does not: far more friction than the project's
    coreProject c;
    load(QDir(repo).filePath("docs/research/r7-prototype/projects/camelback.fvd"), c);
    trainSpec sticky = makeTrain(9, 2.2);
    sticky.resistance.bFromProject = false;
    sticky.resistance.fRolling = 0.3f;
    sticky.resistance.fDrag = 0.f;
    trainRide s = rideTrain(c.tracks.last(), sticky);
    report(s.valid && s.stalled && rideFinite(s) && s.stallNode >= 0 && s.stallSection >= 1 && !s.stallText.isEmpty()
           && s.stallPlace_m > 0.0 && s.stallPlace_m < s.trackEnd_m,
           QString("a train with ten times the friction stalls on R7's camelback and says where: %1").arg(s.stallText));
    // the load-dependent and speed terms take energy too
    trainSpec loaded = makeTrain(9, 2.2);
    loaded.resistance.bFromProject = false;
    loaded.resistance.fRolling = 0.03f;
    loaded.resistance.fDrag = 2e-5f;
    trainRide base = rideTrain(c.tracks.last(), loaded);
    loaded.resistance.fRollingLoad = 0.01f;
    loaded.resistance.fLinear = 0.005f;
    trainRide more = rideTrain(c.tracks.last(), loaded);
    trainRide proj = rideTrain(c.tracks.last(), makeTrain(9, 2.2));
    double vb = base.speed_mps.last(), vm = more.speed_mps.last(), vp = proj.speed_mps.last();
    report(std::fabs(vb - vp) < 1e-4 && vm < vb - 0.1 && rideFinite(more),
           QString("the project's own two numbers typed in give the same ride (%1 against %2 m/s at the end); a load term "
                   "and a speed term take more (%3 m/s)").arg(vb, 0, 'f', 4).arg(vp, 0, 'f', 4).arg(vm, 0, 'f', 3));
}

static void checkByPlace(const QString& repo)
{
    coreProject c;
    if(!load(QDir(repo).filePath("docs/research/r7-prototype/projects/camelback.fvd"), c)) return;
    track* t = c.tracks.last();
    trainRide r = rideTrain(t, makeTrain(9, 2.2));
    QVector<bool> passedFront, passedRear;
    QVector<double> front = trainRideCarByNode(r, 0, rideGz, &passedFront);
    QVector<double> rear = trainRideCarByNode(r, 8, rideGz, &passedRear);
    int nodes = t->getNumPoints() + 1;
    // the crest: the highest node; what the front car feels there equals what it feels when it passes that place
    shapeData d = shapeOf(t);
    int top = argExt(d, d.hy, 0, d.s.last(), true);
    double atCrest = carAt(r, 0, r.nodePlace.at(top), rideGz);
    int notFront = 0, notRear = 0;
    for(int i = 0; i < nodes; ++i) {
        if(!passedFront.at(i)) ++notFront;
        if(!passedRear.at(i)) ++notRear;
    }
    // the front car starts 8.8 m ahead of the train's centre: it never passes the first 8.8 m
    int expectFront = 0;
    for(int i = 0; i < nodes; ++i) if(r.nodePlace.at(i) < r.carOffset_m.at(0)) ++expectFront;
    report(front.size() == nodes && rear.size() == nodes && std::fabs(front.at(top) - atCrest) < 1e-9
           && notFront == expectFront && notRear == 0 && front.at(top) < -0.4 && rear.at(top) < -0.3,
           QString("by place: one value per node (%1); at the crest node the front car's value is what it feels passing the "
                   "crest (%2 g; the rear car %3 g); the front car never passes the %4 nodes of the first %5 m, the rear car "
                   "passes every node").arg(nodes).arg(front.at(top), 0, 'f', 3).arg(rear.at(top), 0, 'f', 3).arg(notFront)
           .arg(r.carOffset_m.at(0), 0, 'f', 1));
    QVector<double> seat = trainRideSeatByNode(r, 0, 0, 0, rideGz);
    QVector<double> none = trainRideCarByNode(r, 9, rideGz);
    report(seat.size() == nodes && none.isEmpty(), "by place for a seat; an unknown car gives nothing");
    // samples every 0.01 s from time 0, until the rear car has passed the last node
    bool even = true;
    for(int k = 1; k < r.samples(); ++k) {
        if(std::fabs(r.time_s.at(k) - k*0.01) > 1e-9) even = false;
    }
    double rearEnd = r.position_m.last() + r.carOffset_m.at(8);
    report(even && r.time_s.first() == 0.0 && rearEnd >= r.trackEnd_m && rearEnd < r.trackEnd_m + 0.5
           && r.position_m.first() == r.trackStart_m,
           QString("samples every 0.01 s (%1 of them), the train's centre at the start at time 0, the last when the rear car "
                   "has passed the last node (%2 m past it)").arg(r.samples()).arg(rearEnd - r.trackEnd_m, 0, 'f', 3));
}

static const double kEarlyLateTol = 0.002;     // g

// A straight held at 30 m/s of the given length, then a smooth drop: a
// geometric section whose pitch rate eases from 0 to -40 degrees per second
// (the transition is 3 s long; the section ends after its own length), free speed.
static void dropTrack(track& t, float straightLength)
{
    t.newSection(straight);
    section* s = t.lSections[0];
    s->bSpeed = false;
    s->fVel = 30.f;
    s->rollFunc->changeLength(straightLength, 0);
    t.newSection(geometric);
    section* g = t.lSections[1];
    g->normForce->changeLength(3.f, 0);
    subfunc* sf = g->normForce->funcList[0];
    sf->symArg = -40.f;
    sf->parent->translateValues(sf);
    t.updateTrack(0, 0);
}

// The drop's forces, sample by sample, while the car is on the geometric
// section: the largest jump from one sample to the mean of its neighbours
// (jitter), and the values against the place along the drop.
struct dropForces
{
    QVector<double> along, value[3];
    double jitter[3];
    double accelJitter;
};

static dropForces forcesOnDrop(track& t)
{
    dropForces d;
    for(int a = 0; a < 3; ++a) d.jitter[a] = 0.0;
    d.accelJitter = 0.0;
    trainRide r = rideTrain(&t, trainSpec::point());
    measureNodes n = measureRegistry::collect(&t);
    int first = n.sectionFirst.at(1), last = n.sectionLast.at(1);
    double from = r.nodePlace.at(first) + 1.0, to = r.nodePlace.at(last) - 1.0;
    for(int k = 1; k + 1 < r.samples(); ++k) {
        double x = r.position_m.at(k);
        if(x < from || x > to) continue;
        d.along << x - r.nodePlace.at(first);
        for(int a = 0; a < 3; ++a) {
            d.value[a] << r.car(k, 0, a);
            d.jitter[a] = qMax(d.jitter[a], std::fabs(r.car(k, 0, a) - 0.5*(r.car(k - 1, 0, a) + r.car(k + 1, 0, a))));
        }
        d.accelJitter = qMax(d.accelJitter, std::fabs(r.accel_mps2.at(k) - 0.5*(r.accel_mps2.at(k - 1) + r.accel_mps2.at(k + 1))));
    }
    return d;
}

static double valueAt(const dropForces& d, int axis, double x)
{
    for(int i = 1; i < d.along.size(); ++i) {
        if(d.along.at(i) >= x) {
            double f = (x - d.along.at(i - 1))/(d.along.at(i) - d.along.at(i - 1));
            return d.value[axis].at(i - 1) + f*(d.value[axis].at(i) - d.value[axis].at(i - 1));
        }
    }
    return d.value[axis].last();
}

static void checkEarlyAndLate()
{
    track early(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f), late(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f);
    dropTrack(early, 10.f);
    dropTrack(late, 3000.f);
    dropForces e = forcesOnDrop(early), l = forcesOnDrop(late);
    double diff[3] = { 0.0, 0.0, 0.0 };
    for(int i = 0; i < e.along.size(); ++i) {
        if(e.along.at(i) < l.along.first() || e.along.at(i) > l.along.last()) continue;
        for(int a = 0; a < 3; ++a) diff[a] = qMax(diff[a], std::fabs(e.value[a].at(i) - valueAt(l, a, e.along.at(i))));
    }
    double worst = qMax(diff[0], qMax(diff[1], diff[2]));
    report(e.along.size() > 50 && l.along.size() > 50 && worst < kEarlyLateTol
           && l.jitter[rideGx] < kEarlyLateTol && e.jitter[rideGx] < kEarlyLateTol,
           QString("the same smooth drop 10 m and 3000 m along a track: the forces agree within "
                   "gz %1, gy %2, gx %3 g (limit %4 g); sample-to-sample jitter of gx early %5 g, late %6 g, of the "
                   "acceleration early %7, late %8 m/s^2")
           .arg(diff[0], 0, 'g', 2).arg(diff[1], 0, 'g', 2).arg(diff[2], 0, 'g', 2).arg(kEarlyLateTol)
           .arg(e.jitter[rideGx], 0, 'g', 2).arg(l.jitter[rideGx], 0, 'g', 2).arg(e.accelJitter, 0, 'g', 2)
           .arg(l.accelJitter, 0, 'g', 2) + QString(" (%1 and %2 samples on the drop)").arg(e.along.size()).arg(l.along.size()));
}

static void checkTiming()
{
    // 100,000 nodes: a straight of 1000 m held at 10 m/s, rolling as it goes
    track t(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f);
    t.newSection(straight);
    section* s = t.lSections[0];
    s->bSpeed = false;
    s->fVel = 10.f;
    s->rollFunc->changeLength(1010.f, 0);
    subfunc* sf = s->rollFunc->funcList[0];
    sf->symArg = 20.f;
    sf->parent->translateValues(sf);       // a straight is as long as its roll function
    t.updateTrack(0, 0);
    int nodes = t.getNumPoints() + 1;
    trainSpec big = makeTrain(10, 2.5);
    big.lRowOffsets.clear();
    big.lRowOffsets << 0.45f << -0.45f;
    big.lSeatOffsets.clear();
    big.lSeatOffsets << -0.825f << -0.275f << 0.275f << 0.825f;
    QElapsedTimer timer;
    timer.start();
    trainRide r = rideTrain(&t, big);
    double ms = timer.nsecsElapsed()/1e6;
    // (the limit leaves room for a busy PC; the plan's own number is half a second)
    report(r.valid && nodes >= 100000 && ms < 400.0,
           QString("timing: a %1-node track with 10 cars of 2 x 4 seats rides in %2 ms (limit 400 ms; the plan asks for "
                   "well under 500 ms), %3 samples").arg(nodes).arg(ms, 0, 'f', 1).arg(r.samples()));
    // the same long track: the front car's gx from one sample to the mean of its neighbours
    double jitter = 0.0;
    for(int k = 1; k + 1 < r.samples(); ++k) {
        double x = r.position_m.at(k) + r.carOffset_m.at(0);
        if(x < 20.0 || x > r.trackEnd_m - 20.0) continue;
        jitter = qMax(jitter, std::fabs(r.car(k, 0, rideGx) - 0.5*(r.car(k - 1, 0, rideGx) + r.car(k + 1, 0, rideGx))));
    }
    report(jitter < kEarlyLateTol, QString("on that 1010 m track (held speed) the front car's gx jitters by %1 g from sample "
                                           "to sample (limit %2 g)").arg(jitter, 0, 'g', 2).arg(kEarlyLateTol));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    if(argc < 2) {
        fprintf(stderr, "usage: trainride <repository folder>\n");
        return 1;
    }
    QString repo = QString::fromLocal8Bit(argv[1]);
    checkSpec(repo);
    checkPointOnBaseline(repo);
    checkR7(repo);
    checkClosedForm();
    checkAlongClosedForm();
    checkSeats(repo);
    checkUnchanged(repo);
    checkStall(repo);
    checkByPlace(repo);
    checkEarlyAndLate();
    checkTiming();
    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
