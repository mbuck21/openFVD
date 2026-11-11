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
// Belongs here (object "track"; today the Track Properties dialog and the
// track list): friction, resistance, heartline, colours, style, wireframe,
// heartline drawing mode, name. Friction and resistance are done; the others
// come with a later unit.

#include "command.h"
#include "track.h"

namespace {

// TrackProperties::on_buttonBox_accepted() (ui/trackproperties.cpp), its
// model steps: the box's double is compared with the float, assigned to it,
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

    return list;
}
