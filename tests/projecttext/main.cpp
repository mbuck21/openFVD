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

// Test for core/projecttext.*, the project as text (docs/project-text-format.md).
//
//   projecttext <repository folder> [<folder to keep the texts in>]
//
// For every saved project in samples/baseline/ (not the .2016.* files),
// samples/import/ and samples/investigations/: load the binary file A, save it
// again (B1), write it as text (T1), read T1 into a fresh project, save that
// (B2) and write it as text again (T2). B2 must equal B1 byte for byte and T2
// must equal T1. Then: floats survive the text exactly; an unknown key is
// accepted with a warning; a wrong type, a missing key, an unknown section
// type and a freeform transition are refused with a message naming the
// place, without touching the project; a cut-off text never crashes.
// One line per check, then PASS or FAIL.

#include "../../core/track.h"
#include "../../core/projectfile.h"
#include "../../core/projecttext.h"
#include "../../core/exportfuncs.h"
#include "../../lenassert.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QTemporaryDir>
#include <QtDebug>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <sstream>

using namespace std;

static int failures = 0;
static QString keepFolder;   // the optional second argument: where to keep the texts

static void check(bool ok, const QString& what)
{
    printf("%s %s\n", ok ? "OK  " : "FAIL", what.toLocal8Bit().constData());
    fflush(stdout);
    if(!ok) ++failures;
}

// The model logs thousands of lines and a harmless warning per project; only
// a failed model check (lenAssert, qCritical in a release build) is shown.
static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(type == QtCriticalMsg || type == QtFatalMsg) {
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
}

// Copied from tests/coreonly/main.cpp (class coreProject): what the windowed
// program's projectWidget does for the file reader, without the windows. It
// also counts the calls, so a refused text can be shown to leave it alone.
class coreProject : public projectFileClient
{
public:
    coreProject() : texPath(":/background.png"), calls(0) {}
    ~coreProject() { clear(); }

    QList<track*> tracks;
    QString texPath;
    int calls;

    void clear()
    {
        while(!tracks.isEmpty()) {
            delete tracks.takeFirst();
        }
    }

    void projectCleared() { ++calls; clear(); }

    bool projectGroundTexture(const QString& path)
    {
        ++calls;
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
        ++calls;
        track* t = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
        t->name = QString("unnamed Track");
        t->display.colors[0] = QColor(20, 20, 130);
        t->display.colors[1] = QColor(255, 51, 51);
        t->display.colors[2] = QColor(51, 255, 51);
        tracks.append(t);
        return t;
    }

    void projectTrackSmoothingUpdated(track*) { ++calls; }
    void projectTrackLoaded(track*) { ++calls; }
    void projectLoadFinished() { ++calls; }
    void projectTrackSaving(track*) {}

private:
    // the image resources of resources.qrc
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

static QByteArray readAll(const QString& path)
{
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return QByteArray();
    return f.readAll();
}

// The binary file of a project, through a file as the program writes it.
static QByteArray binaryOf(coreProject& p, const QString& tmpPath)
{
    QString r = saveProjectFile(tmpPath, p.texPath, p.tracks, &p);
    if(r.startsWith("Error")) return QByteArray();
    return readAll(tmpPath);
}

static QString firstDifference(const QByteArray& a, const QByteArray& b)
{
    int n = qMin(a.size(), b.size());
    for(int i = 0; i < n; ++i) {
        if(a[i] != b[i]) return QString("first difference at byte %1").arg(i);
    }
    return QString("sizes %1 and %2").arg(a.size()).arg(b.size());
}

static QString firstLineDifference(const QByteArray& a, const QByteArray& b)
{
    QList<QByteArray> la = a.split('\n'), lb = b.split('\n');
    int n = qMin(la.size(), lb.size());
    for(int i = 0; i < n; ++i) {
        if(la[i] != lb[i]) {
            return QString("line %1: \"%2\" / \"%3\"").arg(i+1)
                    .arg(QString::fromUtf8(la[i].trimmed())).arg(QString::fromUtf8(lb[i].trimmed()));
        }
    }
    return QString("%1 and %2 lines").arg(la.size()).arg(lb.size());
}

// ---------------------------------------------------------------------------

static bool sameBits(float a, float b)
{
    if(a != a && b != b) return true;   // NaN stays NaN; its payload is not kept
    return memcmp(&a, &b, sizeof(float)) == 0;
}

static void checkFloats()
{
    float nextAfterOne = nextafterf(1.f, 2.f);
    float values[] = { 0.03f, 0.1f, 0.2f, 0.3f, 1.f/3.f, 2.f/3.f, 9.80665f, 1.1f, 2e-5f, 1e-7f,
                       FLT_MIN, FLT_MAX, -FLT_MAX, 1.4e-45f /* smallest denormal */, 1e-40f,
                       16777216.f, 16777215.f, nextAfterOne, 123456.789f, -0.f, 0.f, -1.f, 1.f,
                       3.14159265f, 360.f, -179.99998f, 0.99999994f,
                       std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN() };
    const int n = sizeof(values)/sizeof(values[0]);

    QJsonArray a;
    for(int i = 0; i < n; ++i) a.append(projectText::floatToJson(values[i]));
    QJsonObject o;
    o["values"] = a;
    QByteArray text = QJsonDocument(o).toJson(QJsonDocument::Compact);
    QJsonArray back = QJsonDocument::fromJson(text).object().value("values").toArray();

    int bad = 0;
    for(int i = 0; i < n; ++i) {
        float f = 0.f;
        if(i >= back.size() || !projectText::floatFromJson(back[i], &f) || !sameBits(f, values[i])) {
            ++bad;
            printf("     %.9g did not survive (%s)\n", values[i], text.constData());
        }
    }
    check(bad == 0, QString("floats: %1 awkward values (0.03, 1/3, the smallest denormal, FLT_MAX, -0, "
                            "infinity, NaN, ...) read back to the same bits").arg(n));
    check(text.startsWith("{\"values\":[0.03,0.1,0.2,0.3,"),
          QString("floats: written in their shortest form: %1...").arg(QString(text.left(40))));
}

// ---------------------------------------------------------------------------

struct roundTrip {
    bool ok;
    QByteArray b1, t1;
};

static roundTrip checkProject(const QString& path, const QString& tmp)
{
    roundTrip r;
    r.ok = false;
    QString name = QFileInfo(path).fileName();

    coreProject a;
    QString status = loadProjectFile(path, &a);
    if(status.startsWith("Error")) {
        check(false, QString("%1: binary load: %2").arg(name, status));
        return r;
    }
    QByteArray fileA = readAll(path);
    r.b1 = binaryOf(a, tmp + "/b1.fvd");
    QString w1 = writeProjectText(r.t1, a.texPath, a.tracks, &a);
    if(!w1.startsWith("Project Saved")) {
        check(false, QString("%1: write text: %2").arg(name, w1));
        return r;
    }

    coreProject b;
    QString s2 = readProjectText(r.t1, &b);
    bool readOk = s2 == QString("Load Successful.") || (status.startsWith("Warning") && s2.startsWith("Warning"));
    QByteArray b2 = binaryOf(b, tmp + "/b2.fvd");
    QByteArray t2;
    writeProjectText(t2, b.texPath, b.tracks, &b);

    int sections = 0;
    for(int i = 0; i < a.tracks.size(); ++i) sections += a.tracks[i]->lSections.size();

    check(readOk, QString("%1: text read back: %2").arg(name, s2));
    check(!r.b1.isEmpty() && b2 == r.b1,
          QString("%1: binary -> text -> binary is byte-identical (%2 tracks, %3 sections, %4 bytes)%5")
          .arg(name).arg(a.tracks.size()).arg(sections).arg(r.b1.size())
          .arg(b2 == r.b1 ? QString() : QString(": ") + firstDifference(r.b1, b2)));
    check(t2 == r.t1, QString("%1: text -> model -> text is identical (%2 bytes)%3")
          .arg(name).arg(r.t1.size()).arg(t2 == r.t1 ? QString() : QString(": ") + firstLineDifference(r.t1, t2)));
    printf("     info: %s: the re-saved binary %s the file on disk\n", name.toLocal8Bit().constData(),
           fileA == r.b1 ? "equals" : "differs from");
    if(!keepFolder.isEmpty()) {
        QFile f1(keepFolder + "/" + name + ".json");
        if(f1.open(QIODevice::WriteOnly)) f1.write(r.t1);
        QFile f2(keepFolder + "/" + name + ".again.json");
        if(f2.open(QIODevice::WriteOnly)) f2.write(t2);
    }
    r.ok = readOk && b2 == r.b1 && t2 == r.t1;
    return r;
}

// ---------------------------------------------------------------------------
// Editing a text for the negative checks

static QByteArray editTrack(const QByteArray& text, std::function<void(QJsonObject&)> change)
{
    QJsonObject root = QJsonDocument::fromJson(text).object();
    QJsonArray tracks = root["tracks"].toArray();
    QJsonObject t = tracks[0].toObject();
    change(t);
    tracks[0] = t;
    root["tracks"] = tracks;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

static QByteArray editSection(const QByteArray& text, int index, std::function<void(QJsonObject&)> change)
{
    return editTrack(text, [&](QJsonObject& t) {
        QJsonArray s = t["sections"].toArray();
        QJsonObject o = s[index].toObject();
        change(o);
        s[index] = o;
        t["sections"] = s;
    });
}

static int sectionOfType(const QByteArray& text, const char* type)
{
    QJsonArray s = QJsonDocument::fromJson(text).object()["tracks"].toArray()[0].toObject()["sections"].toArray();
    for(int i = 0; i < s.size(); ++i) {
        if(s[i].toObject()["type"].toString() == QLatin1String(type)) return i;
    }
    return -1;
}

// A text that must be refused: the message starts "Error", names the place,
// and the project was not touched.
static void expectRefused(const QByteArray& text, const QString& what, const QStringList& mustName)
{
    coreProject p;
    QString r = readProjectText(text, &p);
    bool named = true;
    for(int i = 0; i < mustName.size(); ++i) {
        if(!r.contains(mustName[i])) named = false;
    }
    check(r.startsWith("Error") && named && p.calls == 0 && p.tracks.isEmpty(),
          QString("%1 is refused: %2").arg(what, r));
}

static void checkMessages(const QByteArray& t1, const QByteArray& b1, const QString& tmp)
{
    int curve = sectionOfType(t1, "curved");
    int force = sectionOfType(t1, "force");
    if(curve < 0 || force < 0) {
        check(false, "four-sections.fvd has a curved and a force section");
        return;
    }
    QString curveName = QString("track 1, section %1").arg(curve+1);
    QString forceName = QString("track 1, section %1").arg(force+1);

    // an unknown key: a warning, and the same project
    {
        QByteArray t = editSection(t1, curve, [](QJsonObject& o) { o["colour"] = QString("red"); });
        coreProject p;
        QString r = readProjectText(t, &p);
        QByteArray b = binaryOf(p, tmp + "/unknown.fvd");
        check(r.startsWith("Warning") && r.contains(curveName) && r.contains("\"colour\"") && b == b1,
              QString("an unknown key is accepted with a warning and changes nothing: %1").arg(r));
    }

    // an edited input does change the project
    {
        QByteArray t = editSection(t1, curve, [](QJsonObject& o) { o["radius"] = 25.5; });
        coreProject p;
        QString r = readProjectText(t, &p);
        QByteArray b = binaryOf(p, tmp + "/edited.fvd");
        check(r == QString("Load Successful.") && !b.isEmpty() && b != b1 && p.tracks[0]->lSections[curve]->fRadius == 25.5f,
              QString("an edited radius reaches the model (%1)").arg(r));
    }

    // an edited recalculated value changes nothing
    {
        QByteArray t = editSection(t1, curve, [](QJsonObject& o) {
            QJsonObject rec = o["recalculatedOnLoad"].toObject();
            rec["angle"] = 12.5;
            o["recalculatedOnLoad"] = rec;
        });
        coreProject p;
        readProjectText(t, &p);
        check(binaryOf(p, tmp + "/recalc.fvd") == b1, "an edited value under recalculatedOnLoad is ignored");
    }

    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["radius"] = QString("big"); }),
                  "a wrong type", QStringList() << curveName << "\"radius\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { o["speedMode"] = true; }),
                  "a wrong type", QStringList() << forceName << "\"speedMode\"");
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject a = t["anchor"].toObject();
                      a.remove("speed");
                      t["anchor"] = a;
                  }), "a missing key", QStringList() << "track 1, anchor" << "\"speed\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { o.remove("normalForce"); }),
                  "a missing function", QStringList() << forceName << "\"normalForce\"");
    expectRefused(editSection(t1, 0, [](QJsonObject& o) { o["type"] = QString("loop"); }),
                  "an unknown section type", QStringList() << "track 1, section 1" << "\"loop\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      QJsonArray f = o["roll"].toArray();
                      QJsonObject t = f[0].toObject();
                      t["shape"] = QString("freeform");
                      f[0] = t;
                      o["roll"] = f;
                  }), "a freeform transition", QStringList() << forceName + " (force), roll, transition 1" << "freeform");
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      QJsonArray f = o["roll"].toArray();
                      QJsonObject t = f[0].toObject();
                      t["shape"] = QString("wobbly");
                      f[0] = t;
                      o["roll"] = f;
                  }), "an unknown shape", QStringList() << forceName << "\"wobbly\"");
    expectRefused(editTrack(t1, [](QJsonObject& t) { t["style"] = QString("wooden"); }),
                  "an unknown track style", QStringList() << "track 1" << "\"wooden\"");
    {
        QJsonObject root = QJsonDocument::fromJson(t1).object();
        root["formatVersion"] = 2;
        expectRefused(QJsonDocument(root).toJson(), "a newer format version", QStringList() << "formatVersion");
    }

    // cut-off texts: never a crash, always an error
    {
        int refused = 0, tried = 0;
        int step = qMax(1, t1.size()/400);
        for(int n = 0; n < t1.size()-1; n += (n < 300 ? 1 : step)) {
            coreProject p;
            QString r = readProjectText(t1.left(n), &p);
            ++tried;
            if(r.startsWith("Error") && p.calls == 0) ++refused;
        }
        check(refused == tried, QString("%1 cut-off texts are all refused without a crash (%2 refused)").arg(tried).arg(refused));
    }
    expectRefused(QByteArray(), "an empty text", QStringList() << "JSON");
    expectRefused(QByteArray("[1, 2, 3]\n"), "a list in place of the project", QStringList() << "object");
}

// a freeform transition in the model cannot be written
static void checkFreeformWrite(const QString& path)
{
    coreProject a;
    loadProjectFile(path, &a);
    int force = -1;
    for(int i = 0; i < a.tracks[0]->lSections.size(); ++i) {
        if(a.tracks[0]->lSections[i]->type == forced) { force = i; break; }
    }
    if(force < 0) {
        check(false, "four-sections.fvd has a force section");
        return;
    }
    a.tracks[0]->lSections[force]->normForce->funcList[0]->degree = freeform;
    QByteArray text;
    QString r = writeProjectText(text, a.texPath, a.tracks, &a);
    QString where = QString("track 1, section %1, normalForce, transition 1").arg(force+1);
    check(r.startsWith("Error") && r.contains(where) && text.isEmpty(),
          QString("a freeform transition in the model is not written: %1").arg(r));
}

// tozero, and several transitions in one function, made in the text
static void checkToZero(const QByteArray& t1, const QString& tmp)
{
    int force = sectionOfType(t1, "force");
    QByteArray t = editSection(t1, force, [](QJsonObject& o) {
        QJsonArray f = o["normalForce"].toArray();
        QJsonObject last = f[f.size()-1].toObject();
        double to = last["to"].toDouble();
        QJsonObject piece;
        piece["shape"] = QString("tozero");
        piece["to"] = to + 0.5;
        piece["shapeParameter"] = 0;
        piece["center"] = 0;
        piece["tension"] = 0;
        piece["locked"] = false;
        f.append(piece);
        QJsonObject cubic = piece;
        cubic["shape"] = QString("cubic");
        cubic["to"] = to + 1.25;
        cubic["change"] = 1.5;
        f.append(cubic);
        o["normalForce"] = f;
    });
    coreProject p;
    QString r = readProjectText(t, &p);
    QByteArray b = binaryOf(p, tmp + "/tozero.fvd");
    QByteArray t2;
    writeProjectText(t2, p.texPath, p.tracks, &p);

    coreProject q;
    readProjectText(t2, &q);
    QByteArray b2 = binaryOf(q, tmp + "/tozero2.fvd");
    QByteArray t3;
    writeProjectText(t3, q.texPath, q.tracks, &q);

    bool ok = r == QString("Load Successful.") && !b.isEmpty();
    subfunc* zero = NULL;
    if(ok) {
        func* nf = p.tracks[0]->lSections[force]->normForce;
        zero = nf->funcList[nf->funcList.size()-2];
        ok = zero->degree == tozero && zero->symArg == -zero->startValue && fabs(zero->endValue()) < 1e-6f;
    }
    check(ok, QString("a tozero transition written in the text ends at zero (%1)").arg(r));
    check(ok && b2 == b && t3 == t2, "a project made in the text round-trips exactly (tozero, three transitions)");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);

    if(argc < 2) {
        printf("usage: projecttext <repository folder>\nFAIL\n");
        return 1;
    }
    QDir repo(QString::fromLocal8Bit(argv[1]));
    QTemporaryDir tmpDir;
    if(!tmpDir.isValid()) {
        printf("no temporary folder\nFAIL\n");
        return 1;
    }
    QString tmp = tmpDir.path();
    if(argc > 2) keepFolder = QString::fromLocal8Bit(argv[2]);

    checkFloats();

    {
        std::stringstream ss;
        float a = 1.f, b = 2.f, c = 3.f;
        writeBytes(&ss, (const char*)&a, sizeof(float));
        writeBytes(&ss, (const char*)&b, sizeof(float));
        writeBytes(&ss, (const char*)&c, sizeof(float));
        glm::vec3 v = readVec3(&ss);
        printf("     info: readVec3 of the floats 1, 2, 3 gives (%g, %g, %g) with this compiler\n", v.x, v.y, v.z);
    }

    QStringList projects;
    QStringList baseline = QDir(repo.filePath("samples/baseline")).entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
    for(int i = 0; i < baseline.size(); ++i) {
        if(!baseline[i].contains(".2016.")) projects << repo.filePath("samples/baseline/" + baseline[i]);
    }
    QStringList others = QStringList() << "samples/import" << "samples/investigations";
    for(int k = 0; k < others.size(); ++k) {
        QStringList l = QDir(repo.filePath(others[k])).entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
        for(int i = 0; i < l.size(); ++i) projects << repo.filePath(others[k] + "/" + l[i]);
    }
    check(projects.size() >= 10, QString("%1 projects found").arg(projects.size()));

    QByteArray fourT1, fourB1;
    QString fourPath;
    bool sawBezier = false, sawCsv = false;
    for(int i = 0; i < projects.size(); ++i) {
        roundTrip r = checkProject(projects[i], tmp);
        if(projects[i].endsWith("baseline/four-sections.fvd")) {
            fourT1 = r.t1;
            fourB1 = r.b1;
            fourPath = projects[i];
        }
        if(r.t1.contains("\"type\": \"bezier\"")) sawBezier = true;
        if(r.t1.contains("\"type\": \"nolimitscsv\"")) sawCsv = true;
    }
    check(sawBezier && sawCsv, "a Bezier section and a NoLimits CSV section were among them");

    if(fourT1.isEmpty()) {
        check(false, "samples/baseline/four-sections.fvd was read");
    } else {
        checkMessages(fourT1, fourB1, tmp);
        checkFreeformWrite(fourPath);
        checkToZero(fourT1, tmp);

        // the file functions
        coreProject a;
        loadProjectFile(fourPath, &a);
        QString textPath = tmp + "/four-sections.fvd.json";
        QString s = saveProjectTextFile(textPath, a.texPath, a.tracks, &a);
        coreProject b;
        QString l = loadProjectTextFile(textPath, &b);
        check(s == QString("Project Saved!") && l == QString("Load Successful.") && readAll(textPath) == fourT1
              && binaryOf(b, tmp + "/file.fvd") == fourB1, "saveProjectTextFile and loadProjectTextFile");
        coreProject c;
        check(loadProjectTextFile(tmp + "/missing.json", &c).startsWith("Error"), "a missing text file is an error");
    }

    if(failures == 0) {
        printf("PASS\n");
        return 0;
    }
    printf("%d check(s) failed\nFAIL\n", failures);
    return 1;
}
