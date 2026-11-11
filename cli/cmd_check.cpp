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

// fvdcore check: the findings of the check registry (core/finding.h) as
// sentences, or as JSON. Exit code 0 when it ran, 1 on an error (the project
// does not load, a bad option), 3 when --fail-on is given and a finding of
// that severity or worse exists.

#include "commands.h"
#include "project.h"

#include "../core/finding.h"
#include "../core/mnode.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <cstdio>

static const char* severityWord(findingSeverity s)
{
    return s == findingProblem ? "problem" : (s == findingWarning ? "warning" : "information");
}

static QJsonValue num(double v)
{
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

static int runCheck(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--json" << "--list",
                     QStringList() << "--track" << "--check" << "--fail-on",
                     positional, flags, values, error)) {
        fprintf(stderr, "%s\nusage: %s\n", qPrintable(error), cmdCheck.usage);
        return 1;
    }
    if(flags.contains("--list")) {
        const QList<checkDef>& list = checkRegistry::all();
        QString text;
        for(int i = 0; i < list.size(); ++i) {
            text += list.at(i).id + "\t" + list.at(i).name + "\t" + list.at(i).description + "\n";
        }
        QByteArray bytes = text.toUtf8();
        fwrite(bytes.constData(), 1, bytes.size(), stdout);
        return 0;
    }
    if(positional.size() != 1) {
        fprintf(stderr, "usage: %s\n", cmdCheck.usage);
        return 1;
    }
    int trackNumber = 1;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    int failOn = -1;
    if(values.contains("--fail-on")) {
        QString w = values["--fail-on"];
        if(w == "problem") failOn = findingProblem;
        else if(w == "warning") failOn = findingWarning;
        else if(w == "information") failOn = findingInformation;
        else { fprintf(stderr, "--fail-on is problem, warning or information\n"); return 1; }
    }
    if(values.contains("--check") && !checkRegistry::find(values["--check"])) {
        fprintf(stderr, "unknown check %s (fvdcore check --list names them)\n", qPrintable(values["--check"]));
        return 1;
    }

    coreProject project;
    if(!loadProject(positional.at(0), project, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    track* t = trackByNumber(project, trackNumber, error);
    if(!t) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }

    QList<finding> found = values.contains("--check") ? checkRegistry::run(t, values["--check"]) : checkRegistry::runAll(t);
    int counts[3] = { 0, 0, 0 };
    bool failed = false;
    for(int i = 0; i < found.size(); ++i) {
        counts[found.at(i).severity]++;
        if(failOn >= 0 && (int)found.at(i).severity >= failOn) failed = true;
    }

    QString text;
    if(flags.contains("--json")) {
        QJsonObject top;
        QJsonArray list;
        for(int i = 0; i < found.size(); ++i) {
            const finding& f = found.at(i);
            QJsonObject o;
            o["check"] = f.checkId;
            o["measure"] = f.measureId;
            o["severity"] = severityWord(f.severity);
            o["from_node"] = f.fromNode;
            o["to_node"] = f.toNode;
            o["from_time_s"] = f.fromNode/F_HZ;
            o["to_time_s"] = f.toNode/F_HZ;
            o["from_dist_m"] = num(t->getPoint(f.fromNode)->fTotalLength);
            o["to_dist_m"] = num(t->getPoint(f.toNode)->fTotalLength);
            o["worst_node"] = f.worstNode;
            o["worst_value"] = num(f.worstValue);
            o["limit"] = num(f.limit);
            o["below"] = f.below;
            o["non_finite"] = f.nonFinite;
            o["text"] = f.text;
            list.append(o);
        }
        top["findings"] = list;
        top["problems"] = counts[findingProblem];
        top["warnings"] = counts[findingWarning];
        top["information"] = counts[findingInformation];
        text = QString::fromUtf8(QJsonDocument(top).toJson(QJsonDocument::Indented));
    } else {
        for(int i = 0; i < found.size(); ++i) {
            const finding& f = found.at(i);
            text += QString("%1: %2 [%3 m to %4 m along the track]\n").arg(severityWord(f.severity)).arg(f.text)
                    .arg(QString::number(t->getPoint(f.fromNode)->fTotalLength, 'f', 1))
                    .arg(QString::number(t->getPoint(f.toNode)->fTotalLength, 'f', 1));
        }
        text += QString("%1 problem(s), %2 warning(s), %3 information\n")
                .arg(counts[findingProblem]).arg(counts[findingWarning]).arg(counts[findingInformation]);
    }
    QByteArray bytes = text.toUtf8();
    fwrite(bytes.constData(), 1, bytes.size(), stdout);
    return failed ? 3 : 0;
}

const command cmdCheck = {
    "check",
    "fvdcore check <project.fvd> [--track N] [--check ID] [--json] [--fail-on problem|warning|information]   |   fvdcore check --list",
    "the findings of the checks as sentences (exit code 3 with --fail-on when one that bad exists)",
    runCheck
};
