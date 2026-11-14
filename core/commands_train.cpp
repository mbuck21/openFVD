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

// The train group of commands (core/command.h): the track's train
// (core/tracktrain.h), kept in the track's additions under "train".
//
// A train command changes no node: the train is beside the design (decision
// record 0014, point 1). Each is marked settlesItself, so run() does not
// compute the track again after it: the track was in the state saving and
// reopening gives before the command (every command, load, undo and redo
// ends there, decision record 0013), and a change of the additions leaves it
// there. Each is one undo step like any edit; the step's state holds the
// additions (trackState::additions).

#include "command.h"
#include "track.h"
#include "tracktrain.h"
#include "traintype.h"
#include "projecttext.h"

#include <QJsonArray>
#include <cmath>

#include <functional>

namespace {

typedef std::function<bool(trackTrain&, const commandArgs&, QString*)> trainChange;

// The track's train, changed, checked and written back into its additions.
// Refused (nothing changed) when the train it gives is not valid.
bool editTrain(track* t, const commandArgs& args, QString* error, const trainChange& change)
{
    trackTrain train = trainOfTrack(t);
    if(!change(train, args, error)) return false;
    QString why;
    if(!train.spec.isValid(&why)) {
        if(error) *error = "the train would not be valid: " + why;
        return false;
    }
    QJsonObject before = t->additions;
    setTrainOfTrack(t, train);
    if(t->additions != before) t->hasChanged = true;
    return true;
}

void unassume(trackTrain& train, const QStringList& fields)
{
    for(int i = 0; i < fields.size(); ++i) train.lAssumed.removeAll(fields.at(i));
}

// The command sets these keys: a stored value of them this version could not
// use (trackTrain::kept) is replaced by the new one. "resistance" covers the
// keys inside it too.
void sets(trackTrain& train, const QStringList& keys)
{
    QStringList all = train.kept.keys();
    for(int i = 0; i < all.size(); ++i) {
        for(int k = 0; k < keys.size(); ++k) {
            if(all.at(i) == keys.at(k) || all.at(i).startsWith(keys.at(k) + ".")) train.kept.remove(all.at(i));
        }
    }
}

bool changeSimulate(trackTrain& train, const commandArgs& args, QString*)
{
    train.bSimulate = args.value("on").toBool();
    sets(train, QStringList() << "simulate");
    return true;
}

// The catalogue entry (trainSpec::fromType()), with its stand-ins named.
// What is not part of the description stays: whether the train is simulated
// and whether the extended airtime line is allowed. The reference goes back
// to the train's centre. Every field of the description is set, so any of
// them kept as stored (unreadable here) is replaced.
bool changeType(trackTrain& train, const commandArgs& args, QString* error)
{
    QString id = args.value("type").toString();
    const QList<trainType>& catalogue = trainCatalogue::defaultCatalogue();
    if(catalogue.isEmpty()) {
        if(error) *error = "the train catalogue could not be read";
        return false;
    }
    const trainType* type = trainCatalogue::findById(catalogue, id);
    if(type == nullptr) {
        if(error) *error = QString("there is no train type \"%1\" in the catalogue").arg(id);
        return false;
    }
    QStringList assumed;
    train.spec = trainSpec::fromType(*type, &assumed);
    train.lAssumed = assumed;
    sets(train, QStringList() << "type" << "cars" << "carPitch" << "rows" << "seats" << "seatUp" << "carMass"
         << "riderMass" << "load" << "carMassFactors" << "massAtDesignPoint" << "massAboveRail" << "resistance"
         << "referenceCar" << "restraint" << "assumed");
    return true;
}

// A whole number from 1 to cars, as a stored JSON value; 0 when it is not one.
int carNumberIn(const QJsonValue& v, int cars)
{
    double d = v.toDouble(0.0);
    if(!v.isDouble() || !std::isfinite(d) || std::floor(d) != d || d < 1.0 || d > cars) return 0;
    return (int)d;
}

// The count of cars.
// - The reference: one in use that is no longer in the train becomes the rear
//   car. One kept as stored (beyond the cars it was saved with) is taken into
//   use when it is within the new train, else it stays kept as it was.
// - Mass factors: ones in use that no longer fit (not one per car) are kept
//   as stored, not dropped (the ride then uses none); ones kept as stored are
//   taken into use when there is one per car now.
// - A train of more than one car without a car pitch takes the stand-in pitch.
//   The pitch is kept when the train goes down to one car, where it is not used.
bool changeCars(trackTrain& train, const commandArgs& args, QString*)
{
    trainSpec& s = train.spec;
    s.iCars = args.value("cars").toInt();
    sets(train, QStringList() << "cars");
    if(s.iReferenceCar >= s.iCars) s.iReferenceCar = s.iCars - 1;
    if(train.kept.contains("referenceCar")) {
        int car = carNumberIn(train.kept.value("referenceCar"), s.iCars);
        if(car > 0) {
            s.iReferenceCar = car - 1;
            train.kept.remove("referenceCar");
        }
    }
    if(!s.lCarMassFactor.isEmpty() && s.lCarMassFactor.size() != s.iCars) {
        QJsonArray stored;
        for(int i = 0; i < s.lCarMassFactor.size(); ++i) stored.append(projectText::floatToJson(s.lCarMassFactor.at(i)));
        train.kept["carMassFactors"] = stored;
        s.lCarMassFactor.clear();
    } else if(train.kept.contains("carMassFactors")) {
        QJsonArray stored = train.kept.value("carMassFactors").toArray();
        QList<float> factors;
        bool ok = stored.size() == s.iCars;
        for(int i = 0; ok && i < stored.size(); ++i) {
            float f = 0.f;
            ok = projectText::floatFromJson(stored.at(i), &f) && std::isfinite(f) && f > 0.f && f <= 1000.f;
            factors << f;
        }
        if(ok) {
            s.lCarMassFactor = factors;
            train.kept.remove("carMassFactors");
        }
    }
    if(s.iCars > 1 && !(s.fCarPitch > 0.f)) {
        s.fCarPitch = trainSpec::standInCarPitch();
        sets(train, QStringList() << "carPitch");
        if(!train.lAssumed.contains("fCarPitch")) train.lAssumed << "fCarPitch";
    }
    unassume(train, QStringList() << "iCars");
    return true;
}

bool changeCarPitch(trackTrain& train, const commandArgs& args, QString*)
{
    train.spec.fCarPitch = (float)args.value("pitch").toDouble();
    sets(train, QStringList() << "carPitch");
    unassume(train, QStringList() << "fCarPitch");
    return true;
}

bool changeRows(trackTrain& train, const commandArgs& args, QString*)
{
    train.spec.setRows(args.value("count").toInt(), (float)args.value("spacing").toDouble());
    sets(train, QStringList() << "rows");
    unassume(train, QStringList() << "rowCount" << "lRowOffsets");
    return true;
}

bool changeSeats(trackTrain& train, const commandArgs& args, QString*)
{
    train.spec.setSeats(args.value("count").toInt(), (float)args.value("spacing").toDouble());
    sets(train, QStringList() << "seats");
    unassume(train, QStringList() << "seatCount" << "lSeatOffsets");
    return true;
}

bool changeSeatHeight(trackTrain& train, const commandArgs& args, QString*)
{
    train.spec.fSeatUp = (float)args.value("height").toDouble();
    sets(train, QStringList() << "seatUp");
    return true;
}

bool changeReference(trackTrain& train, const commandArgs& args, QString* error)
{
    int car = args.value("car").toInt();
    if(car > train.spec.iCars) {
        if(error) *error = QString("the train has %1 car%2; there is no car %3").arg(train.spec.iCars)
                .arg(train.spec.iCars == 1 ? "" : "s").arg(car);
        return false;
    }
    train.spec.iReferenceCar = car - 1;
    sets(train, QStringList() << "referenceCar");
    return true;
}

bool changeRestraint(trackTrain& train, const commandArgs& args, QString*)
{
    QString word = args.value("restraint").toString();
    train.spec.sRestraint = word == "none" ? QString() : word;
    sets(train, QStringList() << "restraint");
    return true;
}

bool changeExtendedAirtime(trackTrain& train, const commandArgs& args, QString*)
{
    train.bExtendedAirtime = args.value("allowed").toBool();
    sets(train, QStringList() << "extendedAirtime");
    return true;
}

bool changeResistance(trackTrain& train, const commandArgs& args, QString*)
{
    rideResistance& r = train.spec.resistance;
    r.bFromProject = args.value("fromProject").toBool();
    r.fRolling = (float)args.value("rolling").toDouble();
    r.fRollingLoad = (float)args.value("rollingLoad").toDouble();
    r.fLinear = (float)args.value("linear").toDouble();
    r.fDrag = (float)args.value("drag").toDouble();
    sets(train, QStringList() << "resistance");
    return true;
}

commandApplyFn trainApply(const trainChange& change)
{
    return [change](track* t, const commandTarget&, const commandArgs& args, QString* error) {
        return editTrain(t, args, error, change);
    };
}

// The whole train taken away: the key "train" goes, unknown fields inside it too.
bool applyClear(track* t, const commandTarget&, const commandArgs&, QString*)
{
    if(trackHasTrain(t)) {
        clearTrainOfTrack(t);
        t->hasChanged = true;
    }
    return true;
}

commandSpec trainSpecOf(const QString& name, const QString& description, const QString& label)
{
    commandSpec s;
    s.name = name;
    s.description = description;
    s.target = targetTrack;
    s.label = label;
    s.settlesItself = true;   // changes no node; see the top of this file
    return s;
}

} // namespace

QList<commandSpec> trainCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s = trainSpecOf("train.setSimulate",
                                    "Switches the simulation of the track's train on or off: on, the train is ridden over the finished track; off (a new track's default), the program's own single point is all there is. The track's shape does not change.",
                                    "Simulate train");
        s.params << commandParam::boolParam("on", "True to ride the track's train over the track, false to leave it described but not ridden.");
        s.example.insert("on", true);
        s.apply = trainApply(changeSimulate);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setType",
                                    "Takes the track's train from a type of the train catalogue (data/traintypes.json): cars, car pitch, rows, seats, restraint and, when the catalogue has them, friction and drag; a value the catalogue does not have is a stand-in and is listed as assumed. The reference goes back to the train's centre. Does not change the heartline (track.setHeartline does).",
                                    "Change train type");
        s.params << commandParam::textParam("type", "The catalogue id of the train type, for example nl2-hyper-coaster.");
        s.example.insert("type", QString("nl2-hyper-coaster"));
        s.apply = trainApply(changeType);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setCars",
                                    "Sets the number of cars of the track's train. A reference car beyond the new last car becomes the last car; a train of more than one car without a car pitch takes the stand-in pitch of 2.5 m (marked assumed); going down to one car keeps the pitch, unused. A reference car or mass factors stored but not usable with the old number of cars are taken into use once they fit, and kept as stored otherwise.",
                                    "Change number of cars");
        s.params << commandParam::intParam("cars", "none", "The number of cars, 1 or more.", 1, 100);
        s.merges = true;
        s.example.insert("cars", 7);
        s.apply = trainApply(changeCars);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setCarPitch",
                                    "Sets the car pitch of the track's train: the distance from one car's centre to the next, along the track's centre line. A train of one car does not use it; its pitch is kept (not set back to 0) so that a later change to more cars keeps the length chosen.",
                                    "Change car pitch");
        s.params << commandParam::floatParam("pitch", "m", "The car pitch, centre to centre.", 0.5, 50.0);
        s.merges = true;
        s.example.insert("pitch", 2.7);
        s.apply = trainApply(changeCarPitch);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setRows",
                                    "Sets the rows of each car of the track's train: how many, and how far apart along the car, centred on the car's centre.",
                                    "Change rows per car");
        s.params << commandParam::intParam("count", "none", "The number of rows per car.", 1, 20)
                 << commandParam::floatParam("spacing", "m", "The distance from one row to the next along the car (0.9 is the stand-in).", 0.0, 5.0);
        s.merges = true;
        s.example.insert("count", 3);
        s.example.insert("spacing", 0.85);
        s.apply = trainApply(changeRows);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setSeats",
                                    "Sets the seats of each row of the track's train: how many, and how far apart across the car, centred on the track.",
                                    "Change seats per row");
        s.params << commandParam::intParam("count", "none", "The number of seats per row.", 1, 12)
                 << commandParam::floatParam("spacing", "m", "The distance from one seat to the next across the car (0.55 is the stand-in).", 0.0, 3.0);
        s.merges = true;
        s.example.insert("count", 4);
        s.example.insert("spacing", 0.6);
        s.apply = trainApply(changeSeats);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setSeatHeight",
                                    "Sets where a rider's forces are taken, as a height above the heartline toward the rider's head; 0 (the default) takes them at the heartline.",
                                    "Change rider force point");
        s.params << commandParam::floatParam("height", "m", "The height above the heartline, negative for below it.", -3.0, 3.0);
        s.merges = true;
        s.example.insert("height", 0.15);
        s.apply = trainApply(changeSeatHeight);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setReference",
                                    "Sets the reference of the track's train, the place of the train that starts at the track's start and whose forces are shown beside the design's: the train's centre (the default) or one car.",
                                    "Change reference car");
        s.params << commandParam::intParam("car", "none", "0 for the train's centre, else the car counted from the front (1 is the front car).", 0, 100);
        s.example.insert("car", 1);
        s.apply = trainApply(changeReference);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setRestraint",
                                    "Sets the restraint of the track's train, which the force limits read (the eyes-front line): none, a lap bar, an over-the-shoulder restraint, a vest, a prone position, or other.",
                                    "Change restraint");
        s.params << commandParam::enumParam("restraint", "The restraint: none, lapbar, otsr (over the shoulder), vest, prone or other.",
                                            QStringList() << "none" << "lapbar" << "otsr" << "vest" << "prone" << "other");
        s.example.insert("restraint", QString("otsr"));
        s.apply = trainApply(changeRestraint);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setExtendedAirtime",
                                    "Allows the extended airtime line of the force limits for the track's train (down to -2.8 g briefly, which the standard allows only with special restraints addressed in the ride analysis), or not (the default).",
                                    "Change extended airtime");
        s.params << commandParam::boolParam("allowed", "True to allow the extended airtime line, false for the base line.");
        s.example.insert("allowed", true);
        s.apply = trainApply(changeExtendedAirtime);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.setResistance",
                                    "Sets how the track's train loses energy in the ride: the project's own friction and air resistance (the default), or four terms of its own summed: a share of the weight, a share of the force pressing the cars on the track, a term times speed and a term times speed squared. The design is not changed.",
                                    "Change train resistance");
        s.params << commandParam::boolParam("fromProject", "True to use the track's own friction and resistance (the four numbers below are then kept but not used).")
                 << commandParam::floatParam("rolling", "none", "Retarding force as a share of the train's weight (the meaning of the track's friction).", 0.0, 2.0)
                 << commandParam::floatParam("rollingLoad", "none", "Retarding force as a share of the force pressing each car on the track.", 0.0, 2.0)
                 << commandParam::floatParam("linear", "1/s", "Deceleration per metre per second of speed.", 0.0, 10.0)
                 << commandParam::floatParam("drag", "1/m", "Deceleration per square of the speed (the meaning of the track's resistance).", 0.0, 0.01);
        s.merges = true;
        s.example.insert("fromProject", false);
        s.example.insert("rolling", 0.026);
        s.example.insert("rollingLoad", 0.004);
        s.example.insert("linear", 0.001);
        s.example.insert("drag", 0.0001);
        s.apply = trainApply(changeResistance);
        list << s;
    }

    {
        commandSpec s = trainSpecOf("train.clear",
                                    "Takes the track's train away altogether: the track has no train again (as a new track), and a project with nothing else added is saved exactly as one that never had a train.",
                                    "Clear train");
        s.exampleMayChangeNothing = true;   // a track without a train stays as it was
        s.apply = applyClear;
        list << s;
    }

    return list;
}
