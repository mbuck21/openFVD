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

// The section group of commands (core/command.h). Empty for now; a later
// unit fills it.
//
// Belongs here (objects "section", "straight" and "curve"; today the section
// list and the section panels): add, delete, rename, straight length, curve
// angle, radius, direction, lead-in, lead-out, the fixed-speed tick, the
// fixed speed, time or distance, orientation.
//
// Each edit becomes a commandSpec appended to the list below. Its apply
// function changes the model and recomputes exactly as the window's slot does
// today: read the slot and do not move its float arithmetic. A command for
// one kind of section gives an exampleTarget that finds such a section.
// commands_track.cpp shows complete ones.

#include "command.h"

QList<commandSpec> sectionCommands()
{
    QList<commandSpec> list;
    return list;
}
