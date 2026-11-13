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

// ---------------------------------------------------------------------------
// The ranges a text is checked against (docs/project-text-format.md,
// "Checked ranges"). Where a box of the windows sets the value, the range is
// the box's, widened to cover every display unit (a speed box allows 0.1 to
// 500 in km/h, mph or m/s, so 0.025 to 500 m/s). The others keep the model
// out of what it was never built for: a division by zero, a loop that does
// not end, or more nodes than memory holds.
// ---------------------------------------------------------------------------

static const float LIMIT_SPEED_MIN = 0.025f;      // m/s, 0.1 km/h
static const float LIMIT_SPEED_MAX = 500.f;       // m/s
static const float LIMIT_RADIUS_MIN = 0.3f;       // m, 1 foot
static const float LIMIT_RADIUS_MAX = 500.f;      // m
static const float LIMIT_LEAD_MAX = 500.f;        // degrees
static const float LIMIT_TIMEWARP = 10.f;         // centre and tension
static const float LIMIT_HEARTLINE = 5.f;         // m, either way
static const float LIMIT_FRICTION_MAX = 2.f;
static const float LIMIT_RESISTANCE_MAX = 1e-3f;  // the box shows 100 for 1e-3
static const int LIMIT_DRAW_HEARTLINE = 2;        // "Heartline only"
static const int LIMIT_SMOOTH_LENGTH = 6000;      // nodes, the dialog's 6 s
static const int LIMIT_SMOOTH_ITERATIONS = 16;
static const int LIMIT_SMOOTH_NODE = 10000000;    // a custom region's node numbers
static const float LIMIT_TIME_END = 2000.f;       // s, a time-based function's end
static const float LIMIT_DISTANCE_END = 10000.f;  // m, a straight's or distance-based function's end
static const float LIMIT_CURVE_END = 5000.f;      // degrees, a curve's roll function's end (the angle box)
static const double LIMIT_NODES = 2000000.;       // nodes a section may need at a held speed
static const double LIMIT_BEZIER_LENGTH = 20000.; // m, the length of a Bezier section's control polygon

namespace projectText {

qint64 floatSteps(float a, float b)
{
    qint32 ia, ib;
    memcpy(&ia, &a, sizeof(float));
    memcpy(&ib, &b, sizeof(float));
    // the bit patterns of negative floats run backwards; fold them so that
    // the integers are in the order of the floats
    const qint64 lowest = std::numeric_limits<qint32>::min();
    qint64 la = ia < 0 ? lowest - ia : (qint64)ia;
    qint64 lb = ib < 0 ? lowest - ib : (qint64)ib;
    return la > lb ? la - lb : lb - la;
}

// A binary load keeps the last transition's start as the file has it while
// the end before is worked out again; measured, they stay within a few steps
// (tests/projecttext, the open-and-save cycles). The tolerance is far above
// that, so no model the program makes can be written but not read back.
bool fromWithinTolerance(float from, float endBefore)
{
    return std::isfinite(from) && std::isfinite(endBefore) && floatSteps(from, endBefore) <= FROM_TOLERANCE_STEPS;
}

bool shapeUsesParameter(int shape)
{
    return shape == quadratic || shape == quartic || shape == quintic || shape == plateau;
}

bool shapeParameterAccepted(int shape, float value)
{
    return !shapeUsesParameter(shape) || std::isfinite(value);
}

}

using projectText::fromWithinTolerance;

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
        // every piece's start is stored: a later one normally equals the end
        // of the one before, but a loaded model can hold it a last digit away.
        // A model the reader would refuse is not written.
        if(i > 0 && !fromWithinTolerance(sub->minArgument, f->funcList[i-1]->maxArgument)) {
            error = QString("Error: %1: it starts at %2 but the one before ends at %3, more than %4 float "
                            "steps apart; the text could not be read back.").arg(here)
                    .arg(sub->minArgument, 0, 'g', 9).arg(f->funcList[i-1]->maxArgument, 0, 'g', 9)
                    .arg(projectText::FROM_TOLERANCE_STEPS);
            return false;
        }
        if(!projectText::shapeParameterAccepted(shape, sub->arg1)) {
            error = QString("Error: %1: its shape parameter is %2, which a %3 transition cannot use; "
                            "the text could not be read back.").arg(here).arg(sub->arg1).arg(SHAPE_NAMES[shape]);
            return false;
        }
        o["from"] = floatToJson(sub->minArgument);
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
        // project file's) hands them over: "CSV", the count, then position,
        // direction and lateral, nine floats a point.
        std::stringstream ss;
        s->saveSection(static_cast<std::iostream&>(ss));
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

static QString checkProjectText(const QByteArray& text);

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

    QByteArray text = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if(!text.endsWith('\n')) text.append('\n');

    // Never hand out a text the reader would refuse: run its checks (step 1
    // of reading, no model is built) on the text just made.
    QString refused = checkProjectText(text);
    if(!refused.isEmpty()) {
        return QString("Error: the project cannot be written as text, because the text would not read back: ")
                + refused.mid(refused.startsWith("Error: ") ? 7 : 0);
    }
    out = text;
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

    // Every number read as an input must be finite: the model's loops and
    // divisions were never meant for infinity or NaN (a number too large for
    // a float becomes infinity, so it is refused here too).
    float getFloat(const char* key)
    {
        QJsonValue v = value(key);
        float f = 0.f;
        if(!ok()) return f;
        if(!floatFromJson(v, &f)) {
            wrongType(key, "a number");
        } else if(!std::isfinite(f)) {
            wrongType(key, "a finite number (not infinity or NaN, and within the range of a float)");
        }
        return f;
    }

    // A number that the model stores but never computes with: infinity and
    // NaN are kept.
    float getAnyFloat(const char* key)
    {
        QJsonValue v = value(key);
        float f = 0.f;
        if(ok() && !floatFromJson(v, &f)) wrongType(key, "a number");
        return f;
    }

    // A number that must lie within [lo, hi]; unit is for the message.
    float getFloatIn(const char* key, float lo, float hi, const char* unit)
    {
        float f = getFloat(key);
        if(ok() && !(f >= lo && f <= hi)) {
            outOfRange(key, QString::number(f), QString::number(lo), QString::number(hi), unit);
        }
        return f;
    }

    void outOfRange(const char* key, const QString& value, const QString& lo, const QString& hi, const char* unit)
    {
        QString u = (unit && *unit) ? QString(" ") + unit : QString();
        fail(QString("\"%1\" is %2, which is outside the allowed range %3 to %4%5").arg(key, value, lo, hi, u));
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

    int getIntIn(const char* key, int lo, int hi, const char* unit)
    {
        int i = getInt(key);
        if(ok() && (i < lo || i > hi)) {
            outOfRange(key, QString::number(i), QString::number(lo), QString::number(hi), unit);
        }
        return i;
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
        if(a.size() != 3 || !floatFromJson(a[0], &r.x) || !floatFromJson(a[1], &r.y) || !floatFromJson(a[2], &r.z)
                || !std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.z)) {
            wrongType(key, "a list of three finite numbers [x, y, z]");
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
    void readFunction(objectReader& parent, const char* key, float maxEnd, const char* unit,
                      bool lockable, QList<transitionData>* out);
    void readSpeed(objectReader& r, sectionData* out);
    bool readSection(const QJsonValue& v, const QString& where, float anchorSpeed, sectionData* out);
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
    // the smoothing dialog's boxes: length 0 to 6 s, 1 to 16 iterations;
    // track::applyRollSmoothFilter() divides the length by the iterations
    out->length = r.getIntIn("length", 0, LIMIT_SMOOTH_LENGTH, "nodes");
    out->iterations = r.getIntIn("iterations", 1, LIMIT_SMOOTH_ITERATIONS, "");
    out->active = r.getBool("active");
    if(custom) {
        out->name = r.getString("name");
        // the filter allocates toNode - fromNode values; nodes past the
        // track's end are harmless (track::getPoint() stops at the last one)
        out->fromNode = r.getIntIn("fromNode", 0, LIMIT_SMOOTH_NODE, "");
        out->toNode = r.getIntIn("toNode", 0, LIMIT_SMOOTH_NODE, "");
        if(r.ok() && out->toNode < out->fromNode) {
            r.fail(QString("\"toNode\" is %1, before \"fromNode\" %2; the region must not run backwards")
                   .arg(out->toNode).arg(out->fromNode));
        }
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

void textReader::readFunction(objectReader& parent, const char* key, float maxEnd, const char* unit,
                              bool lockable, QList<transitionData>* out)
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
        // A function starts at 0 (func::appendSubFunction(), removeSubFunction())
        // and each transition ends after the one before: a zero or negative
        // length divides by zero in subfunc::getValue().
        // A later transition starts where the one before ends. Its "from" is
        // stored all the same, because a loaded model can hold it a last digit
        // or two away (docs/project-text-format.md, "A transition"); left out,
        // it is the end before. A text of the first version has it under
        // recalculatedOnLoad, which is not read, so it starts there too.
        if(i == 0) {
            t.from = r.getFloat("from");
            if(r.ok() && t.from != 0.f) {
                r.fail(QString("\"from\" is %1; a function starts at 0").arg(t.from));
            }
        } else {
            float before = out->last().to;
            t.from = before;
            if(r.has("from")) {
                t.from = r.getFloat("from");
                if(r.ok() && !fromWithinTolerance(t.from, before)) {
                    r.fail(QString("transition %1 starts at %2 but the one before ends at %3; "
                                   "make them equal or leave \"from\" out")
                           .arg(i+1).arg(t.from, 0, 'g', 9).arg(before, 0, 'g', 9));
                }
            }
        }
        float start = t.from;
        t.to = r.getFloat("to");
        if(r.ok() && !(t.to > start)) {
            r.fail(QString("\"to\" is %1, which is not after %2 where the transition starts; "
                           "a transition must have a length above 0").arg(t.to).arg(start));
        }
        if(r.ok() && t.to > maxEnd) {
            r.fail(QString("\"to\" is %1, past the largest end this section may have, %2 %3")
                   .arg(t.to).arg(maxEnd).arg(unit));
        }
        t.change = (t.shape == tozero) ? 0.f : r.getFloat("change");
        // finite where the shape reads it; elsewhere kept as it is, infinity
        // and NaN included (projectText::shapeParameterAccepted())
        if(projectText::shapeUsesParameter(t.shape)) {
            t.shapeParameter = r.getFloat("shapeParameter");
        } else {
            t.shapeParameter = r.getAnyFloat("shapeParameter");
        }
        // the transition editor's boxes
        t.center = r.getFloatIn("center", -LIMIT_TIMEWARP, LIMIT_TIMEWARP, "");
        t.tension = r.getFloatIn("tension", -LIMIT_TIMEWARP, LIMIT_TIMEWARP, "");
        t.locked = r.getBool("locked");
        if(r.ok() && t.locked && !lockable) {
            r.fail("\"locked\" is true, but a transition of a straight or a curve cannot be locked");
        }
        r.ignore("recalculatedOnLoad");
        r.finish();
        out->append(t);
    }
}

// A held speed is what the section's nodes move at, so it must be above zero:
// a force section over distance at 0 m/s never ends. With the speed from the
// physics the stored value is not used.
void textReader::readSpeed(objectReader& r, sectionData* out)
{
    out->bSpeed = r.getChoice("speedMode", "fixed", "physics");
    if(!r.ok()) return;
    if(out->bSpeed) {
        out->fVel = r.getFloat("speed");
    } else {
        out->fVel = r.getFloatIn("speed", LIMIT_SPEED_MIN, LIMIT_SPEED_MAX, "m/s");
    }
}

static float functionEnd(const QList<transitionData>& f)
{
    return f.isEmpty() ? 0.f : f.last().to;
}

static bool anyLocked(const QList<transitionData>& f)
{
    for(int i = 0; i < f.size(); ++i) {
        if(f[i].locked) return true;
    }
    return false;
}

bool textReader::readSection(const QJsonValue& v, const QString& where, float anchorSpeed, sectionData* out)
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
        readSpeed(sr, out);
        readFunction(sr, "roll", LIMIT_DISTANCE_END, "m", false, &out->roll);
        if(sr.ok() && !out->bSpeed && (double)functionEnd(out->roll)*F_HZ/out->fVel > LIMIT_NODES) {
            sr.fail(QString("%1 m at the held speed of %2 m/s needs more than %3 nodes")
                    .arg(functionEnd(out->roll)).arg(out->fVel).arg(LIMIT_NODES, 0, 'f', 0));
        }
        break;
    case curved:
        readSpeed(sr, out);
        // a radius of zero or below makes the curve turn the wrong way or not
        // at all, and seccurved::updateSection() then never reaches its angle
        out->fRadius = sr.getFloatIn("radius", LIMIT_RADIUS_MIN, LIMIT_RADIUS_MAX, "m");
        out->fDirection = sr.getFloat("direction");
        out->fLeadIn = sr.getFloatIn("leadIn", 0.f, LIMIT_LEAD_MAX, "degrees");
        out->fLeadOut = sr.getFloatIn("leadOut", 0.f, LIMIT_LEAD_MAX, "degrees");
        out->bOrientation = sr.getChoice("orientation", "quaternion", "euler") ? EULER : QUATERNION;
        readFunction(sr, "roll", LIMIT_CURVE_END, "degrees", false, &out->roll);
        if(sr.ok() && !out->bSpeed
                && (double)functionEnd(out->roll)*F_PI/180.*out->fRadius*F_HZ/out->fVel > LIMIT_NODES) {
            sr.fail(QString("a %1 degree curve of %2 m radius at the held speed of %3 m/s needs more than %4 nodes")
                    .arg(functionEnd(out->roll)).arg(out->fRadius).arg(out->fVel).arg(LIMIT_NODES, 0, 'f', 0));
        }
        break;
    case forced:
    case geometric: {
        readSpeed(sr, out);
        out->bOrientation = sr.getChoice("orientation", "quaternion", "euler") ? EULER : QUATERNION;
        out->bArgument = sr.getChoice("argument", "time", "distance") ? DISTANCE : TIME;
        if(sr.ok() && out->bArgument == DISTANCE) {
            out->hasTime = true;
            out->iTime = sr.getInt("timeNodes");   // kept, never used
        }
        if(!sr.ok()) break;
        float maxEnd = out->bArgument == DISTANCE ? LIMIT_DISTANCE_END : LIMIT_TIME_END;
        const char* unit = out->bArgument == DISTANCE ? "m" : "s";
        readFunction(sr, "roll", maxEnd, unit, true, &out->roll);
        if(out->type == forced) {
            readFunction(sr, "normalForce", maxEnd, unit, true, &out->second);
            readFunction(sr, "lateralForce", maxEnd, unit, true, &out->third);
        } else {
            readFunction(sr, "pitchRate", maxEnd, unit, true, &out->second);
            readFunction(sr, "yawRate", maxEnd, unit, true, &out->third);
        }
        if(!sr.ok()) break;
        // the section ends where the shortest unlocked function ends; with all
        // three locked there is no end (secforced::getMaxArgument()), and the
        // windows never allow it (isLockable())
        if(anyLocked(out->roll) && anyLocked(out->second) && anyLocked(out->third)) {
            sr.fail("every function has a locked transition, so the section has no end; "
                    "at least one function must have none");
            break;
        }
        float end = qMax(functionEnd(out->roll), qMax(functionEnd(out->second), functionEnd(out->third)));
        if(out->bArgument == DISTANCE && !out->bSpeed && (double)end*F_HZ/out->fVel > LIMIT_NODES) {
            sr.fail(QString("%1 m at the held speed of %2 m/s needs more than %3 nodes")
                    .arg(end).arg(out->fVel).arg(LIMIT_NODES, 0, 'f', 0));
        }
        break;
    }
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
                    || !floatFromJson(s[1], &p.y) || !floatFromJson(s[2], &p.z)
                    || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                sr.fail(QString("support %1 must be a list of three finite numbers [x, y, z]").arg(i+1));
                return false;
            }
            out->supports.append(p);
        }
        // secbezier::updateSection() makes up to 1000 nodes a metre along
        // the curve, which its control polygon bounds
        if(sr.ok()) {
            double length = 0.;
            for(int i = 0; i + 1 < out->points.size(); ++i) {
                const bezierData& a = out->points[i];
                const bezierData& b = out->points[i+1];
                length += glm::distance(glm::dvec3(a.point), glm::dvec3(a.control2))
                        + glm::distance(glm::dvec3(a.control2), glm::dvec3(b.control1))
                        + glm::distance(glm::dvec3(b.control1), glm::dvec3(b.point));
            }
            if(length > LIMIT_BEZIER_LENGTH) {
                sr.fail(QString("the points span %1 m along their control polygon; at most %2 m")
                        .arg(length).arg(LIMIT_BEZIER_LENGTH, 0, 'f', 0));
            }
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
        // secnlcsv::updateSection() places a node every (anchor speed / 1000) m
        if(sr.ok()) {
            double length = 0.;
            for(int i = 1; i < out->csvPoints.size(); ++i) {
                length += glm::distance(glm::dvec3(out->csvPoints[i-1].position), glm::dvec3(out->csvPoints[i].position));
            }
            if(length*F_HZ/anchorSpeed > LIMIT_NODES) {
                sr.fail(QString("the points span %1 m, which at the anchor speed of %2 m/s needs more than %3 nodes")
                        .arg(length).arg(anchorSpeed).arg(LIMIT_NODES, 0, 'f', 0));
            }
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
        // the anchor speed box; track::updateAnchorGeometrics() divides by it
        // and a NoLimits CSV section spaces its nodes by it
        out->speed = ar.getFloatIn("speed", LIMIT_SPEED_MIN, LIMIT_SPEED_MAX, "m/s");
        out->normalForce = ar.getFloat("normalForce");
        out->lateralForce = ar.getFloat("lateralForce");
        ar.finish();
    }
    if(!error.isEmpty()) return false;

    // the Track Properties boxes
    out->heartline = r.getFloatIn("heartline", -LIMIT_HEARTLINE, LIMIT_HEARTLINE, "m");
    out->friction = r.getFloatIn("friction", 0.f, LIMIT_FRICTION_MAX, "");
    out->resistance = r.getFloatIn("resistance", 0.f, LIMIT_RESISTANCE_MAX, "");
    out->drawTrack = r.getBool("drawTrack");
    out->drawHeartline = r.getIntIn("drawHeartline", 0, LIMIT_DRAW_HEARTLINE, "");
    out->style = (trackStyle)r.getName("style", STYLE_NAMES, 0, STYLE_COUNT);
    out->wireframe = r.getBool("wireframe");
    QJsonArray pov = r.getArray("povPosition");
    if(r.ok() && (pov.size() != 2 || !floatFromJson(pov[0], &out->pov.x) || !floatFromJson(pov[1], &out->pov.y)
                  || !std::isfinite(out->pov.x) || !std::isfinite(out->pov.y))) {
        r.wrongType("povPosition", "a list of two finite numbers [x, y]");
    }

    QJsonArray sections = r.getArray("sections");
    for(int i = 0; i < sections.size() && error.isEmpty(); ++i) {
        sectionData s;
        if(!readSection(sections[i], QString("%1, section %2").arg(where).arg(i+1), out->speed, &s)) return false;
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

// The reader's checks alone (step 1): the error it would give, or an empty
// text when it would accept the text.
static QString checkProjectText(const QByteArray& text)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(text, &parseError);
    if(doc.isNull() || !doc.isObject()) {
        return QString("Error: the text is not one valid JSON object (%1).").arg(parseError.errorString());
    }
    textReader reader;
    projectData data;
    if(!reader.readProject(doc.object(), &data)) return reader.error;
    return QString();
}

// ---------------------------------------------------------------------------
// Reading, step 2: the plain data into the model, as a binary load does it
// ---------------------------------------------------------------------------

// x, y and z as three floats, in the order secnlcsv::loadSection() reads them
static void writeVec3InOrder(std::iostream& io, const glm::vec3& v)
{
    float f[3] = { v.x, v.y, v.z };
    for(int k = 0; k < 3; ++k) {
        writeBytes(&io, (const char*)&f[k], sizeof(float));
    }
}

// As func::loadFunction() and subfunc::loadSubFunc(): the first piece is
// there already, each further one is appended after its predecessor and then
// filled in. The start values (and a tozero piece's change) are left to the
// section's updateSection(), which sets them from the node before it.
//
// The ends are not left to appendSubFunction(): it moves every later piece
// with (end before + end - start), which in single precision is not always
// the end it started from (4.9999986 to 5.999999 becomes 5.9999995), so a
// model built that way can differ from the one the text was written from.
// Once every piece exists (built as before, so that the values on the way are
// those of a binary load), each start and end is set to the text's value as
// it is, with no arithmetic; nothing later in the build moves them (a locked
// piece is stretched to the section's end on every update, the same way in
// the model the text came from).
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
    for(int i = 0; i < list.size(); ++i) {
        f->funcList[i]->minArgument = list[i].from;
        f->funcList[i]->maxArgument = list[i].to;
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

    // the anchor node from these values, as track::loadTrack() builds it
    t->rebuildAnchor();

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
            // own load function (the project file's), in the layout it reads:
            // the count, then x, y and z of each vector as three floats.
            std::stringstream ss;
            std::iostream& io = ss;
            int count = sd.csvPoints.size();
            writeBytes(&io, (const char*)&count, sizeof(int));
            for(int k = 0; k < count; ++k) {
                writeVec3InOrder(io, sd.csvPoints[k].position);
                writeVec3InOrder(io, sd.csvPoints[k].direction);
                writeVec3InOrder(io, sd.csvPoints[k].lateral);
            }
            s->loadSection(io);
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
