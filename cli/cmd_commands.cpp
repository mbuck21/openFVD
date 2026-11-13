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

// fvdcore commands: the model's list of named commands (core/command.h) as
// text or JSON, without opening a project. The same list the session's
// "describe" returns.

#include "commands.h"

#include "../core/command.h"

#include <QJsonDocument>
#include <cstdio>

static int runCommands(const QStringList& args)
{
    bool json = false;
    for(int i = 0; i < args.size(); ++i) {
        if(args.at(i) == "--json") json = true;
        else {
            fprintf(stderr, "usage: %s\n", cmdCommands.usage);
            return 1;
        }
    }
    QByteArray bytes;
    if(json) {
        bytes = QJsonDocument(commandRegistry::describeJson()).toJson(QJsonDocument::Indented);
    } else {
        bytes = commandRegistry::describe().toUtf8();
    }
    fwrite(bytes.constData(), 1, bytes.size(), stdout);
    return 0;
}

const command cmdCommands = {
    "commands",
    "fvdcore commands [--json]",
    "the model's named commands with their parameters, units and ranges (text, or JSON)",
    runCommands
};
