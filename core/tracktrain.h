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

#ifndef TRACKTRAIN_H
#define TRACKTRAIN_H

// The train a track keeps (decision record 0014, points 13 and 16; the build
// plan's piece D5). It lives in the track's additions under the key "train"
// (track::additions, written in the project file's appendix after "EOP",
// core/projectfile.h) and nowhere else: the track holds no parsed copy, so
// undo, redo, restore and load give it back with the additions, exactly.
// A track without the key has no train: trackTrain() (off, the program's own
// point, the reference at the train's centre). The train is beside the
// design: nothing here changes a node.
//
// The JSON shape of "train" (docs/project-text-format.md, "The track's train"):
//   "simulate"        bool, always written: the ride is made with this train
//   "type"            text, the catalogue id (core/traintype.h); left out when empty
//   "cars"            whole number, 1 or more; left out when 1
//   "carPitch"        m, centre of one car to the next; left out when 0
//   "rows"            list of m along the car from its centre, + toward the front; left out when [0]
//   "seats"           list of m across, + toward the rider's right; left out when [0]
//   "seatUp"          m from the heartline to where a rider's force is taken; left out when 0
//   "carMass", "riderMass" (kg), "load" (0 to 1), "carMassFactors" (list, one per car),
//   "massAtDesignPoint" (bool), "massAboveRail" (m): each left out at trainSpec()'s value
//   "resistance"      object {"fromProject", "rolling", "rollingLoad", "linear", "drag"}
//                     (rideResistance); left out when all are at their defaults;
//                     "fromProject" always written inside it, the others when not at their defaults
//   "referenceCar"    whole number, 1 = the front car; left out for the train's centre
//   "restraint"       "lapbar", "otsr", "vest", "prone" or "other"; left out when none
//   "extendedAirtime" bool, the extended -Gz line allowed; left out when false
//   "assumed"         list of the trainSpec fields that hold stand-ins (trainSpec::fromType());
//                     left out when empty
// Numbers are written as the text form writes a float (projectText::floatToJson():
// the shortest decimal that reads back as the same float, and -0 as the
// string "-0"), so every float comes back exactly. Any other key inside "train"
// (written by a later version) is kept and written back as it was.
//
// Values that cannot be read. A known key whose stored value this version
// cannot use (a restraint word it does not know, a load above 1, a number of
// the wrong kind, or a value valid only with others it does not fit, such as
// a reference car beyond the number of cars, or mass factors that are not one
// per car) is never lost: the train in memory uses that field's default, the
// stored value is kept as it was (trackTrain::kept) and written back by every
// train command and every save, and the reason is given
// (trainProblemsOfTrack(); fvdcore ride prints it). Only a command that sets
// that very field replaces it (train.setType sets every field of the
// description; train.clear removes the whole train). train.setCars also
// takes a kept reference car or kept mass factors into use once the new
// number of cars makes them valid, and otherwise leaves them kept.

#include "trainspec.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

class track;

struct trackTrain
{
    bool bSimulate;         // false: the train is described but not ridden (the default, owner's choice 3)
    bool bExtendedAirtime;  // the extended -Gz ("bungee") line of the force limits is allowed. A
                            // separate choice, not a property of the restraint kind: the standard asks
                            // for "special restraints" and leaves it to the ride analysis, and no public
                            // source ties a restraint word to it (R3 section 2, R13 table item 4)
    trainSpec spec;         // trainSpec(): the program's own point, the reference at the train's centre
    QStringList lAssumed;   // the fields of spec that hold stand-ins, as trainSpec::fromType() names them

    // Stored values this version could not use, kept as they were (see
    // "Values that cannot be read" above): the train's own key ("restraint",
    // "resistance.rolling" for a key inside "resistance") to its original JSON
    // value. The field in spec holds its default meanwhile. trainToJson()
    // writes these back unchanged; a command that sets the field drops it here.
    QJsonObject kept;
    // Why each kept value (and anything else odd, such as a train that is not
    // valid as a whole) could not be used, in plain words. Not part of ==.
    QStringList lProblems;

    trackTrain();
    // Every field but lProblems.
    bool operator==(const trackTrain& o) const;
    bool operator!=(const trackTrain& o) const { return !(*this == o); }

    // The restraint kind for the force limits (core/forcelimits.h,
    // limitOptions::sRestraint): the spec's restraint word, "" when none.
    QString restraint() const { return spec.sRestraint; }
    bool extendedAirtimeAllowed() const { return bExtendedAirtime; }
};

// The key of track::additions the train is kept under: "train".
QString trainAdditionsKey();

// The restraint words a train may hold ("" is none).
QStringList trainRestraintWords();

// A "train" object read into a trackTrain. A field whose value cannot be used
// takes its default in spec, its stored value goes to kept, and the reason to
// lProblems (and to *problems, when given); the rest is read. Unknown keys are
// ignored here (trainToJson() keeps them).
trackTrain trainFromJson(const QJsonObject& json, QStringList* problems = nullptr);

// The "train" object for train: keep's keys that are not the train's own
// fields first, as they were, then the fields of train as the shape above
// says, then train.kept's values over them. Deterministic: the same train and
// keep give the same object.
QJsonObject trainToJson(const trackTrain& train, const QJsonObject& keep = QJsonObject());

// Whether the track has a "train" key at all (a train was chosen).
bool trackHasTrain(const track* t);

// The track's train: trackTrain() when it has none. Its lProblems (also
// appended to *problems) say what of the stored train could not be used.
trackTrain trainOfTrack(const track* t, QStringList* problems = nullptr);

// What of the track's stored train this version cannot use, in plain words,
// one sentence each; empty when all of it is read (or there is no train).
// For the windows (piece V1) and fvdcore ride to show.
QStringList trainProblemsOfTrack(const track* t);

// Writes train into the track's additions, keeping unknown keys inside
// "train" and train.kept's values. When train is trackTrain() and nothing
// unknown or kept remains, the key "train" is removed, so a project goes back
// to having no appendix. Changes no node and computes nothing.
void setTrainOfTrack(track* t, const trackTrain& train);

// Takes the key "train" away (unknown and kept values inside it too).
void clearTrainOfTrack(track* t);

// The train a ride should use for this track: true and *out filled when the
// track has a train with simulate on (and its spec is valid; else *why says
// what is wrong and the result is false). *problems gets the track's train's
// problems (trainProblemsOfTrack()) whatever the result.
bool simulatedTrainOfTrack(const track* t, trainSpec* out, QString* why = nullptr, QStringList* problems = nullptr);

#endif // TRACKTRAIN_H
