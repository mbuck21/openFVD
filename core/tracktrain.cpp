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
#include <QJsonDocument>
#include <QJsonValue>
#include <cmath>
#include <cstring>

trackTrain::trackTrain() : bSimulate(false), bExtendedAirtime(false)
{
}

bool trackTrain::operator==(const trackTrain& o) const
{
    return bSimulate == o.bSimulate && bExtendedAirtime == o.bExtendedAirtime && spec == o.spec && lAssumed == o.lAssumed
            && kept == o.kept;
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

// A list of finite floats (positive ones when positive), not empty.
bool floatListFrom(const QJsonValue& v, bool positive, QList<float>* out)
{
    if(!v.isArray() || v.toArray().isEmpty()) return false;
    QList<float> list;
    QJsonArray a = v.toArray();
    for(int i = 0; i < a.size(); ++i) {
        float f = 0.f;
        if(!floatFrom(a.at(i), &f) || std::fabs(f) > 1000.f || (positive && !(f > 0.f))) return false;
        list << f;
    }
    *out = list;
    return true;
}

// What a reader needs: the "train" object (or its "resistance"), the train
// being filled, and the prefix of the keys it reads ("" or "resistance.").
struct reader
{
    const QJsonObject& json;
    trackTrain& train;
    QString prefix;

    // A stored value this version cannot use: kept as it was, the reason given.
    void keep(const char* key, const QString& why)
    {
        QString name = prefix + key;
        train.kept[name] = json.value(key);
        // the stored value as JSON text, for the sentence
        QByteArray shown = QJsonDocument(QJsonArray() << json.value(key)).toJson(QJsonDocument::Compact);
        shown = shown.mid(1, shown.size() - 2);
        if(shown.size() > 60) shown = shown.left(57) + "...";
        train.lProblems << QString("\"%1\" is %2, which %3: this version uses its default and keeps the stored value")
                           .arg(name, QString::fromUtf8(shown), why);
    }

    // true and *out set when json[key] is there and usable; a key that is not
    // there leaves *out and returns true; anything else is kept (keep()).
    bool readBool(const char* key, bool* out)
    {
        QJsonValue v = json.value(key);
        if(v.isUndefined()) return true;
        if(!v.isBool()) { keep(key, "is not true or false"); return false; }
        *out = v.toBool();
        return true;
    }

    bool readFloat(const char* key, float low, float high, float* out)
    {
        QJsonValue v = json.value(key);
        if(v.isUndefined()) return true;
        float f;
        if(!floatFrom(v, &f) || f < low || f > high) {
            keep(key, QString("is not a number from %1 to %2").arg(low).arg(high));
            return false;
        }
        *out = f;
        return true;
    }

    bool readWhole(const char* key, int low, int high, int* out)
    {
        QJsonValue v = json.value(key);
        if(v.isUndefined()) return true;
        double d = v.toDouble();
        if(!v.isDouble() || !std::isfinite(d) || std::floor(d) != d || d < low || d > high) {
            keep(key, QString("is not a whole number from %1 to %2").arg(low).arg(high));
            return false;
        }
        *out = (int)d;
        return true;
    }

    bool readFloatList(const char* key, bool positive, QList<float>* out)
    {
        QJsonValue v = json.value(key);
        if(v.isUndefined()) return true;
        QList<float> list;
        if(!floatListFrom(v, positive, &list)) {
            keep(key, QString("is not a list of %1numbers").arg(positive ? "positive " : ""));
            return false;
        }
        *out = list;
        return true;
    }
};

} // namespace

trackTrain trainFromJson(const QJsonObject& json, QStringList* problems)
{
    trackTrain t;
    trainSpec& s = t.spec;
    reader r{json, t, QString()};
    r.readBool("simulate", &t.bSimulate);
    r.readBool("extendedAirtime", &t.bExtendedAirtime);
    QJsonValue type = json.value("type");
    if(type.isString()) s.sTypeId = type.toString();
    else if(!type.isUndefined()) r.keep("type", "is not a text");
    r.readWhole("cars", 1, 1000, &s.iCars);
    r.readFloat("carPitch", 0.f, 1000.f, &s.fCarPitch);
    r.readFloatList("rows", false, &s.lRowOffsets);
    r.readFloatList("seats", false, &s.lSeatOffsets);
    r.readFloat("seatUp", -1000.f, 1000.f, &s.fSeatUp);
    r.readFloat("carMass", 0.f, 1e9f, &s.fCarMass);
    r.readFloat("riderMass", 0.f, 1e9f, &s.fRiderMass);
    r.readFloat("load", 0.f, 1.f, &s.fLoad);
    QList<float> factors;
    if(r.readFloatList("carMassFactors", true, &factors)) {
        // valid only with one factor per car
        if(factors.isEmpty() || factors.size() == s.iCars) s.lCarMassFactor = factors;
        else r.keep("carMassFactors", QString("has %1 factors for %2 cars").arg(factors.size()).arg(s.iCars));
    }
    r.readBool("massAtDesignPoint", &s.bMassAtDesignPoint);
    r.readFloat("massAboveRail", -1000.f, 1000.f, &s.fMassAboveRail);
    QJsonValue res = json.value("resistance");
    if(res.isObject()) {
        QJsonObject ro = res.toObject();
        reader rr{ro, t, QString("resistance.")};
        rr.readBool("fromProject", &s.resistance.bFromProject);
        rr.readFloat("rolling", 0.f, 1000.f, &s.resistance.fRolling);
        rr.readFloat("rollingLoad", 0.f, 1000.f, &s.resistance.fRollingLoad);
        rr.readFloat("linear", 0.f, 1000.f, &s.resistance.fLinear);
        rr.readFloat("drag", 0.f, 1000.f, &s.resistance.fDrag);
    } else if(!res.isUndefined()) {
        r.keep("resistance", "is not an object");
    }
    int reference = 0;   // 1-based in the file, 0 = the train's centre
    if(r.readWhole("referenceCar", 1, 1000, &reference) && reference > 0) {
        // valid only within the train
        if(reference <= s.iCars) s.iReferenceCar = reference - 1;
        else r.keep("referenceCar", QString("is car %1 of a train of %2 car%3").arg(reference).arg(s.iCars).arg(s.iCars == 1 ? "" : "s"));
    }
    QJsonValue restraint = json.value("restraint");
    if(restraint.isString() && trainRestraintWords().contains(restraint.toString())) s.sRestraint = restraint.toString();
    else if(!restraint.isUndefined()) r.keep("restraint", "is not one of lapbar, otsr, vest, prone, other");
    QJsonValue assumed = json.value("assumed");
    bool assumedOk = assumed.isUndefined() || assumed.isArray();
    QStringList names;
    QJsonArray a = assumed.toArray();
    for(int i = 0; assumedOk && i < a.size(); ++i) {
        if(a.at(i).isString()) names << a.at(i).toString();
        else assumedOk = false;
    }
    if(assumedOk) t.lAssumed = names;
    else r.keep("assumed", "is not a list of names");
    QString why;
    if(!s.isValid(&why)) t.lProblems << "the train as read is not valid: " + why;
    if(problems) *problems << t.lProblems;
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

    // the resistance, with any unknown key inside it kept, and its kept values
    const rideResistance& r = s.resistance;
    const rideResistance& rd = d.resistance;
    QJsonObject ro = keep.value("resistance").toObject();
    for(size_t i = 0; i < sizeof(ownResistanceKeys)/sizeof(ownResistanceKeys[0]); ++i) ro.remove(ownResistanceKeys[i]);
    bool resistanceDefault = r.bFromProject == rd.bFromProject && sameFloat(r.fRolling, rd.fRolling)
            && sameFloat(r.fRollingLoad, rd.fRollingLoad) && sameFloat(r.fLinear, rd.fLinear) && sameFloat(r.fDrag, rd.fDrag);
    bool resistanceKept = false;
    for(QJsonObject::const_iterator it = train.kept.constBegin(); it != train.kept.constEnd(); ++it) {
        if(it.key().startsWith("resistance.")) resistanceKept = true;
    }
    if(!resistanceDefault || !ro.isEmpty() || resistanceKept) {
        ro["fromProject"] = r.bFromProject;
        if(!sameFloat(r.fRolling, rd.fRolling)) ro["rolling"] = floatJson(r.fRolling);
        if(!sameFloat(r.fRollingLoad, rd.fRollingLoad)) ro["rollingLoad"] = floatJson(r.fRollingLoad);
        if(!sameFloat(r.fLinear, rd.fLinear)) ro["linear"] = floatJson(r.fLinear);
        if(!sameFloat(r.fDrag, rd.fDrag)) ro["drag"] = floatJson(r.fDrag);
        for(QJsonObject::const_iterator it = train.kept.constBegin(); it != train.kept.constEnd(); ++it) {
            if(it.key().startsWith("resistance.")) ro[it.key().mid(11)] = it.value();
        }
        o["resistance"] = ro;
    }

    if(s.iReferenceCar >= 0) o["referenceCar"] = s.iReferenceCar + 1;
    if(!s.sRestraint.isEmpty()) o["restraint"] = s.sRestraint;
    if(train.bExtendedAirtime) o["extendedAirtime"] = true;
    if(!train.lAssumed.isEmpty()) o["assumed"] = QJsonArray::fromStringList(train.lAssumed);

    // the stored values this version could not use, as they were
    for(QJsonObject::const_iterator it = train.kept.constBegin(); it != train.kept.constEnd(); ++it) {
        if(!it.key().startsWith("resistance.")) o[it.key()] = it.value();
    }
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
        // not an object at all: nothing of it can be used; setTrainOfTrack()
        // writes over it only when a command sets the train
        trackTrain none;
        none.lProblems << "the track's \"train\" is not an object: this version uses no train";
        if(problems) *problems << none.lProblems;
        return none;
    }
    return trainFromJson(v.toObject(), problems);
}

QStringList trainProblemsOfTrack(const track* t)
{
    return trainOfTrack(t).lProblems;
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

bool simulatedTrainOfTrack(const track* t, trainSpec* out, QString* why, QStringList* problems)
{
    if(!trackHasTrain(t)) {
        if(why) *why = "the track has no train";
        return false;
    }
    trackTrain train = trainOfTrack(t, problems);
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
