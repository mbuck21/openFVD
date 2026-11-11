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

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <cstdio>

class track;

// One command of fvdcore: its name, the usage line and one-line description
// that `fvdcore help` prints, and the function that runs it. args holds
// everything after the command's name (--log is already taken out). The
// function returns the exit code: 0 for success, 1 for any error, with the
// error text on stderr.
struct command
{
    const char* name;
    const char* usage;
    const char* description;
    int (*run)(const QStringList& args);
};

// One struct per command, each defined in cli/cmd_<name>.cpp.
extern const command cmdExport;
extern const command cmdNodes;
extern const command cmdCompare;
extern const command cmdSelftest;
extern const command cmdHelp;

// The table in main.cpp (what `help` lists) and the dispatcher: runs the
// command named by args.at(0). An unknown or missing command prints the usage
// to errOut (nothing when errOut is NULL) and returns 1.
const command* const* commandTable(int* count);
int runCommand(const QStringList& args, FILE* errOut);

// What `compare` computes, shared with the self-test.
struct compareRow
{
    QString label;          // "section 3", "track end"
    double distance;        // m between the two end positions (world frame, centre line)
    double dHeading;        // degrees, b minus a, wrapped to -180..180
    double dPitch;          // degrees
    double dRoll;           // degrees (bank), b minus a, wrapped to -180..180
    double dSpeed;          // m/s
};
struct compareResult
{
    QList<compareRow> rows; // one per common section, then the track end
    int sectionsA, sectionsB;
    double lengthA, lengthB;     // m along the centre line
    double durationA, durationB; // s
};
compareResult compareTracks(track* a, track* b);
double wrap180(double angle);   // an angle change in degrees as -180 < angle <= 180

#endif
