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

// The anchor group of commands (core/command.h): the values of the anchor,
// the start of the track, that the anchor boxes of the track panel show:
// position x, y and z, roll, pitch, yaw, speed, normal force, lateral force,
// pitch change rate and yaw change rate.
//
// Each apply function makes exactly the model steps of the panel's slot
// (ui/trackwidget.cpp), in the same order and with the same float arithmetic.
// The slot works out the value in model units (it divides by the length or
// speed factor of the chosen units) and passes it as a double, the type its
// own expression has; the command assigns it to the float as the slot did.
//
// The position is three commands, not one with x, y and z: each box's slot
// sets one coordinate of startPos from the angles as they are, and leaves the
// other two alone (they need not equal what the other boxes show, so setting
// all three would change them).
//
// An edit that changes the anchor node computes the track with
// track::updateFromAnchor(): the first section starts again from the anchor
// as it is now, as it does after saving and reopening.

#include "command.h"
#include "track.h"

#include <limits>
#include <type_traits>

// The slots this file's arithmetic comes from (ui/trackwidget.cpp) called the
// C library's cos, sin, fabs and sqrt, which take and return a double, also
// for a float argument: no float overload was in scope there. The same calls
// here must be those too, or the last bits change. So no "using namespace
// std" in this file; the build stops here if a header brings the float
// overloads into scope.
static_assert(std::is_same<decltype(cos(1.0f)), double>::value, "cos(float) must be the double cos, as in the anchor slots");
static_assert(std::is_same<decltype(sin(1.0f)), double>::value, "sin(float) must be the double sin, as in the anchor slots");
static_assert(std::is_same<decltype(fabs(1.0f)), double>::value, "fabs(float) must be the double fabs, as in the anchor slots");
static_assert(std::is_same<decltype(sqrt(1.0f)), double>::value, "sqrt(float) must be the double sqrt, as in the anchor slots");

namespace {

double arg(const commandArgs& args, const char* name)
{
    return args.value(QString(name)).toDouble();
}

// on_xBox_valueChanged(): the x of startPos that puts the anchor's heartline
// point at x, with the yaw, pitch and roll as they are. Nothing is recomputed:
// startPos only places the track in the scene (the exports do not use it).
bool applySetPositionX(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    float a = t->startYaw * F_PI/180.f;
    float b = t->startPitch * F_PI/180.f;
    float y = t->anchorNode->fRoll * F_PI/180.f;
    t->startPos.x = track::anchorStartPos(a, b, y, t->fHeart, arg(args, "x"), 0., 0.).x;
    return true;
}

// on_yBox_valueChanged(); the slot also rebuilds the mesh (window side).
bool applySetPositionY(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    float a = t->startYaw * F_PI/180.f;
    float b = t->startPitch * F_PI/180.f;
    float y = t->anchorNode->fRoll * F_PI/180.f;
    t->startPos.y = track::anchorStartPos(a, b, y, t->fHeart, 0., arg(args, "y"), 0.).y;

    t->hasChanged = true;
    return true;
}

// on_zBox_valueChanged()
bool applySetPositionZ(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    float a = t->startYaw * F_PI/180.f;
    float b = t->startPitch * F_PI/180.f;
    float y = t->anchorNode->fRoll * F_PI/180.f;
    t->startPos.z = track::anchorStartPos(a, b, y, t->fHeart, 0., 0., arg(args, "z")).z;
    return true;
}

// The end of the roll, pitch and yaw slots: startPos again, so that the
// heartline point at the start stays where the X, Y and Z boxes show it
// (holdX, holdY, holdZ, in metres), then the anchor's change rates from its
// forces.
void holdAnchorPoint(track* t, const commandArgs& args)
{
    float a = t->startYaw * F_PI/180.f;
    float b = t->startPitch * F_PI/180.f;
    float y = t->anchorNode->fRoll * F_PI/180.f;
    t->startPos = track::anchorStartPos(a, b, y, t->fHeart, arg(args, "holdX"), arg(args, "holdY"), arg(args, "holdZ"));

    t->updateAnchorGeometrics();
}

// on_rBox_valueChanged(), after the box has brought the value into -180..180.
bool applySetRoll(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double roll = arg(args, "roll");
    t->anchorNode->setRoll(roll-t->anchorNode->fRoll);
    holdAnchorPoint(t, args);
    t->updateFromAnchor();
    return true;
}

// on_pBox_valueChanged()
bool applySetPitch(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double pitch = arg(args, "pitch");
    t->startPitch = pitch;
    t->anchorNode->changePitch(pitch-t->anchorNode->getPitch(), fabs(t->anchorNode->fRoll) >= 90.f);
    holdAnchorPoint(t, args);
    t->updateFromAnchor();
    return true;
}

// on_jBox_valueChanged(), after the box has brought the value into -180..180.
bool applySetYaw(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double yaw = arg(args, "yaw");
    t->startYaw = yaw;
    holdAnchorPoint(t, args);
    t->updateFromAnchor();
    return true;
}

// on_anchorSpeedBox_valueChanged()
bool applySetSpeed(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    t->anchorNode->fVel = arg(args, "speed");
    t->anchorNode->fEnergy = 0.5*t->anchorNode->fVel*t->anchorNode->fVel + F_G*t->anchorNode->fPosHearty(0.9*t->fHeart);

    t->updateAnchorGeometrics();
    t->updateFromAnchor();
    return true;
}

// on_normalBox_valueChanged()
bool applySetNormalForce(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    t->anchorNode->forceNormal = arg(args, "force");

    t->updateAnchorGeometrics();
    t->updateFromAnchor();
    return true;
}

// on_lateralBox_valueChanged()
bool applySetLateralForce(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    t->anchorNode->forceLateral = arg(args, "force");

    t->updateAnchorGeometrics();
    t->updateFromAnchor();
    return true;
}

// The normal and lateral force at the anchor from its pitch and yaw change
// rates and its speed: the middle of on_pitchChangeBox_valueChanged() and
// on_yawChangeBox_valueChanged(), which each had a copy.
void forcesFromRates(mnode* anchor)
{
    float temp = cos(fabs(anchor->getPitch())*F_PI/180.f);
    float forceAngle = sqrt(temp*temp*anchor->fYawFromLast*anchor->fYawFromLast + anchor->fPitchFromLast*anchor->fPitchFromLast);//deltaAngle;
    float dirFromLast = glm::atan(anchor->fYawFromLast, anchor->fPitchFromLast) - TO_RAD(anchor->fRoll);

    glm::vec3 forceVec;
    if(fabs(forceAngle) < std::numeric_limits<float>::epsilon()) {
        forceVec = glm::vec3(0.f, 1.f, 0.f);
    } else {
        forceVec = glm::vec3(0.f, 1.f, 0.f) + (float)((anchor->fVel*anchor->fVel) / (9.80665 * anchor->fVel/forceAngle * 0.18f/F_PI)) * glm::normalize(glm::vec3(glm::rotate(dirFromLast, -anchor->vDir)*glm::vec4(-anchor->vNorm, 0.f)));
    }
    anchor->forceNormal = - glm::dot(forceVec, glm::normalize(anchor->vNorm));
    anchor->forceLateral = - glm::dot(forceVec, glm::normalize(anchor->vLat));
}

// on_pitchChangeBox_valueChanged(): the box shows degrees per second, the
// node holds degrees per node.
bool applySetPitchRate(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    mnode* anchor = t->anchorNode;
    anchor->fPitchFromLast = arg(args, "rate")/F_HZ;
    forcesFromRates(anchor);
    t->updateFromAnchor();
    return true;
}

// on_yawChangeBox_valueChanged()
bool applySetYawRate(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    mnode* anchor = t->anchorNode;
    anchor->fYawFromLast = arg(args, "rate")/F_HZ;
    forcesFromRates(anchor);
    t->updateFromAnchor();
    return true;
}

// The three hold parameters of the roll, pitch and yaw commands.
void addHoldParams(commandSpec& s)
{
    s.params << commandParam::floatParam("holdX", "m",
                                         "The x of the point that stays in place, the heartline at the start, as the anchor's X box shows it.",
                                         -9999.99, 9999.99)
             << commandParam::floatParam("holdY", "m",
                                         "The height of that point, as the anchor's Y box shows it.",
                                         -9999.99, 9999.99)
             << commandParam::floatParam("holdZ", "m",
                                         "The z of that point, as the anchor's Z box shows it.",
                                         -9999.99, 9999.99);
    s.example.insert("holdX", 3.0);
    s.example.insert("holdY", 6.5);
    s.example.insert("holdZ", -2.0);
}

commandSpec anchorSpec(const char* name, const char* description, const char* label, commandApplyFn apply)
{
    commandSpec s;
    s.name = QString(name);
    s.description = QString(description);
    s.target = targetTrack;
    s.label = QString(label);
    s.merges = true;
    s.apply = apply;
    return s;
}

}

// The expressions of the anchor boxes' slots and of Track Properties,
// unchanged (see track.h).
glm::vec3 track::anchorStartPos(float a, float b, float y, float fHeart, double holdX, double holdY, double holdZ)
{
    glm::vec3 pos;
    pos.x = -(cos(a)*sin(b)*cos(y)+sin(a)*sin(y))*fHeart + holdX;
    pos.y = (cos(b)*cos(y))*fHeart + holdY;
    pos.z = (sin(a)*sin(b)*cos(y)-cos(a)*sin(y))*fHeart + holdZ;
    return pos;
}

QList<commandSpec> anchorCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s = anchorSpec("anchor.setPositionX",
                                   "Moves the start of the track sideways: sets the x of the heartline point at the start.",
                                   "Move anchor (x)", applySetPositionX);
        s.params << commandParam::floatParam("x", "m", "The x of the heartline point at the start, as the anchor's X box shows it.",
                                             -9999.99, 9999.99);
        s.example.insert("x", 12.5);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setPositionY",
                                   "Raises or lowers the start of the track: sets the height of the heartline point at the start.",
                                   "Move anchor (y)", applySetPositionY);
        s.params << commandParam::floatParam("y", "m", "The height of the heartline point at the start, as the anchor's Y box shows it.",
                                             -9999.99, 9999.99);
        s.example.insert("y", 17.25);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setPositionZ",
                                   "Moves the start of the track forward or back: sets the z of the heartline point at the start.",
                                   "Move anchor (z)", applySetPositionZ);
        s.params << commandParam::floatParam("z", "m", "The z of the heartline point at the start, as the anchor's Z box shows it.",
                                             -9999.99, 9999.99);
        s.example.insert("z", -8.75);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setRoll",
                                   "Sets the roll (bank) of the track at its start, keeping the heartline point at the start in place.",
                                   "Change anchor roll", applySetRoll);
        s.params << commandParam::floatParam("roll", "deg", "The roll at the start (the box keeps it within -180 to 180).",
                                             -1800.0, 1800.0);
        s.example.insert("roll", 15.0);
        addHoldParams(s);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setPitch",
                                   "Sets the pitch of the track at its start, keeping the heartline point at the start in place.",
                                   "Change anchor pitch", applySetPitch);
        s.params << commandParam::floatParam("pitch", "deg", "The pitch at the start; positive points up.",
                                             -90.0, 90.0);
        s.example.insert("pitch", 12.0);
        addHoldParams(s);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setYaw",
                                   "Sets the direction the track starts in (its heading), keeping the heartline point at the start in place.",
                                   "Change anchor yaw", applySetYaw);
        s.params << commandParam::floatParam("yaw", "deg", "The heading at the start (the box keeps it within -180 to 180).",
                                             -1800.0, 1800.0);
        s.example.insert("yaw", 30.0);
        addHoldParams(s);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setSpeed",
                                   "Sets the speed of the train at the start of the track.",
                                   "Change start speed", applySetSpeed);
        s.params << commandParam::floatParam("speed", "m/s", "The speed at the start.",
                                             0.025, 500.0);
        s.example.insert("speed", 13.5);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setNormalForce",
                                   "Sets the normal force (the vertical g the rider feels) at the start of the track; the pitch and yaw change rates follow from it.",
                                   "Change start normal force", applySetNormalForce);
        s.params << commandParam::floatParam("force", "g", "The normal force at the start; 1 is what the rider feels standing still on level track.",
                                             -20.0, 20.0);
        s.example.insert("force", 1.4);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setLateralForce",
                                   "Sets the lateral force (the sideways g the rider feels) at the start of the track; the pitch and yaw change rates follow from it.",
                                   "Change start lateral force", applySetLateralForce);
        s.params << commandParam::floatParam("force", "g", "The lateral force at the start.",
                                             -15.0, 15.0);
        s.example.insert("force", 0.3);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setPitchRate",
                                   "Sets how fast the track pitches at its start; the normal and lateral force follow from it.",
                                   "Change start pitch rate", applySetPitchRate);
        s.params << commandParam::floatParam("rate", "deg/s", "The pitch change at the start, in degrees per second.",
                                             -1e9, 1e9);
        s.example.insert("rate", 7.5);
        list << s;
    }
    {
        commandSpec s = anchorSpec("anchor.setYawRate",
                                   "Sets how fast the track turns left or right at its start; the normal and lateral force follow from it.",
                                   "Change start yaw rate", applySetYawRate);
        s.params << commandParam::floatParam("rate", "deg/s", "The yaw change at the start, in degrees per second.",
                                             -1e9, 1e9);
        s.example.insert("rate", -6.0);
        list << s;
    }

    return list;
}
