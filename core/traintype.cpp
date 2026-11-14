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

#include "traintype.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>
#include <QDebug>
#include <cmath>

// Schema 2 added carPitch, rowOffsets, seatOffsets, friction and drag per
// type. A file of schema 1 (without them) is still read: they are then not
// present, as when the file has null.
static const int SCHEMA_VERSION = 2;
static const int OLDEST_SCHEMA_VERSION = 1;

namespace {

bool knownConfidence(const QString& conf)
{
    return conf == "verified" || conf == "likely" || conf == "estimate" || conf == "unverified";
}

// Reads one sourced list of numbers out of obj[key], as readSourced() reads a
// number: missing, null or a null value gives a list that is not present.
bool readSourcedList(const QJsonObject& obj, const QString& key, sourcedList& out, QString* error)
{
    out = sourcedList();
    const QJsonValue v = obj.value(key);
    if (v.isUndefined() || v.isNull())
        return true;
    if (!v.isObject())
    {
        *error = QString("\"%1\" must be an object with value, source and confidence (or null)").arg(key);
        return false;
    }
    const QJsonObject o = v.toObject();
    const QJsonValue val = o.value("value");
    if (val.isUndefined() || val.isNull())
        return true;
    if (!val.isArray() || val.toArray().isEmpty())
    {
        *error = QString("\"%1\".value must be a list of numbers that is not empty").arg(key);
        return false;
    }
    QList<float> values;
    const QJsonArray arr = val.toArray();
    for (int i = 0; i < arr.size(); ++i)
    {
        if (!arr.at(i).isDouble() || !std::isfinite(arr.at(i).toDouble()) || std::fabs(arr.at(i).toDouble()) > 100.0)
        {
            *error = QString("\"%1\".value must hold finite numbers of metres (between -100 and 100)").arg(key);
            return false;
        }
        values.append((float)arr.at(i).toDouble());
    }
    if (!o.value("source").isString())
    {
        *error = QString("\"%1\".source must be a string").arg(key);
        return false;
    }
    const QString conf = o.value("confidence").toString();
    if (!knownConfidence(conf))
    {
        *error = QString("\"%1\".confidence must be verified, likely, estimate or unverified").arg(key);
        return false;
    }
    out.bPresent = true;
    out.lValues = values;
    out.sSource = o.value("source").toString();
    out.sConfidence = conf;
    return true;
}

// Reads one sourced value out of obj[key]. A missing key or null gives a value
// that is not present; a bad shape sets *error (and returns false).
bool readSourced(const QJsonObject& obj, const QString& key, sourcedValue& out, QString* error)
{
    out = sourcedValue();
    const QJsonValue v = obj.value(key);
    if (v.isUndefined() || v.isNull())
        return true;
    if (!v.isObject())
    {
        *error = QString("\"%1\" must be an object with value, source and confidence (or null)").arg(key);
        return false;
    }
    const QJsonObject o = v.toObject();
    const QJsonValue val = o.value("value");
    if (val.isUndefined() || val.isNull())
        return true; // an empty record counts as "not known"
    if (!val.isDouble() || !std::isfinite(val.toDouble()))
    {
        *error = QString("\"%1\".value must be a finite number").arg(key);
        return false;
    }
    if (!o.value("source").isString())
    {
        *error = QString("\"%1\".source must be a string").arg(key);
        return false;
    }
    const QString conf = o.value("confidence").toString();
    if (!knownConfidence(conf))
    {
        *error = QString("\"%1\".confidence must be verified, likely, estimate or unverified").arg(key);
        return false;
    }
    out.bPresent = true;
    out.fValue = (float)val.toDouble();
    out.sSource = o.value("source").toString();
    out.sConfidence = conf;
    return true;
}

// Optional string field: missing or null gives an empty string.
bool readString(const QJsonObject& obj, const QString& key, QString& out, QString* error)
{
    const QJsonValue v = obj.value(key);
    if (v.isUndefined() || v.isNull())
    {
        out.clear();
        return true;
    }
    if (!v.isString())
    {
        *error = QString("\"%1\" must be a string").arg(key);
        return false;
    }
    out = v.toString();
    return true;
}

// Optional whole number: missing or null gives -1.
bool readCount(const QJsonObject& obj, const QString& key, int& out, QString* error)
{
    const QJsonValue v = obj.value(key);
    out = -1;
    if (v.isUndefined() || v.isNull())
        return true;
    const double d = v.toDouble(-1.0);
    if (!v.isDouble() || !std::isfinite(d) || d < 0.0 || d != std::floor(d) || d > 100000.0)
    {
        *error = QString("\"%1\" must be a whole number or null").arg(key);
        return false;
    }
    out = (int)d;
    return true;
}

bool readTrain(const QJsonObject& obj, trainType& t, QString* error)
{
    if (!readString(obj, "id", t.sId, error))
        return false;
    if (t.sId.isEmpty())
    {
        *error = "an entry has no \"id\"";
        return false;
    }
    // From here on the message names the entry.
    QString problem;
    const QJsonValue name = obj.value("name");
    if (!name.isString() || name.toString().isEmpty())
        problem = "\"name\" is missing";
    else if (!readString(obj, "manufacturer", t.sManufacturer, &problem)
          || !readString(obj, "nl2Name", t.sNl2Name, &problem)
          || !readString(obj, "notes", t.sNotes, &problem)
          || !readCount(obj, "seatsAcross", t.iSeatsAcross, &problem)
          || !readCount(obj, "rowsPerCar", t.iRowsPerCar, &problem)
          || !readCount(obj, "carsPerTrain", t.iCarsPerTrain, &problem)
          || !readSourced(obj, "carLength", t.carLength, &problem)
          || !readSourced(obj, "carPitch", t.carPitch, &problem)
          || !readSourcedList(obj, "rowOffsets", t.rowOffsets, &problem)
          || !readSourcedList(obj, "seatOffsets", t.seatOffsets, &problem)
          || !readSourced(obj, "friction", t.friction, &problem)
          || !readSourced(obj, "drag", t.drag, &problem))
    {
    }
    else if (t.carPitch.bPresent && !(t.carPitch.fValue > 0.0f))
        problem = "\"carPitch\".value must be above 0";
    else if ((t.friction.bPresent && t.friction.fValue < 0.0f) || (t.drag.bPresent && t.drag.fValue < 0.0f))
        problem = "\"friction\" and \"drag\" must be 0 or more";
    else if (t.rowOffsets.bPresent && t.iRowsPerCar > 0 && t.rowOffsets.lValues.size() != t.iRowsPerCar)
        problem = QString("\"rowOffsets\" has %1 rows, \"rowsPerCar\" says %2").arg(t.rowOffsets.lValues.size()).arg(t.iRowsPerCar);
    else if (t.seatOffsets.bPresent && t.iSeatsAcross > 0 && t.seatOffsets.lValues.size() != t.iSeatsAcross)
        problem = QString("\"seatOffsets\" has %1 seats, \"seatsAcross\" says %2").arg(t.seatOffsets.lValues.size()).arg(t.iSeatsAcross);
    else if (!readSourced(obj, "heartline", t.heartline, &problem))
    {
    }
    else if (!t.heartline.bPresent)
        problem = "\"heartline\" is missing";
    else if (!readString(obj, "restraint", t.sRestraint, &problem))
    {
    }
    else if (t.sRestraint != "" && t.sRestraint != "lapbar" && t.sRestraint != "otsr"
          && t.sRestraint != "vest" && t.sRestraint != "other")
        problem = "\"restraint\" must be lapbar, otsr, vest, other or empty";
    else
    {
        const QJsonValue mr = obj.value("minRadius");
        if (!mr.isUndefined() && !mr.isNull() && !mr.isObject())
            problem = "\"minRadius\" must be an object";
        else if (mr.isObject())
        {
            const QJsonObject mro = mr.toObject();
            if (!readSourced(mro, "vertical", t.minRadiusVertical, &problem)
             || !readSourced(mro, "horizontal", t.minRadiusHorizontal, &problem))
                problem = "minRadius." + problem;
        }
    }
    if (!problem.isEmpty())
    {
        *error = QString("%1: %2").arg(t.sId, problem);
        return false;
    }
    t.sName = name.toString();
    return true;
}

} // namespace

QString trainCatalogue::defaultPath()
{
    return QStringLiteral(":/data/traintypes.json");
}

QList<trainType> trainCatalogue::load(const QString& path, QString* error)
{
    QString dummy;
    QString* err = error ? error : &dummy;
    err->clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *err = QString("cannot open %1").arg(path);
        return QList<trainType>();
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull() || !doc.isObject())
    {
        *err = QString("%1 is not valid JSON: %2").arg(path, parseError.errorString());
        return QList<trainType>();
    }
    const QJsonObject root = doc.object();
    const QJsonValue ver = root.value("schemaVersion");
    const double version = ver.toDouble(-1.0);
    if (!ver.isDouble() || version != std::floor(version) || version < OLDEST_SCHEMA_VERSION || version > SCHEMA_VERSION)
    {
        *err = QString("schemaVersion must be %1 to %2").arg(OLDEST_SCHEMA_VERSION).arg(SCHEMA_VERSION);
        return QList<trainType>();
    }
    if (!root.value("trains").isArray())
    {
        *err = "\"trains\" must be a list";
        return QList<trainType>();
    }

    QList<trainType> result;
    QStringList seen;
    const QJsonArray arr = root.value("trains").toArray();
    for (int i = 0; i < arr.size(); ++i)
    {
        if (!arr.at(i).isObject())
        {
            *err = QString("entry %1 is not an object").arg(i + 1);
            return QList<trainType>();
        }
        trainType t;
        if (!readTrain(arr.at(i).toObject(), t, err))
            return QList<trainType>();
        if (seen.contains(t.sId))
        {
            *err = QString("%1: duplicate id").arg(t.sId);
            return QList<trainType>();
        }
        seen.append(t.sId);
        result.append(t);
    }
    return result;
}

const QList<trainType>& trainCatalogue::defaultCatalogue()
{
    static QList<trainType> cache;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        QString error;
        cache = load(defaultPath(), &error);
        if (!error.isEmpty())
            qWarning() << "train types:" << error;
        bLoaded = true;
    }
    return cache;
}

const trainType* trainCatalogue::findById(const QList<trainType>& trains, const QString& id)
{
    for (int i = 0; i < trains.size(); ++i)
        if (trains.at(i).sId == id)
            return &trains.at(i);
    return nullptr;
}
