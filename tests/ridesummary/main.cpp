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

// Test of core/ridesummary.*: the ride sheet's numbers. Known-answer checks on
// every project in samples/baseline/ (length, top speed, peak forces, duration,
// sections, nothing written), airtime on a track built through the model with a
// held vertical force, inversions on a full roll and an overbank, the biggest
// drop on synthetic heights, the band edges and the threshold hovering on
// synthetic arrays, and an empty and a stalled track. Gets the repository
// folder as its first argument; prints one line per check and PASS or FAIL.

#include "../../core/track.h"
#include "../../core/projectfile.h"
#include "../../core/measure.h"
#include "../../core/finding.h"
#include "../../core/ridesummary.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStringList>
#include <QTemporaryDir>
#include <QtDebug>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static bool allOk = true;

static void report(bool ok, const QString& text)
{
    printf("%s: %s\n", ok ? "ok" : "FAIL", qPrintable(text));
    if(!ok) allOk = false;
}

// A stalled train makes the model's own NaN checks (lenAssert, a qCritical in
// a release build) fire; that is the case under test there.
static int expectedCriticals = 0;

static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg && expectedCriticals > 0 && msg.startsWith("Assertion")) return;
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
}

// Copied from tests/measures/main.cpp (class coreProject): what the windowed
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
        bool loaded = true;
        texPath = path;
        if(!canLoad(texPath)) {
            texPath = QString(":/background.png");
            loaded = false;
        }
        return loaded;
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

private:
    static bool canLoad(const QString& path)
    {
        if(path.startsWith(":/")) {
            static const char* resources[] = { ":/background.png", ":/metal.png", ":/sky/negx.jpg", ":/sky/negy.jpg",
                                               ":/sky/negz.jpg", ":/sky/posx.jpg", ":/sky/posy.jpg", ":/sky/posz.jpg" };
            for(unsigned i = 0; i < sizeof(resources)/sizeof(resources[0]); ++i) {
                if(path == resources[i]) return true;
            }
            return false;
        }
        return !QImage(path).isNull();
    }
};

static QByteArray readAll(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
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

static bool close(double a, double b, double tol)
{
    return fabs(a - b) <= tol;
}

// ---------------------------------------------------------------------------
// Synthetic arrays through the pure helpers
// ---------------------------------------------------------------------------

static void checkAirtimeBands()
{
    rideSummaryOptions o;
    // one entry per band edge: each worth 1 s
    QVector<float> g;
    g << 0.5f << 0.1f << std::nextafterf(0.1f, 1.f) << 0.f << -0.4999999f << -0.5f << std::nextafterf(-0.5f, 0.f)
      << -1.f << NAN << INFINITY << -INFINITY << 1.f;
    QVector<double> dt(g.size(), 1.0);
    double floater = -1, ejector = -1;
    rideAirtime(g, dt, o, &floater, &ejector);
    // floater: 0.1, 0, -0.4999999, nextafter(-0.5) = 4 s; ejector: -0.5, -1 = 2 s
    // (-INFINITY is at or below the bound but not a finite number: neither)
    report(floater == 4.0 && ejector == 2.0,
           QString("band edges: +0.1 g is a floater, just above it is not; -0.5 g is an ejector, just above it a floater; "
                   "NaN and infinity count as neither (floater %1 s, ejector %2 s)").arg(floater).arg(ejector));
    dt[0] = 0.25; dt[1] = 0.5;
    rideAirtime(g, dt, o, &floater, &ejector);
    report(floater == 3.5, QString("airtime adds the node's own step (floater %1 s)").arg(floater));
}

static void checkInversionHelper()
{
    QVector<double> hover;
    hover << 0 << 134 << 136 << 134 << 136 << 133 << 136 << 0;
    report(rideCountInversions(hover, 135.0, 5.0) == 1, "hovering around 135 degrees (never below 130) counts once");
    QVector<double> twice;
    twice << 0 << 136 << 129.9 << 136 << 0;
    report(rideCountInversions(twice, 135.0, 5.0) == 2, "falling below 130 degrees and rising again counts a second inversion");
    QVector<double> exact;
    exact << 135.0 << 90.0;
    report(rideCountInversions(exact, 135.0, 5.0) == 0, "exactly 135 degrees is not more than 135");
    QVector<double> nan;
    nan << 0 << NAN << 160 << NAN << 0;
    report(rideCountInversions(nan, 135.0, 5.0) == 1, "non-finite angles are skipped");
}

static QVector<double> profile(const double* points, int n)
{
    // linear interpolation between the given heights, 10 samples each
    QVector<double> h;
    for(int i = 0; i + 1 < n; ++i) {
        for(int k = 0; k < 10; ++k) h.append(points[i] + (points[i+1]-points[i])*k/10.0);
    }
    h.append(points[n-1]);
    return h;
}

static void checkBiggestFall()
{
    int from, to;
    // up to 50, down to 30, a bump up of 0.5 m, down to 10, up to 20
    double a[] = { 0, 50, 30, 30.5, 10, 20 };
    QVector<double> h = profile(a, 6);
    double drop = rideBiggestFall(h, 1.0, false, &from, &to);
    report(close(drop, 40.0, 1e-9) && close(h[from], 50.0, 1e-9) && close(h[to], 10.0, 1e-9),
           QString("a drop with a 0.5 m bump in the middle is one drop (%1 m, from %2 to %3)").arg(drop).arg(from).arg(to));
    double b[] = { 0, 50, 30, 32, 10, 20 };
    h = profile(b, 6);
    drop = rideBiggestFall(h, 1.0, false, &from, &to);
    report(close(drop, 22.0, 1e-9) && close(h[from], 32.0, 1e-9) && close(h[to], 10.0, 1e-9),
           QString("with a 2 m bump it is two drops, 20 m and 22 m; the biggest is the second, 32 m down to 10 m (%1 m)").arg(drop));
    double climb = rideBiggestFall(h, 1.0, true, &from, &to);
    report(close(climb, 50.0, 1e-9) && close(h[from], 0.0, 1e-9) && close(h[to], 50.0, 1e-9),
           QString("the biggest climb is the same measure upside down: 0 m up to 50 m (%1 m)").arg(climb));
    double c[] = { 0, 20, 19.5, 30, 0 };
    h = profile(c, 5);
    climb = rideBiggestFall(h, 1.0, true, &from, &to);
    report(close(climb, 30.0, 1e-9) && close(h[from], 0.0, 1e-9) && close(h[to], 30.0, 1e-9),
           QString("a 0.5 m dip on the way up does not split a climb (%1 m)").arg(climb));
    QVector<double> flat(20, 5.0);
    drop = rideBiggestFall(flat, 1.0, false, &from, &to);
    report(drop == 0.0 && from == -1 && to == -1, "level ground has no drop");
    QVector<double> nan;
    nan << NAN << 10 << NAN << 4 << 7;
    drop = rideBiggestFall(nan, 1.0, false, &from, &to);
    report(close(drop, 6.0, 1e-9), "non-finite heights are skipped");
}

// ---------------------------------------------------------------------------
// Through the model
// ---------------------------------------------------------------------------

// A level forced section: the vertical force ramps from its start value to
// value in 0.1 s and holds for hold seconds. Returns the track's summary.
static rideSummary heldForce(float value, float hold, double* expectTotal)
{
    track t(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f);
    t.newSection(forced);
    section* s = t.lSections[0];
    float total = 0.1f + hold;
    s->rollFunc->changeLength(total, 0);
    s->latForce->changeLength(total, 0);
    s->normForce->changeLength(0.1f, 0);
    s->normForce->appendSubFunction(hold, 0);
    // the way ui/transitionwidget.cpp changes a transition
    subfunc* sf = s->normForce->funcList[0];
    sf->symArg = value - sf->startValue;
    sf->parent->translateValues(sf);
    t.updateTrack(0, 0);
    if(expectTotal) *expectTotal = total;
    return computeRideSummary(&t);
}

static void checkAirtimeModel()
{
    double total;
    rideSummary r = heldForce(0.f, 3.f, &total);
    report(close(r.duration_s, total, 0.01),
           QString("0 g for 3 s: the section lasts %1 s (expected %2)").arg(r.duration_s).arg(total));
    // the ramp from 1 g to 0 g in 0.1 s spends 0.01 s at or below +0.1 g
    report(close(r.floaterAirtime_s, 3.01, 0.01) && r.ejectorAirtime_s == 0.0 && close(r.totalAirtime_s, r.floaterAirtime_s, 1e-12),
           QString("0 g for 3 s: floater %1 s (expected 3.01), ejector %2 s (expected 0), total %3 s")
           .arg(r.floaterAirtime_s).arg(r.ejectorAirtime_s).arg(r.totalAirtime_s));
    report(close(r.normalMin.value, 0.0, 1e-3), QString("... lowest vertical force %1 g").arg(r.normalMin.value));

    r = heldForce(-1.f, 3.f, &total);
    // the ramp from 1 g to -1 g in 0.1 s: 0.03 s between +0.1 and -0.5 g, 0.025 s between -0.5 and -1 g
    report(close(r.ejectorAirtime_s, 3.025, 0.01) && close(r.floaterAirtime_s, 0.03, 0.01)
           && close(r.totalAirtime_s, 3.055, 0.02),
           QString("-1 g for 3 s: ejector %1 s (expected 3.025), floater %2 s (expected 0.03), total %3 s (expected 3.055)")
           .arg(r.ejectorAirtime_s).arg(r.floaterAirtime_s).arg(r.totalAirtime_s));
    report(close(r.normalMin.value, -1.0, 0.01) && r.normalMin.valid, QString("... lowest vertical force %1 g").arg(r.normalMin.value));
    report(r.sections.size() == 1 && r.sections[0].typeName == "force" && close(r.sections[0].duration_s, r.duration_s, 1e-12),
           "... one section, a force section, as long as the ride");
}

// A straight at a held 10 m/s whose roll rate ramps from 0 to rate deg/s over
// 100 m (10 s): the total roll is rate * 5 degrees.
static rideSummary rolled(float rate)
{
    track t(glm::vec3(0.f, 50.f, 0.f), 0.f, 1.1f);
    t.newSection(straight);
    section* s = t.lSections[0];
    s->bSpeed = false;
    s->fVel = 10.f;
    s->rollFunc->changeLength(100.f, 0);
    subfunc* sf = s->rollFunc->funcList[0];
    sf->symArg = rate;
    sf->parent->translateValues(sf);
    t.updateTrack(0, 0);
    return computeRideSummary(&t);
}

static void checkInversionsModel()
{
    rideSummary full = rolled(72.f);
    report(full.inversions == 1 && full.maxTilt.value > 175.0,
           QString("a full 360 degree roll counts 1 inversion (largest angle from upright %1 degrees, at %2 s)")
           .arg(full.maxTilt.value, 0, 'f', 1).arg(full.maxTilt.time_s, 0, 'f', 1));
    rideSummary over = rolled(22.f);
    report(over.inversions == 0 && over.maxTilt.value > 100.0 && over.maxTilt.value < 130.0,
           QString("a 100 to 130 degree overbank counts 0 (largest angle %1 degrees)").arg(over.maxTilt.value, 0, 'f', 1));
    rideSummary level = rolled(0.f);
    report(level.inversions == 0 && level.maxTilt.value < 1.0, QString("a level straight is never tilted (%1 degrees)").arg(level.maxTilt.value));
}

static void checkEmptyAndStalled(const QString& repo)
{
    // (the default constructor of track leaves its members unset: build one the usual way)
    track none(glm::vec3(0.f, 5.f, 0.f), 0.f, 1.1f);
    rideSummary e = computeRideSummary(&none);
    report(e.empty && e.sectionCount == 0 && e.length_m == 0.0 && e.duration_s == 0.0 && !e.highest.valid
           && e.inversions == 0 && e.sections.isEmpty() && e.findings.isEmpty() && !e.nonFinite,
           "a track without sections gives an empty result and no crash");
    rideSummary nul = computeRideSummary(NULL);
    report(nul.empty, "no track at all gives an empty result");

    coreProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &p);
    if(p.tracks.isEmpty() || p.tracks[0]->lSections.size() < 3) {
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
    if(!stalled) {
        report(false, "the train never stalled");
        return;
    }
    rideSummary r = computeRideSummary(t);
    bool finite = std::isfinite(r.length_m) && std::isfinite(r.duration_s) && std::isfinite(r.floaterAirtime_s)
            && std::isfinite(r.ejectorAirtime_s) && std::isfinite(r.heightRange_m) && std::isfinite(r.topSpeed.value)
            && std::isfinite(r.normalMax.value) && std::isfinite(r.normalMin.value);
    bool sectionsFinite = true;
    for(int i = 0; i < r.sections.size(); ++i) {
        if(!std::isfinite(r.sections[i].length_m) || !std::isfinite(r.sections[i].duration_s)) sectionsFinite = false;
    }
    report(r.nonFinite && finite && sectionsFinite && r.findingCount[findingProblem] > 0,
           QString("a stalled train: the non-finite flag is set, every sum and extreme is a number (top speed %1 m/s, "
                   "duration %2 s), the findings say so (%3 problem(s))")
           .arg(r.topSpeed.value).arg(r.duration_s).arg(r.findingCount[findingProblem]));

    // The length stops at the stall: that of the last node whose length is a
    // number (not 0, which it was when the last node's length was taken).
    int lastNode = t->getNumPoints();
    int lastFinite = -1;
    for(int i = lastNode; i >= 0; --i) {
        if(std::isfinite(t->getPoint(i)->fTotalLength)) { lastFinite = i; break; }
    }
    report(!std::isfinite(t->getPoint(lastNode)->fTotalLength) && lastFinite >= 0 && r.lengthStopsAtStall
           && r.length_m > 0.0 && r.length_m == (double)t->getPoint(lastFinite)->fTotalLength,
           QString("a stalled train: the length is %1 m, that of the last node with a length (node %2 of %3), and the result says it stops at the stall")
           .arg(r.length_m).arg(lastFinite).arg(lastNode));
    rideSummary fine = computeRideSummary(&none);
    report(!fine.lengthStopsAtStall && !e.lengthStopsAtStall, "an empty track does not claim a stall");
}

// ---------------------------------------------------------------------------
// Every baseline project
// ---------------------------------------------------------------------------

// The check "forceLimits" (core/designforcelimits.*) on arrays made by hand: the
// resampling to a uniform trace, the mapping of the engine's findings back to
// nodes, and a track that stalls.
static void checkDesignLimitsArrays()
{
    // nodes 1 ms apart for 3 s, then 2 ms apart for 1 s: a linear ramp comes out as a ramp
    QVector<double> times;
    QVector<float> ramp;
    double now = 0.0;
    for(int i = 0; i < 4000; ++i) { times.append(now); ramp.append((float)now); now += (i < 3000) ? 0.001 : 0.002; }
    QVector<float> zero(times.size(), 0.f);
    forceTrace tr = designPointTrace(times, ramp, zero, ramp, times.size() - 1);
    bool ok = tr.dt == 1.0/F_HZ && tr.size() > 3000 && tr.gx.size() == tr.gz.size();
    double worst = 0.0;
    for(int s = 0; ok && s < tr.size(); ++s) worst = qMax(worst, std::fabs(tr.gx[s] - s*tr.dt));
    report(ok && worst < 1e-5 && tr.size() == (int)std::floor(times.last()/tr.dt + 1e-6) + 1,
           QString("designPointTrace: a ramp over unevenly spaced nodes is resampled exactly (%1 samples, largest error %2 g)").arg(tr.size()).arg(worst));

    // 6 s at 1 g with 0.6 s at 9 g (above the +Gz limit for that long): findings on those nodes
    times.clear();
    QVector<float> normal;
    for(int i = 0; i < 6000; ++i) { times.append(i*0.001); normal.append((i >= 2000 && i < 2600) ? 9.f : 1.f); }
    QVector<float> zeros(times.size(), 0.f);
    designLimitsInfo info;
    QList<finding> f = designPointForceLimits(times, zeros, zeros, normal, &info);
    ok = info.bRan && !f.isEmpty() && info.iFindings == f.size();
    bool inside = true;
    for(int i = 0; i < f.size(); ++i) {
        const finding& x = f.at(i);
        if(x.checkId != "forceLimits" || x.fromNode < 1900 || x.toNode > 2800 || x.worstNode < x.fromNode || x.worstNode > x.toNode
                || x.severity != findingWarning || x.text.isEmpty()) inside = false;
    }
    report(ok && inside, QString("forceLimits: 0.6 s at 9 g gives %1 finding(s), all on nodes 1900 to 2800").arg(f.size()));
    report(info.value() == QString("%1 findings").arg(f.size()) && info.label().contains("public numbers"),
           "the ride sheet's words: " + info.label() + ": " + info.value());

    QVector<float> calm(times.size(), 1.f);
    f = designPointForceLimits(times, zeros, zeros, calm, &info);
    report(f.isEmpty() && info.bRan && info.value() == "within", "forceLimits: a steady 1 g is within the limits");

    // a force that is not a number: judged up to the node before it
    QVector<float> stall = normal;
    for(int i = 4000; i < stall.size(); ++i) stall[i] = std::numeric_limits<float>::quiet_NaN();
    f = designPointForceLimits(times, zeros, zeros, stall, &info);
    bool before = true;
    for(int i = 0; i < f.size(); ++i) if(f.at(i).toNode >= 4000) before = false;
    report(info.bRan && info.iJudgedNodes == 4000 && before && !f.isEmpty(),
           QString("forceLimits: a stalled train is judged up to the first force that is not a number (%1 nodes judged)").arg(info.iJudgedNodes));

    designLimitsInfo none;
    f = designPointForceLimits(QVector<double>(), QVector<float>(), QVector<float>(), QVector<float>(), &none);
    report(f.isEmpty() && !none.bRan, "forceLimits: no nodes, no finding");

    // what the check costs on 150,000 nodes (150 s at 1 ms) of a rolling force, without the measures
    QVector<double> longTimes;
    QVector<float> longG, longZ;
    for(int i = 0; i < 150000; ++i) {
        longTimes.append(i*0.001);
        longG.append((float)(0.3*std::sin(i*0.002)));
        longZ.append((float)(1.0 + 1.5*std::sin(i*0.003)));
    }
    QElapsedTimer clock;
    clock.start();
    f = designPointForceLimits(longTimes, longG, longG, longZ, &info);
    printf("  time: the check forceLimits on %d nodes (measures not included): %lld ms, %d finding(s)\n",
           (int)longTimes.size(), (long long)clock.elapsed(), (int)f.size());
    report(info.bRan, "forceLimits: runs on 150,000 nodes");
}

static int turnSamples[2] = { 0, 0 };       // nodes in a flat left turn / right turn
static int turnSign[2] = { 0, 0 };          // ... whose lateral force has the sign expected

static void checkProject(const QString& file, const QString& tmp)
{
    QString name = QFileInfo(file).fileName();
    coreProject p;
    QString loaded = loadProjectFile(file, &p);
    if(loaded.contains("Error") || p.tracks.isEmpty()) {
        report(false, name + " loads");
        return;
    }
    QString saved1 = QDir(tmp).filePath("before.fvd"), saved2 = QDir(tmp).filePath("after.fvd");
    saveProjectFile(saved1, p.texPath, p.tracks, &p);
    QByteArray nodesBefore = nodeBytes(p);

    QList<rideSummary> sums;
    for(int k = 0; k < p.tracks.size(); ++k) {
        track* t = p.tracks[k];
        rideSummary r = computeRideSummary(t);
        sums.append(r);
        QString label = QString("%1 track %2").arg(name).arg(k+1);
        if(t->lSections.isEmpty()) {
            report(r.empty, label + ": no sections, an empty ride sheet");
            continue;
        }
        int last = t->getNumPoints();
        QVector<QVector<float> > m = measureRegistry::values(t, QStringList() << "speed" << "forceNormal" << "forceLateral" << "forceAlong" << "height");
        double maxSpeed = -1e30, minSpeed = 1e30;
        float nmax = -1e30f, nmin = 1e30f, lmax = -1e30f, lmin = 1e30f, amax = -1e30f, amin = 1e30f, hmax = -1e30f, hmin = 1e30f;
        for(int i = 0; i <= last; ++i) {
            double v = t->getPoint(i)->fVel;
            if(std::isfinite(v)) { maxSpeed = qMax(maxSpeed, v); minSpeed = qMin(minSpeed, v); }
            if(std::isfinite(m[1][i])) { nmax = qMax(nmax, m[1][i]); nmin = qMin(nmin, m[1][i]); }
            if(std::isfinite(m[2][i])) { lmax = qMax(lmax, m[2][i]); lmin = qMin(lmin, m[2][i]); }
            if(std::isfinite(m[3][i])) { amax = qMax(amax, m[3][i]); amin = qMin(amin, m[3][i]); }
            if(std::isfinite(m[4][i])) { hmax = qMax(hmax, m[4][i]); hmin = qMin(hmin, m[4][i]); }
        }
        bool okLen = r.length_m == (double)t->getPoint(last)->fTotalLength
                && r.heartLength_m == (double)t->getPoint(last)->fTotalHeartLength;
        report(okLen, label + QString(": length %1 m (heartline %2 m) equals the last node's totals").arg(r.length_m, 0, 'f', 2).arg(r.heartLength_m, 0, 'f', 2));
        report(r.topSpeed.value == maxSpeed && r.lowSpeed.value == minSpeed && r.topSpeed.node >= 0 && r.topSpeed.node <= last,
               label + QString(": top speed %1 m/s, lowest %2 m/s equal the extremes over the nodes").arg(r.topSpeed.value, 0, 'f', 2).arg(r.lowSpeed.value, 0, 'f', 2));
        report(t->getPoint(r.topSpeed.node)->fVel == (float)r.topSpeed.value
               && t->getPoint(r.topSpeed.node)->fTotalLength == (float)r.topSpeed.dist_m,
               label + ": the top speed is where it says");
        report((float)r.normalMax.value == nmax && (float)r.normalMin.value == nmin && (float)r.lateralRight.value == lmax
               && (float)r.lateralLeft.value == lmin && (float)r.alongMax.value == amax && (float)r.alongMin.value == amin,
               label + QString(": peak forces equal the extremes of the measures (vertical %1 to %2 g, lateral %3 to %4 g, along %5 to %6 g)")
               .arg(nmin, 0, 'f', 2).arg(nmax, 0, 'f', 2).arg(lmin, 0, 'f', 2).arg(lmax, 0, 'f', 2).arg(amin, 0, 'f', 2).arg(amax, 0, 'f', 2));
        report((float)r.highest.value == hmax && (float)r.lowest.value == hmin && close(r.heightRange_m, (double)hmax - hmin, 1e-3),
               label + QString(": highest %1 m and lowest %2 m equal the height measure's extremes").arg(hmax, 0, 'f', 2).arg(hmin, 0, 'f', 2));
        report(r.biggestDrop.amount_m <= r.heightRange_m + 1e-6 && r.biggestClimb.amount_m <= r.heightRange_m + 1e-6
               && r.biggestDrop.amount_m >= 0.0,
               label + QString(": biggest drop %1 m and climb %2 m fit inside the height range").arg(r.biggestDrop.amount_m, 0, 'f', 2).arg(r.biggestClimb.amount_m, 0, 'f', 2));

        bool stepped = true;
        for(int s = 0; s < t->lSections.size(); ++s) {
            if(t->lSections[s]->type == bezier || t->lSections[s]->type == nolimitscsv) stepped = false;
        }
        if(stepped) {
            report(close(r.duration_s, last/F_HZ, 1e-6),
                   label + QString(": duration %1 s equals the node count / %2 for time-stepped sections").arg(r.duration_s, 0, 'f', 3).arg(F_HZ));
        } else {
            report(r.duration_s > 0.0 && close(r.duration_s, last/F_HZ, 0.05*last/F_HZ),
                   label + QString(": duration %1 s is within 5% of the node count / %2 (distance-spaced sections)").arg(r.duration_s, 0, 'f', 3).arg(F_HZ));
        }
        double sumLen = 0.0, sumDur = 0.0;
        for(int s = 0; s < r.sections.size(); ++s) { sumLen += r.sections[s].length_m; sumDur += r.sections[s].duration_s; }
        report(r.sections.size() == t->lSections.size() && close(sumLen, r.length_m, 0.01) && close(sumDur, r.duration_s, 1e-6)
               && r.sections.last().lastNode == last,
               label + QString(": %1 sections whose lengths and durations add up to the totals").arg(r.sections.size()));
        report(r.totalAirtime_s == r.floaterAirtime_s + r.ejectorAirtime_s && r.floaterAirtime_s >= 0.0 && r.ejectorAirtime_s >= 0.0
               && r.totalAirtime_s <= r.duration_s + 1e-9,
               label + QString(": airtime floater %1 s, ejector %2 s, total %3 s").arg(r.floaterAirtime_s, 0, 'f', 2).arg(r.ejectorAirtime_s, 0, 'f', 2).arg(r.totalAirtime_s, 0, 'f', 2));
        QList<finding> f = checkRegistry::runAll(t);
        report(f.size() == r.findings.size() && r.findingCount[0] + r.findingCount[1] + r.findingCount[2] == f.size(),
               label + QString(": %1 finding(s) counted by severity").arg(f.size()));
        {
            // the ride sheet's line counts the findings of the check forceLimits; the gen- projects are within
            // the limits, the others (four-sections, n1-) break some (cross-checked with `fvdcore limits --trace`)
            int limitFindings = 0;
            for(int k = 0; k < f.size(); ++k) if(f.at(k).checkId == "forceLimits") ++limitFindings;
            QElapsedTimer clock;
            clock.start();
            QList<finding> again = checkRegistry::run(t, "forceLimits");
            printf("  time: the whole check forceLimits (measures, resampling, engine) on %s, %d nodes: %lld ms, %d finding(s)\n",
                   qPrintable(label), (int)last + 1, (long long)clock.elapsed(), (int)again.size());
            // Pinned only as far as the along-track measure allows: the totals of the projects with
            // spikes move with issue 96 (the along-track force), and are pinned after it.
            const QString fname = QFileInfo(file).fileName();
            int onVertical = 0, firstNode = -1, lastNodeOf = -1;
            for(int k = 0; k < f.size(); ++k) {
                if(f.at(k).checkId != "forceLimits" || f.at(k).measureId != "forceNormal") continue;
                ++onVertical;
                if(firstNode < 0 || f.at(k).fromNode < firstNode) firstNode = f.at(k).fromNode;
                if(f.at(k).toNode > lastNodeOf) lastNodeOf = f.at(k).toNode;
            }
            if(fname == "n1-tension-force-chain.fvd") {
                report(limitFindings == 1 && onVertical == 1 && firstNode >= 2200 && lastNodeOf <= 2900,
                       label + QString(": exactly one force-limit finding, on the vertical force, within 2.2 s to 2.9 s (%1 to %2 nodes)").arg(firstNode).arg(lastNodeOf));
            } else if(fname == "four-sections.fvd" || fname == "n1-tension-loop.fvd") {
                report(onVertical >= 1, label + QString(": at least one force-limit finding on the vertical force at the spike (%1)").arg(onVertical));
            }
            const bool gen = QFileInfo(file).fileName().startsWith("gen-");
            report(r.forceLimits.bRan && r.forceLimits.iFindings == limitFindings && (!gen || limitFindings == 0),
                   label + QString(": force limits say \"%1\" (%2 finding(s))").arg(r.forceLimits.value()).arg(limitFindings));
        }
        printf("  inversions %d, largest angle from upright %.1f degrees\n", r.inversions, r.maxTilt.value);

        // the sign of the lateral force: flat turns
        for(int i = 1; i <= last; ++i) {
            mnode* n = t->getPoint(i);
            if(fabs(n->fRoll) < 3.f && fabs(n->getPitch()) < 3.f && fabs(n->fYawFromLast) > 0.005f && std::isfinite(m[2][i])) {
                int side = n->fYawFromLast > 0 ? 0 : 1;      // a positive heading change is a left turn
                turnSamples[side]++;
                if((side == 0) == (m[2][i] > 0.f)) turnSign[side]++;
            }
        }
    }

    // nothing was written
    QByteArray nodesAfter = nodeBytes(p);
    saveProjectFile(saved2, p.texPath, p.tracks, &p);
    report(nodesBefore == nodesAfter && readAll(saved1) == readAll(saved2) && !readAll(saved1).isEmpty(),
           name + ": nodes and the saved file are the same before and after the ride sheet");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    if(argc < 2) {
        fprintf(stderr, "usage: ridesummary <repository folder>\n");
        return 1;
    }
    QString repo = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }

    rideSummaryOptions defaults;
    report(defaults.floaterUpperG == 0.1f && defaults.ejectorUpperG == -0.5f && defaults.inversionAngleDeg == 135.0
           && defaults.inversionReleaseDeg == 5.0 && defaults.dropHysteresisM == 1.0,
           "the default thresholds are the owner's: floater +0.1 g, ejector -0.5 g, inversion 135 degrees (release 5, drop bump 1 m)");
    rideSummaryOptions changed;
    changed.floaterUpperG = 0.25f;
    changed.inversionAngleDeg = 120.0;
    report(rideFloaterLabel(defaults).contains("+0.1 g") && rideFloaterLabel(defaults).contains("-0.5 g")
           && rideInversionLabel(defaults).contains("135") && rideEjectorLabel(defaults).contains("-0.5 g")
           && rideFloaterLabel(changed).contains("+0.25") && rideInversionLabel(changed).contains("120")
           && rideSheetNote().contains("heartline"),
           "the sheet's labels are built from the options: " + rideFloaterLabel(defaults) + "; " + rideInversionLabel(changed));
    report(rideSectionTypeName(forced) == "force" && rideSectionTypeName(nolimitscsv) == "nolimits-csv", "section type names");

    checkAirtimeBands();
    checkInversionHelper();
    checkBiggestFall();
    checkAirtimeModel();
    checkInversionsModel();
    checkEmptyAndStalled(repo);
    checkDesignLimitsArrays();

    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList files = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    int projects = 0;
    for(int i = 0; i < files.size(); ++i) {
        if(files[i].contains(".2016.")) continue;
        ++projects;
        checkProject(baseline.filePath(files[i]), tmp.path());
    }
    report(projects >= 5, QString("%1 baseline projects checked").arg(projects));

    printf("  flat turns: left %d nodes (%d with a positive lateral force), right %d nodes (%d with a negative one)\n",
           turnSamples[0], turnSign[0], turnSamples[1], turnSign[1]);
    bool signOk = true;
    for(int s = 0; s < 2; ++s) {
        if(turnSamples[s] > 0 && turnSign[s] < 0.95*turnSamples[s]) signOk = false;
    }
    report(signOk && (turnSamples[0] + turnSamples[1]) >= 100,
           "lateral force sign: a left turn presses the rider toward his right (positive), a right turn toward his left (negative)");

    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
