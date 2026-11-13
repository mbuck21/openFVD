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

#include <QByteArray>
#include <QDir>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <cstdio>

#include "../core/finding.h"
#include "../core/ridesummary.h"

class track;
class coreProject;

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
extern const command cmdSummary;
extern const command cmdMeasures;
extern const command cmdCheck;
extern const command cmdToText;
extern const command cmdFromText;
extern const command cmdImport;
extern const command cmdSession;
extern const command cmdCommands;
extern const command cmdLimits;
extern const command cmdSelftest;
extern const command cmdHelp;

// The table in main.cpp (what `help` lists) and the dispatcher: runs the
// command named by args.at(0). An unknown or missing command prints the usage
// to errOut (nothing when errOut is NULL) and returns 1.
const command* const* commandTable(int* count);
int runCommand(const QStringList& args, FILE* errOut);

// Code several commands share (one copy, defined in the command's file).
// The ride sheet as the JSON object `summary --json` prints per track
// (cmd_summary.cpp); withSections false leaves out the table of section ends.
QJsonObject summarySheetJson(const rideSummary& r, int trackNumber, const QString& trackName, bool withSections);
// The findings as the JSON object `check --json` prints (cmd_check.cpp).
QJsonObject checkFindingsJson(track* t, const QList<finding>& found);
// The three exports of every track with sections and the re-saved project, into
// dir as <stem>.* (cmd_export.cpp); the names written are appended to report.
bool writeProjectExports(coreProject& project, const QDir& dir, const QString& stem, QStringList& report, QString& error);
// The session's loop on text streams (cmd_session.cpp): reads one request per
// line from in and writes one reply per line to out until `quit` or the end.
// The project is loaded from file; trackNumber is the default track (1-based).
int runSession(const QString& projectFile, int trackNumber, FILE* in, FILE* out, QString* error);
// The same on text in memory: the replies (one line each) to the requests in input.
QByteArray sessionRunText(const QString& projectFile, int trackNumber, const QByteArray& input, QString* error);

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
