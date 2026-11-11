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

// The project as text (docs/project-text-format.md). Writing reads the model
// field by field, exactly the fields track::saveTrack() and the save
// functions it calls write. Reading first turns the whole text into plain
// data (projectData below), checking every key, and only then builds the
// model with the calls and in the order of readProject() / track::loadTrack(),
// so that a project read from text computes, and saves, exactly as the same
// project read from its binary file.

#include "projecttext.h"
#include "track.h"
#include "smoothhandler.h"
#include "exportfuncs.h"
#include "lenassert.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringList>
#include <QtDebug>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

using namespace std;

static const char* FORMAT_NAME = "fvd-project-text";
static const int FORMAT_VERSION = 1;
static const char* BINARY_VERSION = "v0.77";   // what writeProject() writes

// ---------------------------------------------------------------------------
// Names for the enums
// ---------------------------------------------------------------------------

static const char* SHAPE_NAMES[] = { "linear", "quadratic", "cubic", "quartic", "quintic",
                                     "sinusoidal", "plateau", "tozero", "freeform" };
static const int SHAPE_COUNT = 9;

static const char* STYLE_NAMES[] = { "generic", "genericflat", "vekoma", "bm", "triangle",
                                     "box", "smallflat", "doublespine" };
static const int STYLE_COUNT = 8;

// index = enum secType
static const char* SECTION_NAMES[] = { "anchor", "straight", "curved", "force", "geometric",
                                       "bezier", "nolimitscsv" };
static const int SECTION_COUNT = 7;

static int indexOfName(const QString& name, const char* const* names, int count)
{
    for(int i = 0; i < count; ++i) {
        if(name == QLatin1String(names[i])) return i;
    }
    return -1;
}

static QString nameList(const char* const* names, int from, int count)
{
    QStringList l;
    for(int i = from; i < count; ++i) l.append(QString("\"%1\"").arg(names[i]));
    return l.join(", ");
}

// ---------------------------------------------------------------------------
// Floats
// ---------------------------------------------------------------------------

namespace projectText {

QJsonValue floatToJson(float f)
{
    if(f != f) return QJsonValue(QString("nan"));
    if(std::isinf(f)) return QJsonValue(QString(f > 0 ? "inf" : "-inf"));
    if(f == 0.f && std::signbit(f)) return QJsonValue(QString("-0"));

    // The reader turns the JSON number into a double and that into a float;
    // the first precision for which that gives f back is the one written.
    // Qt writes a double in its shortest form, so the text shows these digits.
    for(int digits = 1; digits <= 9; ++digits) {
        double d = QByteArray::number((double)f, 'g', digits).toDouble();
        if((float)d == f) return QJsonValue(d);
    }
    return QJsonValue((double)f);   // the double itself always reads back
}

bool floatFromJson(const QJsonValue& v, float* f)
{
    if(v.isDouble()) {
        *f = (float)v.toDouble();
        return true;
    }
    if(v.isString()) {
        QString s = v.toString();
        if(s == "nan") { *f = std::numeric_limits<float>::quiet_NaN(); return true; }
        if(s == "inf") { *f = std::numeric_limits<float>::infinity(); return true; }
        if(s == "-inf") { *f = -std::numeric_limits<float>::infinity(); return true; }
        if(s == "-0") { *f = -0.f; return true; }
    }
    return false;
}

}

using projectText::floatToJson;
using projectText::floatFromJson;

static QJsonArray vecToJson(const glm::vec3& v)
{
    QJsonArray a;
    a.append(floatToJson(v.x));
    a.append(floatToJson(v.y));
    a.append(floatToJson(v.z));
    return a;
}

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

// The plain 8-bit colour, built in zeroed memory so that its 16 bytes are
// defined (the binary file stores the raw bytes of the QColor).
static void plainColor(QColor* out, int r, int g, int b, int a)
{
    memset((void*)out, 0, sizeof(QColor));
    out->setRgb(r, g, b, a);
}

static QJsonObject colorToJson(const QColor& c)
{
    QJsonObject o;
    QColor plain;
    plainColor(&plain, c.red(), c.green(), c.blue(), c.alpha());
    o["color"] = plain.name(QColor::HexRgb);
    o["alpha"] = c.alpha();
    if(memcmp((const void*)&plain, (const void*)&c, sizeof(QColor)) != 0) {
        o["exactBytes"] = QString(QByteArray((const char*)&c, sizeof(QColor)).toHex());
    }
    return o;
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------

namespace {

class textWriter
{
public:
    QString error;

    QJsonObject writeTrack(track* t, int trackNo);

private:
    bool writeFunction(func* f, const QString& where, QJsonArray* out);
    QJsonObject writeSmoothing(smoothHandler* h, bool custom);
    bool writeSection(section* s, smoothHandler* smooth, const QString& where, QJsonObject* out);
};

bool textWriter::writeFunction(func* f, const QString& where, QJsonArray* out)
{
    for(int i = 0; i < f->funcList.size(); ++i) {
        subfunc* sub = f->funcList[i];
        QString here = QString("%1, transition %2").arg(where).arg(i+1);
        int shape = (int)sub->degree;
        if(shape < 0 || shape >= SHAPE_COUNT) {
            error = QString("Error: %1: unknown shape number %2.").arg(here).arg(shape);
            return false;
        }
        if(sub->degree == freeform) {
            error = QString("Error: %1: a freeform transition cannot be saved as text "
                            "(the project file does not keep its control points either).").arg(here);
            return false;
        }
        QJsonObject o;
        QJsonObject recalc;
        o["shape"] = QString(SHAPE_NAMES[shape]);
        if(i == 0) {
            o["from"] = floatToJson(sub->minArgument);
        } else {
            recalc["from"] = floatToJson(sub->minArgument);
        }
        o["to"] = floatToJson(sub->maxArgument);
        if(sub->degree == tozero) {
            recalc["change"] = floatToJson(sub->symArg);
        } else {
            o["change"] = floatToJson(sub->symArg);
        }
        o["shapeParameter"] = floatToJson(sub->arg1);
        o["center"] = floatToJson(sub->centerArg);
        o["tension"] = floatToJson(sub->tensionArg);
        o["locked"] = sub->locked;
        recalc["startValue"] = floatToJson(sub->startValue);
        o["recalculatedOnLoad"] = recalc;
        out->append(o);
    }
    return true;
}

QJsonObject textWriter::writeSmoothing(smoothHandler* h, bool custom)
{
    QJsonObject o;
    o["length"] = h->getLength();
    o["iterations"] = h->getIterations();
    o["active"] = h->active;
    if(custom) {
        o["name"] = h->sName;
        o["fromNode"] = h->getFrom();
        o["toNode"] = h->getTo();
    } else {
        QJsonObject recalc;
        recalc["name"] = h->sName;
        recalc["fromNode"] = h->getFrom();
        recalc["toNode"] = h->getTo();
        o["recalculatedOnLoad"] = recalc;
    }
    return o;
}

static QString speedModeName(bool bSpeed)
{
    return QString(bSpeed ? "physics" : "fixed");
}

bool textWriter::writeSection(section* s, smoothHandler* smooth, const QString& where, QJsonObject* out)
{
    QJsonObject o;
    QJsonObject recalc;
    int type = (int)s->type;
    if(type < 1 || type >= SECTION_COUNT) {
        error = QString("Error: %1: unknown section type number %2.").arg(where).arg(type);
        return false;
    }
    o["type"] = QString(SECTION_NAMES[type]);
    if(s->type != nolimitscsv) {
        o["name"] = s->sName;
    }

    QJsonArray roll, second, third;
    switch(s->type) {
    case straight:
        o["speedMode"] = speedModeName(s->bSpeed);
        o["speed"] = floatToJson(s->fVel);
        recalc["length"] = floatToJson(s->fHLength);
        if(!writeFunction(s->rollFunc, where + ", roll", &roll)) return false;
        o["roll"] = roll;
        break;
    case curved:
        o["speedMode"] = speedModeName(s->bSpeed);
        o["speed"] = floatToJson(s->fVel);
        recalc["angle"] = floatToJson(s->fAngle);
        o["radius"] = floatToJson(s->fRadius);
        o["direction"] = floatToJson(s->fDirection);
        o["leadIn"] = floatToJson(s->fLeadIn);
        o["leadOut"] = floatToJson(s->fLeadOut);
        o["orientation"] = QString(s->bOrientation == EULER ? "euler" : "quaternion");
        if(!writeFunction(s->rollFunc, where + ", roll", &roll)) return false;
        o["roll"] = roll;
        break;
    case forced:
    case geometric:
        o["speedMode"] = speedModeName(s->bSpeed);
        o["speed"] = floatToJson(s->fVel);
        o["orientation"] = QString(s->bOrientation == EULER ? "euler" : "quaternion");
        o["argument"] = QString(s->bArgument == DISTANCE ? "distance" : "time");
        if(s->bArgument == DISTANCE) {
            o["timeNodes"] = s->iTime;   // kept as it is, unused by a distance section
        } else {
            recalc["timeNodes"] = s->iTime;
        }
        if(!writeFunction(s->rollFunc, where + ", roll", &roll)) return false;
        o["roll"] = roll;
        if(s->type == forced) {
            if(!writeFunction(s->normForce, where + ", normalForce", &second)) return false;
            if(!writeFunction(s->latForce, where + ", lateralForce", &third)) return false;
            o["normalForce"] = second;
            o["lateralForce"] = third;
        } else {
            if(!writeFunction(s->normForce, where + ", pitchRate", &second)) return false;
            if(!writeFunction(s->latForce, where + ", yawRate", &third)) return false;
            o["pitchRate"] = second;
            o["yawRate"] = third;
        }
        break;
    case bezier: {
        QJsonArray points;
        for(int i = 0; i < s->bezList.size(); ++i) {
            bezier_t* b = s->bezList[i];
            QJsonObject p;
            p["point"] = vecToJson(b->P1);
            p["controlPoint1"] = vecToJson(b->Kp1);
            p["controlPoint2"] = vecToJson(b->Kp2);
            p["continuousRoll"] = b->contRoll;
            p["relativeRoll"] = b->relRoll;
            p["roll"] = floatToJson(b->roll);
            points.append(p);
        }
        QJsonArray supports;
        for(int i = 0; i < s->supList.size(); ++i) {
            supports.append(vecToJson(s->supList[i]));
        }
        o["points"] = points;
        o["supports"] = supports;
        break;
    }
    case nolimitscsv: {
        // The points are private to secnlcsv; its own save function (the
        // stringstream one the undo list uses) hands them over: "CSV", the
        // count, then position, direction and lateral, nine floats a point.
        std::stringstream ss;
        s->saveSection(ss);
        readString(&ss, 3);
        int count = readInt(&ss);
        QJsonArray points;
        for(int i = 0; i < count; ++i) {
            glm::vec3 v[3];
            for(int k = 0; k < 3; ++k) {
                float x = readFloat(&ss);
                float y = readFloat(&ss);
                float z = readFloat(&ss);
                v[k] = glm::vec3(x, y, z);
            }
            QJsonObject p;
            p["position"] = vecToJson(v[0]);
            p["direction"] = vecToJson(v[1]);
            p["lateral"] = vecToJson(v[2]);
            points.append(p);
        }
        o["points"] = points;
        break;
    }
    default:
        break;
    }

    o["smoothing"] = writeSmoothing(smooth, false);
    if(!recalc.isEmpty()) o["recalculatedOnLoad"] = recalc;
    *out = o;
    return true;
}

QJsonObject textWriter::writeTrack(track* t, int trackNo)
{
    QString where = QString("track %1").arg(trackNo);
    QJsonObject o;
    o["name"] = t->name;

    QJsonObject colors;
    colors["default"] = colorToJson(t->display.colors[0]);
    colors["activeSection"] = colorToJson(t->display.colors[1]);
    colors["transition"] = colorToJson(t->display.colors[2]);
    o["colors"] = colors;

    QJsonObject anchor;
    anchor["position"] = vecToJson(t->startPos);
    anchor["roll"] = floatToJson(t->anchorNode->fRoll);
    anchor["pitch"] = floatToJson(t->startPitch);
    anchor["yaw"] = floatToJson(t->startYaw);
    anchor["speed"] = floatToJson(t->anchorNode->fVel);
    anchor["normalForce"] = floatToJson(t->anchorNode->forceNormal);
    anchor["lateralForce"] = floatToJson(t->anchorNode->forceLateral);
    o["anchor"] = anchor;

    o["heartline"] = floatToJson(t->fHeart);
    o["friction"] = floatToJson(t->fFriction);
    o["resistance"] = floatToJson(t->fResistance);
    o["drawTrack"] = t->drawTrack;
    o["drawHeartline"] = t->drawHeartline;
    int style = (int)t->style;
    if(style < 0 || style >= STYLE_COUNT) {
        error = QString("Error: %1: unknown track style number %2.").arg(where).arg(style);
        return QJsonObject();
    }
    o["style"] = QString(STYLE_NAMES[style]);
    o["wireframe"] = t->display.bWireframe;
    QJsonArray pov;
    pov.append(floatToJson(t->povPos.x));
    pov.append(floatToJson(t->povPos.y));
    o["povPosition"] = pov;

    // Smoothing entries go by position: 0 the whole track, 1..n the sections,
    // then the custom regions (track::newSection(), track::loadTrack()).
    const int n = t->lSections.size();
    if(t->smoothList.size() < n+1) {
        error = QString("Error: %1: the track has %2 sections but only %3 smoothing entries.")
                .arg(where).arg(n).arg(t->smoothList.size());
        return QJsonObject();
    }

    QJsonArray sections;
    for(int i = 0; i < n; ++i) {
        QJsonObject so;
        if(!writeSection(t->lSections[i], t->smoothList[i+1], QString("%1, section %2").arg(where).arg(i+1), &so)) {
            return QJsonObject();
        }
        sections.append(so);
    }
    o["sections"] = sections;

    QJsonObject smoothing;
    smoothing["wholeTrack"] = writeSmoothing(t->smoothList[0], false);
    QJsonArray custom;
    for(int i = n+1; i < t->smoothList.size(); ++i) {
        custom.append(writeSmoothing(t->smoothList[i], true));
    }
    smoothing["custom"] = custom;
    o["smoothing"] = smoothing;
    return o;
}

} // namespace

QString writeProjectText(QByteArray& out, const QString& texPath, const QList<track*>& tracks, projectFileClient* client)
{
    out.clear();
    textWriter w;
    QJsonArray trackArray;
    for(int i = 0; i < tracks.size(); ++i) {
        if(client) client->projectTrackSaving(tracks[i]);
        QJsonObject t = w.writeTrack(tracks[i], i+1);
        if(!w.error.isEmpty()) return w.error;
        trackArray.append(t);
    }

    QJsonObject root;
    root["format"] = QString(FORMAT_NAME);
    root["formatVersion"] = FORMAT_VERSION;
    root["binaryVersion"] = QString(BINARY_VERSION);
    root["groundTexture"] = texPath;
    root["tracks"] = trackArray;

    out = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if(!out.endsWith('\n')) out.append('\n');
    return QString("Project Saved!");
}

// ---------------------------------------------------------------------------
// Reading, step 1: the text into plain data, every key checked
// ---------------------------------------------------------------------------

namespace {

struct transitionData {
    eDegree shape;
    float from, to, change, shapeParameter, center, tension;
    bool locked;
};

struct smoothData {
    QString name;
    int fromNode, toNode, length, iterations;
    bool active;
};

struct bezierData {
    glm::vec3 point, control1, control2;
    float roll;
    bool continuousRoll, relativeRoll;
};

struct csvData {
    glm::vec3 position, direction, lateral;
};

struct sectionData {
    secType type;
    QString name;
    bool bSpeed;
    float fVel;
    float fRadius, fDirection, fLeadIn, fLeadOut;
    bool bOrientation, bArgument;
    bool hasTime;
    int iTime;
    QList<transitionData> roll, second, third;
    QList<bezierData> points;
    QList<glm::vec3> supports;
    QList<csvData> csvPoints;
    smoothData smooth;
};

struct trackData {
    QString name;
    // the raw bytes, as the binary file holds them: a QColor copy would not
    // carry the two bytes of padding at its end
    char colorBytes[3][sizeof(QColor)];
    glm::vec3 position;
    float roll, pitch, yaw, speed, normalForce, lateralForce;
    float heartline, friction, resistance;
    bool drawTrack;
    int drawHeartline;
    trackStyle style;
    bool wireframe;
    glm::vec2 pov;
    QList<sectionData> sections;
    smoothData whole;
    QList<smoothData> custom;
};

struct projectData {
    QString groundTexture;
    QList<trackData> tracks;
};

// One JSON object being read: remembers which keys were asked for, so that
// the others can be reported, and where it is, for the messages.
class objectReader
{
public:
    objectReader(const QJsonObject& o, const QString& where, QString* error, QStringList* warnings)
        : obj(o), place(where), err(error), warn(warnings) {}

    bool ok() const { return err->isEmpty(); }

    // keys that are written but deliberately not read
    void ignore(const char* key) { used.insert(QString(key)); }

    bool has(const char* key) const { return obj.contains(QString(key)); }

    // the keys nobody asked for become warnings
    void finish()
    {
        if(!ok()) return;
        QStringList keys = obj.keys();
        for(int i = 0; i < keys.size(); ++i) {
            if(!used.contains(keys[i])) {
                QString w = QString("%1: unknown key \"%2\" was ignored.").arg(place, keys[i]);
                qWarning("%s", qPrintable(w));
                warn->append(w);
            }
        }
    }

    QJsonValue value(const char* key)
    {
        if(!ok()) return QJsonValue();
        used.insert(QString(key));
        if(!obj.contains(QString(key))) {
            fail(QString("the key \"%1\" is missing").arg(key));
            return QJsonValue();
        }
        return obj.value(QString(key));
    }

    void fail(const QString& what)
    {
        if(ok()) *err = QString("Error: %1: %2.").arg(place, what);
    }

    void wrongType(const char* key, const char* expected)
    {
        fail(QString("\"%1\" must be %2").arg(key, expected));
    }

    float getFloat(const char* key)
    {
        QJsonValue v = value(key);
        float f = 0.f;
        if(ok() && !floatFromJson(v, &f)) wrongType(key, "a number");
        return f;
    }

    int getInt(const char* key)
    {
        QJsonValue v = value(key);
        if(!ok()) return 0;
        double d = v.toDouble(0.5);
        if(!v.isDouble() || d != std::floor(d) || d < -2147483648.0 || d > 2147483647.0) {
            wrongType(key, "a whole number");
            return 0;
        }
        return (int)d;
    }

    bool getBool(const char* key)
    {
        QJsonValue v = value(key);
        if(ok() && !v.isBool()) wrongType(key, "true or false");
        return v.toBool();
    }

    QString getString(const char* key)
    {
        QJsonValue v = value(key);
        if(ok() && !v.isString()) wrongType(key, "a text in quotes");
        return v.toString();
    }

    // one of the names, given in order of the enum (from index 'first')
    int getName(const char* key, const char* const* names, int first, int count)
    {
        QString s = getString(key);
        if(!ok()) return first;
        int i = indexOfName(s, names, count);
        if(i < first) {
            fail(QString("\"%1\" is \"%2\", which is not one of %3").arg(key, s, nameList(names, first, count)));
            return first;
        }
        return i;
    }

    bool getChoice(const char* key, const char* a, const char* b)
    {
        QString s = getString(key);
        if(!ok()) return false;
        if(s == QLatin1String(a)) return false;
        if(s == QLatin1String(b)) return true;
        fail(QString("\"%1\" is \"%2\", which is not \"%3\" or \"%4\"").arg(key, s, a, b));
        return false;
    }

    QJsonObject getObject(const char* key)
    {
        QJsonValue v = value(key);
        if(ok() && !v.isObject()) wrongType(key, "an object { ... }");
        return v.toObject();
    }

    QJsonArray getArray(const char* key)
    {
        QJsonValue v = value(key);
        if(ok() && !v.isArray()) wrongType(key, "a list [ ... ]");
        return v.toArray();
    }

    glm::vec3 getVec3(const char* key)
    {
        QJsonArray a = getArray(key);
        glm::vec3 r(0.f, 0.f, 0.f);
        if(!ok()) return r;
        if(a.size() != 3 || !floatFromJson(a[0], &r.x) || !floatFromJson(a[1], &r.y) || !floatFromJson(a[2], &r.z)) {
            wrongType(key, "a list of three numbers [x, y, z]");
        }
        return r;
    }

    const QString& where() const { return place; }

private:
    QJsonObject obj;
    QString place;
    QString* err;
    QStringList* warn;
    QSet<QString> used;
};

class textReader
{
public:
    QString error;
    QStringList warnings;

    bool readProject(const QJsonObject& root, projectData* out);

private:
    bool expectObject(const QJsonValue& v, const QString& where)
    {
        if(!v.isObject()) {
            error = QString("Error: %1: must be an object { ... }.").arg(where);
            return false;
        }
        return true;
    }

    void readColor(objectReader& parent, const char* key, char* out);
    void readSmoothing(objectReader& parent, const char* key, bool custom, smoothData* out);
    void readSmoothingObject(const QJsonObject& o, const QString& where, bool custom, smoothData* out);
    void readFunction(objectReader& parent, const char* key, QList<transitionData>* out);
    bool readSection(const QJsonValue& v, const QString& where, sectionData* out);
    bool readTrack(const QJsonValue& v, const QString& where, trackData* out);
};

void textReader::readColor(objectReader& parent, const char* key, char* out)
{
    QJsonObject o = parent.getObject(key);
    if(!parent.ok()) return;
    objectReader r(o, QString("%1, %2").arg(parent.where(), key), &error, &warnings);
    QString name = r.getString("color");
    int alpha = r.getInt("alpha");
    if(!r.ok()) return;
    QColor rgb(name);
    if(name.length() != 7 || !name.startsWith('#') || !rgb.isValid()) {
        r.fail(QString("\"color\" is \"%1\", which is not a colour written #rrggbb").arg(name));
        return;
    }
    if(alpha < 0 || alpha > 255) {
        r.fail(QString("\"alpha\" is %1, which is not between 0 and 255").arg(alpha));
        return;
    }
    QColor plain;
    plainColor(&plain, rgb.red(), rgb.green(), rgb.blue(), alpha);
    memcpy(out, (const void*)&plain, sizeof(QColor));
    if(r.has("exactBytes")) {
        QString hex = r.getString("exactBytes");
        if(!r.ok()) return;
        QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
        if(hex.length() != 2*(int)sizeof(QColor) || bytes.size() != (int)sizeof(QColor)
                || QString(bytes.toHex()) != hex.toLower()) {
            r.fail(QString("\"exactBytes\" must be %1 hexadecimal digits").arg(2*sizeof(QColor)));
            return;
        }
        QColor exact;
        memcpy((void*)&exact, bytes.constData(), sizeof(QColor));
        if(exact.red() == rgb.red() && exact.green() == rgb.green() && exact.blue() == rgb.blue() && exact.alpha() == alpha) {
            memcpy(out, bytes.constData(), sizeof(QColor));
        } else {
            QString w = QString("%1: \"exactBytes\" does not match \"color\" and \"alpha\"; "
                                "the colour was taken from \"color\" and \"alpha\".").arg(r.where());
            qWarning("%s", qPrintable(w));
            warnings.append(w);
        }
    }
    r.finish();
}

void textReader::readSmoothingObject(const QJsonObject& o, const QString& where, bool custom, smoothData* out)
{
    objectReader r(o, where, &error, &warnings);
    out->length = r.getInt("length");
    out->iterations = r.getInt("iterations");
    out->active = r.getBool("active");
    if(custom) {
        out->name = r.getString("name");
        out->fromNode = r.getInt("fromNode");
        out->toNode = r.getInt("toNode");
    } else {
        r.ignore("recalculatedOnLoad");
        out->fromNode = 0;
        out->toNode = 0;
    }
    r.finish();
}

void textReader::readSmoothing(objectReader& parent, const char* key, bool custom, smoothData* out)
{
    QJsonObject o = parent.getObject(key);
    if(!parent.ok()) return;
    readSmoothingObject(o, QString("%1, %2").arg(parent.where(), key), custom, out);
}

void textReader::readFunction(objectReader& parent, const char* key, QList<transitionData>* out)
{
    QJsonArray a = parent.getArray(key);
    if(!parent.ok()) return;
    if(a.isEmpty()) {
        parent.fail(QString("\"%1\" must hold at least one transition").arg(key));
        return;
    }
    for(int i = 0; i < a.size() && error.isEmpty(); ++i) {
        QString where = QString("%1, %2, transition %3").arg(parent.where(), key).arg(i+1);
        if(!expectObject(a[i], where)) return;
        objectReader r(a[i].toObject(), where, &error, &warnings);
        transitionData t;
        t.shape = (eDegree)r.getName("shape", SHAPE_NAMES, 0, SHAPE_COUNT);
        if(r.ok() && t.shape == freeform) {
            r.fail("a freeform transition cannot be read from text "
                   "(its control points are not part of a project)");
            return;
        }
        t.from = (i == 0) ? r.getFloat("from") : 0.f;
        t.to = r.getFloat("to");
        t.change = (t.shape == tozero) ? 0.f : r.getFloat("change");
        t.shapeParameter = r.getFloat("shapeParameter");
        t.center = r.getFloat("center");
        t.tension = r.getFloat("tension");
        t.locked = r.getBool("locked");
        r.ignore("recalculatedOnLoad");
        r.finish();
        out->append(t);
    }
}

bool textReader::readSection(const QJsonValue& v, const QString& where, sectionData* out)
{
    if(!expectObject(v, where)) return false;
    objectReader r(v.toObject(), where, &error, &warnings);
    out->type = (secType)r.getName("type", SECTION_NAMES, 1, SECTION_COUNT);
    if(!r.ok()) return false;

    // what a section created by track::appendSectionFromFile() holds where
    // the binary file has nothing
    out->name = QString("unnamed");
    out->bSpeed = false;
    out->fVel = 0.f;
    out->fRadius = out->fDirection = out->fLeadIn = out->fLeadOut = 0.f;
    out->bOrientation = QUATERNION;
    out->bArgument = TIME;
    out->hasTime = false;
    out->iTime = 0;

    objectReader sr(v.toObject(), QString("%1 (%2)").arg(where, SECTION_NAMES[out->type]), &error, &warnings);
    sr.ignore("type");
    if(out->type != nolimitscsv) {
        out->name = sr.getString("name");
    }
    switch(out->type) {
    case straight:
        out->bSpeed = sr.getChoice("speedMode", "fixed", "physics");
        out->fVel = sr.getFloat("speed");
        readFunction(sr, "roll", &out->roll);
        break;
    case curved:
        out->bSpeed = sr.getChoice("speedMode", "fixed", "physics");
        out->fVel = sr.getFloat("speed");
        out->fRadius = sr.getFloat("radius");
        out->fDirection = sr.getFloat("direction");
        out->fLeadIn = sr.getFloat("leadIn");
        out->fLeadOut = sr.getFloat("leadOut");
        out->bOrientation = sr.getChoice("orientation", "quaternion", "euler") ? EULER : QUATERNION;
        readFunction(sr, "roll", &out->roll);
        break;
    case forced:
    case geometric:
        out->bSpeed = sr.getChoice("speedMode", "fixed", "physics");
        out->fVel = sr.getFloat("speed");
        out->bOrientation = sr.getChoice("orientation", "quaternion", "euler") ? EULER : QUATERNION;
        out->bArgument = sr.getChoice("argument", "time", "distance") ? DISTANCE : TIME;
        if(sr.ok() && out->bArgument == DISTANCE) {
            out->hasTime = true;
            out->iTime = sr.getInt("timeNodes");
        }
        readFunction(sr, "roll", &out->roll);
        if(out->type == forced) {
            readFunction(sr, "normalForce", &out->second);
            readFunction(sr, "lateralForce", &out->third);
        } else {
            readFunction(sr, "pitchRate", &out->second);
            readFunction(sr, "yawRate", &out->third);
        }
        break;
    case bezier: {
        QJsonArray points = sr.getArray("points");
        for(int i = 0; i < points.size() && error.isEmpty(); ++i) {
            QString pw = QString("%1, point %2").arg(sr.where()).arg(i+1);
            if(!expectObject(points[i], pw)) return false;
            objectReader pr(points[i].toObject(), pw, &error, &warnings);
            bezierData b;
            b.point = pr.getVec3("point");
            b.control1 = pr.getVec3("controlPoint1");
            b.control2 = pr.getVec3("controlPoint2");
            b.continuousRoll = pr.getBool("continuousRoll");
            b.relativeRoll = pr.getBool("relativeRoll");
            b.roll = pr.getFloat("roll");
            pr.finish();
            out->points.append(b);
        }
        QJsonArray supports = sr.getArray("supports");
        for(int i = 0; i < supports.size() && error.isEmpty(); ++i) {
            QJsonArray s = supports[i].toArray();
            glm::vec3 p;
            if(!supports[i].isArray() || s.size() != 3 || !floatFromJson(s[0], &p.x)
                    || !floatFromJson(s[1], &p.y) || !floatFromJson(s[2], &p.z)) {
                sr.fail(QString("support %1 must be a list of three numbers [x, y, z]").arg(i+1));
                return false;
            }
            out->supports.append(p);
        }
        break;
    }
    case nolimitscsv: {
        QJsonArray points = sr.getArray("points");
        for(int i = 0; i < points.size() && error.isEmpty(); ++i) {
            QString pw = QString("%1, point %2").arg(sr.where()).arg(i+1);
            if(!expectObject(points[i], pw)) return false;
            objectReader pr(points[i].toObject(), pw, &error, &warnings);
            csvData c;
            c.position = pr.getVec3("position");
            c.direction = pr.getVec3("direction");
            c.lateral = pr.getVec3("lateral");
            pr.finish();
            out->csvPoints.append(c);
        }
        break;
    }
    default:
        break;
    }
    readSmoothing(sr, "smoothing", false, &out->smooth);
    sr.ignore("recalculatedOnLoad");
    sr.finish();
    return error.isEmpty();
}

bool textReader::readTrack(const QJsonValue& v, const QString& where, trackData* out)
{
    if(!expectObject(v, where)) return false;
    objectReader r(v.toObject(), where, &error, &warnings);
    out->name = r.getString("name");

    QJsonObject colors = r.getObject("colors");
    if(r.ok()) {
        objectReader cr(colors, where + ", colors", &error, &warnings);
        readColor(cr, "default", out->colorBytes[0]);
        readColor(cr, "activeSection", out->colorBytes[1]);
        readColor(cr, "transition", out->colorBytes[2]);
        cr.finish();
    }
    if(!error.isEmpty()) return false;

    QJsonObject anchor = r.getObject("anchor");
    if(r.ok()) {
        objectReader ar(anchor, where + ", anchor", &error, &warnings);
        out->position = ar.getVec3("position");
        out->roll = ar.getFloat("roll");
        out->pitch = ar.getFloat("pitch");
        out->yaw = ar.getFloat("yaw");
        out->speed = ar.getFloat("speed");
        out->normalForce = ar.getFloat("normalForce");
        out->lateralForce = ar.getFloat("lateralForce");
        ar.finish();
    }

    out->heartline = r.getFloat("heartline");
    out->friction = r.getFloat("friction");
    out->resistance = r.getFloat("resistance");
    out->drawTrack = r.getBool("drawTrack");
    out->drawHeartline = r.getInt("drawHeartline");
    out->style = (trackStyle)r.getName("style", STYLE_NAMES, 0, STYLE_COUNT);
    out->wireframe = r.getBool("wireframe");
    QJsonArray pov = r.getArray("povPosition");
    if(r.ok() && (pov.size() != 2 || !floatFromJson(pov[0], &out->pov.x) || !floatFromJson(pov[1], &out->pov.y))) {
        r.wrongType("povPosition", "a list of two numbers [x, y]");
    }

    QJsonArray sections = r.getArray("sections");
    for(int i = 0; i < sections.size() && error.isEmpty(); ++i) {
        sectionData s;
        if(!readSection(sections[i], QString("%1, section %2").arg(where).arg(i+1), &s)) return false;
        out->sections.append(s);
    }

    QJsonObject smoothing = r.getObject("smoothing");
    if(r.ok()) {
        objectReader smr(smoothing, where + ", smoothing", &error, &warnings);
        readSmoothing(smr, "wholeTrack", false, &out->whole);
        QJsonArray custom = smr.getArray("custom");
        for(int i = 0; i < custom.size() && error.isEmpty(); ++i) {
            QString cw = QString("%1, smoothing, custom %2").arg(where).arg(i+1);
            if(!expectObject(custom[i], cw)) return false;
            smoothData d;
            readSmoothingObject(custom[i].toObject(), cw, true, &d);
            out->custom.append(d);
        }
        smr.finish();
    }
    r.finish();
    return error.isEmpty();
}

bool textReader::readProject(const QJsonObject& root, projectData* out)
{
    objectReader r(root, QString("the project"), &error, &warnings);
    QString format = r.getString("format");
    if(r.ok() && format != QLatin1String(FORMAT_NAME)) {
        r.fail(QString("\"format\" is \"%1\", not \"%2\"").arg(format, FORMAT_NAME));
    }
    int version = r.getInt("formatVersion");
    if(r.ok() && version != FORMAT_VERSION) {
        r.fail(QString("\"formatVersion\" is %1; this program reads version %2").arg(version).arg(FORMAT_VERSION));
    }
    QString binary = r.getString("binaryVersion");
    if(r.ok() && binary != QLatin1String(BINARY_VERSION)) {
        r.fail(QString("\"binaryVersion\" is \"%1\"; this program reads \"%2\"").arg(binary, BINARY_VERSION));
    }
    out->groundTexture = r.getString("groundTexture");
    QJsonArray tracks = r.getArray("tracks");
    for(int i = 0; i < tracks.size() && error.isEmpty(); ++i) {
        trackData t;
        if(!readTrack(tracks[i], QString("track %1").arg(i+1), &t)) return false;
        out->tracks.append(t);
    }
    r.finish();
    return error.isEmpty();
}

} // namespace

// ---------------------------------------------------------------------------
// Reading, step 2: the plain data into the model, as a binary load does it
// ---------------------------------------------------------------------------

// readVec3() (core/exportfuncs.cpp) builds the vector from three readFloat()
// calls given as constructor arguments, and C++ leaves the order in which
// they run to the compiler. Ask once which float lands in x.
static bool readVec3ReadsInOrder()
{
    std::stringstream ss;
    float a = 1.f, b = 2.f, c = 3.f;
    writeBytes(&ss, (const char*)&a, sizeof(float));
    writeBytes(&ss, (const char*)&b, sizeof(float));
    writeBytes(&ss, (const char*)&c, sizeof(float));
    glm::vec3 v = readVec3(&ss);
    return v.x == 1.f;
}

static void writeVec3ForReadVec3(std::stringstream& ss, const glm::vec3& v, bool inOrder)
{
    float f[3] = { v.x, v.y, v.z };
    for(int k = 0; k < 3; ++k) {
        writeBytes(&ss, (const char*)&f[inOrder ? k : 2-k], sizeof(float));
    }
}

// As func::loadFunction() and subfunc::loadSubFunc(): the first piece is
// there already, each further one is appended after its predecessor and then
// filled in. The start values (and a tozero piece's change) are left to the
// section's updateSection(), which sets them from the node before it; a later
// piece starts where appendSubFunction() puts it, at the end of the one before.
static void buildFunction(func* f, const QList<transitionData>& list)
{
    for(int i = 0; i < list.size(); ++i) {
        if(i > 0) f->appendSubFunction(1, i-1);
        subfunc* sub = f->funcList[i];
        const transitionData& t = list[i];
        sub->degree = t.shape;
        if(i == 0) sub->minArgument = t.from;
        sub->maxArgument = t.to;
        sub->arg1 = t.shapeParameter;
        if(t.shape != tozero) sub->symArg = t.change;
        sub->centerArg = t.center;
        sub->tensionArg = t.tension;
        sub->locked = t.locked;
    }
}

// As smoothHandler::loadSmooth().
static void buildSmoothing(smoothHandler* h, const smoothData& d, bool custom)
{
    if(custom) {
        h->sName = d.name;
        h->setFrom(d.fromNode);
        h->setTo(d.toNode);
    }
    h->setLength(d.length);
    h->setIterations(d.iterations);
    h->active = d.active;
    h->update();
}

// As track::loadTrack(), then the rest of readProject()'s work on a track.
static void buildTrack(track* t, const trackData& d, projectFileClient* client)
{
    t->name = d.name;
    for(int i = 0; i < 3; ++i) {
        memcpy((void*)&t->display.colors[i], (const void*)d.colorBytes[i], sizeof(QColor));
    }

    t->startPos = d.position;
    t->anchorNode->fRoll = d.roll;
    t->startPitch = d.pitch;
    t->startYaw = d.yaw;
    t->anchorNode->fVel = d.speed;
    t->anchorNode->forceNormal = d.normalForce;
    t->anchorNode->forceLateral = d.lateralForce;

    t->fHeart = d.heartline;
    t->fFriction = d.friction;
    t->fResistance = d.resistance;

    t->drawTrack = d.drawTrack;
    t->drawHeartline = d.drawHeartline;
    t->style = d.style;
    t->display.bWireframe = d.wireframe;

    t->povPos.x = d.pov.x;
    t->povPos.y = d.pov.y;

    // the same expressions, in the same order, as track::loadTrack()
    t->anchorNode->fEnergy = 0.5f*t->anchorNode->fVel*t->anchorNode->fVel + F_G*t->anchorNode->fPosHearty(0.9*t->fHeart);

    t->anchorNode->changePitch(t->startPitch, false);
    t->anchorNode->setRoll(t->anchorNode->fRoll);

    t->updateAnchorGeometrics();

    t->anchorNode->updateNorm();

    const bool inOrder = readVec3ReadsInOrder();
    for(int i = 0; i < d.sections.size(); ++i) {
        const sectionData& sd = d.sections[i];
        t->appendSectionFromFile(sd.type);
        section* s = t->activeSection;
        switch(sd.type) {
        case straight:
            s->bSpeed = sd.bSpeed;
            s->sName = sd.name;
            s->fVel = sd.fVel;
            buildFunction(s->rollFunc, sd.roll);
            break;
        case curved:
            s->bSpeed = sd.bSpeed;
            s->sName = sd.name;
            s->fVel = sd.fVel;
            s->fRadius = sd.fRadius;
            s->fDirection = sd.fDirection;
            s->fLeadIn = sd.fLeadIn;
            s->fLeadOut = sd.fLeadOut;
            s->bOrientation = sd.bOrientation;
            buildFunction(s->rollFunc, sd.roll);
            break;
        case forced:
        case geometric:
            s->bSpeed = sd.bSpeed;
            s->sName = sd.name;
            s->fVel = sd.fVel;
            if(sd.hasTime) s->iTime = sd.iTime;
            s->bOrientation = sd.bOrientation;
            s->bArgument = sd.bArgument;
            buildFunction(s->rollFunc, sd.roll);
            buildFunction(s->normForce, sd.second);
            buildFunction(s->latForce, sd.third);
            break;
        case bezier:
            s->sName = sd.name;
            for(int k = 0; k < sd.points.size(); ++k) {
                bezier_t* b = new bezier_t;
                b->P1 = sd.points[k].point;
                b->Kp1 = sd.points[k].control1;
                b->Kp2 = sd.points[k].control2;
                b->contRoll = sd.points[k].continuousRoll;
                b->relRoll = sd.points[k].relativeRoll;
                b->roll = sd.points[k].roll;
                b->fVel = 0;
                s->bezList.append(b);
            }
            for(int k = 0; k < sd.supports.size(); ++k) {
                s->supList.append(sd.supports[k]);
            }
            break;
        case nolimitscsv: {
            // secnlcsv keeps its points private; hand them over through its
            // own (stringstream) load function, in the layout it reads.
            std::stringstream ss;
            int count = sd.csvPoints.size();
            writeBytes(&ss, (const char*)&count, sizeof(int));
            for(int k = 0; k < count; ++k) {
                writeVec3ForReadVec3(ss, sd.csvPoints[k].position, inOrder);
                writeVec3ForReadVec3(ss, sd.csvPoints[k].direction, inOrder);
                writeVec3ForReadVec3(ss, sd.csvPoints[k].lateral, inOrder);
            }
            s->loadSection(ss);
            break;
        }
        default:
            break;
        }
        s->updateSection();
    }

    // entry 0 the whole track, 1..n the sections, then the custom regions
    buildSmoothing(t->smoothList[0], d.whole, false);
    for(int i = 0; i < d.sections.size(); ++i) {
        buildSmoothing(t->smoothList[i+1], d.sections[i].smooth, false);
    }
    for(int i = 0; i < d.custom.size(); ++i) {
        int index = d.sections.size() + 1 + i;
        if(index >= t->smoothList.size()) t->smoothList.append(new smoothHandler(t, -2));
        buildSmoothing(t->smoothList[index], d.custom[i], true);
    }

    t->updateTrack(0, 0);
    if(t->mListener != NULL)
        t->mListener->trackLoaded(t);

    // as readProject() does after track::loadTrack()
    t->bAutoSmooth = true;
    char customChar;
    t->updateSmoothEntries(&customChar);
    client->projectTrackSmoothingUpdated(t);
    t->applyRollSmooth();
}

QString readProjectText(const QByteArray& in, projectFileClient* client)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(in, &parseError);
    if(doc.isNull()) {
        // the line, counted from the offset Qt gives
        int line = 1 + in.left(parseError.offset).count('\n');
        return QString("Error: the text is not valid JSON (line %1: %2).").arg(line).arg(parseError.errorString());
    }
    if(!doc.isObject()) {
        return QString("Error: the project: the text must be one object { ... }.");
    }

    textReader reader;
    projectData data;
    if(!reader.readProject(doc.object(), &data)) {
        return reader.error;
    }

    QStringList warnings = reader.warnings;
    client->projectCleared();
    if(!client->projectGroundTexture(data.groundTexture)) {
        warnings.prepend(QString("Could not find corresponding texture. Loaded default texture instead."));
    }
    for(int i = 0; i < data.tracks.size(); ++i) {
        track* t = client->projectNewTrack();
        buildTrack(t, data.tracks[i], client);
        client->projectTrackLoaded(t);
    }
    client->projectLoadFinished();

    if(!warnings.isEmpty()) {
        return QString("Warning: ") + warnings.join(QString(" "));
    }
    return QString("Load Successful.");
}

QString saveProjectTextFile(const QString& fileName, const QString& texPath, const QList<track*>& tracks, projectFileClient* client)
{
    QByteArray text;
    QString result = writeProjectText(text, texPath, tracks, client);
    if(result.startsWith("Error")) return result;

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly)) {
        return QString("Error: File is NULL");
    }
    if(file.write(text) != text.size()) {
        return QString("Error: the file could not be written completely.");
    }
    file.close();
    return result;
}

QString loadProjectTextFile(const QString& fileName, projectFileClient* client)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        return QString("Error: File is NULL");
    }
    QByteArray text = file.readAll();
    file.close();
    return readProjectText(text, client);
}
