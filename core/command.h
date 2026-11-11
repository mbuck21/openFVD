#ifndef COMMAND_H
#define COMMAND_H

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

// Named commands: every edit a person can make, as a name, typed parameters
// in model units, a description and a function that changes the model. The
// window's controls, the command-line program and an outside automation
// interface all run the same commands through commandRegistry::run(), which
// records each as one undo step in the track's trackHistory
// (docs/decisions/0011-undo-by-snapshot-commands-on-top.md).
//
// A new command: write its commandSpec in the group's file
// (commands_anchor.cpp, commands_section.cpp, commands_transition.cpp,
// commands_track.cpp, commands_smoothing.cpp) and append it to the list that
// file's function returns. Nothing else is registered by hand; the test
// tests/commands/ runs every command with undo and redo and fails when a
// description, a unit or an example is missing.

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <functional>

#include "function.h"

class track;
class section;
class func;
class subfunc;
class trackHistory;

// What a command acts on, given by numbers, never by pointer: internally
// 0-based, -1 for "not applicable". function is an eFunctype (function.h):
// a force section holds roll, normal and lateral and a geometric section roll,
// pitch and yaw in the same three slots (rollFunc, normForce, latForce).
// track is the track's place in the project; the history a command runs in
// already holds the track, so track is carried for logs and the windows, not
// used to find it. In text, numbers are shown 1-based, as the program shows them.
struct commandTarget
{
    int track;
    int section;
    int function;
    int transition;

    explicit commandTarget(int track = -1, int section = -1, int function = -1, int transition = -1);

    static commandTarget forTrack(int track);
    static commandTarget forSection(int track, int section);
    static commandTarget forTransition(int track, int section, int function, int transition);

    // "track 1, section 3, roll transition 2" (1-based); "nothing" when all are -1.
    QString toText() const;

    // {"track": 1, "section": 3, "function": "roll", "transition": 2},
    // 1-based, keys that are -1 left out. fromJson() reads the same and says
    // what is wrong in error.
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& json, commandTarget* out, QString* error);

    bool operator==(const commandTarget& o) const;
    bool operator!=(const commandTarget& o) const { return !(*this == o); }
};

// The text names of eFunctype: "roll", "normal", "lateral", "pitch", "yaw".
QString functionName(int function);
int functionFromName(const QString& name);     // -1 when unknown

// What a command's target must name.
enum commandTargetKind
{
    targetTrack,        // the track
    targetSection,      // a section of it
    targetTransition    // a transition: section, function and transition
};

QString targetKindName(commandTargetKind kind);  // "track", "section", "transition"

// A target found in the track. Found again at every run; never stored.
struct resolvedTarget
{
    section* sec;
    func* function;
    subfunc* transition;

    resolvedTarget() : sec(0), function(0), transition(0) {}
};

// Finds what target names in t, as far as kind needs. False, with a text
// naming the part that does not exist (1-based), when it is not there.
bool resolveTarget(track* t, const commandTarget& target, commandTargetKind kind,
                   resolvedTarget* out, QString* error);

// The arguments of a command: values by parameter name. A float is held as a
// double, an int as an int, a bool as a bool, an enum and a text as a QString.
typedef QVariantMap commandArgs;

// Arguments as JSON and back (a later unit reads commands as JSON lines).
// fromJson() takes the values as JSON has them (numbers are doubles);
// commandSpec::validate() turns them into the types above.
QJsonObject commandArgsToJson(const commandArgs& args);
commandArgs commandArgsFromJson(const QJsonObject& json);

enum commandParamType
{
    paramFloat,
    paramInt,
    paramBool,
    paramEnum,
    paramText
};

QString paramTypeName(commandParamType type);   // "float", "int", "bool", "enum", "text"

// One parameter of a command.
struct commandParam
{
    QString name;           // lowerCamel, as in the JSON ("length", "change")
    commandParamType type;
    QString unit;           // "m", "m/s", "deg", "g", "s", "deg/s", ... or "none"
    QString description;    // one sentence in plain words
    QStringList enumNames;  // for an enum: the allowed names
    bool hasRange;          // for a number: minimum and maximum apply
    double minimum;
    double maximum;

    commandParam();

    static commandParam floatParam(const QString& name, const QString& unit, const QString& description);
    static commandParam floatParam(const QString& name, const QString& unit, const QString& description,
                                   double minimum, double maximum);
    static commandParam intParam(const QString& name, const QString& unit, const QString& description);
    static commandParam intParam(const QString& name, const QString& unit, const QString& description,
                                 int minimum, int maximum);
    static commandParam boolParam(const QString& name, const QString& description);
    static commandParam enumParam(const QString& name, const QString& description, const QStringList& names);
    static commandParam textParam(const QString& name, const QString& description);
};

// Changes the model as the window's control does today, and recomputes the
// track. args are validated (every parameter present, of its type, in its
// range). False with a text in error when the edit cannot be made; the run
// then puts the track back as it was.
typedef std::function<bool(track*, const commandTarget&, const commandArgs&, QString* error)> commandApplyFn;

// Picks a target in t for the command's example arguments (the test runs
// every command on every project where this finds one). False when t has
// nothing the command applies to.
typedef std::function<bool(track* t, int trackIndex, commandTarget* out)> commandExampleTargetFn;

// One command.
struct commandSpec
{
    QString name;           // "<object>.<verb><Property>": objects anchor, track, section,
                            // straight, curve, transition, smoothing ("transition.setChange")
    QString description;    // one sentence in plain words
    commandTargetKind target;
    QList<commandParam> params;
    QString label;          // the undo list's text ("Change friction")
    bool merges;            // a run of this command on the same target, typed digit by
                            // digit, is one undo step (trackHistory, merging)
    commandArgs example;    // arguments the test runs (and documentation shows)
    commandExampleTargetFn exampleTarget;   // optional; by default the track, its first
                            // section, or the first roll transition of the first
                            // section that has one
    bool exampleMayChangeNothing;   // the example can leave some tracks as they were
    commandApplyFn apply;

    commandSpec();

    const commandParam* param(const QString& name) const;

    // Checks args against params: every parameter given, no other name, each
    // of its type and in its range. Writes the values in their types to out
    // (out may be &args). The error names the parameter.
    bool validate(const commandArgs& args, commandArgs* out, QString* error) const;

    // The merge key of a run on target: empty when the command does not merge.
    QString mergeKey(const commandTarget& target) const;

    // The target the example runs on in t, by exampleTarget or the default.
    bool findExampleTarget(track* t, int trackIndex, commandTarget* out) const;

    QString describeText() const;
    QJsonObject describeJson() const;
};

// The five groups. Each lives in its own file so that units adding commands
// to different groups do not touch the same file.
QList<commandSpec> anchorCommands();        // commands_anchor.cpp
QList<commandSpec> sectionCommands();       // commands_section.cpp
QList<commandSpec> transitionCommands();    // commands_transition.cpp
QList<commandSpec> trackCommands();         // commands_track.cpp
QList<commandSpec> smoothingCommands();     // commands_smoothing.cpp

// The list of every command, and the one way to run one.
class commandRegistry
{
public:
    // Every command, in group order (anchor, section, transition, track,
    // smoothing), each group in its file's order. Built once.
    static const QList<commandSpec>& all();
    static const commandSpec* find(const QString& name);

    // Runs one command on history's track: checks the name, the arguments and
    // the target, opens a step labelled with the spec's label and with target
    // as its selection, applies, and closes the step: committed on success
    // (nothing is recorded when nothing changed), rolled back on failure, so
    // a failed run leaves the track as it was. Inside an open scope the run
    // joins that scope's step. False with a text in error on any failure.
    static bool run(trackHistory* history, const QString& name, const commandTarget& target,
                    const commandArgs& args, QString* error);
    // The same for a spec given directly (run() finds it by name first).
    static bool runSpec(trackHistory* history, const commandSpec& spec, const commandTarget& target,
                        const commandArgs& args, QString* error);

    // Every command with its parameters, from the specs alone: what an agent
    // reads. As text, and as a JSON array of describeJson() objects.
    static QString describe();
    static QJsonArray describeJson();
};

#endif // COMMAND_H
