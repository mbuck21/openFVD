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

// The section group of commands (core/command.h).
//
// Objects "section", "straight" and "curve": the section list and the section
// panels of the track window. Each apply function does the model steps and the
// updateTrack() call of the window's slot (ui/trackwidget.cpp), with the same
// arguments in the same order. The slots work out the value in model units
// (a length divided by the length factor, a speed divided by the speed factor)
// before they run the command, so every number here is in model units: metres,
// metres per second, degrees. The ranges of the lengths and speeds cover what
// the boxes can send under every unit setting (the boxes limit the displayed
// number: a straight 0.1 to 500, a radius 1 to 500, a speed 0.1 to 500; in feet
// the smallest straight is 0.0305 m and the smallest radius 0.305 m, in km/h
// the smallest speed 0.0278 m/s), so a command refuses nothing a box accepts.
//
// Sections made by the importers (Bezier and NoLimits CSV) are not offered by
// section.add: no window adds them by hand. The panels have no controls for
// them, so the edit commands refuse them.
//
// The lead-in and lead-out are clamped as the boxes clamp them: the two never
// add up to more than the angle of the curve (the command clamps, it does not
// refuse).

#include "command.h"
#include "track.h"
#include "section.h"
#include "function.h"
#include "mnode.h"

namespace {

QStringList newSectionTypes()
{
    return QStringList() << "straight" << "curved" << "force" << "geometric";
}

bool isEditable(const section* s)
{
    return s->type == straight || s->type == curved || s->type == forced || s->type == geometric;
}

// the target section, which must be one of the four kinds the panels edit
bool resolveEditable(track* t, const commandTarget& target, section** out, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetSection, &r, error)) return false;
    if(!isEditable(r.sec)) {
        if(error) *error = QString("section %1 is a Bezier or NoLimits CSV section; it has no settings to edit here").arg(target.section+1);
        return false;
    }
    *out = r.sec;
    return true;
}

bool resolveOfType(track* t, const commandTarget& target, secType type, const char* what, section** out, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetSection, &r, error)) return false;
    if(r.sec->type != type) {
        if(error) *error = QString("section %1 is not %2").arg(target.section+1).arg(what);
        return false;
    }
    *out = r.sec;
    return true;
}

// ---- add, delete and rename ----------------------------------------------------

// trackWidget::addSection() and appendSection(): the section is created at
// place "after" (0: first), named "unnamed", and the track is computed from it.
bool applyAdd(track* t, const commandTarget&, const commandArgs& args, QString* error)
{
    QString name = args.value("type").toString();
    secType type;
    if(name == "straight") type = straight;
    else if(name == "curved") type = curved;
    else if(name == "force") type = forced;
    else if(name == "geometric") type = geometric;
    else {
        if(error) *error = QString("\"type\" must be straight, curved, force or geometric");
        return false;
    }
    int after = args.value("after").toInt();
    if(after < 0 || after > t->lSections.size()) {
        if(error) *error = QString("\"after\" must be 0 to %1 (the number of the section the new one goes after)").arg(t->lSections.size());
        return false;
    }
    t->newSection(type, after);
    section* added = t->lSections[after];
    added->sName = QString("unnamed");
    t->updateTrack(added, 0);
    return true;
}

// on_deleteButton_released(): track::removeSection()
bool applyDelete(track* t, const commandTarget& target, const commandArgs&, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetSection, &r, error)) return false;
    t->removeSection(target.section);
    return true;
}

// on_sectionListWidget_itemChanged(): writeNames()
bool applySetName(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetSection, &r, error)) return false;
    r.sec->sName = args.value("name").toString();
    return true;
}

// ---- the speed ----------------------------------------------------------------

// on_straightSpeedCheck_, on_curvedSpeedCheck_ and on_advancedSpeedCheck_:
// bSpeed true means the speed comes from the physics, so a fixed speed is
// bSpeed false.
bool applySetSpeedFixed(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveEditable(t, target, &s, error)) return false;
    s->bSpeed = !args.value("fixed").toBool();
    t->updateTrack(s, 0);
    return true;
}

// on_straightSpeedBox_, on_curvedSpeedBox_ and on_advancedSpeedBox_
bool applySetSpeed(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveEditable(t, target, &s, error)) return false;
    s->fVel = args.value("speed").toDouble();
    t->updateTrack(s, 0);
    return true;
}

// ---- straight and curve -------------------------------------------------------

// on_straightLengthBox_valueChanged()
bool applySetLength(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, straight, "a straight", &s, error)) return false;
    s->rollFunc->setMaxArgument(args.value("length").toDouble());
    t->updateTrack(s, 0);
    return true;
}

// on_curvedAngleBox_valueChanged()
bool applySetAngle(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, curved, "a curve", &s, error)) return false;
    s->rollFunc->setMaxArgument(args.value("angle").toDouble());
    t->updateTrack(s, 0);
    return true;
}

// on_curvedRadiusBox_valueChanged()
bool applySetRadius(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, curved, "a curve", &s, error)) return false;
    s->fRadius = args.value("radius").toDouble();
    t->updateTrack(s, 0);
    return true;
}

// on_curvedDirectionBox_valueChanged()
bool applySetDirection(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, curved, "a curve", &s, error)) return false;
    s->fDirection = args.value("direction").toDouble();
    t->updateTrack(s, 0);
    return true;
}

// on_curvedLeadInBox_valueChanged(): lead-in and lead-out together never
// exceed the angle; a larger lead-in is cut to what is left.
bool applySetLeadIn(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, curved, "a curve", &s, error)) return false;
    double arg1 = args.value("leadIn").toDouble();
    if(arg1+s->fLeadOut > s->fAngle) {
        arg1 = s->fAngle - s->fLeadOut;
    }
    s->fLeadIn = arg1;
    t->updateTrack(s, 0);
    return true;
}

// on_curvedLeadOutBox_valueChanged()
bool applySetLeadOut(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveOfType(t, target, curved, "a curve", &s, error)) return false;
    double arg1 = args.value("leadOut").toDouble();
    if(arg1+s->fLeadIn > s->fAngle) {
        arg1 = s->fAngle - s->fLeadIn;
    }
    s->fLeadOut = arg1;
    t->updateTrack(s, 0);
    return true;
}

// ---- the options ---------------------------------------------------------------

// on_argumentBox_currentIndexChanged(): the slot converts nothing, the section
// just runs on the other argument. Only force and geometric sections offer the
// choice.
bool applySetArgument(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveEditable(t, target, &s, error)) return false;
    if(s->type != forced && s->type != geometric) {
        if(error) *error = QString("only force and geometric sections choose between time and distance");
        return false;
    }
    QString name = args.value("argument").toString();
    if(name != "time" && name != "distance") {
        if(error) *error = QString("\"argument\" must be time or distance");
        return false;
    }
    s->bArgument = name == "distance" ? DISTANCE : TIME;
    t->updateTrack(s, 0);
    return true;
}

// on_orientationBox_currentIndexChanged(): quaternion is false, euler is true
bool applySetOrientation(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    section* s;
    if(!resolveEditable(t, target, &s, error)) return false;
    if(s->type == straight) {
        if(error) *error = QString("a straight has no orientation choice");
        return false;
    }
    QString name = args.value("orientation").toString();
    if(name != "euler" && name != "quaternion") {
        if(error) *error = QString("\"orientation\" must be euler or quaternion");
        return false;
    }
    s->bOrientation = name == "euler" ? EULER : QUATERNION;
    t->updateTrack(s, 0);
    return true;
}

// ---- example targets -------------------------------------------------------------

typedef std::function<bool(section*)> sectionTest;

bool findSection(track* t, int trackIndex, commandTarget* out, const sectionTest& test)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        if(test(t->lSections[i])) {
            *out = commandTarget::forSection(trackIndex, i);
            return true;
        }
    }
    return false;
}

bool anyEditable(track* t, int ti, commandTarget* out)
{
    return findSection(t, ti, out, [](section* s) { return isEditable(s); });
}

bool aStraight(track* t, int ti, commandTarget* out)
{
    return findSection(t, ti, out, [](section* s) { return s->type == straight; });
}

bool aCurve(track* t, int ti, commandTarget* out)
{
    return findSection(t, ti, out, [](section* s) { return s->type == curved; });
}

// an editable section whose speed comes from the physics, so that fixing it changes something
bool physicsSpeed(track* t, int ti, commandTarget* out)
{
    return findSection(t, ti, out, [](section* s) { return isEditable(s) && s->bSpeed; });
}

// force sections first: on a geometric section that runs on distance a
// recompute from the edited point is not the whole-track recompute (issue 63)
bool timeSection(track* t, int ti, commandTarget* out)
{
    if(findSection(t, ti, out, [](section* s) { return s->type == forced && s->bArgument == TIME; })) return true;
    return findSection(t, ti, out, [](section* s) { return s->type == geometric && s->bArgument == TIME; });
}

// Euler angles on a force section can fail the model's check on the lateral
// vector (mnode.cpp, vLat.y) when the track is steep, as the window's box would,
// so the example prefers a curve, then a geometric section on time.
bool eulerCapable(track* t, int ti, commandTarget* out)
{
    if(findSection(t, ti, out, [](section* s) { return s->type == curved && s->bOrientation == QUATERNION; })) return true;
    return findSection(t, ti, out, [](section* s) { return s->type == geometric && s->bArgument == TIME && s->bOrientation == QUATERNION; });
}

bool anySection(track* t, int ti, commandTarget* out)
{
    *out = commandTarget::forTrack(ti);
    return !t->lSections.isEmpty();
}

}

QList<commandSpec> sectionCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s;
        s.name = "section.add";
        s.description = "Adds a new section of the given kind, named \"unnamed\", after the section with the given number (0 for the very start); the track is recomputed from it.";
        s.target = targetTrack;
        s.params << commandParam::enumParam("type",
                                            "The kind of section: straight, curved, force or geometric. (Bezier and NoLimits CSV sections are made only by the importers.)",
                                            newSectionTypes());
        s.params << commandParam::intParam("after", "none",
                                           "The number of the section the new one goes after, counting from 1; 0 puts it first and the number of sections puts it last.",
                                           0, 100000);
        s.label = "Add section";
        s.example.insert("type", QString("curved"));
        s.example.insert("after", 0);
        s.exampleTarget = anySection;
        s.apply = applyAdd;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.delete";
        s.description = "Removes the target section; the sections after it move up and the track is recomputed.";
        s.target = targetSection;
        s.label = "Delete section";
        s.exampleTarget = anyEditable;
        s.apply = applyDelete;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.setName";
        s.description = "Gives the section a name, as typed in the section list.";
        s.target = targetSection;
        s.params << commandParam::textParam("name", "The new name; it may be empty.");
        s.label = "Rename section";
        s.example.insert("name", QString("Lift hill"));
        s.exampleTarget = anyEditable;
        s.apply = applySetName;
        list << s;
    }

    {
        commandSpec s;
        s.name = "straight.setLength";
        s.description = "Sets the length of a straight; the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::floatParam("length", "m", "The straight's length.", 0.03, 500.0);
        s.label = "Change straight length";
        s.merges = true;
        s.example.insert("length", 12.375);
        s.exampleTarget = aStraight;
        s.apply = applySetLength;
        list << s;
    }

    {
        commandSpec s;
        s.name = "curve.setAngle";
        s.description = "Sets how far a curve turns in total; the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::floatParam("angle", "deg", "The angle the curve turns through.", 1.0, 5000.0);
        s.label = "Change curve angle";
        s.merges = true;
        s.example.insert("angle", 77.5);
        s.exampleTarget = aCurve;
        s.apply = applySetAngle;
        list << s;
    }

    {
        commandSpec s;
        s.name = "curve.setRadius";
        s.description = "Sets the radius of a curve; the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::floatParam("radius", "m", "The radius of the curve.", 0.3, 500.0);
        s.label = "Change curve radius";
        s.merges = true;
        s.example.insert("radius", 23.25);
        s.exampleTarget = aCurve;
        s.apply = applySetRadius;
        list << s;
    }

    {
        commandSpec s;
        s.name = "curve.setDirection";
        s.description = "Sets the direction a curve turns in, the angle of the plane it turns in around the track's direction.";
        s.target = targetSection;
        s.params << commandParam::floatParam("direction", "deg", "The direction of the curve, from -180 to 180.", -180.0, 180.0);
        s.label = "Change curve direction";
        s.merges = true;
        s.example.insert("direction", 33.5);
        s.exampleTarget = aCurve;
        s.apply = applySetDirection;
        list << s;
    }

    {
        commandSpec s;
        s.name = "curve.setLeadIn";
        s.description = "Sets how much of a curve's angle is used to ease into the turn; a value that would take lead-in and lead-out past the curve's angle is cut to what is left.";
        s.target = targetSection;
        s.params << commandParam::floatParam("leadIn", "deg", "The angle of the lead-in.", 0.0, 500.0);
        s.label = "Change curve lead-in";
        s.merges = true;
        s.example.insert("leadIn", 5.5);
        s.exampleTarget = aCurve;
        s.apply = applySetLeadIn;
        list << s;
    }

    {
        commandSpec s;
        s.name = "curve.setLeadOut";
        s.description = "Sets how much of a curve's angle is used to ease out of the turn; a value that would take lead-in and lead-out past the curve's angle is cut to what is left.";
        s.target = targetSection;
        s.params << commandParam::floatParam("leadOut", "deg", "The angle of the lead-out.", 0.0, 500.0);
        s.label = "Change curve lead-out";
        s.merges = true;
        s.example.insert("leadOut", 4.5);
        s.exampleTarget = aCurve;
        s.apply = applySetLeadOut;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.setSpeedFixed";
        s.description = "Chooses whether a straight, curve, force or geometric section runs at a speed you give (true) or at the speed the physics gives (false); the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::boolParam("fixed", "true for the fixed speed (see section.setSpeed), false for the speed from the physics (friction and gravity).");
        s.label = "Switch fixed speed on or off";
        s.example.insert("fixed", true);
        s.exampleTarget = physicsSpeed;
        s.apply = applySetSpeedFixed;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.setSpeed";
        s.description = "Sets the fixed speed of a straight, curve, force or geometric section; it is used while the section runs at a fixed speed (section.setSpeedFixed).";
        s.target = targetSection;
        s.params << commandParam::floatParam("speed", "m/s", "The speed.", 0.027, 500.0);
        s.label = "Change section speed";
        s.merges = true;
        s.example.insert("speed", 17.25);
        s.exampleTarget = anyEditable;
        s.apply = applySetSpeed;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.setArgument";
        s.description = "Chooses whether the transitions of a force or geometric section are measured in time or in distance; the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::enumParam("argument", "time for seconds, distance for metres.",
                                            QStringList() << "time" << "distance");
        s.label = "Change time or distance";
        s.example.insert("argument", QString("distance"));
        s.exampleTarget = timeSection;
        s.apply = applySetArgument;
        list << s;
    }

    {
        commandSpec s;
        s.name = "section.setOrientation";
        s.description = "Chooses how a curve, force or geometric section works out its roll, pitch and yaw: with Euler angles or with a quaternion; the track is recomputed from it.";
        s.target = targetSection;
        s.params << commandParam::enumParam("orientation", "euler or quaternion.",
                                            QStringList() << "euler" << "quaternion");
        s.label = "Change orientation";
        s.example.insert("orientation", QString("euler"));
        s.exampleTarget = eulerCapable;
        s.apply = applySetOrientation;
        list << s;
    }

    return list;
}
