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

// fvdcore help: lists the commands of the table in main.cpp. With a command's
// name, prints that command's usage line too.

#include "commands.h"

#include <cstdio>
#include <cstring>

static int runHelp(const QStringList& args)
{
    int count = 0;
    const command* const* table = commandTable(&count);
    if(args.size() == 1) {
        for(int i = 0; i < count; ++i) {
            if(args.at(0) == table[i]->name) {
                printf("%s\n\n%s\n", table[i]->description, table[i]->usage);
                return 0;
            }
        }
        fprintf(stderr, "no command called %s\n", qPrintable(args.at(0)));
        return 1;
    }
    printf("fvdcore: the FVD++ model without the windows. Metric units only.\n\n"
           "usage: fvdcore <command> [arguments]   (--log <file> keeps Qt's debug output)\n\n"
           "commands:\n");
    for(int i = 0; i < count; ++i) {
        printf("  %-9s %s\n", table[i]->name, table[i]->description);
    }
    printf("\n");
    for(int i = 0; i < count; ++i) {
        printf("  %s\n", table[i]->usage);
    }
    return 0;
}

const command cmdHelp = {
    "help",
    "fvdcore help [command]",
    "list the commands",
    runHelp
};
