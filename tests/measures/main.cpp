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

// Test of core/measure.* and core/finding.*: the registries, every measure on
// every project in samples/baseline/, that computing them writes nothing, the
// along-track force against the program's own energy model, the rates against
// the plot's formulas, and the basic check. Gets the repository folder as its
// first argument; prints one line per check and PASS or FAIL.

#include "../../core/track.h"
#include "../../core/section.h"
#include "../../core/projectfile.h"
#include "../../core/elementfile.h"
#include "../../core/measure.h"
#include "../../core/finding.h"
#include "../../core/heartline.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QtDebug>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static bool allOk = true;

static void report(bool ok, const QString& text)
{
    printf("%s: %s\n", ok ? "ok" : "FAIL", qPrintable(text));
    if(!ok) allOk = false;
}

// The exporters and the loader log thousands of lines: drop everything but
// a failed model check.
static int expectedCriticals = 0;     // model checks that fail on purpose (the stall test)

static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg && expectedCriticals > 0 && msg.startsWith("Assertion")) return;
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
}

// Copied from tests/coreonly/main.cpp (class coreProject): what the windowed
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

static QStringList allIds()
{
    QStringList ids;
    const QVector<measureDef>& table = measureRegistry::all();
    for(int i = 0; i < table.size(); ++i) ids << table.at(i).id;
    return ids;
}

static QByteArray readAll(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

// The re-saved project and the three exports of every track, as bytes.
static QList<QByteArray> writeEverything(coreProject& p, const QString& dir)
{
    QList<QByteArray> out;
    QString saved = QDir(dir).filePath("p.fvd");
    saveProjectFile(saved, p.texPath, p.tracks, &p);
    out << readAll(saved);
    float fRollThresh = sin(85.0*F_PI/180.f);
    for(int i = 0; i < p.tracks.size(); ++i) {
        track* t = p.tracks[i];
        if(t->lSections.isEmpty()) continue;
        int last = t->lSections.size()-1;
        QString a = QDir(dir).filePath(QString("t%1.nl2elem").arg(i));
        QString b = QDir(dir).filePath(QString("t%1.tangent.nlelem").arg(i));
        QString c = QDir(dir).filePath(QString("t%1.spline.nlelem").arg(i));
        writeNL2Element(a, t, 2.f, 0, last);
        writeNL1TangentElement(b, t, 2.f, 0, last, fRollThresh, false);
        writeNL1SplineElement(c, t, 2.f, 0, last, fRollThresh, false);
        out << readAll(a) << readAll(b) << readAll(c);
    }
    return out;
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

static void checkRegistries()
{
    const QVector<measureDef>& m = measureRegistry::all();
    QSet<QString> ids;
    bool ok = !m.isEmpty();
    for(int i = 0; i < m.size(); ++i) {
        const measureDef& d = m.at(i);
        if(d.id.isEmpty() || d.name.isEmpty() || d.unit.isEmpty() || d.description.isEmpty() || !d.fill
                || !(d.displayMin < d.displayMax) || ids.contains(d.id) || measureRegistry::find(d.id) != &d) {
            ok = false;
            printf("  bad measure entry %d (%s)\n", i, qPrintable(d.id));
        }
        ids.insert(d.id);
    }
    report(ok, QString("%1 measures, each with an id, a name, a unit, a description and a range; ids unique").arg(m.size()));
    QStringList need;
    need << "speed" << "height" << "forceNormal" << "forceLateral" << "forceAlong" << "jerkNormal" << "jerkLateral"
         << "jerkAlong" << "rollRate" << "rollAccel" << "pitchRate" << "yawRate";
    bool all = true;
    for(int i = 0; i < need.size(); ++i) if(!measureRegistry::find(need.at(i))) { all = false; printf("  missing %s\n", qPrintable(need.at(i))); }
    report(all && measureRegistry::find("nonsense") == NULL, "every measure of this stage is registered; an unknown id is not found");

    const QList<checkDef>& c = checkRegistry::all();
    QSet<QString> cids;
    ok = !c.isEmpty();
    for(int i = 0; i < c.size(); ++i) {
        const checkDef& d = c.at(i);
        if(d.id.isEmpty() || d.name.isEmpty() || d.description.isEmpty() || !d.run || cids.contains(d.id)
                || checkRegistry::find(d.id) != &d) ok = false;
        cids.insert(d.id);
    }
    report(ok && checkRegistry::find("basic"), QString("%1 check(s), each with an id, a name and a description; ids unique; basic present").arg(c.size()));
}

// findOutside() on numbers whose answer is known.
static void checkFindOutside()
{
    // Speed under 3 m/s from node 41200 to 43000, lowest 2.4 at 42000.
    QVector<float> v(50000, 10.f);
    for(int i = 41200; i <= 43000; ++i) v[i] = 2.9f;
    v[42000] = 2.4f;
    QList<finding> f = findOutside(v, "basic", "speed", 3.f, INFINITY, findingProblem);
    QString expect = "Speed falls to 2.4 m/s, below 3 m/s, between 41.2 s and 43.0 s";
    bool ok = f.size() == 1 && f[0].fromNode == 41200 && f[0].toNode == 43000 && f[0].worstNode == 42000
            && f[0].worstValue == 2.4f && f[0].limit == 3.f && f[0].below && f[0].text == expect;
    report(ok, QString("one stretch with the right ends, worst point and sentence (\"%1\")").arg(f.isEmpty() ? QString() : f[0].text));

    // Two stretches 50 nodes apart merge; 200 apart do not; both sides.
    QVector<float> w(10000, 1.f);
    for(int i = 1000; i < 1100; ++i) w[i] = 6.f;
    for(int i = 1150; i < 1200; ++i) w[i] = 5.5f;
    for(int i = 1400; i < 1500; ++i) w[i] = 5.2f;
    for(int i = 3000; i < 3010; ++i) w[i] = -2.f;
    f = findOutside(w, "basic", "forceNormal", -1.5f, 5.f, findingProblem);
    ok = f.size() == 3
            && f[0].fromNode == 1000 && f[0].toNode == 1199 && !f[0].below && f[0].worstValue == 6.f
            && f[1].fromNode == 1400 && f[1].toNode == 1499
            && f[2].fromNode == 3000 && f[2].toNode == 3009 && f[2].below && f[2].worstValue == -2.f;
    report(ok, QString("gaps under 100 nodes merge, longer ones do not, both limits, node order (%1 findings)").arg(f.size()));
    if(f.size()) printf("  e.g. \"%s\"\n", qPrintable(f[0].text));
}

// Values that are not numbers: counted as outside, once, with their own sentence.
static void checkNonFinite()
{
    QVector<float> v(10000, 10.f);
    for(int i = 2000; i < 2500; ++i) v[i] = 2.f;      // slow, then stalls 50 nodes later
    for(int i = 2550; i < v.size(); ++i) v[i] = NAN;
    QList<finding> f = findOutside(v, "basic", "speed", 3.f, INFINITY, findingProblem);
    QString expect = "Speed is not a number (the train stalls) between 2.0 s and 10.0 s";
    bool ok = f.size() == 1 && f[0].fromNode == 2000 && f[0].toNode == 9999 && f[0].nonFinite
            && f[0].worstNode == 2550 && f[0].below && f[0].text == expect;
    report(ok, QString("NaN speed counts as outside, the stretch runs to the last node (\"%1\")").arg(f.isEmpty() ? QString() : f[0].text));

    QVector<float> w(1000, 1.f);
    w[500] = INFINITY;
    w[501] = -INFINITY;
    w[502] = NAN;
    f = findOutside(w, "basic", "forceNormal", -1.5f, 5.f, findingProblem);
    ok = f.size() == 1 && f[0].fromNode == 500 && f[0].toNode == 502 && f[0].nonFinite && f[0].worstNode == 500;
    QList<finding> g = findOutside(w, "basic", "forceNormal", -INFINITY, 5.f, findingProblem);
    ok = ok && g.size() == 1 && !g[0].below && g[0].nonFinite;
    report(ok, QString("an infinite or NaN force is reported once, whichever limits are checked (%1, %2 findings)").arg(f.size()).arg(g.size()));
}

// Tracks of one node and of no sections: every measure and check runs.
static void checkTinyTracks()
{
    QStringList ids = allIds();
    track empty(glm::vec3(0.f, 0.f, 0.f), 0.f, 1.1f);
    QVector<QVector<float> > v = measureRegistry::values(&empty, ids);
    bool ok = v.size() == ids.size();
    for(int k = 0; k < v.size(); ++k) if(!v[k].isEmpty()) ok = false;
    ok = ok && checkRegistry::runAll(&empty).isEmpty();
    report(ok, "a track without sections: every measure is empty, no finding");

    track one(glm::vec3(0.f, 0.f, 0.f), 0.f, 1.1f);
    one.newSection(forced);
    one.lSections[0]->lNodes.resize(1);     // as just after a zero-length section is added
    v = measureRegistry::values(&one, ids);
    ok = v.size() == ids.size() && one.getNumPoints() == 0;
    for(int k = 0; k < v.size(); ++k) {
        if(v[k].size() != 1 || !std::isfinite(v[k][0])) { ok = false; printf("  %s: %d values\n", qPrintable(ids[k]), v[k].size()); }
    }
    checkRegistry::runAll(&one);
    report(ok, "a track of one node: every measure gives one finite value, the checks run");
}

// A baseline project changed through the model until the train stalls in a
// force section: the speed goes NaN (secforced has no stop at zero speed) and
// the basic check must say so.
static void checkStall(const QString& repo)
{
    coreProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &p);
    if(p.tracks.isEmpty() || p.tracks[0]->lSections.size() < 3) {
        report(false, "four-sections.fvd loads for the stall test");
        return;
    }
    track* t = p.tracks[0];
    // section 2 is a curve at held speed, section 3 the force section with the
    // physics: lower the held speed until the pull-up stalls
    section* held = t->lSections[1];
    int firstBad = -1;
    float speed = 0.f;
    float tries[] = { 5.f, 3.f, 2.f, 1.f, 0.5f };
    // the model's own NaN checks (lenAssert, a qCritical in a release build)
    // fire once the speed is NaN; that is the case under test
    expectedCriticals = 1;
    for(unsigned k = 0; k < sizeof(tries)/sizeof(tries[0]) && firstBad < 0; ++k) {
        speed = tries[k];
        held->fVel = speed;
        t->updateTrack(1, 0);
        for(int i = 0; i <= t->getNumPoints(); ++i) {
            if(!std::isfinite(t->getPoint(i)->fVel)) { firstBad = i; break; }
        }
    }
    expectedCriticals = 0;
    if(firstBad < 0) {
        report(false, "stall test: the train never stalled");
        return;
    }
    int last = t->getNumPoints();
    QStringList ids = allIds();
    QVector<QVector<float> > v = measureRegistry::values(t, ids);
    bool sizes = true;
    for(int k = 0; k < v.size(); ++k) if(v[k].size() != last+1) sizes = false;
    QList<finding> f = checkRegistry::run(t, "basic");
    // the stalled stretch: from firstBad to the end of that run of non-finite
    // speeds (the geometric section after it starts from a speed again)
    int lastBad = firstBad;
    while(lastBad < last && !std::isfinite(t->getPoint(lastBad+1)->fVel)) ++lastBad;
    bool found = false;
    for(int k = 0; k < f.size(); ++k) {
        if(f[k].measureId == "speed" && f[k].nonFinite && f[k].fromNode <= firstBad && f[k].toNode >= lastBad) {
            found = true;
            printf("  %s\n", qPrintable(f[k].text));
        }
    }
    report(sizes && found, QString("stall: held speed %1 m/s before the force section, speed NaN from node %2 to %3 (of %4); "
                                   "every measure runs and basic reports a speed finding covering it").arg(speed).arg(firstBad).arg(lastBad).arg(last));
}

// A track built here: a quarter loop up (curved section, direction 90) at a
// held 10 m/s, then the same with the speed from the physics.
static void checkSyntheticClimb()
{
    track t(glm::vec3(0.f, 0.f, 0.f), 0.f, 1.1f);
    t.newSection(curved);
    section* s = t.lSections[0];
    s->bSpeed = false;
    s->fVel = 10.f;
    // the direction that makes the curve go up
    float directions[] = { 0.f, 90.f, 180.f, 270.f };
    for(unsigned d = 0; d < 4; ++d) {
        s->fDirection = directions[d];
        t.updateTrack(0, 0);
        if(t.getPoint(t.getNumPoints())->vDir.y > 0.9f) break;
    }

    QVector<float> along = measureRegistry::values(&t, "forceAlong");
    float maxUp = -1.f;
    bool equal = along.size() == t.getNumPoints()+1;
    bool positive = true;
    for(int i = 0; i < along.size(); ++i) {
        mnode* n = t.getPoint(i);
        maxUp = qMax(maxUp, n->vDir.y);
        if(along[i] != n->vDir.y) equal = false;
        if(n->vDir.y > 0.01f && !(along[i] > 0.f)) positive = false;
    }
    report(maxUp > 0.9f && equal && positive,
           QString("held-speed climb (curve direction %1): along-track force = vDir.y exactly and positive (track rises to vDir.y %2)")
           .arg(s->fDirection).arg(maxUp, 0, 'f', 3));

    // the same climb with the physics, entered at 25 m/s
    t.anchorNode->fVel = 25.f;
    t.anchorNode->fEnergy = 0.5f*25.f*25.f + F_G*t.anchorNode->fPosHearty(0.9f*t.fHeart);
    s->bSpeed = true;
    t.updateTrack(0, 0);
    along = measureRegistry::values(&t, "forceAlong");
    float worst = 0.f, maxDirY = 0.f;
    for(int i = 15; i < along.size()-15; ++i) {
        if(t.getPoint(i)->vDir.y > 0.3f) {
            maxDirY = qMax(maxDirY, t.getPoint(i)->vDir.y);
            if(fabs(along[i]) > fabs(worst)) worst = along[i];
        }
    }
    // Not exactly 0: friction (-0.03 g), air (-0.01 g), and, at the
    // heartline on the inside of a curve up of radius R, the heartline slows
    // less than the rail: a rider there is pressed back by about
    // vDir.y (1.1 h / R) less the drag (issue 96; the closed form is
    // checkVerticalCircle()). Small either way.
    report(maxDirY > 0.5f && std::fabs(worst) < 0.15f,
           QString("physics climb: the train slows as gravity says, along-track force stays small (largest %1 g) while vDir.y reaches %2")
           .arg(worst, 0, 'f', 4).arg(maxDirY, 0, 'f', 3));

    // a horizontal straight with the physics: friction and air slow the train
    track h(glm::vec3(0.f, 0.f, 0.f), 0.f, 1.1f);
    h.newSection(straight);
    h.lSections[0]->bSpeed = true;
    h.lSections[0]->fHLength = 50.f;
    h.updateTrack(0, 0);
    along = measureRegistry::values(&h, "forceAlong");
    double mean = 0.0;
    int count = 0;
    for(int i = 15; i < along.size()-15; ++i) { mean += along[i]; ++count; }
    mean /= qMax(count, 1);
    double expect = -(h.fFriction + h.fResistance*10.f*10.f/F_G);
    report(count > 100 && mean < 0.0 && fabs(mean - expect) < 0.002,
           QString("level straight with physics: along-track force %1 g, negative (braking), expected about %2 g from friction and resistance")
           .arg(mean, 0, 'f', 5).arg(expect, 0, 'f', 5));
}

// (a) The closed form (issue 96): a vertical circle built as a track, a curved
// section of rail radius R = 10 m turning up from level, heartline h = 1.1 m,
// no friction and no air, entered at 20 m/s. The design's energy point (0.1 h
// above the rail) runs on a circle of radius R - 0.1 h, so at the angle theta
// turned
//     v^2 = v0^2 - 2 g (R - 0.1 h)(1 - cos theta),  a = -g (R - 0.1 h) sin theta / R
// (a = v dv/ds of the centre line), the heartline runs rh = (R - h)/R metres
// per metre of rail, constant (drh = 0), and a rider at the heartline feels
//     gx = sin theta + rh a / g,
// 0.1198 g with the track vertical. The same circle as tests/trainride's
// "along-track force at the heartline on a vertical circle", built here as a
// real track so that the measure itself is checked.
static void oldForceAlong(const measureNodes& n, QVector<float>& out, int halfSpan);
static double circleNoise[2] = { 0.0, 0.0 };       // the measure, the form before issue 96 (g)

static void checkVerticalCircle()
{
    const float R = 10.f, h = 1.1f, v0 = 20.f;
    track t(glm::vec3(0.f, 0.f, 0.f), 0.f, h);
    t.fFriction = 0.f;
    t.fResistance = 0.f;
    // the anchor's speed before the section is made (its first node copies the anchor)
    t.anchorNode->fVel = v0;
    t.anchorNode->fEnergy = 0.5f*v0*v0 + F_G*t.anchorNode->fPosHearty(0.9f*h);
    t.newSection(curved);
    section* s = t.lSections[0];
    s->bSpeed = true;
    s->fRadius = R;
    s->fLeadIn = 0.f;
    s->fLeadOut = 0.f;
    s->rollFunc->changeLength(126.f, 0);    // 2.2 rad of turning (a curve's roll function runs over its angle)
    // the direction that makes the curve go up
    float directions[] = { 0.f, 90.f, 180.f, 270.f };
    for(unsigned d = 0; d < 4; ++d) {
        s->fDirection = directions[d];
        t.updateTrack(0, 0);
        if(t.getNumPoints() > 100 && t.getPoint(100)->vDir.y > 0.1f) break;
    }
    measureNodes n = measureRegistry::collect(&t);
    QVector<float> along = measureRegistry::values(&t, "forceAlong");
    const double rh = ((double)R - h)/R, g = 9.80665, r09 = (double)R - 0.1*h;
    double worst = 0.0, at90 = 0.0, form90 = 0.0, old90 = 0.0, best90 = 1e9, thLast = 0.0;
    int used = 0;
    const int margin = 2*MEASURE_ALONG_HALFSPAN + 5;
    for(int i = margin; i + margin < n.size(); ++i) {
        const mnode* m = n.nodes[i];
        // the angle turned, from the frame: vDir.y = sin theta, vNorm.y = -cos theta
        // (vNorm points from the heartline to the rail)
        double th = std::atan2((double)m->vDir.y, -(double)m->vNorm.y);
        double a = -g*r09*std::sin(th)/R;
        double gx = std::sin(th) + rh*a/g;
        worst = qMax(worst, std::fabs(along[i] - gx));
        if(std::fabs(th - M_PI/2) < best90) {
            best90 = std::fabs(th - M_PI/2);
            at90 = along[i];
            form90 = gx;
            old90 = std::sin(th) + a/(rh*g);
        }
        thLast = th;
        ++used;
    }
    // (d) the float noise on this smooth shape, where nothing changes fast:
    // RMS of the second difference / sqrt(6), the measure and the form before
    // issue 96 over the same nodes
    QVector<float> old(n.size());
    oldForceAlong(n, old, MEASURE_ALONG_HALFSPAN);
    double sumNew = 0.0, sumOld = 0.0;
    int k = 0;
    for(int i = margin; i + margin < n.size(); ++i) {
        double dn = (double)along[i+1] - 2.0*along[i] + along[i-1], dold = (double)old[i+1] - 2.0*old[i] + old[i-1];
        sumNew += dn*dn/6.0;
        sumOld += dold*dold/6.0;
        ++k;
    }
    circleNoise[0] = k ? std::sqrt(sumNew/k) : 0.0;
    circleNoise[1] = k ? std::sqrt(sumOld/k) : 0.0;
    report(used > 500 && thLast > 2.0 && best90 < 0.01 && worst < 1e-3 && std::fabs(at90 - 0.1198) < 5e-4,
           QString("(a) the along-track force on a vertical circle built as a track (rail radius %1 m, heartline %2 m, "
                   "frictionless, %3 nodes up to %4 rad): worst %5 g from the closed form at every node more than %6 from "
                   "the ends (limit 0.001 g); with the track vertical the measure gives %7 g, the closed form %8 g "
                   "(the form before issue 96 gave %9 g)")
           .arg(R).arg(h).arg(used).arg(thLast, 0, 'f', 2).arg(worst, 0, 'g', 2).arg(margin).arg(at90, 0, 'f', 4)
           .arg(form90, 0, 'f', 4).arg(old90, 0, 'f', 4));
}

struct physicsStats
{
    int nodes, skipped;
    double maxEnergyDiff;       // |forceAlong - what fEnergy implies|, g
    double sumModel, sumModel2; // forceAlong - (-(mu + k v^2/g) * dsc/ds)
    double sumRes, sumRes2;     // the same after the 0.9-heartline term
    double sumAlong;
    physicsStats() : nodes(0), skipped(0), maxEnergyDiff(0), sumModel(0), sumModel2(0), sumRes(0), sumRes2(0), sumAlong(0) {}
};

// Compares the design's acceleration a in forceAlong (as vDir.y + a / g, a =
// v dv/ds over the centre-line distance s the measure uses) on physics
// sections (not Bezier, whose speed comes from its points) with the program's
// energy model:
//   fEnergy -= v^3/F_HZ * k;  v = sqrt(2 (fEnergy - g (y09 + s mu)))
// so between two nodes  d(v^2/2) = dE - g dy09 - g mu ds_c,  and with
// y09 = y_heart + 0.9 h vNorm.y:
//   vDir.y + a/g = (vDir.y - dy_h/ds) + dE/(g ds) - mu ds_c/ds - 0.9 h dvNorm.y/ds
// where dE/(g ds) is about -k v^2/g ds_c/ds. The first bracket is vDir.y (1 -
// rh) where the heartline runs rh metres per metre of rail, plus a curvature
// error of the differencing, and is taken out by its own value; the last term is the program's 0.9
// factor: the energy uses the height of a point 0.1 h above the rail, not the
// rider's, so the speed changes a little as the track rolls or pitches.
//
// Only "plain" windows are compared: the section's speed comes from the
// physics; no roll smoothing in the window (smoothing turns the frame after the
// speed was computed, so the stored speed no longer matches the 0.9 point's
// current height); speed at least 3 m/s, vertical force between -2 and +6 g
// and rail and heartline about equally long (the test projects include
// deliberately broken loops of 30000 g, tighter than the heartline offset).
static bool plainWindow(const measureNodes& n, int i, int halfSpan, int* loOut, int* hiOut)
{
    const section* sec = n.sectionAt(i);
    if(measureSpeedIsHeld(sec) || sec->type == bezier || sec->type == nolimitscsv) return false;
    int s = n.sectionOf[i];
    int lo = qMax(i - halfSpan, n.sectionFirst[s] + 1);    // as measureForceAlong()
    int hi = qMin(i + halfSpan, n.sectionLast[s]);
    if(hi <= lo || n.heartDist[hi] - n.heartDist[lo] < 1e-3) return false;
    // the rail and the heartline travel about the same distance (not so in
    // the test projects' loops that are tighter than the heartline offset)
    double ratio = ((double)n.nodes[hi]->fTotalLength - n.nodes[lo]->fTotalLength) / (n.heartDist[hi] - n.heartDist[lo]);
    if(ratio < 0.5 || ratio > 2.0) return false;
    for(int k = lo; k <= hi; ++k) {
        const mnode* m = n.nodes[k];
        float fn = m->forceNormal + m->smoothNormal;
        if(m->fSmoothSpeed != 0.f || m->fVel < 3.f || fn < -2.f || fn > 6.f) return false;
    }
    *loOut = lo;
    *hiOut = hi;
    return true;
}

// Since issue 96 the measure is the force at the heartline, vDir.y + (rh a +
// v^2 drh/ds) / g; what is compared with the energy model here is its part
// that comes from the design's speed, the centre line's acceleration a, as
// vDir.y + a / g over the centre-line distance s the measure uses.
static void physicsCompare(const measureNodes& n, physicsStats& st)
{
    const track* t = n.trk;
    float h = t->fHeart;
    QVector<float> unused(n.size());
    QVector<double> accel, place;
    measureForceAlong(n, unused, MEASURE_ALONG_HALFSPAN, &accel, &place);
    QVector<double> along(n.size());
    for(int i = 0; i < n.size(); ++i) along[i] = n.nodes[i]->vDir.y + accel[i]/F_G;
    for(int i = 0; i < n.size(); ++i) {
        int lo, hi;
        if(!plainWindow(n, i, MEASURE_ALONG_HALFSPAN, &lo, &hi)) {
            ++st.skipped;
            continue;
        }
        double ds = place[hi] - place[lo];
        mnode* a = n.nodes[lo];
        mnode* b = n.nodes[hi];
        mnode* c = n.nodes[i];
        double y09a = a->vPosHeart(h*0.9f).y, y09b = b->vPosHeart(h*0.9f).y;
        double dE = (double)b->fEnergy - a->fEnergy;
        double dsc = (double)b->fTotalLength - a->fTotalLength;
        double dv2 = dE - F_G*((y09b - y09a) + dsc*t->fFriction);
        double fromEnergy = c->vDir.y + dv2/(F_G*ds);
        st.maxEnergyDiff = qMax(st.maxEnergyDiff, fabs(along[i] - fromEnergy));

        double v = c->fVel;
        double model = -(t->fFriction + t->fResistance*v*v/F_G)*dsc/ds;
        double term09 = -0.9*h*((double)b->vNorm.y - a->vNorm.y)/ds;
        // vDir.y against the heartline's rise per metre of centre line: not 0
        // where the heartline runs a different length from the rail (rh)
        double geo = c->vDir.y - ((double)b->vPos.y - a->vPos.y)/ds;
        double d1 = along[i] - model;
        double d2 = d1 - term09 - geo;  st.sumModel += d1; st.sumModel2 += d1*d1;
        st.sumRes += d2; st.sumRes2 += d2*d2;
        st.sumAlong += along[i];
        ++st.nodes;
    }
}

// The measure as it was before issue 96, kept here only to compare its noise
// with the new one: vDir.y + v dv/ds_heart / g, the speed's change over the
// heartline distance between the float positions, same window.
static void oldForceAlong(const measureNodes& n, QVector<float>& out, int halfSpan)
{
    for(int i = 0; i < n.size(); ++i) {
        const mnode* node = n.nodes.at(i);
        int s = n.sectionOf.at(i);
        double along = node->vDir.y;
        if(!measureSpeedIsHeld(n.sectionAt(i))) {
            int lo = qMax(i - halfSpan, n.sectionFirst.at(s) + 1);
            int hi = qMin(i + halfSpan, n.sectionLast.at(s));
            double ds = (hi > lo) ? n.heartDist.at(hi) - n.heartDist.at(lo) : 0.0;
            if(hi > lo && !(ds <= 1e-6)) {
                double vlo = n.nodes.at(lo)->fVel, vhi = n.nodes.at(hi)->fVel;
                along += (vhi*vhi - vlo*vlo) / (2.0*ds) / F_G;
            }
        }
        out[i] = (float)along;
    }
}

// Nodes near the test projects' broken loops: within reach of a step where
// the heartline runs less than half as far as the rail (a loop tighter than
// twice its heartline height) or of a design force beyond 10 g. tests/trainride
// (proof (1)) leaves the same places out; there neither the ride nor the
// measure has a meaning. The reach is the measure's: rh's rate over
// HEARTLINE_STRETCH_HALFSPAN entries of rh, each over HEARTLINE_TURN_HALFSPAN,
// and a node more.
static QVector<bool> nearBrokenLoop(const measureNodes& n)
{
    const int reach = HEARTLINE_STRETCH_HALFSPAN + HEARTLINE_TURN_HALFSPAN + 1;
    QVector<bool> near(n.size(), false);
    for(int i = 1; i < n.size(); ++i) {
        const mnode* q = n.nodes.at(i);
        bool bad = (q->fDistFromLast > 0.f && q->fHeartDistFromLast < 0.5f*q->fDistFromLast)
                || std::fabs(q->forceNormal + q->smoothNormal) > 10.f || std::fabs(q->forceLateral + q->smoothLateral) > 10.f;
        if(!bad) continue;
        for(int j = qMax(0, i - reach); j < qMin(n.size(), i + reach + 1); ++j) near[j] = true;
    }
    return near;
}

// Noise of the along-track force: RMS of the second difference over plain
// nodes whose two neighbours also have full, plain windows, divided by
// sqrt(6) (white noise); the real signal changes far too slowly to count at
// 1 ms. kind: 0 the measure with the given half-span, 1 the form before issue
// 96. Summed in sum[0]/count[0] over every such node and in sum[1]/count[1]
// over those away from the broken loops (ordinary track), whose second
// differences / sqrt(6) are also kept in ordinary (for the median).
static void noiseOf(const measureNodes& n, int halfSpan, int kind, double sum[2], int count[2], QVector<double>& ordinary)
{
    QVector<float> v(n.size());
    if(kind == 0) measureForceAlong(n, v, halfSpan);
    else oldForceAlong(n, v, halfSpan);
    QVector<bool> broken = nearBrokenLoop(n);
    for(int i = 1; i+1 < n.size(); ++i) {
        int lo, hi, lo2, hi2;
        if(!plainWindow(n, i-1, halfSpan, &lo, &hi) || hi - lo != 2*halfSpan) continue;
        if(!plainWindow(n, i+1, halfSpan, &lo2, &hi2) || hi2 - lo2 != 2*halfSpan) continue;
        double d = (double)v[i+1] - 2.0*v[i] + v[i-1];
        sum[0] += d*d/6.0;
        ++count[0];
        if(!broken.at(i)) {
            sum[1] += d*d/6.0;
            ++count[1];
            ordinary.append(std::fabs(d)/std::sqrt(6.0));
        }
    }
}

static bool sameFloat(float a, float b) { return memcmp(&a, &b, sizeof(float)) == 0; }

static QVector<double> noiseOrdinary[3];        // per node, as noiseOf() keeps them

static double median(QVector<double> v)
{
    if(v.isEmpty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v.at(v.size()/2);
}

static void checkProject(const QString& file, const QString& tmpDir, int& largestNodes, QString& largestName,
                         physicsStats& total, double noise[3][2], int noiseCount[3][2])
{
    QString name = QFileInfo(file).fileName();
    coreProject p;
    QString result = loadProjectFile(file, &p);
    if(result.contains("Error")) {
        report(false, name + ": loads (" + result + ")");
        return;
    }

    // what is written and stored before any measure was computed
    QList<QByteArray> before = writeEverything(p, tmpDir);
    QByteArray nodesBefore = nodeBytes(p);

    QStringList ids = allIds();
    bool sizesOk = true, finiteOk = true, fieldsOk = true, plotOk = true, heldOk = true;
    int heldNodes = 0;
    for(int ti = 0; ti < p.tracks.size(); ++ti) {
        track* t = p.tracks[ti];
        if(t->lSections.isEmpty()) continue;
        measureNodes n = measureRegistry::collect(t);
        int expected = t->getNumPoints()+1;
        if(n.size() != expected) sizesOk = false;
        for(int i = 0; i < n.size() && sizesOk; ++i) if(n.nodes[i] != t->getPoint(i)) sizesOk = false;

        QVector<QVector<float> > v = measureRegistry::values(n, ids);
        for(int k = 0; k < ids.size(); ++k) {
            if(v[k].size() != expected) { sizesOk = false; printf("  %s: %s has %d values, expected %d\n", qPrintable(name), qPrintable(ids[k]), v[k].size(), expected); }
            for(int i = 0; i < v[k].size(); ++i) {
                if(!std::isfinite(v[k][i])) {
                    if(finiteOk) printf("  %s: %s node %d is %f\n", qPrintable(name), qPrintable(ids[k]), i, v[k][i]);
                    finiteOk = false;
                }
            }
        }
        // single-measure call gives the same as the batch
        if(measureRegistry::values(t, "forceAlong") != v[ids.indexOf("forceAlong")]) sizesOk = false;

        const QVector<float>& speed = v[ids.indexOf("speed")];
        const QVector<float>& fn = v[ids.indexOf("forceNormal")];
        const QVector<float>& fl = v[ids.indexOf("forceLateral")];
        const QVector<float>& along = v[ids.indexOf("forceAlong")];
        const QVector<float>& jn = v[ids.indexOf("jerkNormal")];
        const QVector<float>& jl = v[ids.indexOf("jerkLateral")];
        const QVector<float>& ja = v[ids.indexOf("jerkAlong")];
        const QVector<float>& ra = v[ids.indexOf("rollAccel")];
        const QVector<float>& rr = v[ids.indexOf("rollRate")];
        for(int i = 0; i < n.size(); ++i) {
            mnode* m = t->getPoint(i);
            if(!sameFloat(speed[i], m->fVel) || !sameFloat(fn[i], m->forceNormal + m->smoothNormal)
                    || !sameFloat(fl[i], m->forceLateral + m->smoothLateral)
                    || !sameFloat(rr[i], m->fRollSpeed + m->fSmoothSpeed)) fieldsOk = false;
            if(measureSpeedIsHeld(n.sectionAt(i))) {
                ++heldNodes;
                if(!sameFloat(along[i], m->vDir.y)) heldOk = false;
            }
        }

        // the rates by hand, at a few nodes, as the plot writes them (time argument)
        int picks[] = { 0, 19, 20, 21, n.size()/3, n.size()/2, n.size()-1 };
        for(unsigned k = 0; k < sizeof(picks)/sizeof(picks[0]); ++k) {
            int j = picks[k];
            if(j < 0 || j >= n.size()) continue;
            mnode* c = t->getPoint(j);
            mnode* pr;
            unsigned int diff;
            if(j > 19) { pr = t->getPoint(j-20); diff = 20; } else { pr = t->getPoint(j); diff = 1; }
            float pn = (c->forceNormal + c->smoothNormal - pr->forceNormal - pr->smoothNormal)*F_HZ/diff;
            float pl = (c->forceLateral + c->smoothLateral - pr->forceLateral - pr->smoothLateral)*F_HZ/diff;
            float pa = (c->fRollSpeed + c->fSmoothSpeed - pr->fRollSpeed - pr->fSmoothSpeed)*F_HZ/diff;
            double ha = j > 19 ? ((double)along[j] - along[j-20])*1000.0/20.0 : 0.0;
            if(!sameFloat(jn[j], pn) || !sameFloat(jl[j], pl) || !sameFloat(ra[j], pa) || fabs(ja[j] - ha) > 1e-3) {
                plotOk = false;
                printf("  %s node %d: jerkNormal %g/%g jerkLateral %g/%g rollAccel %g/%g jerkAlong %g/%g\n", qPrintable(name), j,
                       jn[j], pn, jl[j], pl, ra[j], pa, ja[j], ha);
            }
        }

        physicsCompare(n, total);
        // [0] the measure over one node, [1] with its span, [2] the form before issue 96 with the same span
        noiseOf(n, 1, 0, noise[0], noiseCount[0], noiseOrdinary[0]);
        noiseOf(n, MEASURE_ALONG_HALFSPAN, 0, noise[1], noiseCount[1], noiseOrdinary[1]);
        noiseOf(n, MEASURE_ALONG_HALFSPAN, 1, noise[2], noiseCount[2], noiseOrdinary[2]);
        if(n.size() > largestNodes) { largestNodes = n.size(); largestName = name + QString(" track %1").arg(ti+1); }

        QList<finding> f = checkRegistry::runAll(t);
        printf("  %s track %d: %d nodes, %d finding(s) from the basic check\n", qPrintable(name), ti+1, n.size(), f.size());
        for(int k = 0; k < f.size(); ++k) printf("    %s\n", qPrintable(f[k].text));

        // limits the track is known to break: one stretch over the whole track
        float vmin = 1e9f, vmax = -1e9f, nmin = 1e9f, nmax = -1e9f;
        int vminAt = 0, nmaxAt = 0;
        for(int i = 0; i < n.size(); ++i) {
            if(speed[i] < vmin) { vmin = speed[i]; vminAt = i; }
            vmax = qMax(vmax, speed[i]);
            nmin = qMin(nmin, fn[i]);
            if(fn[i] > nmax) { nmax = fn[i]; nmaxAt = i; }
        }
        basicCheckLimits lim;
        lim.minSpeed = vmax + 1.f;
        lim.minNormal = -1e9f;
        lim.maxNormal = nmin - 1.f;
        QList<finding> g = basicCheck(t, lim);
        bool known = g.size() == 2
                && g[0].measureId == "speed" && g[0].fromNode == 0 && g[0].toNode == n.size()-1 && g[0].worstNode == vminAt && g[0].below
                && g[1].measureId == "forceNormal" && g[1].fromNode == 0 && g[1].toNode == n.size()-1 && g[1].worstNode == nmaxAt && !g[1].below;
        report(known, QString("%1 track %2: limits set past the whole track give one stretch per measure, worst at the right node").arg(name).arg(ti+1));
    }
    report(sizesOk, name + ": every measure gives one value per node (getNumPoints()+1, same nodes as getPoint)");
    report(finiteOk, name + ": every value is a finite number");
    report(fieldsOk, name + ": speed, vertical and lateral force and roll rate equal the node fields (with smoothing)");
    report(heldOk, name + QString(": along-track force = vDir.y on held-speed sections (%1 nodes)").arg(heldNodes));
    report(plotOk, name + ": force changes and roll acceleration equal the plot's formulas at hand-picked nodes");

    QByteArray nodesAfter = nodeBytes(p);
    QList<QByteArray> after = writeEverything(p, tmpDir);
    bool same = before.size() == after.size() && !before.isEmpty() && !before[0].isEmpty();
    for(int i = 0; same && i < before.size(); ++i) if(before[i] != after[i]) same = false;
    report(nodesBefore == nodesAfter && same,
           name + QString(": nothing written (%1 node bytes; re-saved project and %2 export files identical)").arg(nodesBefore.size()).arg(before.size()-1));
}

// The time of every measure in one pass (the fastest of five runs, in ms).
static double timeAllMeasures(const track* t)
{
    QStringList ids = allIds();
    qint64 best = -1;
    for(int k = 0; k < 5; ++k) {
        QElapsedTimer timer;
        timer.start();
        QVector<QVector<float> > v = measureRegistry::values(t, ids);
        qint64 ns = timer.nsecsElapsed();
        if(v.size() != ids.size()) return -1.0;
        if(best < 0 || ns < best) best = ns;
    }
    return best/1e6;
}

// A long track built as tests/commands builds one (timeCommands()): force
// sections and straights at a held 20 m/s appended to gen-normal-shapes until
// it has 150,000 nodes, saved and loaded. Every measure must stay well inside
// an edit's budget of 150 ms.
static void timeLongTrack(const QString& repo, const QString& tmpDir)
{
    coreProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/gen-normal-shapes.fvd"), &p);
    if(p.tracks.isEmpty()) {
        report(false, "gen-normal-shapes.fvd loads for the long track");
        return;
    }
    track* t = p.tracks[0];
    for(int k = 0; t->getNumPoints() < 150000 && k < 1000; ++k) {
        section* added = t->appendSectionFromFile(k % 2 ? straight : forced);
        added->bSpeed = 0;
        added->fVel = 20.f;
        t->updateTrack(added, 0);
    }
    QString file = QDir(tmpDir).filePath("long.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);
    coreProject longP;
    loadProjectFile(file, &longP);
    if(longP.tracks.isEmpty()) {
        report(false, "the long track loads");
        return;
    }
    double ms = timeAllMeasures(longP.tracks[0]);
    report(ms >= 0.0 && ms < 50.0, QString("all %1 measures on a long track (%2 nodes): %3 ms (fastest of 5; limit 50 ms, a third "
                                           "of an edit's 150 ms)").arg(allIds().size()).arg(longP.tracks[0]->getNumPoints() + 1)
           .arg(ms, 0, 'f', 2));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    if(argc < 2) {
        fprintf(stderr, "usage: measures <repository folder>\n");
        return 1;
    }
    QString repo = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }

    checkRegistries();
    checkFindOutside();
    checkNonFinite();
    checkTinyTracks();
    checkSyntheticClimb();
    checkVerticalCircle();
    checkStall(repo);

    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList files = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    int projects = 0;
    int largestNodes = 0;
    QString largestName;
    physicsStats st;
    double noise[3][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    int noiseCount[3][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
    for(int i = 0; i < files.size(); ++i) {
        if(files[i].contains(".2016.")) continue;
        ++projects;
        checkProject(baseline.filePath(files[i]), tmp.path(), largestNodes, largestName, st, noise, noiseCount);
    }
    report(projects >= 5, QString("%1 baseline projects checked").arg(projects));

    // along-track force against the energy model, over every physics node
    if(st.nodes) {
        double meanModel = st.sumModel/st.nodes, rmsModel = sqrt(st.sumModel2/st.nodes);
        double meanRes = st.sumRes/st.nodes, rmsRes = sqrt(st.sumRes2/st.nodes);
        double meanAlong = st.sumAlong/st.nodes;
        report(st.maxEnergyDiff < 2e-3,
               QString("the design's acceleration in the along-track force matches the speed change the stored energy implies, within %1 g over %2 plain physics nodes (%3 others left out)")
               .arg(st.maxEnergyDiff, 0, 'g', 3).arg(st.nodes).arg(st.skipped));
        report(meanAlong < 0.0 && meanAlong > -0.2,
               QString("vDir.y + a / g on physics sections is small and negative on average (%1 g)").arg(meanAlong, 0, 'f', 5));
        printf("  against friction and resistance alone, -(mu + k v^2/g): mean difference %.5f g, RMS %.5f g\n", meanModel, rmsModel);
        printf("  after the 0.9-heartline term of the energy model: mean %.6f g, RMS %.6f g\n", meanRes, rmsRes);
        report(rmsRes < 2e-3 && fabs(meanRes) < 5e-4,
               "the difference from friction and resistance is explained by the energy model's 0.9-heartline height");
    } else {
        report(false, "no physics nodes found in the baseline projects");
    }
    // (d) the noise
    double z[3][2];
    const char* what[3] = { "the measure over one node", "the measure (its span)", "the form before issue 96 (same span)" };
    for(int k = 0; k < 3; ++k) {
        for(int c = 0; c < 2; ++c) z[k][c] = noiseCount[k][c] ? sqrt(noise[k][c]/noiseCount[k][c]) : 0.0;
        printf("  float noise of the along-track force, %s: RMS %.2e g (median %.2e g) on ordinary track (%d nodes), RMS %.2e g "
               "with the broken test loops (%d nodes)\n", what[k], z[k][1], median(noiseOrdinary[k]), noiseCount[k][1],
               z[k][0], noiseCount[k][0]);
    }
    // On the baseline projects a second difference also counts real changes
    // of the pitching term (a force transition's corner makes a step in it),
    // which the form before issue 96 did not have; the vertical circle, where
    // nothing changes fast, shows the float noise alone.
    report(circleNoise[0] > 0.0 && circleNoise[0] <= circleNoise[1] && z[1][1] < 2e-4,
           QString("(d) along-track force noise: on the vertical circle %1 g (before issue 96 %2 g); on the baseline projects' "
                   "ordinary track RMS %3 g, median %4 g (before %5 g and %6 g; limit for the RMS 2e-4 g)")
           .arg(circleNoise[0], 0, 'e', 2).arg(circleNoise[1], 0, 'e', 2).arg(z[1][1], 0, 'e', 2)
           .arg(median(noiseOrdinary[1]), 0, 'e', 2).arg(z[2][1], 0, 'e', 2).arg(median(noiseOrdinary[2]), 0, 'e', 2));

    // time of the full set on the largest project and on a long track
    if(largestNodes) {
        coreProject p;
        QString file = largestName.section(' ', 0, 0);
        loadProjectFile(baseline.filePath(file), &p);
        int ti = largestName.section(' ', 2, 2).toInt() - 1;
        double ms = timeAllMeasures(p.tracks[ti]);
        printf("  all %d measures on %s (%d nodes): %.2f ms (fastest of 5)\n", allIds().size(), qPrintable(largestName), largestNodes, ms);
    }
    timeLongTrack(repo, tmp.path());

    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
