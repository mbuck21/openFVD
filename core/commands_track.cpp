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

// The track group of commands (core/command.h).
//
// Object "track": what the Track Properties dialog edits (friction,
// resistance, heartline, the three colours, the heartline drawing mode, style,
// wireframe) and the track's name, typed into the project's track list. The
// dialog runs them inside one step that its OK commits and its Cancel rolls
// back (ui/trackproperties.cpp).

#include "command.h"
#include "track.h"

namespace {

// The Track Properties dialog's OK (TrackProperties::applyBoxes(), run from
// done(), ui/trackproperties.cpp), its model steps: the box's double is compared with the float, assigned to it,
// and the whole track recomputed; nothing when they are equal.
bool applySetFriction(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double value = args.value("friction").toDouble();
    if(t->fFriction != value) {
        t->fFriction = value;
        t->updateTrack(0, 0);
    }
    return true;
}

// The same for resistance. The dialog's box shows it in units of 1e-5 and
// passes value*1e-5; the command takes the model's value itself.
bool applySetResistance(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double value = args.value("resistance").toDouble();
    if(t->fResistance != value) {
        t->fResistance = value;
        t->updateTrack(0, 0);
    }
    return true;
}

// The dialog's heartline change, its model steps in their order: the
// heartline point of the anchor is taken with the old height, the height is
// set, and the anchor position (startPos) is moved so that the heartline
// point stays where it was; then the whole track is recomputed. The anchor
// position expression is copied verbatim from the dialog (the anchor commands
// compute the same expression for roll, pitch and yaw; one shared core
// function can replace both copies later, without changing the arithmetic).
bool applySetHeartline(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    double value = args.value("height").toDouble();
    if(t->fHeart != value) {
        mnode* anchor = t->anchorNode;
        glm::vec3 temp = anchor->vPosHeart(t->fHeart);
        float roll = anchor->fRoll*F_PI/180;
        float pitch = anchor->getPitch()*F_PI/180;
        float yaw = t->startYaw*F_PI/180;
        float x = -cos(yaw)*temp.z+sin(yaw)*temp.x+t->startPos.x;
        float y = temp.y+t->startPos.y;
        float z = cos(yaw)*temp.x+sin(yaw)*temp.z+t->startPos.z;

        t->fHeart = value;

        t->startPos.x = -(cos(yaw)*sin(pitch)*cos(roll)+sin(yaw)*sin(roll))*t->fHeart + x;
        t->startPos.y = (cos(pitch)*cos(roll))*t->fHeart + y;
        t->startPos.z = (sin(yaw)*sin(pitch)*cos(roll)-cos(yaw)*sin(roll))*t->fHeart + z;

        t->updateTrack(0, 0);
    }
    return true;
}

const QStringList colorNames = QStringList() << "default" << "section" << "transition";

// "#rrggbb" to a colour, as QColor(r, g, b) makes it (alpha 255).
bool colorFromText(const QString& text, QColor* out)
{
    if(text.size() != 7 || !text.startsWith('#')) return false;
    bool ok1, ok2, ok3;
    int r = text.mid(1, 2).toInt(&ok1, 16);
    int g = text.mid(3, 2).toInt(&ok2, 16);
    int b = text.mid(5, 2).toInt(&ok3, 16);
    if(!ok1 || !ok2 || !ok3) return false;
    *out = QColor(r, g, b);
    return true;
}

// The dialog's colour buttons: the picked colour goes into the track's
// display colours (saved with the project), which marks the track changed.
bool applySetColor(track* t, const commandTarget&, const commandArgs& args, QString* error)
{
    int which = colorNames.indexOf(args.value("which").toString());
    QColor color;
    if(!colorFromText(args.value("color").toString(), &color)) {
        if(error) *error = QString("\"color\": \"%1\" is not a colour written #rrggbb").arg(args.value("color").toString());
        return false;
    }
    t->display.colors[which] = color;
    t->hasChanged = true;
    return true;
}

const QStringList styleNames = QStringList() << "generic" << "genericflat" << "vekoma" << "bm"
                                             << "triangle" << "box" << "smallflat" << "doublespine";

// The dialog's style list (in the order of enum trackStyle, core/track.h):
// set, and the track marked changed. The 3D view's mesh is built again
// through the track's listener, as the dialog did with buildMeshes(0).
bool applySetStyle(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    enum trackStyle style = (enum trackStyle)styleNames.indexOf(args.value("style").toString());
    if(t->style != style) {
        t->style = style;
        if(t->mListener != NULL) t->mListener->trackGeometryChanged(t, 0);
        t->hasChanged = true;
    }
    return true;
}

// The dialog's wireframe tick, the same way.
bool applySetWireframe(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    bool on = args.value("on").toBool();
    if(t->display.bWireframe != on) {
        t->display.bWireframe = on;
        if(t->mListener != NULL) t->mListener->trackGeometryChanged(t, 0);
        t->hasChanged = true;
    }
    return true;
}

const QStringList drawingNames = QStringList() << "everything" << "trackOnly" << "heartlineOnly";

// The dialog's "Draw ..." list: its index is the track's drawHeartline.
bool applySetHeartlineDrawing(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    t->drawHeartline = drawingNames.indexOf(args.value("mode").toString());
    return true;
}

// The name typed into the project's track list (projectWidget::on_trackListWidget_itemChanged()).
bool applySetName(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    t->name = args.value("name").toString();
    return true;
}

}

QList<commandSpec> trackCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s;
        s.name = "track.setFriction";
        s.description = "Sets the track's friction coefficient: the train loses as much energy as climbing this many metres per metre travelled.";
        s.target = targetTrack;
        s.params << commandParam::floatParam("friction", "none",
                                             "The friction coefficient (the Track Properties box shows 0.03 for a new track).",
                                             0.0, 2.0);
        s.label = "Change friction";
        s.example.insert("friction", 0.0417);
        s.apply = applySetFriction;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setResistance";
        s.description = "Sets the track's air resistance coefficient: a drag that slows the train in proportion to the square of its speed.";
        s.target = targetTrack;
        s.params << commandParam::floatParam("resistance", "none",
                                             "The air resistance coefficient (the Track Properties box shows it times 100000: 2 there is 0.00002 here).",
                                             0.0, 0.001);
        s.label = "Change resistance";
        s.example.insert("resistance", 0.0000317);
        s.apply = applySetResistance;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setHeartline";
        s.description = "Sets the heartline height, the distance from the rails to the rider's heart that the forces are computed at; the anchor is moved so that the heartline's start stays where it was.";
        s.target = targetTrack;
        s.params << commandParam::floatParam("height", "m",
                                             "The heartline height above the rails (1.1 for a new track; negative for a track hung below).",
                                             -5.0, 5.0);
        s.label = "Change heartline";
        s.example.insert("height", 1.234);
        s.apply = applySetHeartline;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setColor";
        s.description = "Sets one of the three colours the track is drawn in: the default colour, the selected section's colour, or the selected transition's colour.";
        s.target = targetTrack;
        s.params << commandParam::enumParam("which", "Which colour: default, section (the selected section) or transition (the selected transition).",
                                            colorNames)
                 << commandParam::textParam("color", "The colour written #rrggbb, red, green and blue in two hexadecimal digits each (#ff3333 is the default section colour).");
        s.label = "Change track colour";
        s.example.insert("which", QString("section"));
        s.example.insert("color", QString("#123456"));
        s.apply = applySetColor;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setStyle";
        s.description = "Sets the style of rails the 3D view draws for the track.";
        s.target = targetTrack;
        s.params << commandParam::enumParam("style", "The rail style: generic, genericflat, vekoma, bm (B&M), triangle, box, smallflat or doublespine.",
                                            styleNames);
        s.label = "Change track style";
        s.example.insert("style", QString("doublespine"));
        s.apply = applySetStyle;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setWireframe";
        s.description = "Switches the 3D view between drawing the track solid and as a wireframe.";
        s.target = targetTrack;
        s.params << commandParam::boolParam("on", "True to draw the track as a wireframe, false to draw it solid.");
        s.label = "Change wireframe";
        s.example.insert("on", true);
        s.apply = applySetWireframe;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setHeartlineDrawing";
        s.description = "Sets what the 3D view draws of the track: the rails and the heartline, the rails only, or the heartline only.";
        s.target = targetTrack;
        s.params << commandParam::enumParam("mode", "What is drawn: everything, trackOnly or heartlineOnly.", drawingNames);
        s.label = "Change heartline drawing";
        s.example.insert("mode", QString("heartlineOnly"));
        s.apply = applySetHeartlineDrawing;
        list << s;
    }

    {
        commandSpec s;
        s.name = "track.setName";
        s.description = "Renames the track.";
        s.target = targetTrack;
        s.params << commandParam::textParam("name", "The track's new name, as the project's track list shows it.");
        s.label = "Rename track";
        s.example.insert("name", QString("Renamed by a command"));
        s.apply = applySetName;
        list << s;
    }

    return list;
}
