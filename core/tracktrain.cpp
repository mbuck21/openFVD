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

#include "tracktrain.h"
#include "track.h"
#include "projecttext.h"

#include <QJsonArray>
#include <QJsonValue>
#include <cmath>
#include <cstring>

trackTrain::trackTrain() : bSimulate(false), bExtendedAirtime(false)
{
}

bool trackTrain::operator==(const trackTrain& o) const
{
    return bSimulate == o.bSimulate && bExtendedAirtime == o.bExtendedAirtime && spec == o.spec && lAssumed == o.lAssumed;
}

QString trainAdditionsKey()
{
    return QStringLiteral("train");
}

QStringList trainRestraintWords()
{
    return QStringList() << "" << "lapbar" << "otsr" << "vest" << "prone" << "other";
}

namespace {

// The train's own keys inside "train"; any other key is kept as it was.
const char* const ownKeys[] = {
    "simulate", "type", "cars", "carPitch", "rows", "seats", "seatUp", "carMass", "riderMass", "load",
    "carMassFactors", "massAtDesignPoint", "massAboveRail", "resistance", "referenceCar", "restraint",
    "extendedAirtime", "assumed"
};
const char* const ownResistanceKeys[] = { "fromProject", "rolling", "rollingLoad", "linear", "drag" };

// The same float, bit for bit (so -0 is not 0).
bool sameFloat(float a, float b)
{
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}

bool sameList(const QList<float>& a, const QList<float>& b)
{
    if(a.size() != b.size()) return false;
    for(int i = 0; i < a.size(); ++i) if(!sameFloat(a.at(i), b.at(i))) return false;
    return true;
}

// A float as the text form writes one (projectText::floatToJson()): the
// shortest decimal that reads back as the same float (0.9f is written 0.9,
// not 0.89999997615814209), and -0 as the string "-0", which a JSON number
// cannot carry. Reading it gives the float back exactly.
QJsonValue floatJson(float f)
{
    return projectText::floatToJson(f);
}

// The reverse, for a finite float only.
bool floatFrom(const QJsonValue& v, float* out)
{
    float f;
    if(!projectText::floatFromJson(v, &f) || !std::isfinite(f)) return false;
    *out = f;
    return true;
}

QJsonArray floatListJson(const QList<float>& list)
{
    QJsonArray a;
    for(int i = 0; i < list.size(); ++i) a.append(floatJson(list.at(i)));
    return a;
}

// Readers: true and *out set when json[key] is there and of the right kind;
// a key that is not there leaves *out and returns true; a key of the wrong
// kind leaves *out, names it in problems and returns false.
bool readBool(const QJsonObject& json, const char* key, bool* out, QStringList* problems)
{
    QJsonValue v = json.value(key);
    if(v.isUndefined()) return true;
    if(!v.isBool()) { if(problems) *problems << QString("\"%1\" is not true or false").arg(key); return false; }
    *out = v.toBool();
    return true;
}

bool readFloat(const QJsonObject& json, const char* key, float low, float high, float* out, QStringList* problems)
{
    QJsonValue v = json.value(key);
    if(v.isUndefined()) return true;
    float f;
    if(!floatFrom(v, &f) || f < low || f > high) {
        if(problems) *problems << QString("\"%1\" is not a number from %2 to %3").arg(key).arg(low).arg(high);
        return false;
    }
    *out = f;
    return true;
}

bool readWhole(const QJsonObject& json, const char* key, int low, int high, int* out, QStringList* problems)
{
    QJsonValue v = json.value(key);
    if(v.isUndefined()) return true;
    double d = v.toDouble();
    if(!v.isDouble() || !std::isfinite(d) || std::floor(d) != d || d < low || d > high) {
        if(problems) *problems << QString("\"%1\" is not a whole number from %2 to %3").arg(key).arg(low).arg(high);
        return false;
    }
    *out = (int)d;
    return true;
}

bool readFloatList(const QJsonObject& json, const char* key, bool positive, QList<float>* out, QStringList* problems)
{
    QJsonValue v = json.value(key);
    if(v.isUndefined()) return true;
    QList<float> list;
    bool ok = v.isArray() && !v.toArray().isEmpty();
    QJsonArray a = v.toArray();
    for(int i = 0; ok && i < a.size(); ++i) {
        float f = 0.f;
        ok = floatFrom(a.at(i), &f) && std::fabs(f) <= 1000.f && (!positive || f > 0.f);
        list << f;
    }
    if(!ok) {
        if(problems) *problems << QString("\"%1\" is not a list of %2numbers").arg(key).arg(positive ? "positive " : "");
        return false;
    }
    *out = list;
    return true;
}

} // namespace

trackTrain trainFromJson(const QJsonObject& json, QStringList* problems)
{
    trackTrain t;
    trainSpec& s = t.spec;
    readBool(json, "simulate", &t.bSimulate, problems);
    readBool(json, "extendedAirtime", &t.bExtendedAirtime, problems);
    QJsonValue type = json.value("type");
    if(type.isString()) s.sTypeId = type.toString();
    else if(!type.isUndefined() && problems) *problems << "\"type\" is not a text";
    readWhole(json, "cars", 1, 1000, &s.iCars, problems);
    readFloat(json, "carPitch", 0.f, 1000.f, &s.fCarPitch, problems);
    readFloatList(json, "rows", false, &s.lRowOffsets, problems);
    readFloatList(json, "seats", false, &s.lSeatOffsets, problems);
    readFloat(json, "seatUp", -1000.f, 1000.f, &s.fSeatUp, problems);
    readFloat(json, "carMass", 0.f, 1e9f, &s.fCarMass, problems);
    readFloat(json, "riderMass", 0.f, 1e9f, &s.fRiderMass, problems);
    readFloat(json, "load", 0.f, 1.f, &s.fLoad, problems);
    readFloatList(json, "carMassFactors", true, &s.lCarMassFactor, problems);
    readBool(json, "massAtDesignPoint", &s.bMassAtDesignPoint, problems);
    readFloat(json, "massAboveRail", -1000.f, 1000.f, &s.fMassAboveRail, problems);
    QJsonValue res = json.value("resistance");
    if(res.isObject()) {
        QJsonObject r = res.toObject();
        readBool(r, "fromProject", &s.resistance.bFromProject, problems);
        readFloat(r, "rolling", 0.f, 1000.f, &s.resistance.fRolling, problems);
        readFloat(r, "rollingLoad", 0.f, 1000.f, &s.resistance.fRollingLoad, problems);
        readFloat(r, "linear", 0.f, 1000.f, &s.resistance.fLinear, problems);
        readFloat(r, "drag", 0.f, 1000.f, &s.resistance.fDrag, problems);
    } else if(!res.isUndefined() && problems) {
        *problems << "\"resistance\" is not an object";
    }
    int reference = 0;   // 1-based in the file, 0 = the train's centre
    if(readWhole(json, "referenceCar", 1, s.iCars, &reference, problems)) s.iReferenceCar = reference - 1;
    QJsonValue restraint = json.value("restraint");
    if(restraint.isString() && trainRestraintWords().contains(restraint.toString())) s.sRestraint = restraint.toString();
    else if(!restraint.isUndefined() && problems) *problems << "\"restraint\" is not one of lapbar, otsr, vest, prone, other";
    QJsonValue assumed = json.value("assumed");
    if(assumed.isArray()) {
        QJsonArray a = assumed.toArray();
        for(int i = 0; i < a.size(); ++i) {
            if(a.at(i).isString()) t.lAssumed << a.at(i).toString();
            else if(problems) *problems << "\"assumed\" holds something that is not a name";
        }
    } else if(!assumed.isUndefined() && problems) {
        *problems << "\"assumed\" is not a list";
    }
    QString why;
    if(!s.isValid(&why) && problems) *problems << why;
    return t;
}

QJsonObject trainToJson(const trackTrain& train, const QJsonObject& keep)
{
    QJsonObject o = keep;
    for(size_t i = 0; i < sizeof(ownKeys)/sizeof(ownKeys[0]); ++i) o.remove(ownKeys[i]);
    const trainSpec& s = train.spec;
    const trainSpec d;   // the defaults
    o["simulate"] = train.bSimulate;
    if(!s.sTypeId.isEmpty()) o["type"] = s.sTypeId;
    if(s.iCars != d.iCars) o["cars"] = s.iCars;
    if(!sameFloat(s.fCarPitch, d.fCarPitch)) o["carPitch"] = floatJson(s.fCarPitch);
    if(!sameList(s.lRowOffsets, d.lRowOffsets)) o["rows"] = floatListJson(s.lRowOffsets);
    if(!sameList(s.lSeatOffsets, d.lSeatOffsets)) o["seats"] = floatListJson(s.lSeatOffsets);
    if(!sameFloat(s.fSeatUp, d.fSeatUp)) o["seatUp"] = floatJson(s.fSeatUp);
    if(!sameFloat(s.fCarMass, d.fCarMass)) o["carMass"] = floatJson(s.fCarMass);
    if(!sameFloat(s.fRiderMass, d.fRiderMass)) o["riderMass"] = floatJson(s.fRiderMass);
    if(!sameFloat(s.fLoad, d.fLoad)) o["load"] = floatJson(s.fLoad);
    if(!sameList(s.lCarMassFactor, d.lCarMassFactor)) o["carMassFactors"] = floatListJson(s.lCarMassFactor);
    if(s.bMassAtDesignPoint != d.bMassAtDesignPoint) o["massAtDesignPoint"] = s.bMassAtDesignPoint;
    if(!sameFloat(s.fMassAboveRail, d.fMassAboveRail)) o["massAboveRail"] = floatJson(s.fMassAboveRail);

    // the resistance, with any unknown key inside it kept
    const rideResistance& r = s.resistance;
    const rideResistance& rd = d.resistance;
    QJsonObject ro = keep.value("resistance").toObject();
    for(size_t i = 0; i < sizeof(ownResistanceKeys)/sizeof(ownResistanceKeys[0]); ++i) ro.remove(ownResistanceKeys[i]);
    bool resistanceDefault = r.bFromProject == rd.bFromProject && sameFloat(r.fRolling, rd.fRolling)
            && sameFloat(r.fRollingLoad, rd.fRollingLoad) && sameFloat(r.fLinear, rd.fLinear) && sameFloat(r.fDrag, rd.fDrag);
    if(!resistanceDefault || !ro.isEmpty()) {
        ro["fromProject"] = r.bFromProject;
        if(!sameFloat(r.fRolling, rd.fRolling)) ro["rolling"] = floatJson(r.fRolling);
        if(!sameFloat(r.fRollingLoad, rd.fRollingLoad)) ro["rollingLoad"] = floatJson(r.fRollingLoad);
        if(!sameFloat(r.fLinear, rd.fLinear)) ro["linear"] = floatJson(r.fLinear);
        if(!sameFloat(r.fDrag, rd.fDrag)) ro["drag"] = floatJson(r.fDrag);
        o["resistance"] = ro;
    }

    if(s.iReferenceCar >= 0) o["referenceCar"] = s.iReferenceCar + 1;
    if(!s.sRestraint.isEmpty()) o["restraint"] = s.sRestraint;
    if(train.bExtendedAirtime) o["extendedAirtime"] = true;
    if(!train.lAssumed.isEmpty()) o["assumed"] = QJsonArray::fromStringList(train.lAssumed);
    return o;
}

bool trackHasTrain(const track* t)
{
    return t != nullptr && t->additions.contains(trainAdditionsKey());
}

trackTrain trainOfTrack(const track* t, QStringList* problems)
{
    if(!trackHasTrain(t)) return trackTrain();
    QJsonValue v = t->additions.value(trainAdditionsKey());
    if(!v.isObject()) {
        if(problems) *problems << "the track's \"train\" is not an object";
        return trackTrain();
    }
    return trainFromJson(v.toObject(), problems);
}

void setTrainOfTrack(track* t, const trackTrain& train)
{
    QJsonObject keep = t->additions.value(trainAdditionsKey()).toObject();
    // what is not the train's own: any key but the train's, and inside
    // "resistance" any key but its own
    QJsonObject unknown = keep;
    for(size_t i = 0; i < sizeof(ownKeys)/sizeof(ownKeys[0]); ++i) unknown.remove(ownKeys[i]);
    QJsonObject unknownResistance = keep.value("resistance").toObject();
    for(size_t i = 0; i < sizeof(ownResistanceKeys)/sizeof(ownResistanceKeys[0]); ++i) unknownResistance.remove(ownResistanceKeys[i]);
    if(train == trackTrain() && unknown.isEmpty() && unknownResistance.isEmpty()) {
        t->additions.remove(trainAdditionsKey());
    } else {
        t->additions[trainAdditionsKey()] = trainToJson(train, keep);
    }
}

void clearTrainOfTrack(track* t)
{
    t->additions.remove(trainAdditionsKey());
}

bool simulatedTrainOfTrack(const track* t, trainSpec* out, QString* why)
{
    if(!trackHasTrain(t)) {
        if(why) *why = "the track has no train";
        return false;
    }
    QStringList problems;
    trackTrain train = trainOfTrack(t, &problems);
    if(!train.bSimulate) {
        if(why) *why = "the track's train is not switched on";
        return false;
    }
    QString invalid;
    if(!train.spec.isValid(&invalid)) {
        if(why) *why = "the track's train is not valid: " + invalid;
        return false;
    }
    if(out) *out = train.spec;
    return true;
}
