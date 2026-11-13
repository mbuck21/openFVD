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

// appendix: checks the project file's appendix (core/projectfile.*), the
// additions after "EOP" (docs/decisions/0014, points 14 and 15;
// docs/stage-3-plan.md, F4), without a window:
// - no re-saved baseline project carries an appendix, and each saves the
//   same bytes again after a reload;
// - a project with additions: the bytes up to "EOP" are those of the same
//   project without them, the appendix is laid out as specified, the JSON is
//   the same for the same additions whatever order they were set in, and
//   save, load, save gives the same bytes; the exports do not change;
// - every kind of damage: what the load does and says;
// - unknown keys survive the deletion of other tracks;
// - undo and redo of an addition and of an edit beside one; the state shares
//   unchanged bytes; a loaded track holds its additions when the reader
//   tells the client it was loaded (where the windows start its history);
// - the text form carries the additions with an exact round trip.
//
//   appendix <repository folder>

#include "../../core/track.h"
#include "../../core/trackstate.h"
#include "../../core/trackhistory.h"
#include "../../core/command.h"
#include "../../core/projectfile.h"
#include "../../core/projecttext.h"
#include "../../core/elementfile.h"
#include "../../core/exportfuncs.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace std;

static bool allOk = true;
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
    if(!ok) allOk = false;
    printf("%s: %s\n", ok ? "ok" : "FAIL", label.toUtf8().constData());
    return ok;
}

// The reader's client, as cli/project.* is: owns the tracks and keeps the
// project's own additions. Records whether each track already held additions
// when it was reported loaded.
class testProject : public projectFileClient
{
public:
    ~testProject() { clear(); }

    QList<track*> tracks;
    QString texPath;
    QJsonObject additions;
    QList<QJsonObject> additionsWhenLoaded;
    bool textureOk;

    testProject() : textureOk(true) {}

    void clear()
    {
        while(!tracks.isEmpty()) delete tracks.takeFirst();
    }

    QStringList droppedReasons;     // each projectAdditionsDropped() heard

    void projectCleared() { clear(); additions = QJsonObject(); additionsWhenLoaded.clear(); droppedReasons.clear(); }
    void projectAdditionsDropped(const QString& reason) { droppedReasons.append(reason); }
    bool projectGroundTexture(const QString& path) { texPath = path; return textureOk; }
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
    void projectTrackLoaded(track* t) { additionsWhenLoaded.append(t->additions); }
    void projectLoadFinished() {}
    void projectTrackSaving(track*) {}
    QJsonObject projectAdditions() { return additions; }
    void projectAdditionsRead(const QJsonObject& read) { additions = read; }
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

// writeProject() into memory
static QByteArray saved(testProject& p)
{
    std::stringbuf buf(ios::in | ios::out | ios::binary);
    std::iostream io(&buf);
    writeProject(io, p.texPath, p.tracks, &p);
    std::string s = buf.str();
    return QByteArray(s.data(), (int)s.size());
}

// readProject() from memory
static QString load(const QByteArray& bytes, testProject& p)
{
    std::stringbuf buf(std::string(bytes.constData(), bytes.size()), ios::in | ios::binary);
    std::iostream io(&buf);
    return readProject(io, &p);
}

static QString baseline(const char* name)
{
    return QDir(repo).filePath(QString("samples/baseline/") + name);
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

// An int as writeBytes() writes one (big-endian)
static QByteArray intBytes(int value)
{
    std::stringbuf buf(ios::in | ios::out | ios::binary);
    std::iostream io(&buf);
    writeBytes(&io, (const char*)&value, sizeof(int));
    std::string s = buf.str();
    return QByteArray(s.data(), (int)s.size());
}

static int intFrom(const QByteArray& bytes, int at)
{
    std::stringbuf buf(std::string(bytes.constData() + at, 4), ios::in | ios::binary);
    std::iostream io(&buf);
    return readInt(&io);
}

// An appendix as the format lays it out, with the parts given.
static QByteArray appendix(int version, const QByteArray& document, int length)
{
    return QByteArray("FVX") + intBytes(version) + intBytes(length) + document + QByteArray("EOX");
}
static QByteArray appendix(const QByteArray& document)
{
    return appendix(APPENDIX_VERSION, document, document.size());
}

static QJsonObject trackAdditionsA()
{
    QJsonObject inner;
    inner["b"] = QJsonArray() << 1 << 2.5 << QString("x");
    inner["a"] = QJsonValue();
    QJsonObject o;
    o["zeta"] = 1;
    o["futureFeature"] = inner;
    o["note"] = QString::fromUtf8("Br\xc3\xbc" "cke, 0.1 + 0.2");
    o["precise"] = 0.1 + 0.2;
    return o;
}

// The same keys and values, set in another order.
static QJsonObject trackAdditionsAReordered()
{
    QJsonObject o;
    o["precise"] = 0.1 + 0.2;
    o["note"] = QString::fromUtf8("Br\xc3\xbc" "cke, 0.1 + 0.2");
    QJsonObject inner;
    inner["a"] = QJsonValue();
    inner["b"] = QJsonArray() << 1 << 2.5 << QString("x");
    o["futureFeature"] = inner;
    o["zeta"] = 1;
    return o;
}

static QJsonObject projectAdditionsA()
{
    QJsonObject o;
    o["somethingNew"] = QJsonObject{{"level", 3}};
    return o;
}

// ---------------------------------------------------------------------------

// No re-saved baseline project carries an appendix, and save, load, save
// gives the same bytes.
static void checkBaseline()
{
    QDir dir(QDir(repo).filePath("samples/baseline"));
    QStringList files = dir.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    int count = 0;
    for(int i = 0; i < files.size(); ++i) {
        if(files[i].contains(".2016.")) continue;   // the 2016 binary's own re-saves
        ++count;
        QString name = files[i];
        testProject p;
        QString result = loadProjectFile(dir.filePath(name), &p);
        bool noAdditions = p.additions.isEmpty();
        for(int t = 0; t < p.tracks.size(); ++t) noAdditions = noAdditions && p.tracks[t]->additions.isEmpty();
        QString out = QDir(tmpPath).filePath("resaved.fvd");
        saveProjectFile(out, p.texPath, p.tracks, &p);
        QByteArray first = readFile(out);
        testProject q;
        QString again = loadProjectFile(out, &q);
        saveProjectFile(out, q.texPath, q.tracks, &q);
        QByteArray second = readFile(out);
        check(result == "Load Successfull." && again == "Load Successfull." && noAdditions
              && p.droppedReasons.isEmpty() && q.droppedReasons.isEmpty()
              && !first.contains("FVX") && first.endsWith("EOP") && first == second,
              QString("baseline %1: loads without additions; re-saved with no FVX, ending in EOP; "
                      "save, load, save gives the same %2 bytes").arg(name).arg(first.size()));
    }
    check(count >= 8, QString("baseline: %1 projects checked").arg(count));
}

// A project with additions: layout, prefix, determinism, round trip, exports.
static void checkWithAdditions()
{
    testProject plain;
    loadProjectFile(baseline("four-sections.fvd"), &plain);
    QByteArray without = saved(plain);
    QList<QByteArray> plainExports = exportsOf(plain.tracks[0]);

    testProject p;
    loadProjectFile(baseline("four-sections.fvd"), &p);
    p.tracks[0]->additions = trackAdditionsA();
    p.additions = projectAdditionsA();
    QByteArray with = saved(p);

    check(with.size() > without.size() && with.left(without.size()) == without,
          QString("the bytes up to EOP (%1) equal the same project without additions").arg(without.size()));
    QByteArray tail = with.mid(without.size());
    bool layout = tail.startsWith("FVX") && tail.endsWith("EOX") && tail.size() >= 14
            && intFrom(tail, 3) == 1 && intFrom(tail, 7) == tail.size() - 14;
    check(layout, QString("after EOP: FVX, version 1, the length %1, the document, EOX")
          .arg(tail.size() >= 11 ? intFrom(tail, 7) : -1));
    QByteArray document = tail.mid(11, tail.size() - 14);
    QJsonObject expectedRoot;
    expectedRoot["needs"] = QJsonArray();
    expectedRoot["project"] = projectAdditionsA();
    expectedRoot["tracks"] = QJsonArray() << trackAdditionsA();
    check(document == QJsonDocument(expectedRoot).toJson(QJsonDocument::Compact) && !document.contains('\n')
          && document.startsWith("{\"needs\":[],\"project\":"),
          QString("the document is compact JSON: %1").arg(QString::fromUtf8(document)));

    testProject r;
    loadProjectFile(baseline("four-sections.fvd"), &r);
    r.additions = projectAdditionsA();
    r.tracks[0]->additions = trackAdditionsAReordered();
    check(saved(r) == with, "the same additions set in another order give the same bytes");

    // through a real file, as the program does
    QString file = QDir(tmpPath).filePath("with.fvd");
    writeFile(file, with);
    testProject q;
    QString result = loadProjectFile(file, &q);
    check(result == "Load Successfull." && q.tracks.size() == 1 && q.droppedReasons.isEmpty(),
          QString("a project with an appendix loads, and the client hears of nothing dropped: %1").arg(result));
    check(q.tracks.size() == 1 && q.tracks[0]->additions == trackAdditionsA() && q.additions == projectAdditionsA(),
          "the track's and the project's additions come back as saved, unknown keys included");
    QString file2 = QDir(tmpPath).filePath("with2.fvd");
    saveProjectFile(file2, q.texPath, q.tracks, &q);
    check(readFile(file2) == with, "save, load, save gives the same bytes");
    check(q.tracks.size() == 1 && exportsOf(q.tracks[0]) == plainExports,
          "the three exports equal those of the same project without additions");

    // the project's additions alone also make an appendix; none at all, none
    testProject s;
    loadProjectFile(baseline("four-sections.fvd"), &s);
    s.additions = projectAdditionsA();
    QByteArray onlyProject = saved(s);
    check(onlyProject.left(without.size()) == without && onlyProject.mid(without.size()).startsWith("FVX"),
          "the project's own additions alone are written in an appendix");
    s.additions = QJsonObject();
    check(saved(s) == without, "with every addition removed the project is written as before, without an appendix");
}

struct damage
{
    QString name;
    QByteArray tail;        // what follows the main "EOP"
    bool refused;           // else dropped
    QString says;           // a part of the message
};

// Every kind of damage: the tracks load and the additions are dropped (a
// warning says so), or the load is refused (an unknown name under "needs").
static void checkDamage()
{
    testProject plain;
    loadProjectFile(baseline("four-sections.fvd"), &plain);
    QByteArray without = saved(plain);
    plain.tracks[0]->additions = trackAdditionsA();
    QByteArray good = saved(plain).mid(without.size());
    QByteArray doc = good.mid(11, good.size() - 14);
    const int len = doc.size();

    QList<damage> cases;
    cases << damage{"bytes after EOP that are not an appendix", QByteArray("junk"), false, "are not an appendix"};
    cases << damage{"cut off inside the tag", QByteArray("FV"), false, "cut off inside its tag"};
    cases << damage{"cut off inside the version", QByteArray("FVX") + intBytes(1).left(2), false, "cut off inside its version"};
    cases << damage{"cut off inside the length", QByteArray("FVX") + intBytes(1) + intBytes(len).left(3), false, "cut off inside its length"};
    cases << damage{"cut off inside the JSON", good.left(11 + len/2), false, "cut off inside its document"};
    cases << damage{"cut off inside EOX", good.left(good.size() - 1), false, "cut off inside its end mark"};
    cases << damage{"a missing EOX", good.left(good.size() - 3), false, "no end mark EOX"};
    cases << damage{"a length that lies (too long)", appendix(1, doc, len + 5), false, "cut off inside its document"};
    cases << damage{"a length that lies (too short)", appendix(1, doc, len - 1), false, "EOX is not where its length"};
    cases << damage{"a negative length", appendix(1, doc, -1), false, "negative length"};
    cases << damage{"bytes after EOX", good + QByteArray("x"), false, "follow the appendix's end mark"};
    cases << damage{"JSON that is not JSON", appendix(QByteArray("{\"needs\":[],")), false, "not valid JSON"};
    cases << damage{"JSON that is not an object", appendix(QByteArray("[1,2]")), false, "not a JSON object"};
    cases << damage{"a wrong tracks count", appendix(QByteArray("{\"needs\":[],\"project\":{},\"tracks\":[{},{}]}")), false,
                    "additions for 2 track(s), the project has 1"};
    cases << damage{"a track entry that is not an object", appendix(QByteArray("{\"needs\":[],\"project\":{},\"tracks\":[3]}")), false,
                    "entry for track 1 is not an object"};
    cases << damage{"no needs list", appendix(QByteArray("{\"project\":{},\"tracks\":[{}]}")), false, "no list \"needs\""};
    cases << damage{"a needs entry that is not a name", appendix(QByteArray("{\"needs\":[1],\"project\":{},\"tracks\":[{}]}")), false,
                    "not a name"};
    cases << damage{"no project object", appendix(QByteArray("{\"needs\":[],\"project\":[],\"tracks\":[{}]}")), false,
                    "no object \"project\""};
    cases << damage{"no tracks list", appendix(QByteArray("{\"needs\":[],\"project\":{}}")), false, "no list \"tracks\""};
    cases << damage{"an unknown key at the top", appendix(QByteArray("{\"needs\":[],\"project\":{},\"tracks\":[{}],\"extra\":1}")), false,
                    "unknown key \"extra\""};
    cases << damage{"appendix version 2", appendix(2, doc, len), false, "saved by a newer version of FVD++ (appendix version 2"};
    cases << damage{"an unknown key named \"ErrorLog\" at the top",
                    appendix(QByteArray("{\"ErrorLog\":1,\"needs\":[],\"project\":{},\"tracks\":[{}]}")), false,
                    "unknown key \"ErrorLog\""};
    cases << damage{"appendix version 0", appendix(0, doc, len), false, "version 0 is not a valid version"};
    cases << damage{"an unknown needs name", appendix(QByteArray("{\"needs\":[\"designForCar\"],\"project\":{},\"tracks\":[{\"train\":{}}]}")),
                    true, "needs \"designForCar\""};
    cases << damage{"an unknown needs name beside other damage",
                    appendix(QByteArray("{\"needs\":[\"designForCar\"],\"tracks\":[{},{}]}")), true, "needs \"designForCar\""};

    for(int i = 0; i < cases.size(); ++i) {
        const damage& d = cases[i];
        testProject p;
        QString result = load(without + d.tail, p);
        if(d.refused) {
            check(result.startsWith("Error") && result.contains(d.says) && p.tracks.isEmpty(),
                  QString("damage, %1: the load is refused, no track is kept: %2").arg(d.name, result));
        } else {
            bool none = p.additions.isEmpty();
            for(int t = 0; t < p.tracks.size(); ++t) none = none && p.tracks[t]->additions.isEmpty();
            check(result.startsWith("Warning: ") && result.contains(d.says)
                  && result.contains("Saving will ask for a new file name, so that the original file keeps its additions.")
                  && p.tracks.size() == 1 && none && saved(p) == without,
                  QString("damage, %1: the track loads as saved, the additions are dropped: %2").arg(d.name, result));
            check(p.droppedReasons.size() == 1 && p.droppedReasons[0].contains(d.says) && result.contains(p.droppedReasons[0]),
                  QString("damage, %1: the client hears that the additions were dropped, with the reason").arg(d.name));
        }
    }

    // a damaged appendix beside another warning: both are said
    testProject p;
    p.textureOk = false;
    QString result = load(without + QByteArray("FV"), p);
    check(result.startsWith("Warning: Could Not find corresponding texture.") && result.contains("could not be read")
          && p.tracks.size() == 1 && p.droppedReasons.size() == 1,
          QString("damage beside a missing texture: one warning says both: %1").arg(result));
}

// Unknown keys stay with their track when another track is deleted.
static void checkTrackDeletion()
{
    testProject p;
    loadProjectFile(baseline("gen-two-tracks.fvd"), &p);
    testProject other;
    loadProjectFile(baseline("four-sections.fvd"), &other);
    p.tracks.append(other.tracks.takeFirst());
    if(!check(p.tracks.size() == 3, "deletion: a project of three tracks")) return;

    QJsonObject a{{"a", 1}, {"futureFeature", QJsonObject{{"x", QJsonArray() << 1 << 2}}}};
    QJsonObject c{{"c", QString("three")}, {"unknownKey", true}};
    p.tracks[0]->additions = a;
    p.tracks[2]->additions = c;
    p.additions = QJsonObject{{"projectKey", 7}};

    delete p.tracks.takeAt(1);
    QByteArray bytes = saved(p);
    testProject q;
    QString result = load(bytes, q);
    check(result == "Load Successfull." && q.tracks.size() == 2 && q.tracks[0]->additions == a && q.tracks[1]->additions == c
          && q.additions == p.additions,
          "deletion: with the middle track deleted, the first and the last keep their own keys");

    delete q.tracks.takeAt(0);
    testProject r;
    load(saved(q), r);
    check(r.tracks.size() == 1 && r.tracks[0]->additions == c && r.additions == p.additions,
          "deletion: with the first track deleted too, the last keeps its unknown keys");

    // deleting the only tracks with additions leaves no appendix when the
    // project has none of its own
    testProject s;
    loadProjectFile(baseline("gen-two-tracks.fvd"), &s);
    QByteArray plain = saved(s);
    s.tracks[1]->additions = c;
    delete s.tracks.takeAt(1);
    testProject t;
    loadProjectFile(baseline("gen-two-tracks.fvd"), &t);
    delete t.tracks.takeAt(1);
    check(saved(s) == saved(t) && !saved(s).contains("FVX") && plain.contains("FVX") == false,
          "deletion: once the only track with additions is deleted, no appendix is written");
}

// Undo and redo of an addition, and of an edit beside unknown keys; the state
// shares unchanged bytes; a loaded track holds its additions when reported loaded.
static void checkUndo()
{
    testProject p;
    loadProjectFile(baseline("four-sections.fvd"), &p);
    p.tracks[0]->additions = QJsonObject{{"unknownKey", QString("kept")}};
    QString file = QDir(tmpPath).filePath("undo.fvd");
    saveProjectFile(file, p.texPath, p.tracks, &p);

    testProject q;
    loadProjectFile(file, &q);
    check(q.additionsWhenLoaded.size() == 1 && q.additionsWhenLoaded[0] == p.tracks[0]->additions,
          "a loaded track already holds its additions when the reader reports it loaded");
    track* t = q.tracks[0];
    QByteArray savedBefore = saved(q);
    trackHistory history(t);   // the head state, as the windows take it after a load

    QString error;
    bool ran = commandRegistry::run(&history, "track.setFriction", commandTarget::forTrack(0), commandArgs{{"friction", 0.2}}, &error);
    check(ran && t->additions == p.tracks[0]->additions, "an edit keeps the unknown keys");
    history.undo();
    check(t->additions == p.tracks[0]->additions && saved(q) == savedBefore, "its undo keeps them and gives the saved bytes back");
    history.redo();
    check(t->additions == p.tracks[0]->additions, "its redo keeps them");
    history.undo();

    QJsonObject withTrain = t->additions;
    withTrain["train"] = QJsonObject{{"cars", 3}};
    trackState before = captureTrackState(t);
    history.beginStep("Set addition");
    t->additions["train"] = QJsonObject{{"cars", 3}};
    history.endStep(true);
    check(history.canUndo() && history.steps().size() >= 1 && history.steps().last().label == "Set addition",
          "an addition made inside a step is recorded as one undo step");
    trackState after = captureTrackState(t, &before);
    check(after.header.constData() == before.header.constData() && after.smoothing.constData() == before.smoothing.constData()
          && after.sections.size() == before.sections.size() && after.additions != before.additions,
          "the state after an addition shares the header and the smoothing with the state before");
    bool sectionsShared = true;
    for(int i = 0; i < after.sections.size() && i < before.sections.size(); ++i) {
        sectionsShared = sectionsShared && after.sections[i].constData() == before.sections[i].constData();
    }
    check(sectionsShared, QString("... and all %1 sections' bytes").arg(after.sections.size()));

    history.undo();
    check(t->additions == p.tracks[0]->additions && saved(q) == savedBefore, "undo of the addition takes it away again");
    history.redo();
    check(t->additions == withTrain, "redo of the addition puts it back");
    QByteArray withTrainBytes = saved(q);
    testProject plain;
    loadProjectFile(baseline("four-sections.fvd"), &plain);
    QByteArray without = saved(plain);
    check(withTrainBytes.left(without.size()) == without && withTrainBytes.mid(without.size()).startsWith("FVX")
          && withTrainBytes.contains("\"train\":{\"cars\":3}"),
          "the saved file then holds the addition, the bytes up to EOP those of the project without additions");

    // an edit after the addition shares the additions' bytes
    trackState s1 = captureTrackState(t);
    commandRegistry::run(&history, "track.setFriction", commandTarget::forTrack(0), commandArgs{{"friction", 0.25}}, &error);
    trackState s2 = captureTrackState(t, &s1);
    check(!s2.additions.isEmpty() && s2.additions.constData() == s1.additions.constData() && s2.header != s1.header,
          "an edit that leaves the additions alone shares their bytes with the state before");

    // a restore puts the additions back whatever the track holds now
    t->additions = QJsonObject{{"other", 1}};
    restoreTrackState(t, before);
    check(t->additions == p.tracks[0]->additions, "a restore puts the captured additions back");
    trackState empty;
    check(empty.additions.isEmpty() && additionsBytes(QJsonObject()).isEmpty() && additionsFromBytes(QByteArray()).isEmpty(),
          "no additions are no bytes");
}

// The text form carries the additions with an exact round trip.
static void checkText()
{
    testProject plain;
    loadProjectFile(baseline("four-sections.fvd"), &plain);
    QByteArray plainText;
    writeProjectText(plainText, plain.texPath, plain.tracks, &plain);
    check(!plainText.contains("\"additions\""), "the text of a project without additions has no key \"additions\"");

    testProject p;
    loadProjectFile(baseline("four-sections.fvd"), &p);
    p.tracks[0]->additions = trackAdditionsA();
    p.additions = projectAdditionsA();
    QByteArray binary = saved(p);
    QByteArray t1;
    QString w = writeProjectText(t1, p.texPath, p.tracks, &p);
    check(w == "Project Saved!" && t1.contains("\"additions\""), QString("a project with additions is written as text: %1").arg(w));

    testProject q;
    QString r = readProjectText(t1, &q);
    check(r == "Load Successful." && q.tracks.size() == 1 && q.tracks[0]->additions == trackAdditionsA()
          && q.additions == projectAdditionsA(), QString("its text reads back with the same additions: %1").arg(r));
    QByteArray t2;
    writeProjectText(t2, q.texPath, q.tracks, &q);
    check(t2 == t1, "text to model to text gives the same text");
    check(q.tracks.size() == 1 && saved(q) == binary, "binary to text to binary gives the same bytes, appendix included");

    // a wrong type under "additions" is refused with the place named
    QByteArray bad = plainText;
    bad.replace("\"anchor\": {", "\"additions\": 5,\n            \"anchor\": {");
    testProject b;
    QString refused = readProjectText(bad, &b);
    check(refused.startsWith("Error") && refused.contains("track 1") && refused.contains("additions"),
          QString("\"additions\" that is not an object is refused: %1").arg(refused));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);
    if(argc < 2) {
        printf("usage: appendix <repository folder>\nFAIL\n");
        return 1;
    }
    repo = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir tmp;
    tmpPath = tmp.path();

    checkBaseline();
    checkWithAdditions();
    checkDamage();
    checkTrackDeletion();
    checkUndo();
    checkText();

    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
