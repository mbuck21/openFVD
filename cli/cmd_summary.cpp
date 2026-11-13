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

// fvdcore summary: the ride sheet (core/ridesummary.h) of a project's tracks,
// as aligned text or, with --json, as one JSON object ({"tracks": [...]}) with stable key names.
// Metric units only. The key names are described in cli/README.md.

#include "commands.h"
#include "project.h"

#include "../core/ridesummary.h"
#include "../core/mnode.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegExp>
#include <cmath>
#include <cstdio>

static QString f(double v, int decimals)
{
    QString text = QString::number(v, 'f', decimals);
    // a value that rounds to zero is printed without a minus sign
    if(text.startsWith('-') && text.indexOf(QRegExp("[1-9]")) < 0) text.remove(0, 1);
    return text;
}

static QString where(const rideSpot& s)
{
    return QString("at %1 m, %2 s (section %3)").arg(f(s.dist_m, 1)).arg(f(s.time_s, 1)).arg(s.section);
}

static QString valueLine(const QString& label, const rideSpot& s, int decimals, const QString& unit)
{
    QString text = QString("  %1").arg(label, -48);
    if(!s.valid) return text + "none\n";
    return text + QString("%1 %2  %3\n").arg(f(s.value, decimals), 9).arg(unit, -3).arg(where(s));
}

static QString fallLine(const QString& label, const rideFall& d)
{
    QString text = QString("  %1").arg(label, -48);
    if(!d.valid) return text + "none\n";
    return text + QString("%1 m  from %2 m (%3 s) down to %4 m (%5 s), height %6 m to %7 m\n")
            .arg(f(d.amount_m, 1), 9).arg(f(d.start.dist_m, 1)).arg(f(d.start.time_s, 1))
            .arg(f(d.end.dist_m, 1)).arg(f(d.end.time_s, 1)).arg(f(d.start.value, 1)).arg(f(d.end.value, 1));
}

static QString climbLine(const QString& label, const rideFall& d)
{
    QString text = QString("  %1").arg(label, -48);
    if(!d.valid) return text + "none\n";
    return text + QString("%1 m  from %2 m (%3 s) up to %4 m (%5 s), height %6 m to %7 m\n")
            .arg(f(d.amount_m, 1), 9).arg(f(d.start.dist_m, 1)).arg(f(d.start.time_s, 1))
            .arg(f(d.end.dist_m, 1)).arg(f(d.end.time_s, 1)).arg(f(d.start.value, 1)).arg(f(d.end.value, 1));
}

static QString plain(const QString& label, const QString& value)
{
    return QString("  %1%2\n").arg(label, -48).arg(value);
}

static QString sheetText(const rideSummary& r, bool withSections)
{
    QString t;
    if(r.empty) {
        t += "  The track has no sections.\n";
        return t;
    }
    t += plain("Length (centre line)", f(r.length_m, 1) + " m");
    t += plain("Length (heartline)", f(r.heartLength_m, 1) + " m");
    t += plain("Duration", f(r.duration_s, 2) + " s");
    t += valueLine("Highest point", r.highest, 1, "m");
    t += valueLine("Lowest point", r.lowest, 1, "m");
    t += plain("Height difference", f(r.heightRange_m, 1) + " m");
    t += fallLine("Biggest drop", r.biggestDrop);
    t += climbLine("Biggest climb", r.biggestClimb);
    t += valueLine("Top speed", r.topSpeed, 1, "m/s");
    t += valueLine("Lowest speed", r.lowSpeed, 1, "m/s");
    rideSummaryOptions options;
    t += plain(rideFloaterLabel(options), f(r.floaterAirtime_s, 2) + " s");
    t += plain(rideEjectorLabel(options), f(r.ejectorAirtime_s, 2) + " s");
    t += plain("Total airtime", f(r.totalAirtime_s, 2) + " s");
    t += valueLine("Highest vertical force", r.normalMax, 2, "g");
    t += valueLine("Lowest vertical force", r.normalMin, 2, "g");
    t += valueLine("Lateral, rider pressed right", r.lateralRight, 2, "g");
    t += valueLine("Lateral, rider pressed left", r.lateralLeft, 2, "g");
    t += valueLine("Along-track, pressed back", r.alongMax, 2, "g");
    t += valueLine("Along-track, thrown forward", r.alongMin, 2, "g");
    t += plain(rideInversionLabel(options), QString::number(r.inversions));
    t += valueLine("Largest angle from upright", r.maxTilt, 1, "deg");
    t += plain("Findings", QString("%1 problem(s), %2 warning(s), %3 information")
               .arg(r.findingCount[findingProblem]).arg(r.findingCount[findingWarning]).arg(r.findingCount[findingInformation]));
    t += "\n  " + rideSheetNote() + "\n";
    if(r.nonFinite) {
        t += "  Note: some values are not numbers (the train stalls); they are left out above.\n";
    }
    if(r.lengthStopsAtStall) {
        t += "  Note: the length stops at the stall (the last node whose length is a number).\n";
    }
    if(withSections) {
        t += "\n  Sections (end position is the centre line in the world frame)\n";
        t += QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12\n").arg("no", 3).arg("type", -13).arg("name", -16)
                .arg("length m", 9).arg("time s", 8).arg("x m", 9).arg("y m", 8).arg("z m", 9)
                .arg("heading", 8).arg("pitch", 7).arg("bank", 8).arg("m/s", 6);
        for(int i = 0; i < r.sections.size(); ++i) {
            const rideSectionInfo& s = r.sections.at(i);
            t += QString("  %1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12\n").arg(s.number, 3).arg(s.typeName, -13).arg(s.name.left(16), -16)
                    .arg(f(s.length_m, 1), 9).arg(f(s.duration_s, 1), 8)
                    .arg(f(s.endPosition.x, 1), 9).arg(f(s.endPosition.y, 1), 8).arg(f(s.endPosition.z, 1), 9)
                    .arg(f(s.endHeading_deg, 1), 8).arg(f(s.endPitch_deg, 1), 7).arg(f(s.endBank_deg, 1), 8)
                    .arg(f(s.endSpeed_mps, 1), 6);
        }
    }
    return t;
}

static QJsonValue num(double v)
{
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

// Seconds and metres that are sums of steps, rounded to 4 decimals (2.617,
// not 2.616999999999823). Only the JSON is rounded; the text sheet prints its
// own digits.
static QJsonValue rounded(double v)
{
    return std::isfinite(v) ? QJsonValue(std::floor(v*10000.0 + 0.5)/10000.0) : QJsonValue(QJsonValue::Null);
}

static QJsonValue spotJson(const rideSpot& s)
{
    if(!s.valid) return QJsonValue(QJsonValue::Null);
    QJsonObject o;
    o["value"] = num(s.value);
    o["node"] = s.node;
    o["section"] = s.section;
    o["time_s"] = rounded(s.time_s);
    o["dist_m"] = rounded(s.dist_m);
    return o;
}

static QJsonValue fallJson(const rideFall& d)
{
    if(!d.valid) return QJsonValue(QJsonValue::Null);
    QJsonObject o;
    o["amount_m"] = rounded(d.amount_m);
    o["start"] = spotJson(d.start);
    o["end"] = spotJson(d.end);
    return o;
}

// The ride sheet as one JSON object, shared with the session's "summary".
// withSections false leaves out "section_list" (the table of section ends).
QJsonObject summarySheetJson(const rideSummary& r, int trackNumber, const QString& trackName, bool withSections)
{
    QJsonObject o;
    o["track"] = trackNumber;
    o["name"] = trackName;
    o["empty"] = r.empty;
    o["non_finite"] = r.nonFinite;
    o["length_stops_at_stall"] = r.lengthStopsAtStall;
    o["sections"] = r.sectionCount;
    o["nodes"] = r.nodeCount;
    o["length_m"] = rounded(r.length_m);
    o["heart_length_m"] = rounded(r.heartLength_m);
    o["duration_s"] = rounded(r.duration_s);
    o["highest"] = spotJson(r.highest);
    o["lowest"] = spotJson(r.lowest);
    o["height_range_m"] = rounded(r.heightRange_m);
    o["biggest_drop"] = fallJson(r.biggestDrop);
    o["biggest_climb"] = fallJson(r.biggestClimb);
    o["top_speed"] = spotJson(r.topSpeed);
    o["lowest_speed"] = spotJson(r.lowSpeed);
    o["floater_airtime_s"] = rounded(r.floaterAirtime_s);
    o["ejector_airtime_s"] = rounded(r.ejectorAirtime_s);
    o["total_airtime_s"] = rounded(r.totalAirtime_s);
    o["normal_max_g"] = spotJson(r.normalMax);
    o["normal_min_g"] = spotJson(r.normalMin);
    o["lateral_right_g"] = spotJson(r.lateralRight);     // rider pressed toward his right (positive)
    o["lateral_left_g"] = spotJson(r.lateralLeft);       // rider pressed toward his left (negative)
    o["along_max_g"] = spotJson(r.alongMax);
    o["along_min_g"] = spotJson(r.alongMin);
    o["inversions"] = r.inversions;
    rideSummaryOptions options;
    QJsonObject labels;
    labels["floater_airtime"] = rideFloaterLabel(options);
    labels["ejector_airtime"] = rideEjectorLabel(options);
    labels["inversions"] = rideInversionLabel(options);
    o["labels"] = labels;
    o["note"] = rideSheetNote();
    o["max_tilt_deg"] = spotJson(r.maxTilt);
    QJsonObject counts;
    counts["problem"] = r.findingCount[findingProblem];
    counts["warning"] = r.findingCount[findingWarning];
    counts["information"] = r.findingCount[findingInformation];
    o["finding_counts"] = counts;
    QJsonArray findings;
    for(int i = 0; i < r.findings.size(); ++i) {
        const finding& fi = r.findings.at(i);
        QJsonObject fo;
        fo["check"] = fi.checkId;
        fo["measure"] = fi.measureId;
        fo["severity"] = fi.severity == findingProblem ? "problem" : (fi.severity == findingWarning ? "warning" : "information");
        fo["from_node"] = fi.fromNode;
        fo["to_node"] = fi.toNode;
        fo["worst_node"] = fi.worstNode;
        fo["worst_value"] = num(fi.worstValue);
        fo["limit"] = num(fi.limit);
        fo["non_finite"] = fi.nonFinite;
        fo["text"] = fi.text;
        findings.append(fo);
    }
    o["findings"] = findings;
    if(!withSections) return o;
    QJsonArray secs;
    for(int i = 0; i < r.sections.size(); ++i) {
        const rideSectionInfo& s = r.sections.at(i);
        QJsonObject so;
        so["number"] = s.number;
        so["type"] = s.typeName;
        so["name"] = s.name;
        so["length_m"] = rounded(s.length_m);
        so["duration_s"] = rounded(s.duration_s);
        so["first_node"] = s.firstNode;
        so["last_node"] = s.lastNode;
        so["end_x"] = num(s.endPosition.x);
        so["end_y"] = num(s.endPosition.y);
        so["end_z"] = num(s.endPosition.z);
        so["end_heading_deg"] = num(s.endHeading_deg);
        so["end_pitch_deg"] = num(s.endPitch_deg);
        so["end_bank_deg"] = num(s.endBank_deg);
        so["end_speed_mps"] = num(s.endSpeed_mps);
        secs.append(so);
    }
    o["section_list"] = secs;
    return o;
}

static int runSummary(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--sections" << "--json", QStringList() << "--track",
                     positional, flags, values, error) || positional.size() != 1) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdSummary.usage);
        return 1;
    }
    int trackNumber = 0;
    bool trackGiven = values.contains("--track");    // without --track: every track
    if(trackGiven) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    coreProject project;
    if(!loadProject(positional.at(0), project, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    QList<int> numbers;
    if(trackGiven) {
        if(!trackByNumber(project, trackNumber, error)) {
            fprintf(stderr, "%s\n", qPrintable(error));
            return 1;
        }
        numbers.append(trackNumber);
    } else {
        for(int i = 1; i <= project.tracks.size(); ++i) numbers.append(i);
    }
    bool json = flags.contains("--json");
    bool withSections = flags.contains("--sections");

    QString text;
    QJsonArray all;
    for(int k = 0; k < numbers.size(); ++k) {
        track* t = trackByNumber(project, numbers.at(k), error);
        rideSummary r = computeRideSummary(t);
        if(json) {
            all.append(summarySheetJson(r, numbers.at(k), t->name, true));
        } else {
            if(k > 0) text += "\n";
            text += QString("Ride sheet: %1, track %2 \"%3\"\n").arg(positional.at(0)).arg(numbers.at(k)).arg(t->name);
            text += sheetText(r, withSections);
        }
    }
    if(json) {
        // Always an object with a "tracks" array (one entry for one track).
        QJsonObject top;
        top["tracks"] = all;
        text = QString::fromUtf8(QJsonDocument(top).toJson(QJsonDocument::Indented));
    }
    QByteArray bytes = text.toUtf8();
    fwrite(bytes.constData(), 1, bytes.size(), stdout);
    return 0;
}

const command cmdSummary = {
    "summary",
    "fvdcore summary <project.fvd> [--track N] [--sections] [--json]",
    "the ride sheet: length, height, drop, duration, speed, airtime, peak forces, inversions, findings",
    runSummary
};
