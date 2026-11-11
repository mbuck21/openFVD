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

#include "command.h"
#include "trackhistory.h"
#include "track.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QMetaType>
#include <climits>
#include <cmath>

// ---- commandTarget ----------------------------------------------------------

commandTarget::commandTarget(int _track, int _section, int _function, int _transition)
    : track(_track), section(_section), function(_function), transition(_transition)
{
}

commandTarget commandTarget::forTrack(int track)
{
    return commandTarget(track);
}

commandTarget commandTarget::forSection(int track, int section)
{
    return commandTarget(track, section);
}

commandTarget commandTarget::forTransition(int track, int section, int function, int transition)
{
    return commandTarget(track, section, function, transition);
}

QString commandTarget::toText() const
{
    QStringList parts;
    if(track >= 0) parts << QString("track %1").arg(track+1);
    if(section >= 0) parts << QString("section %1").arg(section+1);
    if(function >= 0 && transition >= 0) {
        parts << QString("%1 transition %2").arg(functionName(function)).arg(transition+1);
    } else if(function >= 0) {
        parts << QString("%1 function").arg(functionName(function));
    } else if(transition >= 0) {
        parts << QString("transition %1").arg(transition+1);
    }
    return parts.isEmpty() ? QString("nothing") : parts.join(", ");
}

QJsonObject commandTarget::toJson() const
{
    QJsonObject json;
    if(track >= 0) json.insert("track", track+1);
    if(section >= 0) json.insert("section", section+1);
    if(function >= 0) json.insert("function", functionName(function));
    if(transition >= 0) json.insert("transition", transition+1);
    return json;
}

bool commandTarget::fromJson(const QJsonObject& json, commandTarget* out, QString* error)
{
    commandTarget t;
    for(QJsonObject::const_iterator it = json.constBegin(); it != json.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue v = it.value();
        if(key == "function") {
            int f = v.isString() ? functionFromName(v.toString()) : -1;
            if(f < 0) {
                if(error) *error = QString("target: \"function\" must be one of roll, normal, lateral, pitch, yaw");
                return false;
            }
            t.function = f;
            continue;
        }
        int* slot = key == "track" ? &t.track : key == "section" ? &t.section : key == "transition" ? &t.transition : NULL;
        if(slot == NULL) {
            if(error) *error = QString("target: unknown key \"%1\" (it takes track, section, function, transition)").arg(key);
            return false;
        }
        double d = v.toDouble(-1.0);
        if(!v.isDouble() || d < 1.0 || d > INT_MAX || std::floor(d) != d) {
            if(error) *error = QString("target: \"%1\" must be a whole number from 1").arg(key);
            return false;
        }
        *slot = (int)d - 1;
    }
    if(out) *out = t;
    return true;
}

bool commandTarget::operator==(const commandTarget& o) const
{
    return track == o.track && section == o.section && function == o.function && transition == o.transition;
}

QString functionName(int function)
{
    switch(function) {
    case funcRoll: return QString("roll");
    case funcNormal: return QString("normal");
    case funcLateral: return QString("lateral");
    case funcPitch: return QString("pitch");
    case funcYaw: return QString("yaw");
    }
    return QString("function %1").arg(function);
}

int functionFromName(const QString& name)
{
    for(int f = funcRoll; f <= funcYaw; ++f) {
        if(functionName(f) == name) return f;
    }
    return -1;
}

QString targetKindName(commandTargetKind kind)
{
    switch(kind) {
    case targetTrack: return QString("track");
    case targetSection: return QString("section");
    case targetTransition: return QString("transition");
    }
    return QString();
}

namespace {

QString sectionKindText(const section* s)
{
    switch(s->type) {
    case anchor: return QString("an anchor");
    case straight: return QString("a straight");
    case curved: return QString("a curve");
    case forced: return QString("a force section");
    case geometric: return QString("a geometric section");
    case bezier: return QString("a Bezier section");
    case nolimitscsv: return QString("a NoLimits CSV section");
    }
    return QString("a section");
}

}

bool resolveTarget(track* t, const commandTarget& target, commandTargetKind kind,
                   resolvedTarget* out, QString* error)
{
    resolvedTarget r;
    if(t == NULL) {
        if(error) *error = QString("no track");
        return false;
    }
    if(kind == targetTrack) {
        if(out) *out = r;
        return true;
    }

    if(target.section < 0) {
        if(error) *error = QString("the command needs a section");
        return false;
    }
    if(target.section >= t->lSections.size()) {
        if(error) *error = QString("there is no section %1: the track has %2 section%3")
                .arg(target.section+1).arg(t->lSections.size()).arg(t->lSections.size() == 1 ? "" : "s");
        return false;
    }
    r.sec = t->lSections[target.section];
    if(kind == targetSection) {
        if(out) *out = r;
        return true;
    }

    if(target.function < funcRoll || target.function > funcYaw) {
        if(error) *error = QString("the command needs a function (roll, normal, lateral, pitch or yaw)");
        return false;
    }
    func* f = NULL;
    switch(target.function) {
    case funcRoll: f = r.sec->rollFunc; break;
    case funcNormal: case funcPitch: f = r.sec->normForce; break;
    case funcLateral: case funcYaw: f = r.sec->latForce; break;
    }
    if(f == NULL || f->type != target.function) {
        if(error) *error = QString("section %1 is %2; it has no %3 transitions")
                .arg(target.section+1).arg(sectionKindText(r.sec)).arg(functionName(target.function));
        return false;
    }
    r.function = f;
    if(target.transition < 0) {
        if(error) *error = QString("the command needs a transition");
        return false;
    }
    if(target.transition >= f->funcList.size()) {
        if(error) *error = QString("there is no %1 transition %2 in section %3: it has %4")
                .arg(functionName(target.function)).arg(target.transition+1).arg(target.section+1).arg(f->funcList.size());
        return false;
    }
    r.transition = f->funcList[target.transition];
    if(out) *out = r;
    return true;
}

// ---- arguments --------------------------------------------------------------

QJsonObject commandArgsToJson(const commandArgs& args)
{
    QJsonObject json;
    for(commandArgs::const_iterator it = args.constBegin(); it != args.constEnd(); ++it) {
        json.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }
    return json;
}

commandArgs commandArgsFromJson(const QJsonObject& json)
{
    return json.toVariantMap();
}

QString paramTypeName(commandParamType type)
{
    switch(type) {
    case paramFloat: return QString("float");
    case paramInt: return QString("int");
    case paramBool: return QString("bool");
    case paramEnum: return QString("enum");
    case paramText: return QString("text");
    }
    return QString();
}

commandParam::commandParam()
    : type(paramFloat), hasRange(false), minimum(0.0), maximum(0.0)
{
}

commandParam commandParam::floatParam(const QString& name, const QString& unit, const QString& description)
{
    commandParam p;
    p.name = name;
    p.type = paramFloat;
    p.unit = unit;
    p.description = description;
    return p;
}

commandParam commandParam::floatParam(const QString& name, const QString& unit, const QString& description,
                                      double minimum, double maximum)
{
    commandParam p = floatParam(name, unit, description);
    p.hasRange = true;
    p.minimum = minimum;
    p.maximum = maximum;
    return p;
}

commandParam commandParam::intParam(const QString& name, const QString& unit, const QString& description)
{
    commandParam p;
    p.name = name;
    p.type = paramInt;
    p.unit = unit;
    p.description = description;
    return p;
}

commandParam commandParam::intParam(const QString& name, const QString& unit, const QString& description,
                                    int minimum, int maximum)
{
    commandParam p = intParam(name, unit, description);
    p.hasRange = true;
    p.minimum = minimum;
    p.maximum = maximum;
    return p;
}

commandParam commandParam::boolParam(const QString& name, const QString& description)
{
    commandParam p;
    p.name = name;
    p.type = paramBool;
    p.unit = QString("none");
    p.description = description;
    return p;
}

commandParam commandParam::enumParam(const QString& name, const QString& description, const QStringList& names)
{
    commandParam p;
    p.name = name;
    p.type = paramEnum;
    p.unit = QString("none");
    p.description = description;
    p.enumNames = names;
    return p;
}

commandParam commandParam::textParam(const QString& name, const QString& description)
{
    commandParam p;
    p.name = name;
    p.type = paramText;
    p.unit = QString("none");
    p.description = description;
    return p;
}

// ---- commandSpec ------------------------------------------------------------

commandSpec::commandSpec()
    : target(targetTrack), merges(false), exampleMayChangeNothing(false)
{
}

const commandParam* commandSpec::param(const QString& _name) const
{
    for(int i = 0; i < params.size(); ++i) {
        if(params[i].name == _name) return &params[i];
    }
    return NULL;
}

namespace {

bool isNumber(const QVariant& v)
{
    switch(v.userType()) {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    }
    return false;
}

QString numberText(double d)
{
    return QString::number(d, 'g', 9);
}

QString rangeText(const commandParam& p)
{
    return QString("%1 to %2").arg(numberText(p.minimum), numberText(p.maximum));
}

}

bool commandSpec::validate(const commandArgs& args, commandArgs* out, QString* error) const
{
    QStringList names;
    for(int i = 0; i < params.size(); ++i) names << params[i].name;

    for(commandArgs::const_iterator it = args.constBegin(); it != args.constEnd(); ++it) {
        if(param(it.key()) == NULL) {
            if(error) *error = QString("unknown parameter \"%1\" (%2 takes %3)")
                    .arg(it.key(), name, names.isEmpty() ? QString("none") : names.join(", "));
            return false;
        }
    }

    commandArgs result;
    for(int i = 0; i < params.size(); ++i) {
        const commandParam& p = params[i];
        if(!args.contains(p.name)) {
            if(error) *error = QString("parameter \"%1\" is missing (%2%3)")
                    .arg(p.name, paramTypeName(p.type), p.unit == "none" ? QString() : QString(", ") + p.unit);
            return false;
        }
        const QVariant v = args.value(p.name);
        switch(p.type) {
        case paramFloat: {
            if(!isNumber(v)) {
                if(error) *error = QString("parameter \"%1\" must be a number").arg(p.name);
                return false;
            }
            double d = v.toDouble();
            if(!std::isfinite(d)) {
                if(error) *error = QString("parameter \"%1\" must be a finite number").arg(p.name);
                return false;
            }
            if(p.hasRange && (d < p.minimum || d > p.maximum)) {
                if(error) *error = QString("parameter \"%1\" is %2, outside its range %3")
                        .arg(p.name, numberText(d), rangeText(p));
                return false;
            }
            result.insert(p.name, QVariant(d));
            break;
        }
        case paramInt: {
            double d = v.toDouble();
            if(!isNumber(v) || !std::isfinite(d) || std::floor(d) != d || d < INT_MIN || d > INT_MAX) {
                if(error) *error = QString("parameter \"%1\" must be a whole number").arg(p.name);
                return false;
            }
            if(p.hasRange && (d < p.minimum || d > p.maximum)) {
                if(error) *error = QString("parameter \"%1\" is %2, outside its range %3")
                        .arg(p.name, numberText(d), rangeText(p));
                return false;
            }
            result.insert(p.name, QVariant((int)d));
            break;
        }
        case paramBool:
            if(v.userType() != QMetaType::Bool) {
                if(error) *error = QString("parameter \"%1\" must be true or false").arg(p.name);
                return false;
            }
            result.insert(p.name, QVariant(v.toBool()));
            break;
        case paramEnum:
            if(v.userType() != QMetaType::QString || !p.enumNames.contains(v.toString())) {
                if(error) *error = QString("parameter \"%1\" must be one of %2").arg(p.name, p.enumNames.join(", "));
                return false;
            }
            result.insert(p.name, QVariant(v.toString()));
            break;
        case paramText:
            if(v.userType() != QMetaType::QString) {
                if(error) *error = QString("parameter \"%1\" must be a text").arg(p.name);
                return false;
            }
            result.insert(p.name, QVariant(v.toString()));
            break;
        }
    }
    if(out) *out = result;
    return true;
}

QString commandSpec::mergeKey(const commandTarget& t) const
{
    return merges ? name + " " + t.toText() : QString();
}

bool commandSpec::findExampleTarget(track* t, int trackIndex, commandTarget* out) const
{
    if(exampleTarget) return exampleTarget(t, trackIndex, out);
    switch(target) {
    case targetTrack:
        *out = commandTarget::forTrack(trackIndex);
        return true;
    case targetSection:
        if(t->lSections.isEmpty()) return false;
        *out = commandTarget::forSection(trackIndex, 0);
        return true;
    case targetTransition:
        for(int i = 0; i < t->lSections.size(); ++i) {
            if(t->lSections[i]->rollFunc != NULL && !t->lSections[i]->rollFunc->funcList.isEmpty()) {
                *out = commandTarget::forTransition(trackIndex, i, funcRoll, 0);
                return true;
            }
        }
        return false;
    }
    return false;
}

QString commandSpec::describeText() const
{
    QString text = QString("%1 (target: %2)\n  %3\n  Undo label: %4%5\n")
            .arg(name, targetKindName(target), description, label,
                 merges ? QString("; a run of changes to the same target is one step") : QString());
    if(params.isEmpty()) text += "  No parameters.\n";
    for(int i = 0; i < params.size(); ++i) {
        const commandParam& p = params[i];
        QString kind = paramTypeName(p.type);
        if(p.type == paramEnum) kind += " (" + p.enumNames.join(", ") + ")";
        QString extra;
        if(p.unit != "none") extra += ", in " + p.unit;
        if(p.hasRange) extra += ", " + rangeText(p);
        text += QString("  %1: %2%3. %4\n").arg(p.name, kind, extra, p.description);
    }
    text += "  Example: " + QString::fromUtf8(QJsonDocument(commandArgsToJson(example)).toJson(QJsonDocument::Compact)) + "\n";
    return text;
}

QJsonObject commandSpec::describeJson() const
{
    QJsonObject json;
    json.insert("name", name);
    json.insert("description", description);
    json.insert("target", targetKindName(target));
    json.insert("label", label);
    json.insert("merges", merges);
    QJsonArray list;
    for(int i = 0; i < params.size(); ++i) {
        const commandParam& p = params[i];
        QJsonObject pj;
        pj.insert("name", p.name);
        pj.insert("type", paramTypeName(p.type));
        pj.insert("unit", p.unit);
        pj.insert("description", p.description);
        if(p.type == paramEnum) pj.insert("values", QJsonArray::fromStringList(p.enumNames));
        if(p.hasRange) {
            pj.insert("minimum", p.minimum);
            pj.insert("maximum", p.maximum);
        }
        list.append(pj);
    }
    json.insert("parameters", list);
    json.insert("example", commandArgsToJson(example));
    return json;
}

// ---- commandRegistry --------------------------------------------------------

const QList<commandSpec>& commandRegistry::all()
{
    static const QList<commandSpec> list = anchorCommands() + sectionCommands() + transitionCommands()
            + trackCommands() + smoothingCommands();
    return list;
}

const commandSpec* commandRegistry::find(const QString& name)
{
    const QList<commandSpec>& list = all();
    for(int i = 0; i < list.size(); ++i) {
        if(list[i].name == name) return &list[i];
    }
    return NULL;
}

bool commandRegistry::run(trackHistory* history, const QString& name, const commandTarget& target,
                          const commandArgs& args, QString* error)
{
    const commandSpec* spec = find(name);
    if(spec == NULL) {
        if(error) *error = QString("unknown command \"%1\"").arg(name);
        return false;
    }
    return runSpec(history, *spec, target, args, error);
}

bool commandRegistry::runSpec(trackHistory* history, const commandSpec& spec, const commandTarget& target,
                              const commandArgs& args, QString* error)
{
    const QString& name = spec.name;
    if(history == NULL || history->getTrack() == NULL) {
        if(error) *error = QString("%1: no track to run it on").arg(name);
        return false;
    }
    if(!spec.apply) {
        if(error) *error = QString("%1: the command has no apply function").arg(name);
        return false;
    }
    commandArgs valid;
    QString why;
    if(!spec.validate(args, &valid, &why)) {
        if(error) *error = name + ": " + why;
        return false;
    }
    if(!resolveTarget(history->getTrack(), target, spec.target, NULL, &why)) {
        if(error) *error = name + ": " + why;
        return false;
    }

    history->beginStep(spec.label, target, spec.mergeKey(target));
    commandCall call;
    call.name = name;
    call.target = target;
    call.args = valid;
    history->recordCall(call);
    why.clear();
    bool ok = spec.apply(history->getTrack(), target, valid, &why);
    history->endStep(ok);
    if(!ok) {
        if(error) *error = name + " (" + target.toText() + "): " + (why.isEmpty() ? QString("failed") : why);
        return false;
    }
    return true;
}

QString commandRegistry::describe()
{
    const QList<commandSpec>& list = all();
    QString text = QString("%1 commands. Targets are given 1-based (track, section, function, transition);"
                           " functions are roll, normal, lateral, pitch and yaw.\n\n").arg(list.size());
    for(int i = 0; i < list.size(); ++i) {
        text += list[i].describeText() + "\n";
    }
    return text;
}

QJsonArray commandRegistry::describeJson()
{
    const QList<commandSpec>& list = all();
    QJsonArray out;
    for(int i = 0; i < list.size(); ++i) out.append(list[i].describeJson());
    return out;
}
