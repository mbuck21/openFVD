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

// tracktrain: the train in the project (core/tracktrain.*, the train
// commands in core/commands_train.cpp; docs/stage-3-plan.md, D5), without a
// window. On every baseline project:
// - the "train" JSON gives back every field exactly, awkward numbers included,
//   and is the same bytes for the same train;
// - every train command changes no node (the nodes' bytes are identical), is
//   one undo step, and undo and redo give back the saved bytes, appendix
//   included, exactly;
// - with a train the file up to "EOP" is the file without one, the reopened
//   project has the same train and the same three exports, and save, load,
//   save gives the same bytes;
// - train.clear, and switching on then off, give back the file without an
//   appendix;
// - unknown fields inside "train" survive a load, an edit and a save, and
//   so do stored values of known fields this version cannot use;
// - the text form carries the train exactly;
// - the other project's additions are found for "Import Track from Project";
// - the time of a train command against a full recompute, on
//   gen-normal-shapes and on a track of about 150,000 nodes.
//
//   tracktrain <repository folder>

#include "../../core/track.h"
#include "../../core/trackstate.h"
#include "../../core/trackhistory.h"
#include "../../core/command.h"
#include "../../core/projectfile.h"
#include "../../core/projecttext.h"
#include "../../core/elementfile.h"
#include "../../core/tracktrain.h"
#include "../../core/traintype.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>

using namespace std;

static bool allOk = true;
static int checks = 0, failed = 0;
static QString tmpPath;
static QString repo;

static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "critical: %s\n", msg.toLocal8Bit().constData());
        allOk = false;
    }
}

static bool check(bool ok, const QString& label)
{
    ++checks;
    if(!ok) { allOk = false; ++failed; }
    printf("%s: %s\n", ok ? "ok" : "FAIL", label.toUtf8().constData());
    return ok;
}

class testProject : public projectFileClient
{
public:
    ~testProject() { clear(); }
    QList<track*> tracks;
    QString texPath;
    QJsonObject additions;
    QStringList dropped;
    void clear() { while(!tracks.isEmpty()) delete tracks.takeFirst(); }
    void projectCleared() { clear(); additions = QJsonObject(); dropped.clear(); }
    bool projectGroundTexture(const QString& path) { texPath = path; return true; }
    track* projectNewTrack()
    {
        track* t = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
        t->name = QString("unnamed Track");
        tracks.append(t);
        return t;
    }
    void projectTrackSmoothingUpdated(track*) {}
    void projectTrackLoaded(track*) {}
    void projectLoadFinished() {}
    void projectTrackSaving(track*) {}
    QJsonObject projectAdditions() { return additions; }
    void projectAdditionsRead(const QJsonObject& read) { additions = read; }
    void projectAdditionsDropped(const QString& reason) { dropped << reason; }
};

static QByteArray readFile(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

static void writeFile(const QString& fileName, const QByteArray& bytes)
{
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(bytes);
}

static QByteArray saved(testProject& p)
{
    std::stringbuf buf(ios::in | ios::out | ios::binary);
    std::iostream io(&buf);
    writeProject(io, p.texPath, p.tracks, &p);
    std::string s = buf.str();
    return QByteArray(s.data(), (int)s.size());
}

static QString load(const QByteArray& bytes, testProject& p)
{
    std::stringbuf buf(std::string(bytes.constData(), bytes.size()), ios::in | ios::binary);
    std::iostream io(&buf);
    return readProject(io, &p);
}

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

// Every node of the track as raw bytes (the anchor and every section's nodes).
static QByteArray nodeBytes(track* t)
{
    QByteArray b((const char*)t->anchorNode, sizeof(mnode));
    for(int s = 0; s < t->lSections.size(); ++s) {
        const QVector<mnode>& nodes = t->lSections[s]->lNodes;
        b.append((const char*)nodes.constData(), nodes.size()*(int)sizeof(mnode));
    }
    return b;
}

// The bytes up to and including "EOP" (a project without an appendix: all of it).
static QByteArray upToEnd(const QByteArray& file)
{
    int fvx = file.lastIndexOf("EOPFVX");
    return fvx < 0 ? file : file.left(fvx + 3);
}

static QStringList baselineProjects()
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

static bool sameBits(float a, float b) { return memcmp(&a, &b, sizeof(float)) == 0; }

static bool sameListBits(const QList<float>& a, const QList<float>& b)
{
    if(a.size() != b.size()) return false;
    for(int i = 0; i < a.size(); ++i) if(!sameBits(a[i], b[i])) return false;
    return true;
}

// Every field, every float bit for bit.
static bool sameTrain(const trackTrain& a, const trackTrain& b)
{
    const trainSpec& x = a.spec;
    const trainSpec& y = b.spec;
    return a == b && sameBits(x.fCarPitch, y.fCarPitch) && sameListBits(x.lRowOffsets, y.lRowOffsets)
            && sameListBits(x.lSeatOffsets, y.lSeatOffsets) && sameBits(x.fSeatUp, y.fSeatUp)
            && sameBits(x.fCarMass, y.fCarMass) && sameBits(x.fRiderMass, y.fRiderMass) && sameBits(x.fLoad, y.fLoad)
            && sameListBits(x.lCarMassFactor, y.lCarMassFactor) && sameBits(x.fMassAboveRail, y.fMassAboveRail)
            && sameBits(x.resistance.fRolling, y.resistance.fRolling) && sameBits(x.resistance.fRollingLoad, y.resistance.fRollingLoad)
            && sameBits(x.resistance.fLinear, y.resistance.fLinear) && sameBits(x.resistance.fDrag, y.resistance.fDrag);
}

// A train with every field away from its default and awkward numbers.
static trackTrain awkwardTrain()
{
    trackTrain t;
    t.bSimulate = true;
    t.bExtendedAirtime = true;
    trainSpec& s = t.spec;
    s.sTypeId = "nl2-hyper-coaster";
    s.iCars = 9;
    s.fCarPitch = 1.f/3.f;
    s.lRowOffsets = QList<float>() << 0.1f << -0.0f << -0.30000001f << 1e-7f;
    s.lSeatOffsets = QList<float>() << -0.55f << 2.345678e-5f << 0.55f;
    s.fSeatUp = -0.123456789f;
    s.fCarMass = 612.375f;
    s.fRiderMass = 81.1f;
    s.fLoad = 0.7f;
    s.lCarMassFactor = QList<float>() << 1.25f << 1.f << 1.f << 1.f << 1.f << 1.f << 1.f << 1.f << 0.9f;
    s.bMassAtDesignPoint = false;
    s.fMassAboveRail = -0.0f;
    s.resistance.bFromProject = false;
    s.resistance.fRolling = 0.026f;
    s.resistance.fRollingLoad = 3.0e-3f;
    s.resistance.fLinear = 1.17549435e-38f;   // the smallest normal float
    s.resistance.fDrag = 0.0001f;
    s.iReferenceCar = 8;
    s.sRestraint = "prone";
    t.lAssumed << "lRowOffsets" << "fCarMass";
    return t;
}

static QByteArray compact(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

// ---- 1. the JSON shape ----------------------------------------------------

static void checkJson()
{
    trackTrain a = awkwardTrain();
    QString why;
    check(a.spec.isValid(&why), "the awkward train is a valid train " + why);
    QJsonObject j1 = trainToJson(a);
    QByteArray b1 = compact(j1);
    printf("json: %s\n", b1.constData());
    QStringList problems;
    trackTrain back = trainFromJson(QJsonDocument::fromJson(b1).object(), &problems);
    check(problems.isEmpty() && sameTrain(back, a), "json: every field of an awkward train comes back exactly, bit for bit ("
          + problems.join("; ") + ")");
    check(compact(trainToJson(back)) == b1, "json: writing it again gives the same bytes");
    check(b1.contains("\"carPitch\":0.33333334") && b1.contains("0.1,") && b1.contains("\"riderMass\":81.1"),
          "json: numbers are written as the shortest decimal of the float");

    trackTrain d;
    QJsonObject jd = trainToJson(d);
    check(compact(jd) == "{\"simulate\":false}", "json: the default train is {\"simulate\":false}: " + QString(compact(jd)));
    check(trainFromJson(jd) == d && trainFromJson(QJsonObject()) == d, "json: and it, and an empty object, read back as the default");
    check(d.spec == trainSpec::point() && d.spec.iReferenceCar == -1 && !d.bSimulate,
          "json: the default is no simulation, the program's own point, the reference at the train's centre");

    // a field at its default is left out only where reading gives it back
    trackTrain one;
    one.bSimulate = true;
    one.spec.iCars = 3;
    one.spec.fCarPitch = 2.5f;
    QJsonObject jo = trainToJson(one);
    check(compact(jo) == "{\"carPitch\":2.5,\"cars\":3,\"simulate\":true}" && trainFromJson(jo) == one,
          "json: a train of three cars is " + QString(compact(jo)));

    // bad fields: named, the default taken, the rest read
    QJsonObject bad = QJsonDocument::fromJson("{\"simulate\":true,\"cars\":-2,\"carPitch\":\"x\",\"restraint\":\"rope\","
                                              "\"rows\":[0.5,-0.5],\"referenceCar\":7}").object();
    problems.clear();
    trackTrain b = trainFromJson(bad, &problems);
    check(problems.size() >= 4 && b.bSimulate && b.spec.iCars == 1 && b.spec.lRowOffsets.size() == 2 && b.spec.iReferenceCar == -1,
          "json: fields of the wrong kind are named and take their default: " + problems.join("; "));
}

// ---- 2. every train command on every baseline project ----------------------

struct trainRun { QString name; commandArgs args; };

static QList<trainRun> trainRuns()
{
    QList<trainRun> runs;
    runs << trainRun{"train.setSimulate", commandArgs{{"on", true}}};
    runs << trainRun{"train.setType", commandArgs{{"type", QString("nl2-hyper-coaster")}}};
    runs << trainRun{"train.setCars", commandArgs{{"cars", 7}}};
    runs << trainRun{"train.setCarPitch", commandArgs{{"pitch", 2.0 + 1.0/3.0}}};
    runs << trainRun{"train.setRows", commandArgs{{"count", 3}, {"spacing", 0.85}}};
    runs << trainRun{"train.setSeats", commandArgs{{"count", 4}, {"spacing", 0.6}}};
    runs << trainRun{"train.setSeatHeight", commandArgs{{"height", 0.15}}};
    runs << trainRun{"train.setReference", commandArgs{{"car", 7}}};
    runs << trainRun{"train.setRestraint", commandArgs{{"restraint", QString("otsr")}}};
    runs << trainRun{"train.setExtendedAirtime", commandArgs{{"allowed", true}}};
    runs << trainRun{"train.setResistance", commandArgs{{"fromProject", false}, {"rolling", 0.026}, {"rollingLoad", 0.004},
                                                        {"linear", 0.001}, {"drag", 0.0001}}};
    return runs;
}

static void checkCommandsOnBaseline()
{
    QStringList projects = baselineProjects();
    check(projects.size() >= 8, QString("%1 baseline projects").arg(projects.size()));
    // every registered train command is in the list above or is train.clear
    QStringList listed;
    QList<trainRun> runs = trainRuns();
    for(int i = 0; i < runs.size(); ++i) listed << runs[i].name;
    listed << "train.clear";
    const QList<commandSpec>& all = commandRegistry::all();
    QStringList missing;
    for(int i = 0; i < all.size(); ++i) {
        if(all[i].name.startsWith("train.") && !listed.contains(all[i].name)) missing << all[i].name;
        if(all[i].name.startsWith("train.") && !all[i].settlesItself) missing << all[i].name + " (not marked settlesItself)";
    }
    check(missing.isEmpty(), "every train command is exercised here and marked as computing nothing " + missing.join(", "));

    int badNodes = 0, badUndo = 0, badSteps = 0, badEop = 0, badReopen = 0, badClear = 0, badSim = 0, tracksDone = 0;
    QStringList notes;
    for(int p = 0; p < projects.size(); ++p) {
        QString stem = QFileInfo(projects[p]).fileName();
        testProject project;
        QString r = loadProjectFile(projects[p], &project);
        if(!check(!r.startsWith("Error"), stem + " loads")) continue;
        const QByteArray fileWithout = saved(project);
        check(!fileWithout.contains("FVX"), stem + ": re-saved without a train, it has no appendix");
        QList<QList<QByteArray> > exportsWithout;
        for(int ti = 0; ti < project.tracks.size(); ++ti) exportsWithout << exportsOf(project.tracks[ti]);

        for(int ti = 0; ti < project.tracks.size(); ++ti) {
            track* t = project.tracks[ti];
            trackHistory h(t);
            ++tracksDone;
            for(int k = 0; k < runs.size(); ++k) {
                QString label = QString("%1 track %2: %3").arg(stem).arg(ti+1).arg(runs[k].name);
                QByteArray nodesBefore = nodeBytes(t);
                QByteArray fileBefore = saved(project);
                int stepsBefore = h.steps().size();
                QString error;
                bool ok = commandRegistry::run(&h, runs[k].name, commandTarget::forTrack(ti), runs[k].args, &error);
                if(!check(ok, label + " runs " + error)) continue;
                if(nodeBytes(t) != nodesBefore) { ++badNodes; notes << label + ": nodes changed"; }
                QByteArray fileAfter = saved(project);
                if(h.steps().size() != stepsBefore + 1 || fileAfter == fileBefore) { ++badSteps; notes << label + ": not one step that changes the file"; }
                h.undo();
                bool undoOk = saved(project) == fileBefore;
                h.redo();
                bool redoOk = saved(project) == fileAfter;
                if(!undoOk || !redoOk) { ++badUndo; notes << label + ": undo or redo does not give the saved bytes back"; }
            }
        }
        const QByteArray fileWith = saved(project);
        if(upToEnd(fileWith) != fileWithout || !fileWith.contains("EOPFVX")) { ++badEop; notes << stem + ": the bytes up to EOP differ"; }

        // reopened: the same trains, the same exports, the same bytes again
        QList<trackTrain> trains;
        for(int ti = 0; ti < project.tracks.size(); ++ti) trains << trainOfTrack(project.tracks[ti]);
        testProject again;
        QString lr = load(fileWith, again);
        bool same = lr == "Load Successfull." && again.tracks.size() == project.tracks.size() && saved(again) == fileWith;
        for(int ti = 0; same && ti < again.tracks.size(); ++ti) {
            same = sameTrain(trainOfTrack(again.tracks[ti]), trains[ti]) && exportsOf(again.tracks[ti]) == exportsWithout[ti];
        }
        if(!same) { ++badReopen; notes << stem + ": the reopened project differs (" + lr + ")"; }

        // switched on and off again: no key, no appendix
        {
            testProject q;
            loadProjectFile(projects[p], &q);
            for(int ti = 0; ti < q.tracks.size(); ++ti) {
                trackHistory h(q.tracks[ti]);
                QString e;
                commandRegistry::run(&h, "train.setSimulate", commandTarget::forTrack(ti), commandArgs{{"on", true}}, &e);
                commandRegistry::run(&h, "train.setSimulate", commandTarget::forTrack(ti), commandArgs{{"on", false}}, &e);
            }
            if(saved(q) != fileWithout) { ++badSim; notes << stem + ": on and off again is not the file without a train"; }
        }

        // cleared: the file without an appendix, as before the train
        for(int ti = 0; ti < again.tracks.size(); ++ti) {
            trackHistory h(again.tracks[ti]);
            QString e;
            commandRegistry::run(&h, "train.clear", commandTarget::forTrack(ti), commandArgs(), &e);
            if(!h.canUndo()) { ++badClear; notes << stem + ": clear recorded no step"; }
        }
        if(saved(again) != fileWithout) { ++badClear; notes << stem + ": cleared, it is not the file without a train"; }
    }
    for(int i = 0; i < notes.size(); ++i) printf("note: %s\n", notes[i].toUtf8().constData());
    QString n = QString(" (%1 tracks, %2 commands each)").arg(tracksDone).arg(trainRuns().size());
    check(badNodes == 0, "a train command changes no node: the nodes' bytes are identical after every run" + n);
    check(badSteps == 0, "every train command is one undo step that changes the saved file" + n);
    check(badUndo == 0, "undo and redo of every train command give back the saved bytes, appendix included, exactly" + n);
    check(badEop == 0, "with a train the file up to EOP is byte for byte the file without one");
    check(badReopen == 0, "saved and reopened: the same train on every track (every field, bit for bit), the same three exports as "
                          "without a train, and the same bytes when saved again");
    check(badSim == 0, "switched on and off again, the project is saved exactly as without a train (no appendix)");
    check(badClear == 0, "train.clear gives back the file without an appendix");
}

// ---- 3. refused commands ---------------------------------------------------

static void checkRefused()
{
    testProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &p);
    track* t = p.tracks[0];
    trackHistory h(t);
    QByteArray before = saved(p);
    QString e1, e2;
    bool a = commandRegistry::run(&h, "train.setType", commandTarget::forTrack(0), commandArgs{{"type", QString("no-such-train")}}, &e1);
    bool b = commandRegistry::run(&h, "train.setReference", commandTarget::forTrack(0), commandArgs{{"car", 3}}, &e2);
    check(!a && e1.contains("no-such-train") && !b && e2.contains("1 car") && saved(p) == before && h.steps().isEmpty(),
          "an unknown type and a reference car beyond the train are refused, change nothing and record nothing: " + e1 + " / " + e2);
    QString e;
    commandRegistry::run(&h, "train.setCars", commandTarget::forTrack(0), commandArgs{{"cars", 5}}, &e);
    commandRegistry::run(&h, "train.setReference", commandTarget::forTrack(0), commandArgs{{"car", 5}}, &e);
    commandRegistry::run(&h, "train.setCars", commandTarget::forTrack(0), commandArgs{{"cars", 3}}, &e);
    trackTrain tr = trainOfTrack(t);
    check(tr.spec.iCars == 3 && tr.spec.iReferenceCar == 2 && tr.spec.fCarPitch == trainSpec::standInCarPitch()
          && tr.lAssumed.contains("fCarPitch"), "fewer cars than the reference car: the reference becomes the rear car; "
          "more than one car without a pitch takes the stand-in, marked assumed");
    commandRegistry::run(&h, "train.setType", commandTarget::forTrack(0), commandArgs{{"type", QString("nl2-hyper-coaster")}}, &e);
    tr = trainOfTrack(t);
    QStringList assumed;
    const trainType* type = trainCatalogue::findById(trainCatalogue::defaultCatalogue(), "nl2-hyper-coaster");
    check(type != nullptr && tr.spec == trainSpec::fromType(*type, &assumed) && tr.lAssumed == assumed,
          "train.setType gives the catalogue's train and its assumed fields: " + tr.lAssumed.join(", "));
}

// ---- 4. unknown fields inside "train" --------------------------------------

static void checkUnknownFields()
{
    testProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &p);
    QJsonObject train = QJsonDocument::fromJson("{\"simulate\":true,\"cars\":4,\"carPitch\":2.25,\"futureSeatMap\":{\"x\":[1,2.5]},"
                                                "\"resistance\":{\"fromProject\":true,\"futureTerm\":7}}").object();
    p.tracks[0]->additions["train"] = train;
    p.tracks[0]->additions["somethingElse"] = QJsonValue(QString("kept"));
    QString file = QDir(tmpPath).filePath("unknown.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);

    testProject q;
    loadProjectFile(file, &q);
    trackHistory h(q.tracks[0]);
    QString e;
    bool ok = commandRegistry::run(&h, "train.setCars", commandTarget::forTrack(0), commandArgs{{"cars", 6}}, &e)
            && commandRegistry::run(&h, "train.setResistance", commandTarget::forTrack(0),
                                    commandArgs{{"fromProject", false}, {"rolling", 0.02}, {"rollingLoad", 0.0}, {"linear", 0.0}, {"drag", 0.0002}}, &e);
    saveProjectFile(file, q.texPath, q.tracks, &q);
    testProject r;
    loadProjectFile(file, &r);
    QJsonObject t = r.tracks[0]->additions.value("train").toObject();
    check(ok && t.value("cars").toInt() == 6 && t.value("futureSeatMap").toObject().value("x").toArray().size() == 2
          && t.value("resistance").toObject().value("futureTerm").toInt() == 7
          && t.value("resistance").toObject().value("fromProject").toBool() == false
          && r.tracks[0]->additions.value("somethingElse").toString() == "kept",
          "unknown fields inside \"train\" (and inside its resistance, and beside it) survive a load, an edit and a save: "
          + QString(compact(r.tracks[0]->additions)));
    // switched off and back to defaults, the unknown fields keep the key
    trackHistory h2(r.tracks[0]);
    commandRegistry::run(&h2, "train.setSimulate", commandTarget::forTrack(0), commandArgs{{"on", false}}, &e);
    check(r.tracks[0]->additions.contains("train") && r.tracks[0]->additions.value("train").toObject().contains("futureSeatMap"),
          "a train with unknown fields is not dropped when it is switched off");
    commandRegistry::run(&h2, "train.clear", commandTarget::forTrack(0), commandArgs(), &e);
    check(!r.tracks[0]->additions.contains("train") && r.tracks[0]->additions.value("somethingElse").toString() == "kept",
          "train.clear takes the whole key away and leaves the track's other additions");
}

// ---- 4b. stored values this version cannot use are kept --------------------

// The three stored values as they were saved: "restraint":"harness",
// "referenceCar":99, "load":3, each byte for byte in the saved document.
static bool holdsKept(track* t, bool restraintToo)
{
    QByteArray doc = compact(t->additions.value("train").toObject());
    return doc.contains("\"referenceCar\":99") && doc.contains("\"load\":3,")
            && (!restraintToo || doc.contains("\"restraint\":\"harness\""));
}

static void checkKeptValues()
{
    testProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &p);
    p.tracks[0]->additions["train"] = QJsonDocument::fromJson(
                "{\"simulate\":true,\"cars\":2,\"carPitch\":2.5,\"restraint\":\"harness\",\"referenceCar\":99,\"load\":3}").object();
    QString file = QDir(tmpPath).filePath("kept.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);

    testProject q;
    loadProjectFile(file, &q);
    track* t = q.tracks[0];
    QStringList problems = trainProblemsOfTrack(t);
    QString all = problems.join(" | ");
    check(problems.size() == 3 && all.contains("\"restraint\"") && all.contains("\"referenceCar\"") && all.contains("\"load\""),
          "kept: the problems name the restraint, the reference car and the load: " + all);
    trackTrain tr = trainOfTrack(t);
    check(tr.spec.sRestraint.isEmpty() && tr.spec.iReferenceCar == -1 && tr.spec.fLoad == 1.f && tr.spec.iCars == 2
          && tr.kept.size() == 3, "kept: the train in memory uses the defaults of those three and reads the rest");
    trainSpec ridden;
    QStringList rideProblems;
    check(simulatedTrainOfTrack(t, &ridden, nullptr, &rideProblems) && rideProblems == problems,
          "kept: the ride gets the train and its problems");
    check(holdsKept(t, true), "kept: loaded, the three stored values are there");

    trackHistory h(t);
    QString e;
    bool ok = commandRegistry::run(&h, "train.setSimulate", commandTarget::forTrack(0), commandArgs{{"on", false}}, &e);
    check(ok && holdsKept(t, true) && trackHasTrain(t), "kept: train.setSimulate (off) writes the three back unchanged, and the "
          "train is not taken for the default while they are kept");
    ok = commandRegistry::run(&h, "train.setCars", commandTarget::forTrack(0), commandArgs{{"cars", 4}}, &e);
    QByteArray afterCars = compact(t->additions);
    check(ok && holdsKept(t, true) && trainOfTrack(t).spec.iCars == 4,
          "kept: train.setCars 4 keeps them, the reference car 99 still beyond the train");
    h.undo();
    bool undoOk = holdsKept(t, true) && trainOfTrack(t).spec.iCars == 2;
    h.redo();
    check(undoOk && compact(t->additions) == afterCars, "kept: undo and redo give them back exactly");
    saveProjectFile(file, q.texPath, q.tracks, &q);
    testProject r;
    loadProjectFile(file, &r);
    check(r.tracks.size() == 1 && compact(r.tracks[0]->additions) == afterCars && holdsKept(r.tracks[0], true),
          "kept: saved and reopened, the train's document is byte for byte the same");

    track* u = r.tracks[0];
    trackHistory h2(u);
    ok = commandRegistry::run(&h2, "train.setRestraint", commandTarget::forTrack(0), commandArgs{{"restraint", QString("otsr")}}, &e);
    QByteArray doc = compact(u->additions.value("train").toObject());
    check(ok && doc.contains("\"restraint\":\"otsr\"") && holdsKept(u, false) && trainProblemsOfTrack(u).size() == 2,
          "kept: train.setRestraint replaces the restraint and leaves the other two kept: " + QString(doc));
    ok = commandRegistry::run(&h2, "train.setCars", commandTarget::forTrack(0), commandArgs{{"cars", 100}}, &e);
    trackTrain v = trainOfTrack(u);
    check(ok && v.spec.iReferenceCar == 98 && !v.kept.contains("referenceCar")
          && compact(u->additions.value("train").toObject()).contains("\"referenceCar\":99"),
          "kept: train.setCars 100 takes the kept reference car 99 into use, now that it is within the train");
    ok = commandRegistry::run(&h2, "train.clear", commandTarget::forTrack(0), commandArgs(), &e);
    check(ok && !trackHasTrain(u) && u->additions.isEmpty(), "kept: train.clear removes everything");

    // a train of default values but one kept value is not "no train"
    testProject s;
    loadProjectFile(QDir(repo).filePath("samples/baseline/four-sections.fvd"), &s);
    s.tracks[0]->additions["train"] = QJsonDocument::fromJson("{\"simulate\":true,\"restraint\":\"harness\"}").object();
    trackHistory h3(s.tracks[0]);
    commandRegistry::run(&h3, "train.setSimulate", commandTarget::forTrack(0), commandArgs{{"on", false}}, &e);
    check(compact(s.tracks[0]->additions) == "{\"train\":{\"restraint\":\"harness\",\"simulate\":false}}",
          "kept: switched off with only a kept value left, the key stays with that value: " + QString(compact(s.tracks[0]->additions)));
}

// ---- 5. the text form ------------------------------------------------------

static void checkText()
{
    testProject p;
    loadProjectFile(QDir(repo).filePath("samples/baseline/gen-two-tracks.fvd"), &p);
    setTrainOfTrack(p.tracks[1], awkwardTrain());
    QByteArray binary = saved(p);
    QByteArray t1;
    writeProjectText(t1, p.texPath, p.tracks, &p);
    testProject q;
    QString r = readProjectText(t1, &q);
    QByteArray t2;
    writeProjectText(t2, q.texPath, q.tracks, &q);
    check(r == "Load Successful." && q.tracks.size() == 2 && sameTrain(trainOfTrack(q.tracks[1]), awkwardTrain())
          && !trackHasTrain(q.tracks[0]) && t2 == t1 && saved(q) == binary,
          "the text form carries the train exactly: text to model to text, and binary to text to binary, give the same bytes (" + r + ")");
}

// ---- 6. "Import Track from Project" finds each track's additions -----------

static void checkImportReader()
{
    QString plain = QDir(repo).filePath("samples/baseline/gen-two-tracks.fvd");
    projectFileAdditions none = readProjectFileAdditions(plain);
    check(none.problem.isEmpty() && none.trackStarts.size() == 2 && none.tracks.size() == 2 && none.tracks[0].isEmpty()
          && none.projectEnd == readFile(plain).size() - 3, "import: a project without an appendix: two tracks, no additions, its end found");

    testProject p;
    loadProjectFile(plain, &p);
    setTrainOfTrack(p.tracks[1], awkwardTrain());
    QString file = QDir(tmpPath).filePath("import.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);
    QByteArray bytes = readFile(file);
    projectFileAdditions got = readProjectFileAdditions(file);
    // the places importUi finds by its search for "TRC" (just after the tag)
    QList<qint64> scan;
    for(int i = bytes.indexOf("TRC"); i >= 0 && i < upToEnd(bytes).size(); i = bytes.indexOf("TRC", i + 1)) scan << i + 3;
    check(got.problem.isEmpty() && got.trackStarts == scan && got.tracks.size() == 2 && got.tracks[0].isEmpty()
          && sameTrain(trainFromJson(got.tracks[1].value("train").toObject()), awkwardTrain()),
          "import: the second track's train is found, by the same places the import's search finds");

    // damaged appendix: the reason, no additions
    QByteArray damaged = bytes;
    damaged.chop(2);
    QString dfile = QDir(tmpPath).filePath("damaged.fvd");
    writeFile(dfile, damaged);
    projectFileAdditions bad = readProjectFileAdditions(dfile);
    check(!bad.problem.isEmpty() && bad.tracks.size() == 2 && bad.tracks[1].isEmpty() && bad.trackStarts == scan,
          "import: a damaged appendix gives the tracks without additions and says why: " + bad.problem);
}

// ---- 7. time ---------------------------------------------------------------

static double runMs(trackHistory& h, const QString& name, const commandArgs& args)
{
    double best = -1.;
    for(int k = 0; k < 5; ++k) {
        QString e;
        QElapsedTimer timer;
        timer.start();
        bool ok = commandRegistry::run(&h, name, commandTarget::forTrack(0), args, &e);
        double ms = timer.nsecsElapsed()/1e6;
        if(!ok) return -1.;
        h.undo();
        if(best < 0. || ms < best) best = ms;
    }
    return best;
}

static void timeOn(const QString& what, track* t)
{
    trackHistory h(t);
    QByteArray nodes = nodeBytes(t);
    double train = runMs(h, "train.setCars", commandArgs{{"cars", 7}});
    double type = runMs(h, "train.setType", commandArgs{{"type", QString("nl2-hyper-coaster")}});
    double friction = runMs(h, "track.setFriction", commandArgs{{"friction", 0.0417}});
    printf("time: %s (%d nodes): train.setCars %.3f ms, train.setType %.3f ms; track.setFriction (a full recompute) %.1f ms\n",
           what.toUtf8().constData(), t->getNumPoints(), train, type, friction);
    check(train >= 0. && type >= 0. && friction >= 0. && train < friction && type < friction,
          "time: on " + what + " a train command costs less than a recompute of the track");
    check(nodeBytes(t) == nodes, "time: on " + what + " the nodes are as before after the runs and undos");
}

static void checkTime()
{
    QString normal = QDir(repo).filePath("samples/baseline/gen-normal-shapes.fvd");
    {
        testProject p;
        loadProjectFile(normal, &p);
        timeOn("gen-normal-shapes", p.tracks[0]);
    }
    // a long track as tests/commands makes it: force sections and straights
    // at a fixed speed appended until about 150,000 nodes, saved and loaded
    testProject p;
    loadProjectFile(normal, &p);
    track* t = p.tracks[0];
    for(int k = 0; t->getNumPoints() < 150000 && k < 1000; ++k) {
        section* added = t->appendSectionFromFile(k % 2 ? straight : forced);
        added->bSpeed = 0;
        added->fVel = 20.f;
        t->updateTrack(added, 0);
    }
    QString file = QDir(tmpPath).filePath("long.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);
    testProject longP;
    loadProjectFile(file, &longP);
    timeOn("a track of 150,000 nodes", longP.tracks[0]);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    setvbuf(stdout, NULL, _IONBF, 0);
    if(argc < 2) {
        fprintf(stderr, "usage: tracktrain <repository folder>\n");
        return 1;
    }
    repo = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir tmp;
    if(!tmp.isValid()) { printf("FAIL: no temporary folder\n"); return 1; }
    tmpPath = tmp.path();

    check(!trainCatalogue::defaultCatalogue().isEmpty(), "the train catalogue is built into every program made from core/");
    checkJson();
    checkCommandsOnBaseline();
    checkRefused();
    checkUnknownFields();
    checkKeptValues();
    checkText();
    checkImportReader();
    checkTime();

    printf("%d checks, %d failed\n", checks, failed);
    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
