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

// fvdcore ride: a train ridden over a project's finished track
// (core/trainride.h). Per car the lowest and highest of each force and the
// airtime, as text, as JSON (--json) or, with --table, the samples as
// tab-separated text. --type describes a train (the catalogue's); without
// it each track rides its own train when it has one switched on
// (core/tracktrain.h), else the program's own point. --cars, --pitch, --rows,
// --seats and --reference adjust that train for this ride (adjustTrain()).
// Nothing is saved in the project. Metric units only.

#include "commands.h"
#include "project.h"

#include "../core/tracktrain.h"
#include "../core/trainride.h"
#include "../core/trainspec.h"
#include "../core/traintype.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegExp>
#include <cmath>
#include <cstdio>

static QString f(double v, int decimals)
{
    QString text = QString::number(v, 'f', decimals);
    // a value that rounds to zero is printed without a minus sign
    if(text.startsWith('-') && text.indexOf(QRegExp("[1-9]")) < 0) text.remove(0, 1);
    return text;
}

static QJsonValue num(double v, int decimals = -1)
{
    if(!std::isfinite(v)) return QJsonValue(QJsonValue::Null);
    if(decimals < 0) return QJsonValue(v);
    double scale = std::pow(10.0, decimals);
    return QJsonValue(std::floor(v*scale + 0.5)/scale);
}

// The catalogue: --catalogue, else the one built into the program (not into
// fvdcore), else data/traintypes.json beside or above the program's folder.
static QList<trainType> loadCatalogue(const QString& given, QString& error)
{
    QStringList tries;
    if(!given.isEmpty()) {
        tries << given;
    } else {
        tries << trainCatalogue::defaultPath();
        QDir dir(QCoreApplication::applicationDirPath());
        for(int up = 0; up < 4; ++up) {
            tries << dir.filePath("data/traintypes.json");
            if(!dir.cdUp()) break;
        }
        tries << QDir::current().filePath("data/traintypes.json");
    }
    for(int i = 0; i < tries.size(); ++i) {
        if(!tries.at(i).startsWith(":/") && !QFileInfo(tries.at(i)).isFile()) continue;
        QString why;
        QList<trainType> list = trainCatalogue::load(tries.at(i), &why);
        if(!list.isEmpty()) return list;
        if(!given.isEmpty()) {
            error = QString("the catalogue %1 cannot be read: %2").arg(given).arg(why);
            return list;
        }
    }
    error = given.isEmpty() ? QString("the train catalogue data/traintypes.json was not found (give it with --catalogue)")
                            : QString("the catalogue %1 was not found").arg(given);
    return QList<trainType>();
}

static bool wholeNumber(const QMap<QString, QString>& values, const QString& key, int low, int* out, QString& error)
{
    if(!values.contains(key)) return true;
    bool ok;
    int v = values[key].toInt(&ok);
    if(!ok || v < low) {
        error = QString("%1 needs a whole number of %2 or more").arg(key).arg(low);
        return false;
    }
    *out = v;
    return true;
}

// Whether any option adjusts a train (all train options but --type and --catalogue).
static bool adjustsTrain(const QMap<QString, QString>& values)
{
    return values.contains("--cars") || values.contains("--pitch") || values.contains("--rows")
            || values.contains("--seats") || values.contains("--reference");
}

// The options that adjust a train, applied to spec (the --type train, the
// track's own train, or the program's own point). isTrain false: spec is the
// program's own point, which has no reference car of its own, so --reference
// is refused unless --cars makes it a train. False with a sentence in error.
static bool adjustTrain(trainSpec& spec, QStringList& assumed, const QMap<QString, QString>& values, bool isTrain,
                        QString& error)
{
    int cars = spec.iCars, rows = spec.rows(), seats = spec.seatsPerRow();
    if(!wholeNumber(values, "--cars", 1, &cars, error) || !wholeNumber(values, "--rows", 1, &rows, error)
            || !wholeNumber(values, "--seats", 1, &seats, error)) {
        return false;
    }
    if(cars != spec.iCars) {
        spec.iCars = cars;
        assumed.removeAll("iCars");
        if(spec.iReferenceCar >= cars) spec.iReferenceCar = cars - 1;
        if(!spec.lCarMassFactor.isEmpty() && spec.lCarMassFactor.size() != cars) spec.lCarMassFactor.clear();
    }
    if(values.contains("--pitch")) {
        bool ok;
        double pitch = values["--pitch"].toDouble(&ok);
        if(!ok || !(pitch > 0.0)) { error = "--pitch needs a number of metres above 0"; return false; }
        spec.fCarPitch = (float)pitch;
        assumed.removeAll("fCarPitch");
    } else if(spec.iCars > 1 && !(spec.fCarPitch > 0.f)) {
        spec.fCarPitch = trainSpec::standInCarPitch();
        if(!assumed.contains("fCarPitch")) assumed << "fCarPitch";
    }
    if(rows != spec.rows()) {
        spec.setRows(rows);
        if(!assumed.contains("lRowOffsets")) assumed << "lRowOffsets";
    }
    if(seats != spec.seatsPerRow()) {
        spec.setSeats(seats);
        if(!assumed.contains("lSeatOffsets")) assumed << "lSeatOffsets";
    }
    if(values.contains("--reference")) {
        if(!isTrain && !values.contains("--cars")) {
            error = "--reference needs a train: the track has no train switched on, and the program's own single point has "
                    "no reference car (give --type or --cars, or switch the track's train on)";
            return false;
        }
        QString ref = values["--reference"];
        bool ok;
        int car = ref.toInt(&ok);
        if(ref == "centre" || ref == "center") spec.iReferenceCar = -1;
        else if(ref == "front") spec.iReferenceCar = 0;
        else if(ref == "rear") spec.iReferenceCar = spec.iCars - 1;
        else if(ok && car >= 1 && car <= spec.iCars) spec.iReferenceCar = car - 1;
        else { error = QString("--reference is centre, front, rear or a car number from 1 to %1").arg(spec.iCars); return false; }
    }
    if(!spec.isValid(&error)) { error = "the train is not valid: " + error; return false; }
    return true;
}

static QString trainText(const trainRide& r, const QStringList& assumed, const QString& source)
{
    const trainSpec& s = r.spec;
    QString what = s.sTypeId;
    if(what.isEmpty()) what = (s == trainSpec::point()) ? QString("the program's own point") : QString("a train without a catalogue type");
    if(!source.isEmpty()) what += " (" + source + ")";
    QString t = QString("  Train: %1, %2 car(s)").arg(what).arg(s.iCars);
    if(s.iCars > 1) t += QString(" %1 m apart (%2 m)").arg(f(s.fCarPitch, 2)).arg(f(s.trainLength(), 1));
    t += QString(", %1 row(s) of %2 seat(s); reference: %3\n").arg(s.rows()).arg(s.seatsPerRow())
            .arg(s.iReferenceCar < 0 ? QString("the train's centre") : QString("car %1").arg(s.iReferenceCar + 1));
    t += QString("  Resistance: friction %1, drag %2 1/m%3\n").arg(r.friction, 0, 'g', 4).arg(r.drag, 0, 'g', 4)
            .arg(s.resistance.bFromProject ? QString(" (the project's own)")
                                           : QString(", load share %1, speed term %2 1/s").arg(r.rollingLoad, 0, 'g', 4)
                                             .arg(r.linear, 0, 'g', 4));
    if(!assumed.isEmpty()) t += "  Assumed (stand-in values, not the catalogue's): " + assumed.join(", ") + "\n";
    return t;
}

static QString where(const rideExtreme& e)
{
    if(!e.valid) return QString("-");
    return QString("%1 (%2 m)").arg(f(e.value, 2)).arg(f(e.place_m, 1));
}

static QString rideText(const trainRide& r, int trackNumber, const QString& name, const QStringList& assumed,
                        const QString& source)
{
    QString t = QString("Track %1: %2\n").arg(trackNumber).arg(name);
    if(!r.valid) return t + "  Train: " + source + "\n  No ride: " + r.error + "\n";
    t += trainText(r, assumed, source);
    double duration = r.samples() ? r.time_s.last() : 0.0;
    t += QString("  Ride: %1 s, %2 samples every %3 s; the reference starts at the track's start, the ride ends when the "
                 "rearmost row has passed the last node\n").arg(f(duration, 2)).arg(r.samples()).arg(r.options.sampleInterval_s);
    if(r.stalled) t += "  " + r.stallText + "\n";
    for(int i = 0; i < r.notes.size(); ++i) t += "  Note: " + r.notes.at(i) + "\n";
    t += "\n  Per car while its centre is on the track (g, and where: metres along the centre line)\n";
    t += QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n").arg("car", 4).arg("offset m", 9).arg("lowest gz", 16)
            .arg("highest gz", 16).arg("gy left", 16).arg("gy right", 16).arg("gx forward", 16).arg("gx back", 16)
            .arg("floater s", 9).arg("ejector s", 9);
    for(int c = 0; c < r.cars.size(); ++c) {
        const rideCarSummary& s = r.cars.at(c);
        t += QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n").arg(c + 1, 4).arg(f(r.carOffset_m.at(c), 2), 9)
                .arg(where(s.low[rideGz]), 16).arg(where(s.high[rideGz]), 16)
                .arg(where(s.low[rideGy]), 16).arg(where(s.high[rideGy]), 16)
                .arg(where(s.low[rideGx]), 16).arg(where(s.high[rideGx]), 16)
                .arg(f(s.floater_s, 2), 9).arg(f(s.ejector_s, 2), 9);
    }
    t += "\n  gz: pressed into the seat; gy: + rider pressed to his right; gx: + pressed into the seat back.\n"
         "  Car 1 is the front car. Stand-in train dimensions give stand-in results.\n";
    return t;
}

static QJsonValue extremeJson(const rideExtreme& e)
{
    if(!e.valid) return QJsonValue(QJsonValue::Null);
    QJsonObject o;
    o["value"] = num(e.value, 4);
    o["time_s"] = num(e.time_s, 4);
    o["place_m"] = num(e.place_m, 4);
    o["node"] = e.node;
    o["section"] = e.section;
    return o;
}

static QJsonObject rideJson(const trainRide& r, int trackNumber, const QString& name, const QStringList& assumed,
                            const QString& source, bool withSamples, int every)
{
    QJsonObject o;
    o["track"] = trackNumber;
    o["name"] = name;
    o["train_source"] = source;
    o["valid"] = r.valid;
    if(!r.valid) {
        o["error"] = r.error;
        return o;
    }
    const trainSpec& s = r.spec;
    QJsonObject train;
    train["type"] = s.sTypeId;
    train["cars"] = s.iCars;
    train["car_pitch_m"] = num(s.fCarPitch, 4);
    QJsonArray rows, seats;
    for(int i = 0; i < s.rows(); ++i) rows.append(num(s.lRowOffsets.at(i), 4));
    for(int i = 0; i < s.seatsPerRow(); ++i) seats.append(num(s.lSeatOffsets.at(i), 4));
    train["row_offsets_m"] = rows;
    train["seat_offsets_m"] = seats;
    train["reference_car"] = s.iReferenceCar;
    train["assumed"] = QJsonArray::fromStringList(assumed);
    o["train"] = train;
    QJsonObject res;
    res["friction"] = r.friction;
    res["rolling_load"] = r.rollingLoad;
    res["linear_per_s"] = r.linear;
    res["drag_per_m"] = r.drag;
    res["from_project"] = s.resistance.bFromProject;
    o["resistance"] = res;
    o["sample_interval_s"] = r.options.sampleInterval_s;
    o["duration_s"] = num(r.samples() ? r.time_s.last() : 0.0, 4);
    o["stalled"] = r.stalled;
    if(r.stalled) {
        QJsonObject st;
        st["time_s"] = num(r.stallTime_s, 4);
        st["place_m"] = num(r.stallPlace_m, 4);
        st["node"] = r.stallNode;
        st["section"] = r.stallSection;
        st["text"] = r.stallText;
        o["stall"] = st;
    }
    o["notes"] = QJsonArray::fromStringList(r.notes);
    QJsonArray cars;
    static const char* axes[3] = { "gz", "gy", "gx" };
    for(int c = 0; c < r.cars.size(); ++c) {
        const rideCarSummary& sum = r.cars.at(c);
        QJsonObject co;
        co["car"] = c + 1;
        co["offset_m"] = num(r.carOffset_m.at(c), 4);
        for(int a = 0; a < 3; ++a) {
            co[QString("%1_min").arg(axes[a])] = extremeJson(sum.low[a]);
            co[QString("%1_max").arg(axes[a])] = extremeJson(sum.high[a]);
        }
        co["floater_s"] = num(sum.floater_s, 4);
        co["ejector_s"] = num(sum.ejector_s, 4);
        co["below_zero_s"] = num(sum.belowZero_s, 4);
        cars.append(co);
    }
    o["cars"] = cars;
    if(withSamples) {
        QJsonArray time, pos, speed, perCar;
        for(int k = 0; k < r.samples(); k += every) {
            time.append(num(r.time_s.at(k), 4));
            pos.append(num(r.position_m.at(k), 4));
            speed.append(num(r.speed_mps.at(k), 4));
        }
        for(int c = 0; c < r.carCount(); ++c) {
            QJsonObject co;
            for(int a = 0; a < 3; ++a) {
                QJsonArray v;
                for(int k = 0; k < r.samples(); k += every) v.append(num(r.car(k, c, a), 4));
                co[axes[a]] = v;
            }
            perCar.append(co);
        }
        QJsonObject samples;
        samples["time_s"] = time;
        samples["position_m"] = pos;
        samples["speed_mps"] = speed;
        samples["cars"] = perCar;
        o["samples"] = samples;
    }
    return o;
}

static QString rideTable(const trainRide& r, int every)
{
    QString t = "time_s\tposition_m\tspeed_mps";
    for(int c = 0; c < r.carCount(); ++c) t += QString("\tcar%1_gz\tcar%1_gy\tcar%1_gx").arg(c + 1);
    t += "\n";
    for(int k = 0; k < r.samples(); k += every) {
        t += QString::number(r.time_s.at(k), 'f', 3) + "\t" + QString::number(r.position_m.at(k), 'g', 9) + "\t"
                + QString::number(r.speed_mps.at(k), 'g', 9);
        for(int c = 0; c < r.carCount(); ++c) {
            for(int a = 0; a < 3; ++a) t += "\t" + QString::number(r.car(k, c, a), 'g', 6);
        }
        t += "\n";
    }
    return t;
}

static int runRide(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--table" << "--json",
                     QStringList() << "--track" << "--type" << "--catalogue" << "--cars" << "--pitch" << "--rows" << "--seats"
                                   << "--reference" << "--every-s" << "--out",
                     positional, flags, values, error)) {
        fprintf(stderr, "%s\nusage: %s\n", qPrintable(error), cmdRide.usage);
        return 1;
    }
    if(positional.size() != 1) {
        fprintf(stderr, "usage: %s\n", cmdRide.usage);
        return 1;
    }
    int trackNumber = 0;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok || trackNumber < 1) { fprintf(stderr, "--track needs a whole number of 1 or more\n"); return 1; }
    }

    // The train. --type describes one (the catalogue's); the other train
    // options adjust a train: the --type train when given, else the track's
    // own train when it has one switched on, else the program's own point.
    // Worked out per track below (adjustTrain()); checked here once on the
    // point so that a bad value is refused before the project is read.
    const bool typeGiven = values.contains("--type");
    trainSpec typeSpec;
    QStringList typeAssumed;
    if(typeGiven) {
        QList<trainType> types = loadCatalogue(values.value("--catalogue"), error);
        if(types.isEmpty()) { fprintf(stderr, "%s\n", qPrintable(error)); return 1; }
        const trainType* type = trainCatalogue::findById(types, values["--type"]);
        if(!type) {
            fprintf(stderr, "no train type %s in the catalogue\n", qPrintable(values["--type"]));
            return 1;
        }
        typeSpec = trainSpec::fromType(*type, &typeAssumed);
    }
    {
        trainSpec probe = typeGiven ? typeSpec : trainSpec::point();
        QStringList probeAssumed;
        // the reference is checked per track (the number of cars may come from the track's train)
        QMap<QString, QString> noReference = values;
        noReference.remove("--reference");
        if(!adjustTrain(probe, probeAssumed, noReference, typeGiven, error)) { fprintf(stderr, "%s\n", qPrintable(error)); return 1; }
    }
    int every = 1;
    if(values.contains("--every-s")) {
        bool ok;
        double step = values["--every-s"].toDouble(&ok);
        if(!ok || !(step > 0.0)) { fprintf(stderr, "--every-s needs a number of seconds above 0\n"); return 1; }
        every = qMax(1, (int)std::floor(step/rideOptions().sampleInterval_s + 0.5));
    }

    coreProject project;
    if(!loadProject(positional.at(0), project, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    QList<int> numbers;
    if(trackNumber > 0) {
        if(!trackByNumber(project, trackNumber, error)) { fprintf(stderr, "%s\n", qPrintable(error)); return 1; }
        numbers << trackNumber;
    } else {
        for(int i = 0; i < project.tracks.size(); ++i) numbers << i + 1;
    }
    bool table = flags.contains("--table"), json = flags.contains("--json");
    QString text;
    QJsonArray tracks;
    for(int i = 0; i < numbers.size(); ++i) {
        track* t = project.tracks.at(numbers.at(i) - 1);
        // the train this track rides: see adjustTrain()
        trainSpec use = trainSpec::point();
        QStringList useAssumed, problems;
        QString source;
        bool own = false;
        if(typeGiven) {
            use = typeSpec;
            useAssumed = typeAssumed;
            source = QString("given with --type");
        } else {
            QString why;
            if(simulatedTrainOfTrack(t, &use, &why, &problems)) {
                own = true;
                useAssumed = trainOfTrack(t).lAssumed;
                source = QString("the track's own train");
            } else {
                use = trainSpec::point();
                source = trackHasTrain(t) ? QString("the track's train is not used: %1").arg(why)
                                          : QString("the track has no train");
                if(!trackHasTrain(t)) problems.clear();
            }
        }
        if(adjustsTrain(values)) {
            if(!adjustTrain(use, useAssumed, values, typeGiven || own, error)) {
                fprintf(stderr, "track %d (%s): %s\n", numbers.at(i), qPrintable(source), qPrintable(error));
                return 1;
            }
            source += QString("; adjusted by the options for this ride, nothing saved");
        }
        trainRide r = rideTrain(t, use);
        if(json) {
            QJsonObject o = rideJson(r, numbers.at(i), t->name, useAssumed, source, table, every);
            o["train_problems"] = QJsonArray::fromStringList(problems);
            tracks.append(o);
        } else if(table) {
            if(numbers.size() > 1) text += QString("# track %1: %2\n").arg(numbers.at(i)).arg(t->name);
            // the table's first line stays its header unless the track's own train is ridden
            if(own) text += QString("# train: %1\n").arg(source);
            for(int k = 0; k < problems.size(); ++k) text += QString("# note: %1\n").arg(problems.at(k));
            text += r.valid ? rideTable(r, every) : QString("# no ride: %1\n").arg(r.error);
        } else {
            QString one = rideText(r, numbers.at(i), t->name, useAssumed, source);
            // what of the stored train could not be used, after the train's line
            if(!problems.isEmpty()) {
                QString notes;
                for(int k = 0; k < problems.size(); ++k) notes += "  Note (the track's train): " + problems.at(k) + "\n";
                int after = one.indexOf('\n', one.indexOf("  Train: ")) + 1;
                one.insert(after > 0 ? after : one.size(), notes);
            }
            text += one;
            if(i + 1 < numbers.size()) text += "\n";
        }
    }
    QByteArray bytes;
    if(json) {
        QJsonObject root;
        root["tracks"] = tracks;
        root["note"] = QString("A train ridden over the finished track (core/trainride.h): forces in g, gravity included; "
                               "gz pressed into the seat, gy + rider pressed to his right, gx + pressed into the seat back; "
                               "car 1 is the front car. Stand-in train dimensions give stand-in results.");
        bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        bytes = text.toUtf8();
    }
    if(values.contains("--out")) {
        QFile out(values["--out"]);
        if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size()) {
            fprintf(stderr, "cannot write %s\n", qPrintable(values["--out"]));
            return 1;
        }
    } else {
        fwrite(bytes.constData(), 1, bytes.size(), stdout);
    }
    return 0;
}

const command cmdRide = {
    "ride",
    "fvdcore ride <project.fvd> [--track N] [--type ID] [--catalogue FILE] [--cars N] [--pitch M] [--rows N] [--seats N] "
    "[--reference centre|front|rear|N] [--table [--every-s X]] [--json] [--out FILE]",
    "ride a train over the finished track (the --type train, else each track's own train when it has one switched on, "
    "else the program's own point; --cars, --pitch, --rows, --seats and --reference adjust that train for this ride): "
    "per car the lowest and highest force and the airtime; --table the samples every 0.01 s; nothing is saved",
    runRide
};
