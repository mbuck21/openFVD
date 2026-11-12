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

// commands: checks core/trackhistory.* and core/command.* without a window.
//
// 1. Every registered command is complete (name, description, label, apply,
//    every parameter with a unit and a description, example arguments that
//    pass its own validation) and appears in describe().
// 2. The harness later units rely on: every command runs with its example
//    arguments on every baseline project where its target exists, through
//    commandRegistry::run(); the model changes; undo brings back the saved
//    bytes and the three exports exactly; redo brings back the saved bytes
//    and the three exports exactly, on smoothed projects too.
//    A command exercised on no project fails the test.
//    Every anchor command also gives the same exports live as after the
//    project is saved and loaded again (checkAnchorReload()).
// 3. The history: scopes, rollback, nesting, noteEdit, rebase, undo and redo,
//    merging with a fake clock, the two caps, the listener, selections.
// 4. Commands: validation errors name the parameter, unknown commands and
//    targets, rollback of a failing apply, JSON round trip of arguments.
// 5. The dialogs' steps, without a window: Track Properties and the smoothing
//    dialog open one step, run their commands in it, and OK commits it (one
//    step, undone as one) or Cancel rolls it back (the saved bytes as at the
//    start, nothing recorded); smoothing.apply gives the exports the
//    smoothing dialog's OK gave.
// Prints the time of one run plus undo on the largest baseline project.
//
//   commands <repository folder>

#include "../../core/track.h"
#include "../../core/trackstate.h"
#include "../../core/trackhistory.h"
#include "../../core/command.h"
#include "../../core/projectfile.h"
#include "../../core/elementfile.h"
#include "../../core/smoothhandler.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QRegExp>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <climits>
#include <cstring>
#include <sstream>

using namespace std;

static bool allOk = true;
static int checkCount = 0;
static int failCount = 0;
static QString tmpPath;

// The history's warnings, kept for the checks that look for them.
static QStringList historyWarnings;

// Debug output and other warnings are dropped (the exporters print thousands
// of lines); a failed lenAssert (qCritical in release) is shown and fails the
// test.
static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtWarningMsg && msg.startsWith("trackHistory")) {
        historyWarnings.append(msg);
    }
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "critical: %s\n", msg.toLocal8Bit().constData());
        allOk = false;
    }
}

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

// The project file reader's client, as in tests/trackstate/main.cpp.
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

// Which exports differ, and by how many bytes in all (a byte that differs, or
// a difference in length).
static QString exportDiff(const QList<QByteArray>& a, const QList<QByteArray>& b, qint64* bytes = NULL)
{
    const char* names[] = { "nl2elem", "tangent", "spline" };
    QStringList parts;
    qint64 total = 0;
    for(int i = 0; i < 3; ++i) {
        QByteArray x = i < a.size() ? a[i] : QByteArray();
        QByteArray y = i < b.size() ? b[i] : QByteArray();
        if(x == y) continue;
        qint64 n = qAbs(x.size() - y.size());
        for(int k = 0; k < qMin(x.size(), y.size()); ++k) {
            if(x[k] != y[k]) ++n;
        }
        total += n;
        parts << QString("%1 (%2 bytes differ)").arg(names[i]).arg(n);
    }
    if(bytes) *bytes = total;
    return parts.isEmpty() ? QString("same") : parts.join(", ");
}

static QStringList baselineProjects(const QString& repo)
{
    QDir baseline(QDir(repo).filePath("samples/baseline"));
    QStringList projects;
    QStringList names = baseline.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    for(int i = 0; i < names.size(); ++i) {
        if(names[i].contains(".2016.")) continue;
        projects.append(baseline.filePath(names[i]));
    }
    return projects;
}

// ---- 1. every command is complete -------------------------------------------

static void checkSpecs()
{
    const QList<commandSpec>& all = commandRegistry::all();
    check(!all.isEmpty(), QString("the registry lists %1 commands").arg(all.size()));

    QRegExp namePattern("^(anchor|track|section|straight|curve|transition|smoothing)\\.[a-z][A-Za-z]*$");
    QStringList seen;
    QString text = commandRegistry::describe();
    QJsonArray json = commandRegistry::describeJson();
    check(json.size() == all.size(), QString("describeJson() has one entry per command (%1)").arg(json.size()));

    for(int i = 0; i < all.size(); ++i) {
        const commandSpec& s = all[i];
        QStringList missing;
        if(!namePattern.exactMatch(s.name)) missing << "a name of the form <object>.<verb><Property>";
        if(seen.contains(s.name)) missing << "a name of its own";
        seen << s.name;
        if(s.description.trimmed().isEmpty()) missing << "a description";
        if(s.label.trimmed().isEmpty()) missing << "an undo label";
        if(!s.apply) missing << "an apply function";
        if(!s.params.isEmpty() && s.example.isEmpty()) missing << "example arguments";
        for(int p = 0; p < s.params.size(); ++p) {
            const commandParam& par = s.params[p];
            if(par.name.isEmpty()) missing << QString("a name for parameter %1").arg(p+1);
            if(par.description.trimmed().isEmpty()) missing << QString("a description of \"%1\"").arg(par.name);
            if(par.unit.trimmed().isEmpty()) missing << QString("a unit for \"%1\" (\"none\" if it has none)").arg(par.name);
            if(par.type == paramEnum && par.enumNames.isEmpty()) missing << QString("the names of enum \"%1\"").arg(par.name);
            if(par.hasRange && par.minimum > par.maximum) missing << QString("a range for \"%1\" that is not empty").arg(par.name);
            if(!s.example.contains(par.name)) missing << QString("an example value for \"%1\"").arg(par.name);
        }
        QString why;
        if(!s.validate(s.example, NULL, &why)) missing << "example arguments that pass validation (" + why + ")";
        check(missing.isEmpty(), s.name + (missing.isEmpty() ? QString(": complete") : QString(": lacks ") + missing.join(", ")));

        // describe() comes from the spec: the name and every parameter are in it
        bool inText = text.contains(s.name + " (target: ");
        bool inJson = false;
        for(int j = 0; j < json.size(); ++j) {
            QJsonObject o = json[j].toObject();
            if(o.value("name").toString() != s.name) continue;
            QJsonArray ps = o.value("parameters").toArray();
            inJson = ps.size() == s.params.size() && !o.value("description").toString().isEmpty();
            for(int p = 0; inJson && p < s.params.size(); ++p) {
                QJsonObject pj = ps[p].toObject();
                inJson = pj.value("name").toString() == s.params[p].name
                        && pj.value("type").toString() == paramTypeName(s.params[p].type)
                        && pj.value("unit").toString() == s.params[p].unit
                        && pj.value("description").toString() == s.params[p].description
                        && (!s.params[p].hasRange || (pj.value("minimum").toDouble() == s.params[p].minimum
                                                      && pj.value("maximum").toDouble() == s.params[p].maximum));
            }
        }
        for(int p = 0; p < s.params.size(); ++p) {
            if(!text.contains("  " + s.params[p].name + ": ")) inText = false;
        }
        check(inText && inJson, s.name + ": describe() lists it with every parameter, as text and as JSON");
    }
}

// ---- 2. the harness: every command with undo and redo ------------------------

// The baseline projects have no custom smoothing region, and
// smoothing.removeRegion needs one: a region that is off (0 to 1 s) is added
// before the run, outside any history, as a project saved with one loads it.
static void prepareExample(const commandSpec& spec, track* t)
{
    if(spec.name == "smoothing.removeRegion") {
        smoothHandler* region = new smoothHandler(t, -2, NULL, 400, 1, 0, 1000);
        region->active = false;
        t->smoothList.append(region);
    }
}

static void runHarness(const QStringList& projects)
{
    const QList<commandSpec>& all = commandRegistry::all();
    QMap<QString, QStringList> exercised;

    for(int c = 0; c < all.size(); ++c) {
        const commandSpec& spec = all[c];
        for(int p = 0; p < projects.size(); ++p) {
            QString stem = QFileInfo(projects[p]).fileName();
            int trackCount;
            {
                testProject probe;
                loadProjectFile(projects[p], &probe);
                trackCount = probe.tracks.size();
            }
            for(int ti = 0; ti < trackCount; ++ti) {
                // a fresh load for every run, so no command sees another's traces
                testProject project;
                QString result = loadProjectFile(projects[p], &project);
                if(!check(!result.contains("Error") && ti < project.tracks.size(), stem + ": loads (" + result + ")")) continue;
                track* t = project.tracks[ti];
                prepareExample(spec, t);
                commandTarget target;
                if(!spec.findExampleTarget(t, ti, &target)) continue;

                QString label = QString("%1 on %2 track %3 (%4)").arg(spec.name, stem).arg(ti+1).arg(target.toText());
                trackHistory history(t);

                QByteArray savedBefore = savedBytes(t);
                QList<QByteArray> exportsBefore = exportsOf(t);
                trackState stateBefore = captureTrackState(t);

                QString error;
                bool ok = commandRegistry::run(&history, spec.name, target, spec.example, &error);
                if(!check(ok, label + ": runs" + (ok ? QString() : QString(" (") + error + ")"))) continue;
                exercised[spec.name] << QString("%1#%2").arg(stem).arg(ti+1);

                bool changed = captureTrackState(t, &stateBefore) != stateBefore;
                check(changed || spec.exampleMayChangeNothing, label + ": the model changed");
                if(!changed) {
                    check(history.steps().isEmpty(), label + ": nothing changed and no step was recorded");
                    continue;
                }
                check(history.steps().size() == 1 && history.position() == 1 && history.undoLabel() == spec.label
                      && history.steps()[0].calls.size() == 1 && history.steps()[0].calls[0].name == spec.name
                      && history.steps()[0].selection == target,
                      label + ": one step, labelled \"" + spec.label + "\", holding the call and the target");

                QByteArray savedAfter = savedBytes(t);
                QList<QByteArray> exportsAfter = exportsOf(t);

                commandTarget selection;
                check(history.undo(&selection) && selection == target, label + ": undo, returning the target as the selection");
                check(savedBytes(t) == savedBefore, label + ": after undo the saved bytes are exactly those before");
                QList<QByteArray> exportsUndone = exportsOf(t);
                check(exportsUndone == exportsBefore, label + ": after undo the three exports are exactly those before ("
                      + exportDiff(exportsBefore, exportsUndone) + ")");

                check(history.redo(&selection) && selection == target, label + ": redo, returning the target as the selection");
                check(savedBytes(t) == savedAfter, label + ": after redo the saved bytes are exactly those after");
                // the head after a redo is the track as it is: a scope opened
                // now finds no outside change and records nothing
                int warningsBefore = historyWarnings.size();
                history.beginStep("Nothing");
                history.endStep(true);
                check(historyWarnings.size() == warningsBefore && history.steps().size() == 1,
                      label + ": a scope after the redo sees no outside change and records nothing");
                QList<QByteArray> exportsRedone = exportsOf(t);

                check(exportsRedone == exportsAfter, label + ": after redo the three exports are exactly those after ("
                      + exportDiff(exportsAfter, exportsRedone) + ")");
            }
        }
    }

    for(int c = 0; c < all.size(); ++c) {
        QStringList on = exercised.value(all[c].name);
        check(!on.isEmpty(), QString("%1 was exercised on %2 track%3%4").arg(all[c].name).arg(on.size())
              .arg(on.size() == 1 ? "" : "s").arg(on.isEmpty() ? QString() : QString(": ") + on.join(", ")));
    }
    printf("commands exercised: %s\n", QStringList(exercised.keys()).join(", ").toUtf8().constData());
}

// ---- 2b. an anchor edit reaches the track at once ---------------------------

// The fields of two anchor nodes that differ, by name ("fRoll, vLat").
static QString anchorFieldsDiffering(const mnode& a, const mnode& b)
{
    QStringList names;
#define ANCHOR_FIELD(f) if(memcmp(&a.f, &b.f, sizeof(a.f)) != 0) names << #f;
    ANCHOR_FIELD(vPos) ANCHOR_FIELD(vDir) ANCHOR_FIELD(vLat) ANCHOR_FIELD(vNorm) ANCHOR_FIELD(fRoll)
    ANCHOR_FIELD(fVel) ANCHOR_FIELD(fEnergy) ANCHOR_FIELD(forceNormal) ANCHOR_FIELD(forceLateral)
    ANCHOR_FIELD(fPitchFromLast) ANCHOR_FIELD(fYawFromLast)
#undef ANCHOR_FIELD
    if(names.isEmpty() && memcmp(static_cast<const void*>(&a), static_cast<const void*>(&b), sizeof(mnode)) != 0) names << "other fields";
    return names.join(", ");
}

// Every anchor command on every track of every baseline project, against the
// same project saved to a file and loaded into a fresh model: the reloaded
// anchor node is the live one bit for bit, and the three exports are the live
// ones byte for byte. What the track is after an anchor edit is what saving
// and reopening gives. (Before track::updateFromAnchor() the first section
// kept the anchor it was made from; before track::rebuildAnchor() the node
// an edit left behind depended on the order of the edits, where a load builds
// it from the saved values.) Then the same command again with the same
// values: the runs where that changes the track are listed (information).
// Undo and redo of the same runs are checked by runHarness().
static void checkAnchorReload(const QStringList& projects)
{
    const QList<commandSpec>& all = commandRegistry::all();
    int runs = 0, same = 0;
    QStringList notIdempotent;
    for(int c = 0; c < all.size(); ++c) {
        const commandSpec& spec = all[c];
        if(!spec.name.startsWith("anchor.")) continue;
        for(int p = 0; p < projects.size(); ++p) {
            QString stem = QFileInfo(projects[p]).fileName();
            int trackCount;
            {
                testProject probe;
                loadProjectFile(projects[p], &probe);
                trackCount = probe.tracks.size();
            }
            for(int ti = 0; ti < trackCount; ++ti) {
                testProject project;
                loadProjectFile(projects[p], &project);
                if(ti >= project.tracks.size()) continue;
                track* t = project.tracks[ti];
                QString label = QString("reload: %1 on %2 track %3").arg(spec.name, stem).arg(ti+1);

                trackHistory history(t);
                QString error;
                if(!check(commandRegistry::run(&history, spec.name, commandTarget::forTrack(ti), spec.example, &error),
                          label + ": runs " + error)) continue;
                QList<QByteArray> liveExports = exportsOf(t);

                QString file = QDir(tmpPath).filePath("reload.fvd");
                QString saved = saveProjectFile(file, project.texPath, project.tracks, &project);
                testProject reloaded;
                QString loaded = loadProjectFile(file, &reloaded);
                if(!check(!loaded.contains("Error") && ti < reloaded.tracks.size(),
                          label + ": saves and loads again (" + saved + ", " + loaded + ")")) continue;
                track* r = reloaded.tracks[ti];
                ++runs;

                bool sameAnchor = memcmp(static_cast<const void*>(t->anchorNode), static_cast<const void*>(r->anchorNode), sizeof(mnode)) == 0;
                check(sameAnchor, label + ": the reloaded anchor node is the live one bit for bit"
                      + (sameAnchor ? QString() : QString(" (differs in ") + anchorFieldsDiffering(*t->anchorNode, *r->anchorNode) + ")"));
                QList<QByteArray> reloadedExports = exportsOf(r);
                check(reloadedExports == liveExports, label + ": the live exports equal those after saving and reloading ("
                      + exportDiff(liveExports, reloadedExports) + ")");
                if(sameAnchor && reloadedExports == liveExports) ++same;

                // the command left the node settled: building it again changes nothing
                mnode before = *t->anchorNode;
                int passes = t->settleAnchor();
                check(passes == 1 && memcmp(static_cast<const void*>(&before), static_cast<const void*>(t->anchorNode), sizeof(mnode)) == 0,
                      label + QString(": the command leaves the anchor settled (one more rebuild: %1 pass, node unchanged)").arg(passes));

                // the same command again
                trackState once = captureTrackState(t);
                commandRegistry::run(&history, spec.name, commandTarget::forTrack(ti), spec.example, &error);
                if(captureTrackState(t, &once) != once) {
                    notIdempotent << QString("%1 on %2 track %3").arg(spec.name, stem).arg(ti+1);
                }
            }
        }
    }
    printf("reload: %d of %d anchor command runs give the same anchor node and exports live as after saving and reloading\n", same, runs);
    printf("reload: running the command a second time with the same values changes the track in %d: %s\n", notIdempotent.size(),
           notIdempotent.isEmpty() ? "none" : notIdempotent.join("; ").toUtf8().constData());
}

// The anchor commands rebuild the anchor until its roll settles
// (track::settleAnchor(): up to 8 rebuilds, then, for a roll that alternates
// between two floats, the nearest kept roll within 256 floats; a command whose
// anchor does not settle fails with a message). Every baseline track settles
// from the anchor its load built, and so does every roll from -180 to 180
// degrees in steps of 0.25 at three pitches; the largest number of rebuilds
// is printed.
static void checkAnchorSettles(const QStringList& projects)
{
    int most = 0;
    bool all = true;
    QStringList unsettled;
    for(int p = 0; p < projects.size(); ++p) {
        testProject project;
        loadProjectFile(projects[p], &project);
        for(int ti = 0; ti < project.tracks.size(); ++ti) {
            int passes = project.tracks[ti]->settleAnchor();
            if(passes < 0) {
                all = false;
                unsettled << QString("%1 track %2").arg(QFileInfo(projects[p]).fileName()).arg(ti+1);
            }
            most = qMax(most, passes);
        }
    }
    check(all, QString("settle: the anchor of every baseline track settles (at most %1 rebuilds)%2")
          .arg(most).arg(unsettled.isEmpty() ? QString() : QString("; not: ") + unsettled.join(", ")));

    int sweepMost = 0, count = 0, searched = 0;
    double largestMove = 0.;
    QStringList sweepUnsettled;
    const float pitches[] = { 0.f, 12.5f, -40.f };
    for(int k = 0; k < 3; ++k) {
        for(int r = -720; r <= 720; ++r) {
            track t(glm::vec3(0.f, 0.f, 0.f), 0.f, 1.1f);
            t.startPitch = pitches[k];
            t.anchorNode->fRoll = r*0.25f;
            int passes = t.settleAnchor();
            ++count;
            if(passes < 0) sweepUnsettled << QString("roll %1 pitch %2").arg(r*0.25).arg(pitches[k]);
            if(passes > 8) {
                ++searched;
                largestMove = qMax(largestMove, (double)fabs(t.anchorNode->fRoll - r*0.25f));
            }
            sweepMost = qMax(sweepMost, passes);
        }
    }
    check(sweepUnsettled.isEmpty(), QString("settle: %1 rolls from -180 to 180 degrees at pitch 0, 12.5 and -40 settle (at most %2 rebuilds; "
                                            "%3 alternated and were moved to the nearest kept roll, by at most %4 degrees)%5")
          .arg(count).arg(sweepMost).arg(searched).arg(largestMove, 0, 'g', 3)
          .arg(sweepUnsettled.isEmpty() ? QString() : QString("; not: ") + sweepUnsettled.join(", ")));
    printf("settle: the largest number of rebuilds: %d on the baseline tracks, %d over the roll sweep\n", most, sweepMost);
}

// How many float steps apart two floats are (0 when bit for bit equal).
static long long floatSteps(float a, float b)
{
    qint32 x, y;
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    long long u = x < 0 ? (long long)INT_MIN - x : x;
    long long v = y < 0 ? (long long)INT_MIN - y : y;
    return u > v ? u - v : v - u;
}

// A rate command changes only the rate it names. On every baseline track:
// anchor.setPitchRate 50 times with the same value leaves the yaw rate bit for
// bit as it was before the first run, after every run, with the forces after
// run 2 and run 50 the same; the same the other way round; then the two alternated for 50 rounds,
// both rates and both forces the same after rounds 2 and 50. Prints how far the
// first run moved the other rate (in float steps). The 2016 arithmetic (both
// rates converted together) grew the other rate by 1.0197 per run.
static void checkRateCommands(const QStringList& projects)
{
    long long largestMove = 0;
    QStringList notes;
    for(int p = 0; p < projects.size(); ++p) {
        QString stem = QFileInfo(projects[p]).fileName();
        int trackCount;
        {
            testProject probe;
            loadProjectFile(projects[p], &probe);
            trackCount = probe.tracks.size();
        }
        for(int ti = 0; ti < trackCount; ++ti) {
            for(int which = 0; which < 3; ++which) {
                testProject project;
                loadProjectFile(projects[p], &project);
                track* t = project.tracks[ti];
                trackHistory history(t);
                mnode* a = t->anchorNode;
                const float pitch0 = a->fPitchFromLast, yaw0 = a->fYawFromLast;
                QString error;
                bool ran = true;
                float other1 = 0.f, p2 = 0.f, y2 = 0.f, n2 = 0.f, l2 = 0.f;
                bool steady = true;
                for(int run = 1; run <= 50; ++run) {
                    if(which == 0 || which == 2) ran = commandRegistry::run(&history, "anchor.setPitchRate", commandTarget::forTrack(ti), commandArgs{{"rate", 7.5}}, &error) && ran;
                    if(which == 1 || which == 2) ran = commandRegistry::run(&history, "anchor.setYawRate", commandTarget::forTrack(ti), commandArgs{{"rate", -6.0}}, &error) && ran;
                    float other = which == 0 ? a->fYawFromLast : a->fPitchFromLast;
                    if(run == 1) other1 = other;
                    if(which < 2 && memcmp(&other, &other1, sizeof(float)) != 0) steady = false;
                    if(run == 2) { p2 = a->fPitchFromLast; y2 = a->fYawFromLast; n2 = a->forceNormal; l2 = a->forceLateral; }
                }
                bool same50 = memcmp(&p2, &a->fPitchFromLast, 4) == 0 && memcmp(&y2, &a->fYawFromLast, 4) == 0
                        && memcmp(&n2, &a->forceNormal, 4) == 0 && memcmp(&l2, &a->forceLateral, 4) == 0;
                QString label = QString("rates: %1 track %2, %3").arg(stem).arg(ti+1)
                        .arg(which == 0 ? "anchor.setPitchRate 7.5 fifty times" : which == 1 ? "anchor.setYawRate -6 fifty times"
                                                                                  : "the two alternated fifty rounds");
                if(which < 2) {
                    float before = which == 0 ? yaw0 : pitch0;
                    long long move = floatSteps(before, other1);
                    largestMove = qMax(largestMove, move);
                    QString otherName = which == 0 ? "yaw" : "pitch";
                    QString detail = QString("%1 rate %2 deg/s before, %3 after run 1 (%4 float steps), %5 after run 50")
                            .arg(otherName).arg(before*F_HZ, 0, 'g', 9).arg(other1*F_HZ, 0, 'g', 9).arg(move)
                            .arg((which == 0 ? a->fYawFromLast : a->fPitchFromLast)*F_HZ, 0, 'g', 9);
                    if(before != 0.f) notes << stem + QString(" track %1: ").arg(ti+1) + (which == 0 ? "pitch rate typed, " : "yaw rate typed, ") + detail;
                    check(ran && steady && same50 && move == 0, label + ": the other rate stays bit for bit, and the rates and forces after run 2 equal those after run 50 (" + detail + ")");
                } else {
                    check(ran && same50, label + ": both rates and both forces after round 2 equal those after round 50");
                }
            }
        }
    }
    printf("rates: the first run moved the rate not typed by at most %lld float steps\n", largestMove);
    for(int i = 0; i < notes.size(); ++i) printf("  %s\n", notes[i].toUtf8().constData());
}

// ---- 3. the history ---------------------------------------------------------

static qint64 fakeNow = 0;
static qint64 fakeClock() { return fakeNow; }

class countingHistoryListener : public trackHistoryListener
{
public:
    countingHistoryListener() : calls(0) {}
    int calls;
    void historyChanged(trackHistory*) { ++calls; }
};

// A cheap edit outside any command: the track's name is in its saved header,
// and nothing needs recomputing.
static void rename(track* t, const QString& name) { t->name = name; }

// What trackHistory::bytes() should be, from the steps' states.
static qint64 expectedBytes(const trackHistory& h)
{
    const QList<historyStep>& s = h.steps();
    qint64 total = 0;
    for(int i = 0; i < s.size(); ++i) {
        total += i == 0 ? s[i].before.byteSize() : s[i].before.bytesNotSharedWith(s[i-1].after);
        total += s[i].after.bytesNotSharedWith(s[i].before);
    }
    return total;
}

static void checkHistory(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    const QString original = t->name;
    const QByteArray bytes0 = savedBytes(t);

    {
        trackHistory h(t);
        countingHistoryListener listener;
        h.setListener(&listener);

        // scope, commit
        h.beginStep("Rename A", commandTarget::forSection(0, 1));
        rename(t, "A");
        h.endStep(true);
        check(h.steps().size() == 1 && h.position() == 1 && h.undoLabel() == "Rename A" && listener.calls == 1,
              "history: a committed scope records one step and tells the listener once");
        const QByteArray bytesA = savedBytes(t);

        // scope, no change
        h.beginStep("Nothing");
        h.endStep(true);
        h.beginStep("Same name");
        rename(t, "A");
        h.endStep(true);
        check(h.steps().size() == 1 && listener.calls == 1, "history: a scope that changes nothing records nothing");

        // scope, rollback (with a recompute inside)
        h.beginStep("Try");
        rename(t, "tried");
        t->fFriction = 0.5f;
        t->updateTrack(0, 0);
        h.endStep(false);
        check(savedBytes(t) == bytesA && h.steps().size() == 1 && listener.calls == 1,
              "history: a rolled-back scope puts the saved bytes back exactly and records nothing");

        // historyScope: rolled back unless committed
        {
            historyScope scope(&h, "Forgotten");
            rename(t, "forgotten");
        }
        check(savedBytes(t) == bytesA && h.steps().size() == 1, "history: a historyScope that is not committed rolls back");
        {
            historyScope scope(&h, "Rename B", commandTarget::forSection(0, 2));
            rename(t, "B");
            scope.commit();
        }
        check(h.steps().size() == 2 && h.undoLabel() == "Rename B", "history: a committed historyScope records its step");

        // nested scopes join the outer step
        h.beginStep("Outer", commandTarget::forSection(0, 0));
        rename(t, "C1");
        h.beginStep("Inner");
        check(h.scopeDepth() == 2, "history: scopes nest");
        rename(t, "C2");
        h.endStep(true);
        h.endStep(true);
        check(h.steps().size() == 3 && h.undoLabel() == "Outer" && t->name == "C2",
              "history: a nested committed scope is part of the outer step, under the outer label");

        // a nested rollback puts back the inner start, and the outer step goes on
        h.beginStep("Outer 2");
        rename(t, "D1");
        const QByteArray bytesD1 = savedBytes(t);
        h.beginStep("Solver trial");
        rename(t, "trial");
        h.endStep(false);
        check(savedBytes(t) == bytesD1, "history: a nested rollback puts back the state at the inner scope's start");
        h.endStep(true);
        check(h.steps().size() == 4 && t->name == "D1", "history: and the outer step records the rest");

        // refused while a scope is open
        h.beginStep("Open");
        check(!h.canUndo() && !h.undo() && !h.canRedo() && !h.redo(), "history: undo and redo are refused while a scope is open");
        h.endStep(false);

        // undo and redo across several steps, with selections
        int callsBefore = listener.calls;
        commandTarget sel;
        check(h.undo(&sel) && t->name == "C2" && sel == commandTarget(), "history: undo 1 brings back C2 (selection: nothing)");
        check(h.undo(&sel) && t->name == "B" && sel == commandTarget::forSection(0, 0), "history: undo 2 brings back B, selection section 1");
        check(h.undo(&sel) && t->name == "A" && sel == commandTarget::forSection(0, 2), "history: undo 3 brings back A, selection section 3");
        check(h.undo(&sel) && savedBytes(t) == bytes0 && sel == commandTarget::forSection(0, 1),
              "history: undo 4 brings back the saved bytes as loaded, selection section 2");
        check(!h.canUndo() && !h.undo() && h.canRedo() && h.redoLabel() == "Rename A", "history: nothing more to undo; redo is \"Rename A\"");
        check(h.redo(&sel) && savedBytes(t) == bytesA && sel == commandTarget::forSection(0, 1), "history: redo 1 brings back A exactly");
        check(h.redo(&sel) && t->name == "B" && sel == commandTarget::forSection(0, 2), "history: redo 2 brings back B");
        check(listener.calls == callsBefore + 6, QString("history: the listener heard every undo and redo (%1 calls)").arg(listener.calls - callsBefore));

        // a new step drops the redo branch
        rename(t, "E");
        h.noteEdit("Rename E", commandTarget::forSection(0, 3));
        check(h.steps().size() == 3 && h.position() == 3 && !h.canRedo() && h.undoLabel() == "Rename E"
              && h.labels() == (QStringList() << "Rename A" << "Rename B" << "Rename E"),
              "history: a new step after two undos drops the redo branch (" + h.labels().join(", ") + ")");

        // noteEdit with no change, and rebase
        h.noteEdit("No change");
        check(h.steps().size() == 3, "history: noteEdit with nothing changed records nothing");
        rename(t, "loaded from elsewhere");
        h.rebase();
        h.noteEdit("After rebase");
        check(h.steps().size() == 3 && h.headState() == captureTrackState(t),
              "history: rebase takes the track as it is without a step");
        rename(t, "F");
        h.noteEdit("Rename F");
        check(h.undo() && t->name == "loaded from elsewhere", "history: a step after rebase undoes to the rebased state");
        check(h.redo() && t->name == "F", "history: and redoes");

        // calls of a command are recorded in the step
        QString error;
        bool ran = commandRegistry::run(&h, "track.setFriction", commandTarget::forTrack(0), commandArgs{{"friction", 0.2}}, &error);
        check(ran && h.steps().last().calls.size() == 1 && h.steps().last().calls[0].args.value("friction").toDouble() == 0.2
              && h.steps().last().label == "Change friction",
              "history: a command's step holds its call (name, target, validated arguments)");

        // a command inside a scope joins it
        h.beginStep("Two edits as one");
        commandRegistry::run(&h, "track.setFriction", commandTarget::forTrack(0), commandArgs{{"friction", 0.3}}, &error);
        commandRegistry::run(&h, "track.setResistance", commandTarget::forTrack(0), commandArgs{{"resistance", 0.0001}}, &error);
        int stepsBefore = h.steps().size();
        h.endStep(true);
        check(h.steps().size() == stepsBefore + 1 && h.steps().last().calls.size() == 2 && h.undoLabel() == "Two edits as one",
              "history: two commands inside one scope are one step with two calls");

        int callsBeforeClear = listener.calls;
        h.clear();
        check(h.steps().isEmpty() && h.position() == 0 && !h.canUndo() && h.bytes() == 0 && listener.calls == callsBeforeClear + 1,
              "history: clear forgets every step and tells the listener");
    }

    // merging, with a clock the test sets
    {
        rename(t, original);
        trackHistory h(t);
        h.setClock(fakeClock);
        check(h.mergeWindow() == 0, "merge: the window is 0 (never merge) by default");
        fakeNow = 0;
        rename(t, "m1");
        h.noteEdit("Type", commandTarget(), "key");
        fakeNow = 100;
        rename(t, "m2");
        h.noteEdit("Type", commandTarget(), "key");
        check(h.steps().size() == 2, "merge: with window 0, the same key twice is two steps");

        h.clear();
        h.setMergeWindow(1000);
        const QByteArray start = savedBytes(t);
        fakeNow = 10000;
        rename(t, "a1");
        h.noteEdit("Type", commandTarget::forSection(0, 0), "key");
        fakeNow = 10900;
        rename(t, "a12");
        h.noteEdit("Type", commandTarget::forSection(0, 0), "key");
        fakeNow = 11800;
        rename(t, "a123");
        h.noteEdit("Type", commandTarget::forSection(0, 0), "key");
        check(h.steps().size() == 1, "merge: the same key within the window (900 ms apart, twice) extends the step");
        fakeNow = 12801;
        rename(t, "a1234");
        h.noteEdit("Type", commandTarget::forSection(0, 0), "key");
        check(h.steps().size() == 2, "merge: the same key 1001 ms after the last change is a new step");
        fakeNow = 12900;
        rename(t, "other");
        h.noteEdit("Other", commandTarget(), "other key");
        check(h.steps().size() == 3, "merge: another key is a new step");
        fakeNow = 13000;
        rename(t, "other2");
        h.endMergeRun();
        h.noteEdit("Other", commandTarget(), "other key");
        check(h.steps().size() == 4, "merge: after endMergeRun the same key is a new step");
        fakeNow = 13100;
        rename(t, "other3");
        h.noteEdit("Other", commandTarget(), "other key");
        check(h.steps().size() == 4, "merge: and the next one within the window joins that one");
        h.undo();
        fakeNow = 13200;
        rename(t, "after undo");
        h.noteEdit("Other", commandTarget(), "other key");
        check(h.steps().size() == 4 && h.undoLabel() == "Other" && !h.canRedo(),
              "merge: an undo ends the run (the next edit is a new step replacing the redo branch)");
        fakeNow = 13300;
        rename(t, "after undo 2");
        h.noteEdit("Plain");
        fakeNow = 13350;
        rename(t, "after undo 3");
        h.noteEdit("Plain");
        check(h.steps().size() == 6, "merge: steps with no key never merge");
        while(h.undo()) {}
        check(savedBytes(t) == start, "merge: undoing everything brings back the start exactly (a merged step keeps its first state before)");
        h.redo();
        check(t->name == "a123", "merge: and redoing the merged step lands on its last state after");
    }

    // an outside change is folded into the newest step
    {
        rename(t, original);
        trackHistory h(t);
        rename(t, "f1");
        h.noteEdit("S1");
        rename(t, "f2");
        h.noteEdit("S2");
        rename(t, "outside");
        h.foldOutsideChange();
        check(h.steps().size() == 2 && h.position() == 2 && h.headState() == captureTrackState(t),
              "fold: an outside change adds no step");
        check(h.undo() && t->name == "f1" && h.redo() && t->name == "outside",
              "fold: it is in the newest step's state after; the older step still undoes to f1");
        h.undo();
        h.undo();
        check(t->name == original && h.canRedo(), "fold: undone to the start, two steps to redo");
        rename(t, "outside 2");
        h.foldOutsideChange();
        check(h.steps().isEmpty() && !h.canRedo() && !h.canUndo() && h.headState() == captureTrackState(t),
              "fold: with nothing applied it ends Redo and becomes the start");
        h.foldOutsideChange();
        check(h.steps().isEmpty(), "fold: nothing changed, nothing done");
        rename(t, "g1");
        h.noteEdit("G1");
        check(h.undo() && t->name == "outside 2", "fold: the next step undoes to the folded state");
    }

    // the caps
    {
        rename(t, original);
        trackHistory h(t);
        countingHistoryListener listener;
        h.setListener(&listener);
        h.setMaxSteps(3);
        QList<QByteArray> after;
        for(int i = 1; i <= 5; ++i) {
            rename(t, QString("cap %1").arg(i));
            h.noteEdit(QString("Step %1").arg(i));
            after << savedBytes(t);
        }
        check(h.steps().size() == 3 && h.labels() == (QStringList() << "Step 3" << "Step 4" << "Step 5"),
              "caps: with at most 3 steps the oldest are dropped first (" + h.labels().join(", ") + ")");
        while(h.undo()) {}
        check(savedBytes(t) == after[1], "caps: undoing all lands on the state after step 2");
        check(listener.calls == 5 + 3, QString("caps: the listener heard 5 steps and 3 undos (%1)").arg(listener.calls));
        while(h.redo()) {}

        h.setMaxSteps(50000);
        h.clear();
        for(int i = 1; i <= 6; ++i) {
            rename(t, QString("bytes %1").arg(i));
            h.noteEdit(QString("Bytes %1").arg(i));
        }
        qint64 full = 0;
        for(int i = 0; i < h.steps().size(); ++i) full += h.steps()[i].before.byteSize() + h.steps()[i].after.byteSize();
        check(h.bytes() == expectedBytes(h) && h.bytes() < full,
              QString("caps: the history holds %1 bytes, each shared part once (%2 if counted per state)").arg(h.bytes()).arg(full));
        // A rename changes only the header: the sections and the smoothing
        // entries of every step after the first are shared and not counted
        // (the live values, such as the anchor node, are counted per state).
        bool sectionsOnce = true;
        for(int i = 1; i < h.steps().size(); ++i) {
            const trackState& s = h.steps()[i].after;
            qint64 sharedParts = s.smoothing.size();
            for(int k = 0; k < s.sections.size(); ++k) sharedParts += s.sections[k].size();
            if(h.steps()[i].cost > 2 * (s.byteSize() - sharedParts)) sectionsOnce = false;
        }
        check(sectionsOnce, QString("caps: steps 2 to 6 count none of their shared section and smoothing bytes (step 2 costs %1 of a %2-byte state)")
              .arg(h.steps()[1].cost).arg(h.steps()[1].after.byteSize()));
        qint64 limit = h.bytes() - 1;
        h.setMaxBytes(limit);
        check(h.steps().size() == 5 && h.bytes() <= limit && h.bytes() == expectedBytes(h) && h.labels().first() == "Bytes 2",
              QString("caps: a byte cap one below drops the oldest step (%1 steps, %2 bytes)").arg(h.steps().size()).arg(h.bytes()));
        h.setMaxBytes(1);
        check(h.steps().size() == 1 && h.undoLabel() == "Bytes 6", "caps: a cap nothing fits in keeps the newest step");
        h.setMaxBytes(qint64(128)*1024*1024);
    }
    rename(t, original);
}

// A change made outside any step and not noted is folded into the next step;
// beginStep() says so with a warning naming the step.
static void checkOutsideChangeWarning(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    trackHistory h(t);

    historyWarnings.clear();
    h.beginStep("Normal");
    rename(t, "normal");
    h.endStep(true);
    h.undo();
    h.redo();
    h.beginStep("After undo and redo");
    rename(t, "normal 2");
    h.endStep(true);
    check(historyWarnings.isEmpty(), "outside change: none in the normal case, no warning");

    rename(t, "changed behind its back");
    h.beginStep("Rename later");
    rename(t, "later");
    h.endStep(true);
    check(historyWarnings.size() == 1 && historyWarnings[0].contains("changed outside the history")
          && historyWarnings[0].contains("\"Rename later\""),
          "outside change: beginStep warns, naming the step: " + historyWarnings.value(0));
    check(h.undo() && t->name == "changed behind its back",
          "outside change: the behaviour is kept (the change is folded into the step's state before)");
    h.redo();

    historyWarnings.clear();
    rename(t, "again behind its back");
    h.beginStep("Abandoned");
    h.endStep(false);
    h.beginStep("Next");
    h.endStep(true);
    check(historyWarnings.size() == 1 && historyWarnings[0].contains("\"Abandoned\""),
          QString("outside change: a rolled-back scope warns once, and the next scope does not (%1 warnings)").arg(historyWarnings.size()));
    historyWarnings.clear();
}

// ---- 4. commands ------------------------------------------------------------

static void checkCommands(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    trackHistory h(t);
    const QByteArray bytes0 = savedBytes(t);
    QString error;

    struct errorCase { const char* what; commandArgs args; const char* mustName; };
    errorCase cases[] = {
        { "wrong type", commandArgs{{"friction", QString("much")}}, "\"friction\"" },
        { "out of range", commandArgs{{"friction", 2.5}}, "\"friction\"" },
        { "unknown name", commandArgs{{"friction", 0.1}, {"fricton", 0.1}}, "\"fricton\"" },
        { "missing name", commandArgs(), "\"friction\"" },
    };
    for(unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        error.clear();
        bool ok = commandRegistry::run(&h, "track.setFriction", commandTarget::forTrack(0), cases[i].args, &error);
        check(!ok && error.contains(cases[i].mustName) && savedBytes(t) == bytes0 && h.steps().isEmpty(),
              QString("commands: %1 is refused, naming the parameter: %2").arg(cases[i].what, error));
    }
    // (each call is made before its label is built: the order in which a
    // function's arguments are evaluated is not fixed)
    bool ran;
    error.clear();
    ran = commandRegistry::run(&h, "track.setFrictionTypo", commandTarget::forTrack(0), commandArgs{{"friction", 0.1}}, &error);
    check(!ran && error.contains("track.setFrictionTypo"), "commands: an unknown command is refused: " + error);
    error.clear();
    ran = commandRegistry::run(&h, "track.setFriction", commandTarget::forTrack(0), commandArgs{{"friction", true}}, &error);
    check(!ran && error.contains("\"friction\""), "commands: a bool for a number is refused: " + error);

    // targets that do not exist
    QString why;
    bool found;
    found = resolveTarget(t, commandTarget::forSection(0, 9), targetSection, NULL, &why);
    check(!found && why.contains("section 10"), "commands: a section that does not exist is named 1-based: " + why);
    int forcedAt = -1, curvedAt = -1;
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(t->lSections[i]->type == forced && forcedAt < 0) forcedAt = i;
        if(t->lSections[i]->type == curved && curvedAt < 0) curvedAt = i;
    }
    if(forcedAt >= 0) {
        why.clear();
        found = resolveTarget(t, commandTarget::forTransition(0, forcedAt, funcPitch, 0), targetTransition, NULL, &why);
        check(!found && why.contains("pitch"), "commands: a force section has no pitch transitions: " + why);
        why.clear();
        found = resolveTarget(t, commandTarget::forTransition(0, forcedAt, funcNormal, 99), targetTransition, NULL, &why);
        check(!found && why.contains("100"), "commands: a transition that does not exist is named 1-based: " + why);
        resolvedTarget r;
        check(resolveTarget(t, commandTarget::forTransition(0, forcedAt, funcNormal, 0), targetTransition, &r, &why)
              && r.sec == t->lSections[forcedAt] && r.function == t->lSections[forcedAt]->normForce
              && r.transition == t->lSections[forcedAt]->normForce->funcList[0],
              "commands: a force section's normal transition 1 resolves to its section, func and subfunc");
    }
    if(curvedAt >= 0) {
        why.clear();
        found = resolveTarget(t, commandTarget::forTransition(0, curvedAt, funcNormal, 0), targetTransition, NULL, &why);
        check(!found && why.contains("curve"), "commands: a curve has no normal transitions: " + why);
    }
    check(commandTarget::forTransition(0, 2, funcLateral, 0).toText() == "track 1, section 3, lateral transition 1",
          "commands: a target in text is 1-based (" + commandTarget::forTransition(0, 2, funcLateral, 0).toText() + ")");
    commandTarget back;
    commandTarget sample = commandTarget::forTransition(1, 2, funcYaw, 3);
    check(commandTarget::fromJson(sample.toJson(), &back, &why) && back == sample, "commands: a target goes to JSON and back");

    // a failing apply rolls back: the bytes are untouched and nothing is recorded
    commandSpec failing;
    failing.name = "track.setNothing";
    failing.description = "A command for this test that changes the track and then fails.";
    failing.target = targetTrack;
    failing.label = "Fail";
    failing.apply = [](track* tr, const commandTarget&, const commandArgs&, QString* err) {
        tr->name = "half done";
        tr->fFriction = 0.7f;
        tr->updateTrack(0, 0);
        *err = "it failed on purpose";
        return false;
    };
    countingHistoryListener listener;
    h.setListener(&listener);
    error.clear();
    bool ok = commandRegistry::runSpec(&h, failing, commandTarget::forTrack(0), commandArgs(), &error);
    check(!ok && error.contains("it failed on purpose") && savedBytes(t) == bytes0 && h.steps().isEmpty() && listener.calls == 0,
          "commands: a failing apply is rolled back (the saved bytes untouched, no step, no notice): " + error);
    h.setListener(NULL);

    // arguments to JSON and back
    commandSpec typed;
    typed.name = "track.setTyped";
    typed.params << commandParam::floatParam("length", "m", "A length.", 0.0, 100.0)
                 << commandParam::intParam("passes", "none", "A count.", 1, 10)
                 << commandParam::boolParam("on", "A switch.")
                 << commandParam::enumParam("shape", "A shape.", QStringList() << "linear" << "cubic")
                 << commandParam::textParam("name", "A name.");
    commandArgs args{{"length", 12.3456789012345}, {"passes", 3}, {"on", true}, {"shape", QString("cubic")},
                     {"name", QString::fromUtf8("Bo\xc3\xa9 1")}};
    commandArgs valid, again;
    QByteArray line = QJsonDocument(commandArgsToJson(args)).toJson(QJsonDocument::Compact);
    bool v1 = typed.validate(args, &valid, &why);
    bool v2 = typed.validate(commandArgsFromJson(QJsonDocument::fromJson(line).object()), &again, &why);
    check(v1 && v2 && valid == again && again.value("passes").userType() == QMetaType::Int
          && again.value("length").toDouble() == 12.3456789012345,
          "commands: arguments go to a JSON line and back with their types and values: " + QString::fromUtf8(line));
    why.clear();
    found = typed.validate(commandArgs{{"length", 1.0}, {"passes", 2.5}, {"on", true}, {"shape", QString("cubic")}, {"name", QString("x")}}, NULL, &why);
    check(!found && why.contains("\"passes\""), "commands: a fraction for a whole number is refused: " + why);
    why.clear();
    found = typed.validate(commandArgs{{"length", 1.0}, {"passes", 2}, {"on", true}, {"shape", QString("round")}, {"name", QString("x")}}, NULL, &why);
    check(!found && why.contains("\"shape\"") && why.contains("cubic"), "commands: an enum name not in the list is refused, listing the names: " + why);
}

// ---- 5. the dialogs' steps -------------------------------------------------

static bool runIn(trackHistory& h, const QString& name, const commandArgs& args, QString* error)
{
    return commandRegistry::run(&h, name, commandTarget::forTrack(0), args, error);
}

// smoothing.setEntry's arguments that leave entry as it is (as the dialog makes
// them: the dialog smooths once, at OK).
static commandArgs entryArgs(track* t, int entry)
{
    smoothHandler* cur = t->smoothList[entry];
    commandArgs args;
    args.insert("entry", entry);
    args.insert("length", cur->getLength()/1000.);
    args.insert("passes", cur->getIterations());
    args.insert("on", cur->active);
    args.insert("from", cur->getFrom()/1000.);
    args.insert("to", cur->getTo()/1000.);
    args.insert("name", cur->sName);
    args.insert("apply", false);
    return args;
}

// The New button's region.
static commandArgs newRegionArgs()
{
    return commandArgs{{"from", 0.0}, {"to", 1.0}, {"length", 0.4}, {"passes", 1}, {"on", true},
                       {"name", QString("custom Region")}, {"apply", false}};
}

// What the smoothing dialog's list hears from the entries.
class countingSmoothListener : public smoothHandlerListener
{
public:
    QMap<smoothHandler*, int> fields;   // the field bits each entry reported
    QList<smoothHandler*> deleted;
    void smoothHandlerChanged(smoothHandler* which, int f) { fields[which] |= f; }
    void smoothHandlerDeleted(smoothHandler* which) { deleted.append(which); }
};

static void checkDialogSteps(const QString& repo)
{
    const QString fourSections = QDir(repo).filePath("samples/baseline/four-sections.fvd");
    QString error;

    // Track Properties: open (a step), a colour, the heartline and friction, OK
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        const QByteArray start = savedBytes(t);
        const QList<QByteArray> exportsStart = exportsOf(t);

        h.beginStep("Change track properties", commandTarget::forTrack(0));
        bool ok = runIn(h, "track.setColor", commandArgs{{"which", QString("default")}, {"color", QString("#204060")}}, &error)
                && runIn(h, "track.setHeartline", commandArgs{{"height", 0.9}}, &error)
                && runIn(h, "track.setFriction", commandArgs{{"friction", 0.025}}, &error);
        check(ok && !h.canUndo() && h.steps().isEmpty(), "dialogs: Track Properties' commands join its open step (undo waits) " + error);
        h.endStep(true);
        const QByteArray after = savedBytes(t);
        check(h.steps().size() == 1 && h.undoLabel() == "Change track properties" && h.steps()[0].calls.size() == 3
              && t->display.colors[0] == QColor(0x20, 0x40, 0x60) && t->fHeart == 0.9f,
              QString("dialogs: Track Properties' OK records one step with its three calls (%1 steps)").arg(h.steps().size()));
        check(h.undo() && savedBytes(t) == start && exportsOf(t) == exportsStart,
              "dialogs: one undo takes back the colour, the heartline and the friction together");
        check(h.redo() && savedBytes(t) == after, "dialogs: and one redo brings them back");

        // opened again and OK pressed without a change: nothing
        h.beginStep("Change track properties", commandTarget::forTrack(0));
        h.endStep(true);
        check(h.steps().size() == 1 && savedBytes(t) == after, "dialogs: OK without a change records nothing");

        // a colour and the heartline, then Cancel
        h.beginStep("Change track properties", commandTarget::forTrack(0));
        runIn(h, "track.setColor", commandArgs{{"which", QString("transition")}, {"color", QString("#ffffff")}}, &error);
        runIn(h, "track.setHeartline", commandArgs{{"height", -1.0}}, &error);
        bool changed = savedBytes(t) != after;
        h.endStep(false);
        check(changed && savedBytes(t) == after && h.steps().size() == 1 && !h.inScope() && h.canUndo(),
              "dialogs: Track Properties' Cancel puts the saved bytes back, records nothing and ends the step");
    }

    // The smoothing dialog: open (a step; the entries brought up to date in
    // it), tick the whole track, add a region, OK (apply), then Cancel the same
    for(int commit = 0; commit < 2; ++commit) {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        const QByteArray start = savedBytes(t);
        const QList<QByteArray> exportsStart = exportsOf(t);
        const int entries = t->smoothList.size();

        h.beginStep("Change smoothing", commandTarget::forTrack(0));
        char customChar;
        t->updateSmoothEntries(&customChar);
        commandArgs tick = entryArgs(t, 0);   // the whole track (smoothing section 1 alone leaves this project as it is)
        tick.insert("on", true);
        bool ok = runIn(h, "smoothing.setEntry", tick, &error)
                && runIn(h, "smoothing.addRegion", newRegionArgs(), &error)
                && runIn(h, "smoothing.apply", commandArgs(), &error);
        const QList<QByteArray> exportsApplied = exportsOf(t);
        bool changed = savedBytes(t) != start && exportsApplied != exportsStart && t->smoothList.size() == entries + 1;
        check(ok && changed, QString("dialogs: the smoothing dialog's tick, New and OK change the track inside its step (exports %1) %2")
              .arg(exportDiff(exportsStart, exportsApplied), error));
        h.endStep(commit == 1);
        if(commit == 0) {
            check(savedBytes(t) == start && exportsOf(t) == exportsStart && h.steps().isEmpty() && !h.inScope()
                  && t->smoothList.size() == entries,
                  "dialogs: the smoothing dialog's Cancel puts back the saved bytes and the exports, records nothing and ends the step");
        } else {
            const QByteArray after = savedBytes(t);
            check(h.steps().size() == 1 && h.undoLabel() == "Change smoothing" && h.steps()[0].calls.size() == 3,
                  "dialogs: the smoothing dialog's OK records one step with its three calls");
            check(h.undo() && savedBytes(t) == start && exportsOf(t) == exportsStart && t->smoothList.size() == entries,
                  "dialogs: one undo takes back the tick, the region and the smoothing");
            check(h.redo() && savedBytes(t) == after && exportsOf(t) == exportsApplied,
                  QString("dialogs: one redo brings them back (%1)").arg(exportDiff(exportsApplied, exportsOf(t))));
        }
    }

    // smoothing.removeRegion takes the region out (a step of its own)
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        const int entries = t->smoothList.size();
        bool added = runIn(h, "smoothing.addRegion", newRegionArgs(), &error);
        bool removed = runIn(h, "smoothing.removeRegion", commandArgs{{"region", 1}, {"apply", false}}, &error);
        check(added && removed && t->smoothList.size() == entries && h.steps().size() == 2,
              "dialogs: a region added and removed again, two steps " + error);
        error.clear();
        bool refused = !runIn(h, "smoothing.removeRegion", commandArgs{{"region", 1}, {"apply", false}}, &error);
        check(refused && error.contains("\"region\"") && h.steps().size() == 2,
              "dialogs: removing a region that is not there is refused: " + error);
        error.clear();
        refused = !runIn(h, "smoothing.setEntry", commandArgs{{"entry", entries}, {"length", 0.4}, {"passes", 1}, {"on", false},
                                                              {"from", 0.0}, {"to", 1.0}, {"name", QString("x")},
                                                              {"apply", false}}, &error);
        check(refused && error.contains("\"entry\"") && h.steps().size() == 2,
              "dialogs: an entry that is not there is refused: " + error);
    }

    // A command run from outside the dialog tells the dialog's list: a new
    // region reaches it (it gets the other entries' listener), and a renamed
    // or switched region, and a removed one, report it.
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        countingSmoothListener listener;
        error.clear();
        for(int i = 0; i < t->smoothList.size(); ++i) t->smoothList[i]->mListener = &listener;
        bool added = runIn(h, "smoothing.addRegion", newRegionArgs(), &error);
        smoothHandler* region = t->smoothList.last();
        check(added && region->mListener == &listener && (listener.fields.value(region) & smoothHandler::fieldLabel),
              "dialogs: a region added from outside the dialog reaches the dialog's list " + error);
        listener.fields.clear();
        commandArgs rename = entryArgs(t, t->smoothList.size()-1);
        rename.insert("name", QString("Renamed"));
        rename.insert("on", false);
        bool renamed = runIn(h, "smoothing.setEntry", rename, &error);
        int heard = listener.fields.value(region);
        check(renamed && region->sName == "Renamed" && !region->active
              && (heard & smoothHandler::fieldName) && (heard & smoothHandler::fieldActive),
              QString("dialogs: renaming and switching off a region from outside the dialog tells its list (fields %1)").arg(heard));
        bool removed = runIn(h, "smoothing.removeRegion", commandArgs{{"region", 1}, {"apply", false}}, &error);
        check(removed && listener.deleted.contains(region), "dialogs: removing a region from outside the dialog tells its list");
        for(int i = 0; i < t->smoothList.size(); ++i) t->smoothList[i]->mListener = NULL;
    }

    // smoothing.apply on the smoothed project gives what the dialog's OK gave:
    // the entries brought up to date (when it opened), then applyRollSmooth()
    {
        const QString smoothed = QDir(repo).filePath("samples/baseline/gen-smooth-whole.fvd");
        testProject a;
        loadProjectFile(smoothed, &a);
        char customChar;
        a.tracks[0]->updateSmoothEntries(&customChar);
        a.tracks[0]->applyRollSmooth();
        const QList<QByteArray> exportsDialog = exportsOf(a.tracks[0]);

        testProject b;
        loadProjectFile(smoothed, &b);
        trackHistory h(b.tracks[0]);
        bool ok = runIn(h, "smoothing.apply", commandArgs(), &error);
        const QList<QByteArray> exportsCommand = exportsOf(b.tracks[0]);
        check(ok && exportsCommand == exportsDialog && savedBytes(b.tracks[0]) == savedBytes(a.tracks[0]) && h.steps().isEmpty(),
              QString("dialogs: smoothing.apply on gen-smooth-whole gives the exports of the dialog's OK (%1), and records nothing")
              .arg(exportDiff(exportsDialog, exportsCommand)));
    }
}

// The transition commands beyond the generic harness: add then delete leaves
// the saved bytes as they were (after and before the target), locking twice is
// refused and leaves no step, and a lock then an unlock records two steps.
static void checkTransitionCommands(const QString& repo)
{
    testProject project;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &project);
    track* t = project.tracks[0];
    trackHistory h(t);
    QString error;
    commandTarget target;
    const commandSpec* addSpec = commandRegistry::find("transition.add");
    check(addSpec && addSpec->findExampleTarget(t, 0, &target), "transition commands: four-sections has a transition to add to");
    if(!addSpec) return;
    const QByteArray bytes0 = savedBytes(t);

    const char* places[] = { "after", "before" };
    for(int k = 0; k < 2; ++k) {
        QString where = places[k];
        h.clear();
        bool ok = commandRegistry::run(&h, "transition.add", target, commandArgs{{"where", where}}, &error);
        check(ok && savedBytes(t) != bytes0, "transition commands: add " + where + " adds a transition: " + error);
        // the new transition is after the target, or in its place
        commandTarget added = commandTarget::forTransition(target.track, target.section, target.function,
                                                           k == 0 ? target.transition+1 : target.transition);
        ok = commandRegistry::run(&h, "transition.delete", added, commandArgs(), &error);
        check(ok && savedBytes(t) == bytes0, "transition commands: delete of the added one (" + where + ") returns the saved bytes to the start: " + error);
        check(h.steps().size() == 2, "transition commands: add and delete are one step each");
        while(h.canUndo()) h.undo();
        check(savedBytes(t) == bytes0, "transition commands: undo of both returns the saved bytes to the start");
    }

    h.clear();
    commandTarget lockTarget;
    const commandSpec* lockSpec = commandRegistry::find("transition.setLocked");
    if(lockSpec && lockSpec->findExampleTarget(t, 0, &lockTarget)) {
        bool ok = commandRegistry::run(&h, "transition.setLocked", lockTarget, commandArgs{{"locked", true}}, &error);
        check(ok && h.steps().size() == 1, "transition commands: lock is one step: " + error);
        error.clear();
        ok = commandRegistry::run(&h, "transition.setLocked", lockTarget, commandArgs{{"locked", true}}, &error);
        check(!ok && h.steps().size() == 1, "transition commands: locking a locked transition is refused: " + error);
        ok = commandRegistry::run(&h, "transition.setLocked", lockTarget, commandArgs{{"locked", false}}, &error);
        check(ok && h.steps().size() == 2, "transition commands: unlock is a step of its own: " + error);
        while(h.canUndo()) h.undo();
        check(savedBytes(t) == bytes0, "transition commands: undo of lock and unlock returns the saved bytes to the start");
    } else {
        check(false, "transition commands: four-sections has a transition that can be locked");
    }
}

// section.add then section.delete of the new section, for each kind and place;
// a deleted section's smoothing entry comes back with its settings; a name
// that does not change records nothing.
static void checkSectionCommands(const QString& repo)
{
    const QString fourSections = QDir(repo).filePath("samples/baseline/four-sections.fvd");
    QString error;

    const char* kinds[] = { "straight", "curved", "force", "geometric" };
    for(int k = 0; k < 4; ++k) {
        for(int place = 0; place < 3; ++place) {
            testProject project;
            loadProjectFile(fourSections, &project);
            track* t = project.tracks[0];
            trackHistory h(t);
            const QByteArray start = savedBytes(t);
            const int count = t->lSections.size();
            const int after = place == 0 ? 0 : (place == 1 ? count/2 : count);
            const QString label = QString("section commands: %1 added %2").arg(kinds[k]).arg(place == 0 ? "at the start" : (place == 1 ? "in the middle" : "at the end"));

            bool ok = commandRegistry::run(&h, "section.add", commandTarget::forTrack(0),
                                           commandArgs{{"type", QString(kinds[k])}, {"after", after}}, &error);
            check(ok && t->lSections.size() == count+1 && t->lSections[after]->sName == "unnamed" && savedBytes(t) != start,
                  label + ": adds one section, named unnamed (" + error + ")");
            const QByteArray added = savedBytes(t);
            ok = commandRegistry::run(&h, "section.delete", commandTarget::forSection(0, after), commandArgs(), &error);
            check(ok && t->lSections.size() == count && savedBytes(t) == start,
                  label + ": deleting the new section returns the saved bytes to the start (" + error + ")");
            check(h.steps().size() == 2 && h.undo() && savedBytes(t) == added && h.undo() && savedBytes(t) == start,
                  label + ": undo of the delete and the add");
            check(h.redo() && h.redo() && savedBytes(t) == start, label + ": redo of both");
        }
    }

    // a bad kind or place is refused and changes nothing
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        const QByteArray start = savedBytes(t);
        check(!commandRegistry::run(&h, "section.add", commandTarget::forTrack(0),
                                    commandArgs{{"type", QString("bezier")}, {"after", 0}}, &error),
              "section commands: a Bezier section cannot be added");
        check(!commandRegistry::run(&h, "section.add", commandTarget::forTrack(0),
                                    commandArgs{{"type", QString("straight")}, {"after", t->lSections.size()+1}}, &error),
              "section commands: a place past the end is refused");
        check(savedBytes(t) == start && h.steps().isEmpty(), "section commands: refused adds change nothing and record nothing");
    }

    // the smoothing entry of a deleted section
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        commandArgs entry = entryArgs(t, 2);
        entry.insert("length", 0.7);
        entry.insert("passes", 3);
        entry.insert("on", true);
        bool ok = runIn(h, "smoothing.setEntry", entry, &error);
        check(ok, "section commands: the entry of section 2 is set: " + error);
        smoothHandler* e = t->smoothList[2];
        const int length = e->getLength(), passes = e->getIterations();
        const bool on = e->active;
        check(passes == 3 && on, "section commands: the entry holds passes 3 and is on");
        const int sections = t->smoothList.size();
        h.clear();
        const QByteArray before = savedBytes(t);
        ok = commandRegistry::run(&h, "section.delete", commandTarget::forSection(0, 1), commandArgs(), &error);
        check(ok && t->smoothList.size() == sections-1, "section commands: deleting the section removes its entry: " + error);
        check(h.undo() && t->smoothList.size() == sections, "section commands: undo brings the entry back");
        e = t->smoothList[2];
        check(e->getLength() == length && e->getIterations() == passes && e->active == on && savedBytes(t) == before,
              "section commands: the restored entry has its length, passes and on-state");
    }

    // the boxes limit the displayed number (a length or speed in the unit the
    // options set); the slots divide by the unit factor, and the command must
    // accept the smallest and the largest value the box can send under each
    // unit setting (ui/optionsmenu.cpp: length factor 1 or 1/0.3048, speed factor
    // 1, 3.6 or 2.2369356)
    {
        struct boxCase { const char* command; const char* param; double boxMin; double boxMax; bool length; };
        const boxCase cases[] = {
            { "straight.setLength", "length", 0.1, 500.0, true },
            { "curve.setRadius", "radius", 1.0, 500.0, true },
            { "section.setSpeed", "speed", 0.1, 500.0, false }
        };
        const double lengthFactors[] = { 1.0, 1.0/0.3048 };
        const double speedFactors[] = { 1.0, 3.6, 2.2369356 };
        for(int c = 0; c < 3; ++c) {
            const commandSpec* spec = commandRegistry::find(cases[c].command);
            if(!check(spec != NULL, QString("section commands: %1 exists").arg(cases[c].command))) continue;
            const int count = cases[c].length ? 2 : 3;
            for(int f = 0; f < count; ++f) {
                double factor = cases[c].length ? lengthFactors[f] : speedFactors[f];
                for(int end = 0; end < 2; ++end) {
                    double value = (end == 0 ? cases[c].boxMin : cases[c].boxMax)/factor;
                    commandArgs args;
                    args.insert(cases[c].param, value);
                    commandArgs valid;
                    QString why;
                    check(spec->validate(args, &valid, &why),
                          QString("section commands: %1 accepts the box's %2 (%3) under unit factor %4: %5")
                          .arg(cases[c].command, end == 0 ? "smallest" : "largest").arg(value, 0, 'g', 10).arg(factor, 0, 'g', 8).arg(why));
                }
            }
        }
    }

    // a name that does not change records nothing
    {
        testProject project;
        loadProjectFile(fourSections, &project);
        track* t = project.tracks[0];
        trackHistory h(t);
        const QString name = t->lSections[0]->sName;
        bool ok = commandRegistry::run(&h, "section.setName", commandTarget::forSection(0, 0), commandArgs{{"name", name}}, &error);
        check(ok && h.steps().isEmpty(), "section commands: the same name records nothing");
        ok = commandRegistry::run(&h, "section.setName", commandTarget::forSection(0, 0), commandArgs{{"name", name + "x"}}, &error);
        check(ok && h.steps().size() == 1 && t->lSections[0]->sName == name + "x", "section commands: a new name is one step");
        check(h.undo() && t->lSections[0]->sName == name, "section commands: undo gives the old name back");
    }
}

static void timeLargest(const QStringList& projects)
{
    QString largest;
    int nodes = -1;
    for(int i = 0; i < projects.size(); ++i) {
        testProject p;
        loadProjectFile(projects[i], &p);
        for(int j = 0; j < p.tracks.size(); ++j) {
            if(p.tracks[j]->getNumPoints() > nodes) {
                nodes = p.tracks[j]->getNumPoints();
                largest = projects[i];
            }
        }
    }
    testProject p;
    loadProjectFile(largest, &p);
    track* t = p.tracks[0];
    int index = 0;
    for(int j = 1; j < p.tracks.size(); ++j) {
        if(p.tracks[j]->getNumPoints() > t->getNumPoints()) {
            t = p.tracks[j];
            index = j;
        }
    }
    trackHistory h(t);
    QString error;
    QElapsedTimer timer;
    timer.start();
    bool ok = commandRegistry::run(&h, "track.setFriction", commandTarget::forTrack(index), commandArgs{{"friction", 0.0417}}, &error);
    double runMs = timer.nsecsElapsed()/1e6;
    timer.restart();
    bool undone = h.undo();
    double undoMs = timer.nsecsElapsed()/1e6;
    check(ok && undone, "time: the run and the undo succeed");
    printf("time: %s (%d sections, %d nodes): track.setFriction run %.3f ms (with its full recompute), undo %.3f ms; history %lld bytes\n",
           QFileInfo(largest).fileName().toUtf8().constData(), t->lSections.size(), t->getNumPoints(),
           runMs, undoMs, (long long)h.bytes());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    setvbuf(stdout, NULL, _IONBF, 0);   // a line reaches the log even if a later check never returns

    if(argc < 2) {
        fprintf(stderr, "usage: commands <repository folder>\n");
        return 1;
    }
    QString repo = QString::fromLocal8Bit(argv[1]);

    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }
    tmpPath = tmp.path();

    QStringList projects = baselineProjects(repo);
    check(projects.size() >= 8, QString("%1 baseline projects found").arg(projects.size()));

    checkSpecs();
    historyWarnings.clear();
    runHarness(projects);
    checkAnchorReload(projects);
    checkAnchorSettles(projects);
    checkRateCommands(projects);
    checkHistory(repo);
    check(historyWarnings.isEmpty(), QString("the harness and the history checks raised no history warning (%1)")
          .arg(historyWarnings.isEmpty() ? QString("none") : historyWarnings.join(" | ")));
    checkOutsideChangeWarning(repo);
    checkCommands(repo);
    checkTransitionCommands(repo);
    checkSectionCommands(repo);
    checkDialogSteps(repo);
    timeLargest(projects);

    printf("%d checks, %d failed\n", checkCount, failCount);
    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
