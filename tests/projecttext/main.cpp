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
//   projecttext <repository folder> [<folder to keep the texts in> [<folder of more projects>]]
//
// (Give "" as the second argument to keep no texts but name a third.)
//
// For every saved project in samples/baseline/ (not the .2016.* files),
// samples/import/ and samples/investigations/, and in the third argument's
// folder if one is given: load the binary file A, save it
// again (B1), write it as text (T1), read T1 into a fresh project, save that
// (B2) and write it as text again (T2). B2 must equal B1 byte for byte and T2
// must equal T1. The same from a model whose transition ends one more pass of
// func::appendSubFunction() would move (4.9999986 to 5.999999, and more
// found by a search); a later transition's "from" left out, a few last digits
// off, too far off, and in the first version's shape; a model whose gap is
// beyond the tolerance is not written. Every project and built model is
// opened and saved 20 times in a row: the gap between a transition's start
// and the end before must stay within 8 float steps and the text round trip
// must hold after every cycle. Then: floats survive the text exactly; an unknown key is
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

// Values the windows could never produce, which would crash, hang or exhaust
// memory if they reached the model, are refused before the project is touched.
static QJsonObject transition(const char* shape, double to, double change)
{
    QJsonObject t;
    t["shape"] = QString(shape);
    t["to"] = to;
    t["change"] = change;
    t["shapeParameter"] = -10;
    t["center"] = 0;
    t["tension"] = 0;
    t["locked"] = false;
    return t;
}

static void setFirst(QJsonObject& section, const char* function, const char* key, const QJsonValue& v)
{
    QJsonArray f = section[function].toArray();
    QJsonObject t = f[0].toObject();
    t[key] = v;
    f[0] = t;
    section[function] = f;
}

static QJsonArray vec(double x, double y, double z)
{
    QJsonArray a;
    a.append(x);
    a.append(y);
    a.append(z);
    return a;
}

static void checkRanges(const QByteArray& t1, const QString& tmp)
{
    int straightNo = sectionOfType(t1, "straight");
    int curve = sectionOfType(t1, "curved");
    int force = sectionOfType(t1, "force");
    QString straightName = QString("track 1, section %1").arg(straightNo+1);
    QString curveName = QString("track 1, section %1").arg(curve+1);
    QString forceName = QString("track 1, section %1").arg(force+1);

    // the case of the review: an active whole-track entry with 0 iterations
    // would divide by zero in track::applyRollSmoothFilter()
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject s = t["smoothing"].toObject();
                      QJsonObject w = s["wholeTrack"].toObject();
                      w["active"] = true;
                      w["iterations"] = 0;
                      s["wholeTrack"] = w;
                      t["smoothing"] = s;
                  }), "smoothing iterations 0 on an active entry", QStringList() << "track 1, smoothing, wholeTrack" << "\"iterations\"" << "1 to 16");
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject s = t["smoothing"].toObject();
                      QJsonObject w = s["wholeTrack"].toObject();
                      w["iterations"] = 17;
                      s["wholeTrack"] = w;
                      t["smoothing"] = s;
                  }), "smoothing iterations 17", QStringList() << "wholeTrack" << "\"iterations\"");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) {
                      QJsonObject s = o["smoothing"].toObject();
                      s["length"] = -400;
                      o["smoothing"] = s;
                  }), "a negative smoothing length", QStringList() << curveName << "smoothing" << "\"length\"" << "0 to 6000");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) {
                      QJsonObject s = o["smoothing"].toObject();
                      s["length"] = 6001;
                      o["smoothing"] = s;
                  }), "a smoothing length above 6000", QStringList() << curveName << "\"length\"");

    QJsonObject region;
    region["name"] = QString("custom Region");
    region["length"] = 400;
    region["iterations"] = 1;
    region["active"] = true;
    region["fromNode"] = 2000;
    region["toNode"] = 1000;
    expectRefused(editTrack(t1, [&](QJsonObject& t) {
                      QJsonObject s = t["smoothing"].toObject();
                      s["custom"] = QJsonArray() << region;
                      t["smoothing"] = s;
                  }), "a custom smoothing region that runs backwards", QStringList() << "track 1, smoothing, custom 1" << "\"toNode\"");
    region["fromNode"] = -5;
    expectRefused(editTrack(t1, [&](QJsonObject& t) {
                      QJsonObject s = t["smoothing"].toObject();
                      s["custom"] = QJsonArray() << region;
                      t["smoothing"] = s;
                  }), "a negative node of a custom region", QStringList() << "custom 1" << "\"fromNode\"");
    region["fromNode"] = 0;
    region["toNode"] = 2000000000;
    expectRefused(editTrack(t1, [&](QJsonObject& t) {
                      QJsonObject s = t["smoothing"].toObject();
                      s["custom"] = QJsonArray() << region;
                      t["smoothing"] = s;
                  }), "a custom region of two thousand million nodes", QStringList() << "custom 1" << "\"toNode\"");
    {
        // a region past the track's end is harmless and is accepted
        region["toNode"] = 1000000;
        QByteArray t = editTrack(t1, [&](QJsonObject& tr) {
            QJsonObject s = tr["smoothing"].toObject();
            s["custom"] = QJsonArray() << region;
            tr["smoothing"] = s;
        });
        coreProject p;
        QString r = readProjectText(t, &p);
        check(r == QString("Load Successful.") && p.tracks.size() == 1,
              QString("an active custom region reaching past the track's end loads (%1)").arg(r));
    }

    // transitions
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "roll", "to", 0); }),
                  "a transition of zero length", QStringList() << forceName + " (force), roll, transition 1" << "\"to\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "roll", "to", -1); }),
                  "a transition ending before it starts", QStringList() << forceName << "roll, transition 1");
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      QJsonArray f = o["normalForce"].toArray();
                      f.append(transition("linear", f[0].toObject()["to"].toDouble(), 0.5));
                      o["normalForce"] = f;
                  }), "a later transition of zero length", QStringList() << forceName + " (force), normalForce, transition 2" << "\"to\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "roll", "from", 1); }),
                  "a function that does not start at 0", QStringList() << forceName << "\"from\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "lateralForce", "to", 2001); }),
                  "a time-based function past 2000 s", QStringList() << forceName << "lateralForce" << "2000 s");
    expectRefused(editSection(t1, straightNo, [](QJsonObject& o) { setFirst(o, "roll", "to", 10001); }),
                  "a straight longer than 10000 m", QStringList() << straightName << "10000 m");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { setFirst(o, "roll", "to", 5001); }),
                  "a curve of more than 5000 degrees", QStringList() << curveName << "5000 degrees");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "normalForce", "center", 10.5); }),
                  "a centre outside -10 to 10", QStringList() << forceName << "\"center\"" << "-10 to 10");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "normalForce", "tension", -11); }),
                  "a tension outside -10 to 10", QStringList() << forceName << "\"tension\"");
    expectRefused(editSection(t1, straightNo, [](QJsonObject& o) { setFirst(o, "roll", "locked", true); }),
                  "a locked transition on a straight", QStringList() << straightName << "\"locked\"");
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      setFirst(o, "roll", "locked", true);
                      setFirst(o, "normalForce", "locked", true);
                      setFirst(o, "lateralForce", "locked", true);
                  }), "a force section with every function locked", QStringList() << forceName << "locked");

    // speeds
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      o["speedMode"] = QString("fixed");
                      o["speed"] = 0;
                  }), "a held speed of 0", QStringList() << forceName << "\"speed\"" << "m/s");
    expectRefused(editSection(t1, straightNo, [](QJsonObject& o) {
                      o["speedMode"] = QString("fixed");
                      o["speed"] = -3;
                  }), "a negative held speed", QStringList() << straightName << "\"speed\"");
    {
        QByteArray t = editSection(t1, force, [](QJsonObject& o) {
            o["speedMode"] = QString("physics");
            o["speed"] = 0;
        });
        coreProject p;
        QString r = readProjectText(t, &p);
        check(r == QString("Load Successful."),
              QString("a stored speed of 0 is accepted where the speed comes from the physics (%1)").arg(r));
    }
    expectRefused(editSection(t1, force, [](QJsonObject& o) {
                      o["argument"] = QString("distance");
                      o["timeNodes"] = 1000;
                      o["speedMode"] = QString("fixed");
                      o["speed"] = 0.025;
                      setFirst(o, "roll", "to", 60);
                      setFirst(o, "normalForce", "to", 60);
                      setFirst(o, "lateralForce", "to", 60);
                  }), "60 m over distance at a held 0.025 m/s", QStringList() << forceName << "nodes");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) {
                      o["speedMode"] = QString("fixed");
                      o["speed"] = 0.025;
                      o["radius"] = 500;
                  }), "a 90 degree curve of 500 m at a held 0.025 m/s", QStringList() << curveName << "nodes");
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject a = t["anchor"].toObject();
                      a["speed"] = 0;
                      t["anchor"] = a;
                  }), "an anchor speed of 0", QStringList() << "track 1, anchor" << "\"speed\"" << "0.025 to 500 m/s");

    // the curve
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["radius"] = 0; }),
                  "a curve radius of 0", QStringList() << curveName << "\"radius\"" << "0.3 to 500 m");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["radius"] = -15; }),
                  "a negative curve radius", QStringList() << curveName << "\"radius\"");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["leadIn"] = -1; }),
                  "a negative lead-in", QStringList() << curveName << "\"leadIn\"");
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["leadOut"] = 501; }),
                  "a lead-out above 500 degrees", QStringList() << curveName << "\"leadOut\"");

    // the track
    expectRefused(editTrack(t1, [](QJsonObject& t) { t["friction"] = -0.01; }),
                  "a negative friction", QStringList() << "track 1" << "\"friction\"" << "0 to 2");
    expectRefused(editTrack(t1, [](QJsonObject& t) { t["resistance"] = -1e-5; }),
                  "a negative air resistance", QStringList() << "track 1" << "\"resistance\"");
    expectRefused(editTrack(t1, [](QJsonObject& t) { t["heartline"] = 6; }),
                  "a heartline of 6 m", QStringList() << "track 1" << "\"heartline\"" << "-5 to 5 m");
    expectRefused(editTrack(t1, [](QJsonObject& t) { t["drawHeartline"] = 3; }),
                  "an unknown heartline drawing", QStringList() << "track 1" << "\"drawHeartline\"");

    // numbers that are not finite
    expectRefused(editSection(t1, curve, [](QJsonObject& o) { o["direction"] = QString("inf"); }),
                  "an infinite number", QStringList() << curveName << "\"direction\"" << "finite");
    expectRefused(editSection(t1, force, [](QJsonObject& o) { setFirst(o, "normalForce", "change", QString("nan")); }),
                  "a NaN", QStringList() << forceName << "\"change\"" << "finite");
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject a = t["anchor"].toObject();
                      a["roll"] = 1e39;   // a double, too large for a float
                      t["anchor"] = a;
                  }), "a number too large for a float", QStringList() << "track 1, anchor" << "\"roll\"" << "finite");
    expectRefused(editTrack(t1, [](QJsonObject& t) {
                      QJsonObject a = t["anchor"].toObject();
                      a["position"] = vec(0, 1e300, 0);
                      t["anchor"] = a;
                  }), "a position too large for a float", QStringList() << "track 1, anchor" << "\"position\"");

    // point lists that would need more nodes than memory holds
    {
        QJsonObject csv;
        csv["type"] = QString("nolimitscsv");
        QJsonObject p1, p2;
        p1["position"] = vec(0, 5, 0);
        p1["direction"] = vec(0, 0, -1);
        p1["lateral"] = vec(1, 0, 0);
        p2 = p1;
        p2["position"] = vec(0, 5, -300000);
        csv["points"] = QJsonArray() << p1 << p2;
        csv["smoothing"] = QJsonDocument::fromJson(t1).object()["tracks"].toArray()[0].toObject()["sections"]
                .toArray()[0].toObject()["smoothing"];
        expectRefused(editTrack(t1, [&](QJsonObject& t) {
                          QJsonArray s = t["sections"].toArray();
                          s.append(csv);
                          t["sections"] = s;
                      }), "a NoLimits CSV section of 300 km", QStringList() << "nolimitscsv" << "nodes");

        QJsonObject bez;
        bez["type"] = QString("bezier");
        bez["name"] = QString("far");
        QJsonObject b1, b2;
        b1["point"] = vec(0, 5, 0);
        b1["controlPoint1"] = vec(0, 5, 0);
        b1["controlPoint2"] = vec(0, 5, -10000);
        b1["roll"] = 0;
        b1["continuousRoll"] = false;
        b1["relativeRoll"] = false;
        b2 = b1;
        b2["point"] = vec(0, 5, -30000);
        b2["controlPoint1"] = vec(0, 5, -20000);
        b2["controlPoint2"] = vec(0, 5, -30000);
        bez["points"] = QJsonArray() << b1 << b2;
        bez["supports"] = QJsonArray();
        bez["smoothing"] = csv["smoothing"];
        expectRefused(editTrack(t1, [&](QJsonObject& t) {
                          QJsonArray s = t["sections"].toArray();
                          s.append(bez);
                          t["sections"] = s;
                      }), "a Bezier section of 30 km", QStringList() << "bezier" << "control polygon");
    }
    Q_UNUSED(tmp);
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

// ---------------------------------------------------------------------------
// Transition ends that one more pass of func::appendSubFunction() would move.
// It sets a later piece's end to (end before + end) - start, and in single
// precision that is not always the end: 4.9999986 + 5.999999 - 4.9999986 is
// 5.9999995. A text read back must still give exactly the model it came from.

static float floatOfBits(quint32 bits)
{
    float f;
    memcpy(&f, &bits, sizeof(float));
    return f;
}

// what the append does to a piece from a to b
static float appendedEnd(float a, float b)
{
    volatile float sum = a + b;
    volatile float end = sum - a;
    return end;
}

// Pairs (a, b), a < b, that the append would move, found by a fixed search.
// Kept to a section of a few seconds: on a much longer one the forces of the
// sample make the train stall, and a model full of NaN is not what is tested.
static QList<QPair<float, float> > findAwkwardEnds(int wanted)
{
    QList<QPair<float, float> > found;
    quint32 state = 12345u;
    for(int tries = 0; tries < 100000 && found.size() < wanted; ++tries) {
        state = state*1664525u + 1013904223u;
        float a = 0.5f + (float)(state >> 8)/(float)(1u << 24)*5.5f;
        state = state*1664525u + 1013904223u;
        float b = a + 0.05f + (float)(state >> 8)/(float)(1u << 24)*1.5f;
        if(b > a && appendedEnd(a, b) != b) found.append(qMakePair(a, b));
    }
    return found;
}

static int forceSectionOf(track* t)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(t->lSections[i]->type == forced) return i;
    }
    return -1;
}

// The track computed again after an edit made in code, as the windows do.
static void recompute(track* t)
{
    t->updateTrack(0, 0);
    char customChar;
    t->updateSmoothEntries(&customChar);   // the sections' node numbers moved
    t->applyRollSmooth();
}

// The normal force of the project's force section made of the pieces between
// the given ends, set exactly; the other two functions stretched to the last
// end; then the track recomputed. False if the recompute moved an end.
static bool buildEndsModel(coreProject& p, const QString& path, const QList<float>& ends)
{
    loadProjectFile(path, &p);
    if(p.tracks.isEmpty()) return false;
    track* t = p.tracks[0];
    int force = forceSectionOf(t);
    if(force < 0) return false;
    section* s = t->lSections[force];
    func* nf = s->normForce;
    const int n = ends.size() - 1;
    while(nf->funcList.size() > 1) nf->removeSubFunction(nf->funcList.size()-1);
    for(int i = 0; i < n-1; ++i) nf->appendSubFunction(1, i);
    for(int i = 0; i < n; ++i) {
        nf->funcList[i]->locked = false;
        // a new cubic piece's shape parameter is whatever the memory held
        // (subfunc's constructor leaves it); fix it so the test is repeatable
        if(i > 0) nf->funcList[i]->arg1 = 0.f;
        nf->funcList[i]->minArgument = ends[i];
        nf->funcList[i]->maxArgument = ends[i+1];
    }
    func* others[2] = { s->rollFunc, s->latForce };
    for(int k = 0; k < 2; ++k) {
        subfunc* last = others[k]->funcList.last();
        if(!last->locked && last->minArgument < ends.last()) last->maxArgument = ends.last();
    }
    recompute(t);
    for(int i = 0; i < n; ++i) {
        if(nf->funcList[i]->minArgument != ends[i] || nf->funcList[i]->maxArgument != ends[i+1]) return false;
    }
    return true;
}

// Four pieces 0..a/2, a/2..a, a..b, b..b+1.
static bool buildAwkwardModel(coreProject& p, const QString& path, float a, float b)
{
    return buildEndsModel(p, path, QList<float>() << 0.f << a*0.5f << a << b << b + 1.f);
}

static float stepsDown(float f, int steps)
{
    for(int k = 0; k < steps; ++k) f = nextafterf(f, -1e30f);
    return f;
}

// The functions the text holds, of every track and section.
static QList<func*> functionsOf(coreProject& p)
{
    QList<func*> l;
    for(int ti = 0; ti < p.tracks.size(); ++ti) {
        for(int si = 0; si < p.tracks[ti]->lSections.size(); ++si) {
            section* s = p.tracks[ti]->lSections[si];
            if(s->type == straight || s->type == curved) l << s->rollFunc;
            if(s->type == forced || s->type == geometric) l << s->rollFunc << s->normForce << s->latForce;
        }
    }
    return l;
}

// The largest gap, in float steps, between a transition's start and the end
// of the one before.
static qint64 largestGap(coreProject& p)
{
    qint64 gap = 0;
    QList<func*> l = functionsOf(p);
    for(int k = 0; k < l.size(); ++k) {
        for(int i = 1; i < l[k]->funcList.size(); ++i) {
            gap = qMax(gap, projectText::floatSteps(l[k]->funcList[i]->minArgument, l[k]->funcList[i-1]->maxArgument));
        }
    }
    return gap;
}

static int bytesChanged(const QByteArray& a, const QByteArray& b)
{
    int n = qAbs(a.size() - b.size());
    for(int i = 0; i < qMin(a.size(), b.size()); ++i) {
        if(a[i] != b[i]) ++n;
    }
    return n;
}

static qint64 observedLargestGap = 0;   // over every cycle of every project
static const int CYCLES = 20;
static const qint64 GAP_BOUND = 8;

// Open and save the binary file CYCLES times in a row through the model.
// After each opening: the bytes the save changed, the largest gap, and the
// text round trip (binary -> text -> binary identical, text -> text identical).
static void checkCycles(const QString& name, const QString& path, const QString& tmp)
{
    QString cur = tmp + "/cycle.fvd";
    QFile::remove(cur);
    if(!QFile::copy(path, cur)) {
        check(false, QString("%1: copied for the cycles").arg(name));
        return;
    }
    QByteArray before = readAll(cur);
    QStringList changes;
    qint64 gapMax = 0;
    int badTrips = 0;
    for(int c = 1; c <= CYCLES; ++c) {
        coreProject p;
        loadProjectFile(cur, &p);
        gapMax = qMax(gapMax, largestGap(p));
        QByteArray b = binaryOf(p, cur);
        changes << QString::number(bytesChanged(before, b));
        before = b;

        QByteArray t1, t2;
        QString w = writeProjectText(t1, p.texPath, p.tracks, &p);
        coreProject q;
        QString r = readProjectText(t1, &q);
        QByteArray b2 = binaryOf(q, tmp + "/cycle-text.fvd");
        writeProjectText(t2, q.texPath, q.tracks, &q);
        if(!w.startsWith("Project Saved") || r.startsWith("Error") || b.isEmpty() || b2 != b || t2 != t1) {
            if(badTrips == 0 && !keepFolder.isEmpty()) {
                QFile f1(keepFolder + "/" + name + QString(".cycle%1.json").arg(c));
                if(f1.open(QIODevice::WriteOnly)) f1.write(t1);
                QFile f2(keepFolder + "/" + name + QString(".cycle%1.again.json").arg(c));
                if(f2.open(QIODevice::WriteOnly)) f2.write(t2);
            }
            ++badTrips;
        }
    }
    observedLargestGap = qMax(observedLargestGap, gapMax);
    check(gapMax <= GAP_BOUND && badTrips == 0,
          QString("%1: opened and saved %2 times: bytes changed per cycle %3; largest gap %4 float steps "
                  "(bound %5); text round trip exact after %6 of %2 cycles")
          .arg(name).arg(CYCLES).arg(changes.join(",")).arg(gapMax).arg(GAP_BOUND).arg(CYCLES - badTrips));
}

// A model built with the given ends, saved, then put through the cycles.
static void checkCyclesOfEnds(const QString& what, const QString& path, const QList<float>& ends, const QString& tmp)
{
    coreProject p;
    if(!buildEndsModel(p, path, ends)) {
        check(false, QString("%1: built").arg(what));
        return;
    }
    QString start = tmp + "/ends-start.fvd";
    binaryOf(p, start);
    checkCycles(what, start, tmp);
}

// ends a, b (a < b) that an append moves, with b - a longer than a: the case
// the factor-of-two argument (s - a exact when s and a are close) leaves open
static QPair<float, float> findLongAwkward()
{
    quint32 state = 777u;
    for(int tries = 0; tries < 100000; ++tries) {
        state = state*1664525u + 1013904223u;
        float a = 0.2f + (float)(state >> 8)/(float)(1u << 24)*1.3f;
        state = state*1664525u + 1013904223u;
        float b = 2.05f*a + (float)(state >> 8)/(float)(1u << 24)*4.f;
        if(b - a > a && appendedEnd(a, b) != b) return qMakePair(a, b);
    }
    return qMakePair(0.f, 0.f);
}

// 0, e1, ..., e5, e5+1 where the pieces e1..e2, e2..e3, e3..e4 and e4..e5 are
// all moved by an append
static QList<float> findAwkwardChain()
{
    quint32 state = 4242u;
    QList<float> ends;
    ends << 0.f << 0.7f;
    while(ends.size() < 6) {
        state = state*1664525u + 1013904223u;
        float last = ends.last();
        float next = last + 0.1f + (float)(state >> 8)/(float)(1u << 24)*1.4f;
        if(appendedEnd(last, next) != next) ends << next;
    }
    ends << ends.last() + 1.f;
    return ends;
}

static QString endsText(const QList<float>& ends)
{
    QStringList l;
    for(int i = 0; i < ends.size(); ++i) l << QString::number(ends[i], 'g', 9);
    return l.join(", ");
}

static QString hexOf(float f)
{
    quint32 bits;
    memcpy(&bits, &f, sizeof(float));
    return QString("%1 (0x%2)").arg(f, 0, 'g', 9).arg(bits, 8, 16, QChar('0'));
}

// A text in the shape the first version wrote: a later transition's "from"
// under recalculatedOnLoad.
static QByteArray toFirstVersionShape(const QByteArray& text)
{
    static const char* functions[] = { "roll", "normalForce", "lateralForce", "pitchRate", "yawRate" };
    QJsonObject root = QJsonDocument::fromJson(text).object();
    QJsonArray tracks = root["tracks"].toArray();
    for(int ti = 0; ti < tracks.size(); ++ti) {
        QJsonObject t = tracks[ti].toObject();
        QJsonArray sections = t["sections"].toArray();
        for(int si = 0; si < sections.size(); ++si) {
            QJsonObject s = sections[si].toObject();
            for(unsigned k = 0; k < sizeof(functions)/sizeof(functions[0]); ++k) {
                if(!s.contains(functions[k])) continue;
                QJsonArray f = s[functions[k]].toArray();
                for(int i = 1; i < f.size(); ++i) {
                    QJsonObject piece = f[i].toObject();
                    QJsonObject rec = piece["recalculatedOnLoad"].toObject();
                    rec["from"] = piece["from"];
                    piece.remove("from");
                    piece["recalculatedOnLoad"] = rec;
                    f[i] = piece;
                }
                s[functions[k]] = f;
            }
            sections[si] = s;
        }
        t["sections"] = sections;
        tracks[ti] = t;
    }
    root["tracks"] = tracks;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

static QByteArray setNormalFrom(const QByteArray& text, int force, int piece, const QJsonValue& v, bool remove)
{
    return editSection(text, force, [&](QJsonObject& o) {
        QJsonArray f = o["normalForce"].toArray();
        QJsonObject t = f[piece].toObject();
        if(remove) t.remove("from"); else t["from"] = v;
        f[piece] = t;
        o["normalForce"] = f;
    });
}

static void checkExactEnds(const QString& path, const QString& tmp)
{
    float a0 = floatOfBits(0x409FFFFDu), b0 = floatOfBits(0x40BFFFFEu);
    check(appendedEnd(a0, b0) != b0 && appendedEnd(a0, appendedEnd(a0, b0)) == appendedEnd(a0, b0),
          QString("the owner's case: a piece from %1 to %2 is moved to %3 by one more append, and then stays")
          .arg(hexOf(a0), hexOf(b0), hexOf(appendedEnd(a0, b0))));

    QList<QPair<float, float> > cases;
    cases.append(qMakePair(a0, b0));
    cases.append(findAwkwardEnds(5));
    check(cases.size() == 6, QString("a search found %1 more pairs of ends that an append moves").arg(cases.size()-1));

    QByteArray ownerT1, ownerB1;
    int force = -1;
    for(int c = 0; c < cases.size(); ++c) {
        float a = cases[c].first, b = cases[c].second;
        QString what = QString("ends %1 then %2").arg(hexOf(a), hexOf(b));
        coreProject p;
        bool built = buildAwkwardModel(p, path, a, b);
        check(built, QString("%1: set in the model of a force section, they survive the recompute").arg(what));
        if(!built) continue;
        QByteArray b1 = binaryOf(p, tmp + "/awkward1.fvd");
        QByteArray t1;
        writeProjectText(t1, p.texPath, p.tracks, &p);

        coreProject q;
        QString r = readProjectText(t1, &q);
        QByteArray b2 = binaryOf(q, tmp + "/awkward2.fvd");
        QByteArray t2;
        writeProjectText(t2, q.texPath, q.tracks, &q);
        bool same = false;
        if(!q.tracks.isEmpty()) {
            func* nf = q.tracks[0]->lSections[forceSectionOf(q.tracks[0])]->normForce;
            same = nf->funcList.size() == 4 && nf->funcList[2]->minArgument == a && nf->funcList[2]->maxArgument == b
                    && nf->funcList[3]->minArgument == b;
        }
        check(r == QString("Load Successful.") && same && !b1.isEmpty() && b2 == b1 && t2 == t1,
              QString("%1: model -> text -> model gives the same ends, binary and text (%2)%3").arg(what, r)
              .arg(b2 == b1 ? QString() : QString(": ") + firstDifference(b1, b2)));
        if(!keepFolder.isEmpty()) {
            QFile f1(keepFolder + QString("/ends-%1.json").arg(c));
            if(f1.open(QIODevice::WriteOnly)) f1.write(t1);
            QFile f2(keepFolder + QString("/ends-%1.again.json").arg(c));
            if(f2.open(QIODevice::WriteOnly)) f2.write(t2);
        }
        if(c == 0) {
            ownerT1 = t1;
            ownerB1 = b1;
            force = forceSectionOf(p.tracks[0]);
        }
    }
    if(ownerT1.isEmpty()) return;

    // hand editing a later transition's "from"
    QString place = QString("track 1, section %1 (force), normalForce, transition 3").arg(force+1);
    {
        QByteArray t = setNormalFrom(setNormalFrom(ownerT1, force, 2, QJsonValue(), true), force, 3, QJsonValue(), true);
        coreProject p;
        QString r = readProjectText(t, &p);
        check(r == QString("Load Successful.") && binaryOf(p, tmp + "/nofrom.fvd") == ownerB1,
              QString("a later transition without \"from\" starts at the end before (%1)").arg(r));
    }
    expectRefused(setNormalFrom(ownerT1, force, 2, (double)a0 + 0.01, false),
                  "a later \"from\" 0.01 away from the end before", QStringList() << place << "leave \"from\" out");
    const int tol = (int)projectText::FROM_TOLERANCE_STEPS;
    {
        float near = stepsDown(a0, tol);
        QByteArray t = setNormalFrom(ownerT1, force, 2, projectText::floatToJson(near), false);
        coreProject p;
        QString r = readProjectText(t, &p);
        bool kept = r == QString("Load Successful.")
                && p.tracks[0]->lSections[force]->normForce->funcList[2]->minArgument == near;
        check(kept, QString("a later \"from\" %1 float steps away is kept as written (%2)").arg(tol).arg(r));
        float far = stepsDown(a0, tol + 1);
        expectRefused(setNormalFrom(ownerT1, force, 2, projectText::floatToJson(far), false),
                      QString("a later \"from\" %1 float steps away").arg(tol + 1), QStringList() << place);
    }

    // the writer uses the same rule: a model whose gap is beyond it is not
    // written; one just inside is written and reads back exactly
    for(int k = 0; k < 2; ++k) {
        int steps = k == 0 ? tol : tol + 1;
        coreProject p;
        bool built = buildAwkwardModel(p, path, a0, b0);
        if(built) {
            p.tracks[0]->lSections[force]->normForce->funcList[2]->minArgument = stepsDown(a0, steps);
            recompute(p.tracks[0]);
            built = p.tracks[0]->lSections[force]->normForce->funcList[2]->minArgument == stepsDown(a0, steps);
        }
        QByteArray t1;
        QString w = writeProjectText(t1, p.texPath, p.tracks, &p);
        if(k == 1) {
            QString writePlace = QString("track 1, section %1, normalForce, transition 3").arg(force+1);
            check(built && w.startsWith("Error") && w.contains(writePlace) && w.contains("could not be read back") && t1.isEmpty(),
                  QString("a model with a gap of %1 float steps is not written: %2").arg(steps).arg(w));
        } else {
            QByteArray b1 = binaryOf(p, tmp + "/gap1.fvd");
            coreProject q;
            QString r = readProjectText(t1, &q);
            QByteArray t2;
            writeProjectText(t2, q.texPath, q.tracks, &q);
            check(built && w == QString("Project Saved!") && r == QString("Load Successful.")
                  && binaryOf(q, tmp + "/gap2.fvd") == b1 && t2 == t1,
                  QString("a model with a gap of %1 float steps is written and reads back exactly (%2)").arg(steps).arg(r));
        }
    }

    // the shape parameter, one rule both ways: kept as it is (infinity, NaN)
    // where the shape does not read it, finite where it does
    {
        const float inf = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        float values[2] = { inf, nan };
        for(int k = 0; k < 2; ++k) {
            coreProject p;
            bool built = buildAwkwardModel(p, path, a0, b0);
            subfunc* piece = built ? p.tracks[0]->lSections[force]->normForce->funcList[2] : NULL;
            if(piece) piece->arg1 = values[k];
            QByteArray b1 = binaryOf(p, tmp + "/arg1.fvd");
            QByteArray t1, t2;
            QString w = writeProjectText(t1, p.texPath, p.tracks, &p);
            coreProject q;
            QString r = readProjectText(t1, &q);
            writeProjectText(t2, q.texPath, q.tracks, &q);
            check(built && piece->degree == cubic && w == QString("Project Saved!") && r == QString("Load Successful.")
                  && binaryOf(q, tmp + "/arg1b.fvd") == b1 && t2 == t1,
                  QString("a cubic transition's unused shape parameter %1 is written and reads back exactly (%2)")
                  .arg(k == 0 ? "inf" : "nan").arg(r));
            if(built) {
                piece->degree = quartic;
                QByteArray t3;
                QString w3 = writeProjectText(t3, p.texPath, p.tracks, &p);
                QString writePlace = QString("track 1, section %1, normalForce, transition 3").arg(force+1);
                check(w3.startsWith("Error") && w3.contains(writePlace) && t3.isEmpty(),
                      QString("a quartic transition with the shape parameter %1 is not written: %2")
                      .arg(k == 0 ? "inf" : "nan").arg(w3));
            }
        }
        expectRefused(editSection(ownerT1, force, [](QJsonObject& o) { setFirst(o, "roll", "shapeParameter", QString("nan")); }),
                      "a NaN shape parameter on a quartic transition", QStringList() << "roll, transition 1" << "\"shapeParameter\"" << "finite");
    }

    // and every other rule of the reader: the writer checks its own text
    {
        coreProject p;
        loadProjectFile(path, &p);
        p.tracks[0]->fFriction = 3.f;   // the reader allows 0 to 2
        QByteArray t;
        QString w = writeProjectText(t, p.texPath, p.tracks, &p);
        check(w.startsWith("Error") && w.contains("would not read back") && w.contains("\"friction\"") && t.isEmpty(),
              QString("a model the reader would refuse for any other reason is not written: %1").arg(w));
    }

    // open-and-save cycles of the built models
    for(int c = 0; c < cases.size(); ++c) {
        float a = cases[c].first, b = cases[c].second;
        checkCyclesOfEnds(QString("ends %1 then %2").arg(hexOf(a), hexOf(b)), path,
                          QList<float>() << 0.f << a*0.5f << a << b << b + 1.f, tmp);
    }
    {
        QPair<float, float> lng = findLongAwkward();
        check(lng.second > 0.f, "a search found an awkward piece longer than everything before it");
        QList<float> ends = QList<float>() << 0.f << lng.first*0.5f << lng.first << lng.second << lng.second + 1.f;
        checkCyclesOfEnds(QString("a long awkward piece, ends %1").arg(endsText(ends)), path, ends, tmp);
        QList<float> chain = findAwkwardChain();
        checkCyclesOfEnds(QString("four awkward pieces in a row, ends %1").arg(endsText(chain)), path, chain, tmp);
    }
    {
        QByteArray t = toFirstVersionShape(ownerT1);
        coreProject p;
        QString r = readProjectText(t, &p);
        QByteArray t2;
        writeProjectText(t2, p.texPath, p.tracks, &p);
        check(r == QString("Load Successful.") && t != ownerT1
              && binaryOf(p, tmp + "/firstversion.fvd") == ownerB1 && t2 == ownerT1,
              QString("a text of the first version (a later \"from\" under recalculatedOnLoad) reads to the same model (%1)").arg(r));
    }
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
    if(argc > 2) keepFolder = QString::fromLocal8Bit(argv[2]);   // "" keeps nothing

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

    // the optional third argument: a folder of more projects, each put
    // through the same round trip
    if(argc > 3) {
        QDir extra(QString::fromLocal8Bit(argv[3]));
        QStringList l = extra.entryList(QStringList() << "*.fvd", QDir::Files, QDir::Name);
        check(extra.exists() && !l.isEmpty(), QString("%1 projects found in %2").arg(l.size()).arg(extra.absolutePath()));
        for(int i = 0; i < l.size(); ++i) {
            if(l[i].contains(".2016.")) continue;
            checkProject(extra.filePath(l[i]), tmp);
            checkCycles(l[i], extra.filePath(l[i]), tmp);
        }
    }

    // open and save each sample again and again: how far a transition's start
    // drifts from the end before, and the text round trip after every cycle
    for(int i = 0; i < projects.size(); ++i) {
        checkCycles(QFileInfo(projects[i]).fileName(), projects[i], tmp);
    }

    if(fourT1.isEmpty()) {
        check(false, "samples/baseline/four-sections.fvd was read");
    } else {
        checkMessages(fourT1, fourB1, tmp);
        checkRanges(fourT1, tmp);
        checkFreeformWrite(fourPath);
        checkToZero(fourT1, tmp);
        checkExactEnds(fourPath, tmp);
        {
            coreProject p;
            QString r = readProjectText(toFirstVersionShape(fourT1), &p);
            check(r == QString("Load Successful."), QString("four-sections.fvd in the first version's shape reads (%1)").arg(r));
        }

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

    printf("     info: the largest gap seen between a transition's start and the end before, over every cycle: "
           "%lld float steps (the reader's tolerance is %lld)\n",
           (long long)observedLargestGap, (long long)projectText::FROM_TOLERANCE_STEPS);

    if(failures == 0) {
        printf("PASS\n");
        return 0;
    }
    printf("%d check(s) failed\nFAIL\n", failures);
    return 1;
}
