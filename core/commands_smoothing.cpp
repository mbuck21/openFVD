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

// The smoothing group of commands (core/command.h). Empty for now; a later
// unit fills it.
//
// Belongs here (object "smoothing"; today the smoothing dialog): an entry's
// length, passes, range, name, on or off; add and remove a region; apply.
//
// Each edit becomes a commandSpec appended to the list below. Its apply
// function changes the model and recomputes exactly as the dialog does today:
// read it and do not move its float arithmetic. A command that can leave a
// track as it was sets exampleMayChangeNothing. commands_track.cpp shows
// complete ones.

#include "command.h"

QList<commandSpec> smoothingCommands()
{
    QList<commandSpec> list;
    return list;
}
