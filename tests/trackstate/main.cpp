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

// trackstate: checks core/trackstate.* without a window. For every project in
// samples/baseline/, the NoLimits 1 and NoLimits 2 CSV import samples in
// samples/import/ and the saved CSV project there: a state put back gives
// its saved bytes exactly, and the state and exports the program itself shows
// after one more full recompute (on most tracks: exactly what was captured),
// for the state as loaded and before and after model edits made with the
// calls the windows make; unchanged sections share their bytes with the
// state before; names with "é" survive; a listener sees the two notices and
// nothing between them. The point-list import is checked for its state only
// (it leaves values unset). Prints the time of one capture and one restore
// of the largest baseline project. A restore is exact to the last bit on
// transitions whose ends a file load moves by one last digit (built through
// the model on a baseline force section).
//
//   trackstate <repository folder> [<folder of further .fvd projects>]

#include "../../core/track.h"
#include "../../core/trackstate.h"
#include "../../core/projectfile.h"
#include "../../core/elementfile.h"
#include "../../core/nolimitsimporter.h"
#include "../../core/smoothhandler.h"
#include "../../core/exportfuncs.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>

using namespace std;

static bool allOk = true;
static int checkCount = 0;
static int failCount = 0;
static QString tmpPath;

// Debug output and warnings are dropped (the exporters print thousands of
// lines); a failed lenAssert (qCritical in release) is shown and fails the test.
static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "critical: %s\n", msg.toLocal8Bit().constData());
        allOk = false;
    }
}

// Prints one line per check; only failures and a few named checks would be
// too quiet, so every check prints.
static bool check(bool ok, const QString& label)
{
    ++checkCount;
    if(!ok) {
        ++failCount;
        allOk = false;
    }
    printf("%s: %s\n", ok ? "ok" : "FAIL", label.toUtf8().constData());
    return ok;
}

// The project file reader's client, as in tests/coreonly/main.cpp: owns the
// tracks and makes new ones as trackHandler's constructor does.
class testProject : public projectFileClient
{
public:
    ~testProject() { clear(); }

    QList<track*> tracks;
    QString texPath;

    void clear()
    {
        while(!tracks.isEmpty()) {
            delete tracks.takeFirst();
        }
    }

    void projectCleared() { clear(); }
    bool projectGroundTexture(const QString& path) { texPath = path; return true; }

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

// Counts the track's notices in order.
class countingListener : public trackListener
{
public:
    QStringList seen;
    void trackGeometryChanged(track*, int) { seen.append("geometry"); }
    void trackStatusMessage(const QString&, int) { seen.append("status"); }
    void trackSmoothingApplied(track*) { seen.append("smoothing"); }
    void trackSectionAdded(track*, int) { seen.append("sectionAdded"); }
    void trackLoaded(track*) { seen.append("loaded"); }
    void trackAboutToReset(track*) { seen.append("aboutToReset"); }
    void trackReset(track*) { seen.append("reset"); }
};

static QByteArray readFile(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

// What track::saveTrack() writes, through a plain std::iostream.
static QByteArray savedBytes(track* t)
{
    std::stringbuf buf(ios::in | ios::out | ios::binary);
    std::iostream io(&buf);
    t->saveTrack(io);
    std::string s = buf.str();
    return QByteArray(s.data(), (int)s.size());
}

// What track::saveTrack() writes into a real file.
static QByteArray fileSavedBytes(track* t)
{
    QString name = QDir(tmpPath).filePath("track.bin");
    {
        fstream f(name.toLocal8Bit().data(), ios::out | ios::binary | ios::trunc);
        t->saveTrack(f);
    }
    return readFile(name);
}

// The three exports with the export dialog's defaults (ui/batchrunner.cpp).
static QList<QByteArray> exportsOf(track* t)
{
    QList<QByteArray> files;
    if(t->lSections.isEmpty()) return files;
    float fPerNode = 2.0f;
    float fRollThresh = sin(85.0*F_PI/180.f);
    int last = t->lSections.size()-1;
    QDir dir(tmpPath);
    writeNL2Element(dir.filePath("e.nl2elem"), t, fPerNode, 0, last);
    writeNL1TangentElement(dir.filePath("e.tangent.nlelem"), t, fPerNode, 0, last, fRollThresh, false);
    writeNL1SplineElement(dir.filePath("e.spline.nlelem"), t, fPerNode, 0, last, fRollThresh, false);
    files.append(readFile(dir.filePath("e.nl2elem")));
    files.append(readFile(dir.filePath("e.tangent.nlelem")));
    files.append(readFile(dir.filePath("e.spline.nlelem")));
    return files;
}

struct observed
{
    QByteArray saved;
    QList<QByteArray> exports;
    bool operator==(const observed& o) const { return saved == o.saved && exports == o.exports; }
};

static observed observe(track* t)
{
    observed o;
    o.saved = savedBytes(t);
    o.exports = exportsOf(t);
    return o;
}

// Which parts of two states differ, for a failure's line.
static QString stateDiff(const trackState& a, const trackState& b)
{
    QStringList parts;
    if(a.header != b.header) parts << "header";
    if(a.sections.size() != b.sections.size()) {
        parts << "number of sections";
    } else {
        for(int i = 0; i < a.sections.size(); ++i) {
            if(a.sections[i] != b.sections[i]) parts << QString("section %1").arg(i);
        }
    }
    if(a.smoothing != b.smoothing) parts << "smoothing";
    if(a.anchorNode != b.anchorNode) parts << "anchor node";
    if(a.firstNode != b.firstNode) parts << "first node";
    if(a.bezierPoints != b.bezierPoints) parts << "Bezier points";
    if(a.unsavedNames != b.unsavedNames) parts << "CSV names";
    if(a.smoothedUntil != b.smoothedUntil) parts << QString("smoothedUntil %1/%2").arg(a.smoothedUntil).arg(b.smoothedUntil);
    if(a.bAutoSmooth != b.bAutoSmooth) parts << "bAutoSmooth";
    return parts.isEmpty() ? QString("same") : parts.join(", ");
}

static QString observedDiff(const observed& a, const observed& b)
{
    QStringList parts;
    if(a.saved != b.saved) parts << "saved bytes";
    if(a.exports.size() != b.exports.size()) {
        parts << "number of exports";
    } else {
        const char* names[] = { "nl2elem", "tangent", "spline" };
        for(int i = 0; i < a.exports.size() && i < 3; ++i) {
            if(a.exports[i] != b.exports[i]) parts << names[i];
        }
    }
    return parts.isEmpty() ? QString("same") : parts.join(", ");
}

static int countNodes(track* t)
{
    return t->getNumPoints();
}

// ---- model edits, made with the calls the windows make ----------------------

typedef std::function<bool(track*)> editFn;   // false: not applicable to this track

struct edit
{
    const char* name;
    editFn apply;
};

static int firstWithRollFunc(track* t)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(t->lSections[i]->rollFunc != NULL && t->lSections[i]->type != nolimitscsv) return i;
    }
    return -1;
}

// transitionWidget::on_changeSpin_valueChanged()
static bool editTransitionChange(track* t)
{
    int s = firstWithRollFunc(t);
    if(s < 0) return false;
    t->activeSection = t->lSections[s];
    subfunc* f = t->activeSection->rollFunc->funcList[0];
    f->symArg = f->symArg + 30.f;
    f->parent->translateValues(f);
    t->updateTrack(t->activeSection, (int)(f->minArgument*F_HZ-1.5f));
    return true;
}

// transitionWidget::on_lengthSpin_valueChanged()
static bool editTransitionLength(track* t)
{
    int s = firstWithRollFunc(t);
    if(s < 0) return false;
    t->activeSection = t->lSections[s];
    subfunc* f = t->activeSection->rollFunc->funcList[0];
    int index = f->parent->getSubfuncNumber(f);
    f->parent->changeLength((f->maxArgument - f->minArgument)*0.5f, index);
    t->updateTrack(t->activeSection, (int)(f->minArgument*F_HZ-1.5f));
    return true;
}

// transitionWidget::on_lengthSpin_valueChanged() on the normal force of the
// first force section with several transitions: the ones after it move, and
// a restore must keep their arguments to the last digit (a file load does not)
static bool editForceTransitionLength(track* t)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        section* s = t->lSections[i];
        if(s->type != forced || s->normForce->funcList.size() < 3 || s->normForce->funcList[0]->locked) continue;
        t->activeSection = s;
        subfunc* f = s->normForce->funcList[0];
        s->normForce->changeLength(f->maxArgument - f->minArgument + 0.2f, 0);
        t->updateTrack(t->activeSection, (int)(f->minArgument*F_HZ-1.5f));
        return true;
    }
    return false;
}

// trackWidget::on_straightLengthBox_valueChanged()
static bool editStraightLength(track* t)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(t->lSections[i]->type == straight) {
            t->activeSection = t->lSections[i];
            t->activeSection->rollFunc->setMaxArgument(t->activeSection->rollFunc->getMaxArgument() + 7.5f);
            t->updateTrack(t->activeSection, 0);
            return true;
        }
    }
    return false;
}

// trackWidget::addSection(): a new section at the end
static bool editAppendSection(track* t)
{
    t->appendSectionFromFile(curved);
    return true;
}

// trackWidget's delete: track::removeSection() of a section in the middle
static bool editRemoveSection(track* t)
{
    if(t->lSections.size() < 2) return false;
    t->removeSection(t->lSections.size() >= 3 ? 1 : 0);
    return true;
}

// trackWidget::on_anchorSpeedBox_valueChanged()
static bool editAnchorSpeed(track* t)
{
    t->anchorNode->fVel = t->anchorNode->fVel + 3.f;
    t->anchorNode->fEnergy = 0.5*t->anchorNode->fVel*t->anchorNode->fVel + F_G*t->anchorNode->fPosHearty(0.9*t->fHeart);
    t->updateAnchorGeometrics();
    t->updateTrack(0, 0);
    return true;
}

// The smoothing dialog: tick the longest section's entry, OK (applyRollSmooth)
static bool editSmoothing(track* t)
{
    int best = -1;
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(t->lSections[i]->lNodes.size() > 1200 && (best < 0 || t->lSections[i]->lNodes.size() > t->lSections[best]->lNodes.size())) best = i;
    }
    if(best < 0 || best+1 >= t->smoothList.size()) return false;
    t->bAutoSmooth = true;   // set when the smoothing dialog exists
    char customChar;
    t->updateSmoothEntries(&customChar);
    t->smoothList[best+1]->active = !t->smoothList[best+1]->active;
    t->applyRollSmooth();
    return true;
}

// ---- the checks -----------------------------------------------------------

// What a restore of the track's present state must give. A restore puts the
// inputs back and runs one full recompute (updateTrack(0, 0)), as an anchor
// edit does. On most tracks a full recompute changes nothing, and a restore
// gives back exactly what was captured. On some it does change the last
// digits: today's model carries a little history in its nodes (roll
// smoothing is taken off and put on again around the start node of the first
// section; a Bezier section lowers its start node's energy on every update).
// There the restore gives exactly what the program itself shows after one
// more full recompute. This captures the present state, runs that recompute,
// records what it gives, and leaves the track there.
struct reference
{
    trackState captured;    // the state as it was
    trackState full;        // the state after one more full recompute
    observed seen;          // saved bytes and exports as they were
    observed seenFull;      // and after the recompute
    QList<QVector<mnode> > nodes;   // after the recompute, to name a difference
    bool smoothed;          // roll smoothing was on
};
static QString nodeDiff(track* t, const struct reference& r);

static reference takeReference(track* t, const trackState* previous)
{
    reference r;
    r.smoothed = t->smoothingActive();
    r.seen = observe(t);
    r.captured = captureTrackState(t, previous);
    t->updateTrack(0, 0);
    r.seenFull = observe(t);
    r.full = captureTrackState(t, &r.captured);
    for(int i = 0; i < t->lSections.size(); ++i) r.nodes.append(t->lSections[i]->lNodes);
    return r;
}

static QString nodeDiff(track* t, const reference& r)
{
    static const char* names[] = { "vPos","vPos","vPos","vDir","vDir","vDir","vLat","vLat","vLat","vNorm","vNorm","vNorm",
        "fRoll","fVel","fEnergy","forceNormal","forceLateral","smoothNormal","smoothLateral","fDistFromLast","fHeartDistFromLast",
        "fAngleFromLast","fTrackAngleFromLast","fDirFromLast","fPitchFromLast","fYawFromLast","fRollSpeed","fSmoothSpeed","fTotalLength","fTotalHeartLength" };
    QStringList out;
    QSet<int> seenField;
    for(int s = 0; s < t->lSections.size() && s < r.nodes.size(); ++s) {
        if(t->lSections[s]->lNodes.size() != r.nodes[s].size()) return QString("section %1 node count").arg(s);
        for(int n = 0; n < r.nodes[s].size(); ++n) {
            const float* a = reinterpret_cast<const float*>(&r.nodes[s][n]);
            const float* b = reinterpret_cast<const float*>(&t->lSections[s]->lNodes[n]);
            for(unsigned f = 0; f < sizeof(mnode)/sizeof(float); ++f) {
                if(memcmp(a+f, b+f, sizeof(float)) != 0 && !seenField.contains(f)) {
                    seenField.insert(f);
                    out << QString("section %1 node %2 %3: %4 vs %5").arg(s).arg(n).arg(f < 30 ? names[f] : "?").arg(a[f], 0, 'g', 9).arg(b[f], 0, 'g', 9);
                }
            }
        }
    }
    return out.isEmpty() ? QString("nodes same") : out.join(" | ");
}

static void checkRestore(track* t, const reference& r, const QString& label)
{
    restoreTrackState(t, r.captured);
    observed got = observe(t);
    trackState state = captureTrackState(t);
    bool exact = r.seen == r.seenFull && r.captured == r.full;
    check(got.saved == r.seen.saved, label + ": the saved bytes come back exactly");
    // The smoothing entries' saved fields come back as captured, even where
    // the recompute brought an entry's node range up to date.
    trackState expected = r.full;
    expected.smoothing = r.captured.smoothing;
    observed expectedSeen = r.seenFull;
    expectedSeen.saved = r.seen.saved;
    // Smoothed tracks too: a full recompute takes the smoothing ranges from
    // the nodes it has just computed and starts every node it rewrites as a
    // copy of the one before, so it gives what a fresh computation gives.
    bool match = got == expectedSeen && state == expected;
    check(match,
          label + (exact ? QString(": state and exports come back exactly")
                         : QString(": state and exports equal those of one more full recompute (which here changes ")
                           + stateDiff(r.captured, r.full) + "; " + observedDiff(r.seen, r.seenFull) + ")")
          + (r.smoothed ? QString(" (smoothed track)") : QString())
          + (match ? QString() : QString(" [got: ") + stateDiff(expected, state) + "; " + observedDiff(expectedSeen, got) + "; " + nodeDiff(t, r) + "]"));
}

static void checkRoundTrip(track* t, const QString& what)
{
    trackState a = captureTrackState(t);
    check(a.savedBytes() == fileSavedBytes(t), what + ": the state's saved bytes equal what a file save writes");

    reference r = takeReference(t, NULL);
    checkRestore(t, r, what + ": capture, restore, capture");
}

static void checkEdits(track* t, const QString& what)
{
    edit edits[] = {
        { "transition change", editTransitionChange },
        { "transition length", editTransitionLength },
        { "force transition length", editForceTransitionLength },
        { "straight length", editStraightLength },
        { "append section", editAppendSection },
        { "remove section", editRemoveSection },
        { "anchor speed", editAnchorSpeed },
        { "smoothing", editSmoothing },
    };

    trackState start = captureTrackState(t);
    for(unsigned e = 0; e < sizeof(edits)/sizeof(edits[0]); ++e) {
        QString label = what + ": " + edits[e].name;

        reference a = takeReference(t, &start);
        restoreTrackState(t, a.captured);
        if(!edits[e].apply(t)) {
            continue;
        }
        printf("edit: %s\n", label.toUtf8().constData());
        reference b = takeReference(t, &a.captured);

        // A section whose saved bytes did not change shares a's data.
        // (Sections after an edited one often do change: their saved start
        // values follow the section before.)
        bool shared = true;
        int count = 0;
        for(int i = 0; i < b.captured.sections.size(); ++i) {
            bool same = false;
            for(int j = 0; j < a.captured.sections.size(); ++j) {
                if(a.captured.sections[j] == b.captured.sections[i]) same = true;
            }
            bool isShared = false;
            for(int j = 0; j < a.captured.sections.size(); ++j) {
                if(b.captured.sections[i].isSharedWith(a.captured.sections[j])) isShared = true;
            }
            if(isShared) ++count;
            if(same && !isShared) {
                // sharing is looked for at the same place and as far from the end
                int shift = a.captured.sections.size() - b.captured.sections.size();
                bool inPlace = i < a.captured.sections.size() && a.captured.sections[i] == b.captured.sections[i];
                bool shifted = i+shift >= 0 && i+shift < a.captured.sections.size() && a.captured.sections[i+shift] == b.captured.sections[i];
                if(inPlace || shifted) shared = false;
            }
        }
        check(shared, label + QString(": %1 of %2 sections share the bytes of the state before").arg(count).arg(b.captured.sections.size()));

        checkRestore(t, a, label + ": restore before the edit");
        checkRestore(t, b, label + ": restore after the edit");
        checkRestore(t, a, label + ": restore before the edit again");
        checkRestore(t, a, label + ": and once more, twice in a row");
    }
    restoreTrackState(t, start);
}

static void checkNotices(track* t, const QString& what)
{
    countingListener listener;
    trackListener* old = t->mListener;
    t->mListener = &listener;
    trackState s = captureTrackState(t);
    restoreTrackState(t, s);
    t->mListener = old;
    check(listener.seen == (QStringList() << "aboutToReset" << "reset"),
          what + ": the listener sees about-to-reset, then nothing, then reset (" + listener.seen.join(" ") + ")");
}

static void checkTrack(track* t, const QString& what)
{
    checkRoundTrip(t, what);
    if(!t->lSections.isEmpty()) {
        checkEdits(t, what);
    }
}

static void checkProject(const QString& file)
{
    testProject project;
    QString result = loadProjectFile(file, &project);
    QString stem = QFileInfo(file).fileName();
    if(!check(!result.contains("Error"), stem + ": loads (" + result + ")")) return;
    for(int i = 0; i < project.tracks.size(); ++i) {
        checkTrack(project.tracks[i], QString("%1 track %2").arg(stem).arg(i+1));
    }
}

static void checkImport(const QString& repo, const QString& kind)
{
    QString dir = QDir(repo).filePath("samples/import");
    testProject project;
    track* t = project.projectNewTrack();
    QString label;
    if(kind == "nltrack") {
        label = "nl1.nltrack (NoLimits 1 import)";
        noLimitsImporter importer(t, QDir(dir).filePath("nl1.nltrack"));
        if(!check(importer.importAsNlTrack(), label + ": imports")) return;
        // the sample's segments are plain (speed 0): give one a speed, as a
        // station or lift segment would, so the live value is exercised
        if(t->lSections[0]->bezList.size() > 2) {
            t->lSections[0]->bezList[2]->fVel = 12.5f;
            t->updateTrack(0, 0);
        }
    } else if(kind == "txt") {
        label = "pointlist.txt (point-list import)";
        noLimitsImporter importer(t, QDir(dir).filePath("pointlist.txt"));
        if(!check(importer.importAsTxt(), label + ": imports")) return;
        // The importer leaves the first and last point's roll, flags and
        // speed unset (samples/import/README.md): the track differs from run
        // to run, and in one run of this test its exports did not finish in
        // minutes. Only the state itself is checked here: its saved bytes come
        // back exactly, and the whole state is the one the program shows after
        // one more full recompute (a Bezier section changes its start node on
        // every update, and the points' roll helpers with it).
        trackState s = captureTrackState(t);
        check(s.savedBytes() == fileSavedBytes(t), label + ": the state's saved bytes equal what a file save writes");
        t->updateTrack(0, 0);
        trackState full = captureTrackState(t);
        restoreTrackState(t, s);
        trackState s2 = captureTrackState(t);
        check(s2.savedBytes() == s.savedBytes(), label + ": restored, the saved bytes come back exactly");
        check(s2 == full, label + ": and the state is that of one more full recompute (" + stateDiff(full, s2) + "; exports not compared)");
        return;
    } else {
        // projectWidget::importNoLimitsCSVFile(), without the window: the
        // panel's "add" is track::appendSectionFromFile() on an empty track
        label = "nl2.csv (NoLimits 2 CSV import)";
        t->fHeart = 0.0f;
        t->appendSectionFromFile(nolimitscsv);
        dynamic_cast<secnlcsv*>(t->lSections[0])->loadTrack(QDir(dir).filePath("nl2.csv"));
        check(t->getNumPoints() > 1, label + QString(": imports (%1 nodes)").arg(t->getNumPoints()));
        // its name is not in the saved bytes: a live value of the state
        t->lSections[0]->sName = QString::fromUtf8("csv \xc3\xa9");
    }
    checkTrack(t, label);

    if(kind == "nltrack") {
        trackState s = captureTrackState(t);
        t->lSections[0]->bezList[2]->fVel = 0.f;
        restoreTrackState(t, s);
        check(t->lSections[0]->bezList[2]->fVel == 12.5f, label + ": a Bezier point's speed comes back");
    }
    if(kind == "csv") {
        trackState s = captureTrackState(t);
        t->lSections[0]->sName = QString("renamed");
        restoreTrackState(t, s);
        check(t->lSections[0]->sName == QString::fromUtf8("csv \xc3\xa9"), label + ": the CSV section's name (not saved) comes back");
    }
}

// The first section's start node is a copy of the anchor made with the
// section and not refreshed by an anchor edit (a defect of the original
// program, fixed later): a restore must bring back that stale copy.
static void checkStaleFirstNode(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    float firstVel = t->lSections[0]->lNodes[0].fVel;
    editAnchorSpeed(t);
    check(t->lSections[0]->lNodes[0].fVel == firstVel, "four-sections: an anchor speed edit leaves the first section's start node as it was (today's behaviour)");
    trackState s = captureTrackState(t);
    observed o = observe(t);
    restoreTrackState(t, s);
    check(t->lSections[0]->lNodes[0].fVel == firstVel && t->anchorNode->fVel == firstVel + 3.f,
          "four-sections: a restore brings back the anchor and the stale first node");
    check(observe(t) == o, "four-sections: and the same saved bytes and exports");
}

static void checkAccents(const QString& repo)
{
    const QString trackName = QString::fromUtf8("Pist\xc3\xa9 \xc3\xa9t\xc3\xa9");
    const QString sectionName = QString::fromUtf8("Bo\xc3\xa9 1");
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    t->name = trackName;
    t->lSections[1]->sName = sectionName;
    char customChar;
    t->updateSmoothEntries(&customChar);   // the smoothing entries carry the names too

    trackState s = captureTrackState(t);
    observed o = observe(t);
    t->name = QString("x");
    t->lSections[1]->sName = QString("y");
    restoreTrackState(t, s);
    check(t->name == trackName && t->lSections[1]->sName == sectionName && observe(t) == o,
          QString::fromUtf8("names with \xc3\xa9: survive capture and restore"));

    QString file = QDir(tmpPath).filePath("accents.fvd");
    QList<track*> tracks;
    tracks.append(t);
    saveProjectFile(file, project.texPath, tracks, &project);
    testProject loaded;
    QString result = loadProjectFile(file, &loaded);
    bool ok = !result.contains("Error") && loaded.tracks.size() == 1;
    if(ok) {
        track* l = loaded.tracks[0];
        ok = l->name == trackName && l->lSections.size() == t->lSections.size() && l->lSections[1]->sName == sectionName
             && savedBytes(l) == savedBytes(t);
    }
    check(ok, QString::fromUtf8("names with \xc3\xa9: survive a file save and load, the reloaded track saving the same bytes"));
}

// A saved project with an imported NoLimits 2 CSV section: it loads as it was
// saved (it used to come back with x and z swapped, mirrored, on every load).
static void checkCsvProject(const QString& repo)
{
    QString file = QDir(repo).filePath("samples/import/nl2.ref.resaved.fvd");
    QString label = "nl2.ref.resaved.fvd";
    testProject project;
    QString result = loadProjectFile(file, &project);
    if(!check(!result.contains("Error"), label + ": loads (" + result + ")")) return;

    track* t = NULL;
    for(int i = 0; i < project.tracks.size() && t == NULL; ++i) {
        if(!project.tracks[i]->lSections.isEmpty() && project.tracks[i]->lSections[0]->type == nolimitscsv) t = project.tracks[i];
    }
    if(!check(t != NULL, label + ": has a track with a NoLimits CSV section")) return;

    // The whole file does not come back byte for byte: a load names the
    // smoothing entries after their track and section (smoothHandler::update()),
    // and this file was saved by an import with those names still empty. The
    // section itself does, and a second save and load changes nothing.
    QByteArray fileBytes = readFile(file);
    QByteArray section = captureTrackState(t).sections[0];
    int at = fileBytes.indexOf("CSV");
    check(at >= 0 && fileBytes.mid(at, section.size()) == section, label + QString(": the CSV section saved again is byte for byte the %1 bytes in the file").arg(section.size()));

    QString saved = QDir(tmpPath).filePath("csv-resaved.fvd");
    saveProjectFile(saved, project.texPath, project.tracks, &project);
    testProject again;
    loadProjectFile(saved, &again);
    QString saved2 = QDir(tmpPath).filePath("csv-resaved2.fvd");
    saveProjectFile(saved2, again.texPath, again.tracks, &again);
    check(!readFile(saved).isEmpty() && readFile(saved) == readFile(saved2), label + ": saved, loaded and saved again, the two saves are byte for byte the same");
    const mnode& first = t->lSections[0]->lNodes[0];
    check(first.vDir == glm::vec3(1.f, 0.f, 0.f) && first.vLat == glm::vec3(0.f, 0.f, 1.f),
          label + QString(": the first node's direction (%1, %2, %3) and lateral (%4, %5, %6) are those in the file, (1, 0, 0) and (0, 0, 1)")
          .arg(first.vDir.x).arg(first.vDir.y).arg(first.vDir.z).arg(first.vLat.x).arg(first.vLat.y).arg(first.vLat.z));
    checkTrack(t, label);
}

static void checkSharingAcrossAppend(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    trackState a = captureTrackState(t);
    t->appendSectionFromFile(straight);
    trackState b = captureTrackState(t, &a);
    bool shared = b.sections.size() == a.sections.size() + 1;
    for(int i = 0; shared && i < a.sections.size(); ++i) {
        if(!b.sections[i].isSharedWith(a.sections[i])) shared = false;
    }
    check(shared, "four-sections: after appending a section, the state shares the bytes of the other four");
    check(b.bytesNotSharedWith(a) < b.byteSize(), QString("four-sections: the new state costs %1 of its %2 bytes").arg(b.bytesNotSharedWith(a)).arg(b.byteSize()));
}

// ---- a restore is exact to the last bit -----------------------------------
//
// A file load appends each further transition piece with
// func::appendSubFunction(), which recomputes the end of every piece after the
// first as (prev end + this end) - this start. In single precision that is
// not always this end: from 4.9999986 (0x409FFFFD) to 5.999999 (0x40BFFFFE)
// it gives 5.9999995 (0x40BFFFFF). A file load keeps doing that; a restore
// must give the model back as it was. These cases are built through the
// model, so they do not depend on a project that happens to hit it.

static float fromBits(quint32 b) { float f; memcpy(&f, &b, sizeof(f)); return f; }
static quint32 bitsOf(float f) { quint32 b; memcpy(&b, &f, sizeof(b)); return b; }

// What appendSubFunction() makes of a piece from a to b: (a + b) - a, in
// single precision (volatile, so the compiler neither folds nor widens it).
static float endAfterAppend(float a, float b)
{
    volatile float sum = a + b;
    volatile float end = sum - a;
    return end;
}

// Gives fn the pieces (0, ends[0]), (ends[0], ends[1]), ...: pieces are added
// or removed as the transition panel does, then each start and end is set
// directly, so they are exactly the floats given.
static void setPieces(func* fn, const QList<float>& ends)
{
    while(fn->funcList.size() < ends.size()) fn->appendSubFunction(1.f, fn->funcList.size()-1);
    while(fn->funcList.size() > ends.size()) fn->removeSubFunction(fn->funcList.size()-1);
    float start = 0.f;
    for(int p = 0; p < ends.size(); ++p) {
        fn->funcList[p]->minArgument = start;
        fn->funcList[p]->maxArgument = ends[p];
        start = ends[p];
    }
}

static QString hexOf(float f) { return QString("%1 (0x%2)").arg(f, 0, 'g', 9).arg(bitsOf(f), 8, 16, QChar('0')); }

// The pieces of fn match ends bit for bit, each starting where the one before ends.
static bool piecesAre(func* fn, const QList<float>& ends, QString* got)
{
    bool ok = fn->funcList.size() == ends.size();
    QStringList out;
    float start = 0.f;
    for(int p = 0; p < fn->funcList.size(); ++p) {
        subfunc* s = fn->funcList[p];
        if(p < ends.size() && (bitsOf(s->maxArgument) != bitsOf(ends[p]) || bitsOf(s->minArgument) != bitsOf(start))) ok = false;
        out << QString("%1..%2").arg(hexOf(s->minArgument)).arg(hexOf(s->maxArgument));
        if(p < ends.size()) start = ends[p];
    }
    *got = out.join(", ");
    return ok;
}

// One case on the force section s of t: the ends on all three functions of
// the section (the normal force's pieces with changes, so they shape the
// track), recomputed; then capture, restore, capture; a file load of the
// same bytes (which must still move the awkward end, as it always did); an
// edit, restoring A and then B. awkward is the index of the piece whose end a
// load moves; lockLast locks the last normal-force piece (stretched to the
// section's end on every recompute).
static void checkExactCase(track* t, int s, const QList<float>& ends, int awkward, bool lockLast, const QString& label)
{
    section* sec = t->lSections[s];
    QList<func*> funcs;
    funcs << sec->rollFunc << sec->normForce << sec->latForce;
    for(int f = 0; f < funcs.size(); ++f) {
        while(funcs[f]->lockedFunc() != -1) funcs[f]->unlock(funcs[f]->lockedFunc());
        setPieces(funcs[f], ends);
    }
    for(int p = 0; p < sec->normForce->funcList.size(); ++p) {
        sec->normForce->funcList[p]->symArg = (p % 2 == 0) ? 0.1f : -0.1f;
    }
    if(lockLast) sec->normForce->lock(sec->normForce->funcList.size()-1);
    t->updateTrack(0, 0);

    QList<float> normEnds = ends;
    if(lockLast) normEnds.last() = sec->normForce->funcList.last()->maxArgument;   // as the recompute stretched it

    trackState a = captureTrackState(t);
    QString got;
    check(piecesAre(sec->rollFunc, ends, &got) && piecesAre(sec->normForce, normEnds, &got),
          label + ": the model holds the ends as set (" + got + ")");
    t->updateTrack(0, 0);
    check(captureTrackState(t) == a, label + ": one more full recompute changes nothing in the state");

    restoreTrackState(t, a);
    trackState r = captureTrackState(t);
    sec = t->lSections[s];
    bool ok = piecesAre(sec->rollFunc, ends, &got) && piecesAre(sec->normForce, normEnds, &got) && piecesAre(sec->latForce, ends, &got);
    check(ok, label + ": restored, every piece's start and end as captured, bit for bit (" + got + ")");
    check(r == a && r.savedBytes() == a.savedBytes() && fileSavedBytes(t) == a.savedBytes(),
          label + ": capture, restore, capture: equal states and equal saved bytes (" + stateDiff(a, r) + ")");

    // A file load of the same bytes still moves the end as it always did.
    {
        testProject p;
        track* l = p.projectNewTrack();
        QByteArray bytes = a.savedBytes();
        std::stringbuf buf(std::string(bytes.constData(), bytes.size()), ios::in | ios::binary);
        std::iostream io(&buf);
        std::string tag = readString(&io, 3);
        l->loadTrack(io);

        // func::loadFunction() step by step: each further piece is appended
        // (the loop over every piece after the first), then read as saved.
        QList<float> lo, hi;
        lo << 0.f;
        hi << ends[0];
        for(int i = 1; i < ends.size(); ++i) {
            lo << hi[i-1];
            hi << hi[i-1] + 1.f;
            for(int k = 1; k <= i; ++k) {
                volatile float sum = hi[k-1] + hi[k];
                volatile float end = sum - lo[k];
                lo[k] = hi[k-1];
                hi[k] = end;
            }
            lo[i] = ends[i-1];
            hi[i] = ends[i];
        }
        func* fn = l->lSections[s]->rollFunc;
        bool asBefore = tag == "TRC" && fn->funcList.size() == ends.size();
        bool moved = false;
        for(int k = 0; asBefore && k < ends.size(); ++k) {
            if(bitsOf(fn->funcList[k]->maxArgument) != bitsOf(hi[k]) || bitsOf(fn->funcList[k]->minArgument) != bitsOf(lo[k])) asBefore = false;
            if(bitsOf(hi[k]) != bitsOf(ends[k])) moved = true;
        }
        float loaded = asBefore ? fn->funcList[awkward]->maxArgument : 0.f;
        check(asBefore && moved,
              label + ": a file load of those bytes still moves the ends as the original program does (piece "
              + QString::number(awkward+1) + " ends at " + hexOf(loaded) + ", saved " + hexOf(ends[awkward]) + ")");
    }

    // an edit (the first normal-force piece's change), then A and B
    subfunc* f = sec->normForce->funcList[0];
    f->symArg = f->symArg + 0.05f;
    f->parent->translateValues(f);
    t->updateTrack(sec, (int)(f->minArgument*F_HZ-1.5f));
    t->updateTrack(0, 0);
    trackState b = captureTrackState(t, &a);
    check(b != a, label + ": the edit changes the state");
    restoreTrackState(t, a);
    trackState ra = captureTrackState(t);
    check(ra == a && ra.savedBytes() == a.savedBytes(), label + ": after an edit, restoring A gives A (" + stateDiff(a, ra) + ")");
    restoreTrackState(t, b);
    trackState rb = captureTrackState(t);
    check(rb == b && rb.savedBytes() == b.savedBytes(), label + ": then restoring B gives B (" + stateDiff(b, rb) + ")");
    restoreTrackState(t, a);
    check(captureTrackState(t) == a, label + ": and A again");
}

// Pairs (a, b), a < b, with (a + b) - a != b in single precision; a
// deterministic search, so the same pairs every run.
static QList<QPair<float, float> > findAwkwardPairs(int wanted)
{
    QList<QPair<float, float> > pairs;
    quint32 seed = 12345u;
    const quint32 lo = bitsOf(1.f), hi = bitsOf(6.f);
    for(int tries = 0; tries < 100000 && pairs.size() < wanted; ++tries) {
        seed = seed*1664525u + 1013904223u;
        float a = fromBits(lo + seed % (hi - lo));
        seed = seed*1664525u + 1013904223u;
        float d = 0.25f + (seed >> 8)*(3.f/16777216.f);
        float b = a + d;
        if(b > a && bitsOf(endAfterAppend(a, b)) != bitsOf(b)) pairs.append(qMakePair(a, b));
    }
    return pairs;
}

static void checkExactRestore(const QStringList& projects)
{
    // the example: 4.9999986 to 5.999999 comes out of a load as 5.9999995
    const float a = fromBits(0x409FFFFDu), b = fromBits(0x40BFFFFEu);
    check(bitsOf(endAfterAppend(a, b)) == 0x40BFFFFFu && bitsOf(endAfterAppend(a, endAfterAppend(a, b))) == 0x40BFFFFFu,
          "single precision: (a + b) - a from " + hexOf(a) + " to " + hexOf(b) + " is " + hexOf(endAfterAppend(a, b)) + ", and stays there on a second pass");

    // the first track of the repository's projects with a force section and no roll smoothing
    for(int i = 0; i < projects.size(); ++i) {
        testProject project;
        loadProjectFile(projects[i], &project);
        for(int j = 0; j < project.tracks.size(); ++j) {
            track* t = project.tracks[j];
            if(t->smoothingActive()) continue;
            int s = -1;
            for(int k = 0; k < t->lSections.size() && s < 0; ++k) {
                if(t->lSections[k]->type == forced) s = k;
            }
            if(s < 0) continue;

            // The force section is made the last one: with these made-up
            // transitions a section after it can start from a node where its
            // own start values are not numbers (a geometric section's pitch
            // change, which the model reports and copes with).
            while(t->lSections.size() > s+1) t->removeSection(t->lSections.size()-1);
            QString where = QString("%1 track %2 section %3 (made the last)").arg(QFileInfo(projects[i]).fileName()).arg(j+1).arg(s+1);
            QList<float> ends;
            ends << 1.5f << a << b << 7.0f;
            checkExactCase(t, s, ends, 2, false, "exact restore, " + where + ", ends 1.5, 4.9999986, 5.999999, 7");
            ends.last() = 6.5f;
            checkExactCase(t, s, ends, 2, true, "exact restore, " + where + ", last normal-force piece locked");

            QList<QPair<float, float> > pairs = findAwkwardPairs(4);
            check(pairs.size() == 4, QString("search: found %1 more pairs with (a + b) - a != b").arg(pairs.size()));
            for(int k = 0; k < pairs.size(); ++k) {
                QList<float> e;
                e << pairs[k].first*0.5f << pairs[k].first << pairs[k].second << pairs[k].second + 1.f << pairs[k].second + 2.f;
                checkExactCase(t, s, e, 2, false, QString("exact restore, %1, found pair %2: %3 to %4").arg(where).arg(k+1)
                               .arg(hexOf(pairs[k].first)).arg(hexOf(pairs[k].second)));
            }
            return;
        }
    }
    check(false, "exact restore: a baseline track with a force section and no roll smoothing");
}

static void timeLargest(const QStringList& projects)
{
    QString largest;
    int nodes = -1;
    for(int i = 0; i < projects.size(); ++i) {
        testProject p;
        loadProjectFile(projects[i], &p);
        for(int j = 0; j < p.tracks.size(); ++j) {
            if(countNodes(p.tracks[j]) > nodes) {
                nodes = countNodes(p.tracks[j]);
                largest = projects[i];
            }
        }
    }
    testProject p;
    loadProjectFile(largest, &p);
    track* t = p.tracks[0];
    for(int j = 1; j < p.tracks.size(); ++j) {
        if(countNodes(p.tracks[j]) > countNodes(t)) t = p.tracks[j];
    }
    QElapsedTimer timer;
    timer.start();
    trackState s = captureTrackState(t);
    double captureMs = timer.nsecsElapsed()/1e6;
    timer.restart();
    restoreTrackState(t, s);
    double restoreMs = timer.nsecsElapsed()/1e6;
    timer.restart();
    trackState s2 = captureTrackState(t, &s);
    double sharedCaptureMs = timer.nsecsElapsed()/1e6;
    printf("time: %s (%d sections, %d nodes): capture %.3f ms, capture sharing with the one before %.3f ms, restore %.3f ms; state %lld bytes, %lld not shared with the one before\n",
           QFileInfo(largest).fileName().toUtf8().constData(), t->lSections.size(), countNodes(t),
           captureMs, sharedCaptureMs, restoreMs, (long long)s.byteSize(), (long long)s2.bytesNotSharedWith(s));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    setvbuf(stdout, NULL, _IONBF, 0);   // a line reaches the log even if a later check never returns

    if(argc < 2) {
        fprintf(stderr, "usage: trackstate <repository folder> [<folder of further .fvd projects>]\n");
        return 1;
    }
    QString repo = QString::fromLocal8Bit(argv[1]);

    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }
    tmpPath = tmp.path();

    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList projects;
    QStringList names = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    for(int i = 0; i < names.size(); ++i) {
        if(names[i].contains(".2016.")) continue;
        projects.append(baseline.filePath(names[i]));
    }
    check(projects.size() >= 8, QString("%1 baseline projects found").arg(projects.size()));

    for(int i = 0; i < projects.size(); ++i) {
        checkProject(projects[i]);
    }
    checkImport(repo, "nltrack");
    checkImport(repo, "txt");
    checkImport(repo, "csv");
    checkCsvProject(repo);

    {
        testProject project;
        loadProjectFile(projects[0], &project);
        checkNotices(project.tracks[0], QFileInfo(projects[0]).fileName());
    }
    checkStaleFirstNode(repo);
    checkAccents(repo);
    checkSharingAcrossAppend(repo);
    checkExactRestore(projects);
    timeLargest(projects);

    // optional: a folder of further projects, each run through the same
    // capture and restore battery as the baseline projects
    if(argc >= 3) {
        QDir extra(QString::fromLocal8Bit(argv[2]));
        QStringList extraNames = extra.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
        check(extra.exists() && !extraNames.isEmpty(), QString("%1 projects found in %2").arg(extraNames.size()).arg(extra.path()));
        for(int i = 0; i < extraNames.size(); ++i) {
            checkProject(extra.filePath(extraNames[i]));
        }
    }

    printf("%d checks, %d failed\n", checkCount, failCount);
    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
