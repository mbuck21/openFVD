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

// fvdcore limits: the force-limit engine (core/forcelimits.h) on the command
// line. Without --trace it shows the limit file as the program reads it:
// every number by its name with its value, confidence and source. With
// --trace it runs the engine on one rider's force trace read from a
// tab-separated file and prints the findings.

#include "commands.h"
#include "project.h"

#include "../core/forcelimits.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>
#include <cstdio>

static void writeOut(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    fwrite(bytes.constData(), 1, bytes.size(), stdout);
}

static QJsonValue rounded(double v, double scale = 10000.0)
{
    return std::isfinite(v) ? QJsonValue(std::floor(v*scale + 0.5)/scale) : QJsonValue(QJsonValue::Null);
}

// The column of a header word: gx, gy, gz and time_s, or the names `fvdcore
// measures` and `nodes` print (forceAlong, forceLateral / lateral_g,
// forceNormal / normal_g).
static int columnOf(const QStringList& header, const QStringList& names)
{
    for(int i = 0; i < header.size(); ++i) {
        if(names.contains(header.at(i).trimmed())) return i;
    }
    return -1;
}

// Reads a trace: one header line, then one sample per line, separated by tabs
// or spaces. dt comes from the time column (it must be evenly spaced) or from
// dtGiven when that is above zero.
static bool readTrace(const QString& path, double dtGiven, forceTrace& t, QString& error)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = "cannot open " + path;
        return false;
    }
    QTextStream in(&file);
    const QRegularExpression sep("[\\t ,;]+");
    QString line = in.readLine();
    const QStringList header = line.trimmed().split(sep, Qt::SkipEmptyParts);
    const int ct = columnOf(header, QStringList() << "time_s" << "t" << "time");
    const int cx = columnOf(header, QStringList() << "gx" << "forceAlong");
    const int cy = columnOf(header, QStringList() << "gy" << "forceLateral" << "lateral_g");
    const int cz = columnOf(header, QStringList() << "gz" << "forceNormal" << "normal_g");
    if(cz < 0) {
        error = path + ", line 1: the header needs a column gz (or forceNormal / normal_g); gx and gy are 0 when missing";
        return false;
    }
    if(ct < 0 && !(dtGiven > 0.0)) {
        error = path + ", line 1: no time column (time_s); give the step with --dt";
        return false;
    }
    QVector<double> times;
    QVector<int> lines;     // the file's line of each sample
    int lineNo = 1;
    while(!in.atEnd()) {
        line = in.readLine();
        ++lineNo;
        if(line.trimmed().isEmpty()) continue;
        const QStringList cells = line.trimmed().split(sep, Qt::SkipEmptyParts);
        double v[4] = { 0.0, 0.0, 0.0, 0.0 };
        const int cols[4] = { ct, cx, cy, cz };
        for(int k = 0; k < 4; ++k) {
            if(cols[k] < 0) continue;
            bool ok = false;
            if(cols[k] < cells.size()) v[k] = cells.at(cols[k]).toDouble(&ok);
            if(!ok || !std::isfinite(v[k])) {
                error = QString("%1, line %2, column %3: not a number").arg(path).arg(lineNo).arg(header.at(cols[k]));
                return false;
            }
        }
        if(ct >= 0) times.append(v[0]);
        lines.append(lineNo);
        t.gx.append(v[1]);
        t.gy.append(v[2]);
        t.gz.append(v[3]);
    }
    if(t.gz.isEmpty()) {
        error = path + ": no samples";
        return false;
    }
    if(dtGiven > 0.0) {
        t.dt = dtGiven;
    } else {
        if(times.size() < 2) {
            error = path + ": one sample only; give the step with --dt";
            return false;
        }
        // `measures` and `nodes` always add the track's last node: a shorter
        // last step drops that sample
        if(times.size() > 2) {
            const double before = times.at(times.size() - 2) - times.at(times.size() - 3);
            const double last = times.last() - times.at(times.size() - 2);
            if(last < 0.95*before) {
                times.removeLast(); lines.removeLast();
                t.gx.removeLast(); t.gy.removeLast(); t.gz.removeLast();
            }
        }
        const double mean = (times.last() - times.first())/(times.size() - 1);
        for(int i = 1; i < times.size(); ++i) {
            const double step = times.at(i) - times.at(i-1);
            if(!(mean > 0.0) || std::fabs(step - mean) > 0.05*mean) {
                error = QString("%1, line %2: the samples are not evenly spaced in time (step %3 s, mean %4 s); "
                                "take them at a fixed step (for `measures`, --every-s)").arg(path).arg(lines.at(i)).arg(step).arg(mean);
                return false;
            }
        }
        t.dt = mean;
    }
    return true;
}

static int showFile(const forceLimits& s, bool json)
{
    if(json) {
        QJsonObject top;
        top["file"] = s.sPath;
        top["id"] = s.sId;
        top["name"] = s.sName;
        top["standard"] = s.sStandard;
        top["edition"] = s.sEdition;
        top["edition_note"] = s.sEditionNote;
        top["disclaimer"] = s.sDisclaimer;
        QJsonObject sources;
        for(QMap<QString, QString>::const_iterator it = s.lSources.constBegin(); it != s.lSources.constEnd(); ++it) {
            sources[it.key()] = it.value();
        }
        top["sources"] = sources;
        QJsonArray numbers;
        for(int i = 0; i < s.lNumbers.size(); ++i) {
            const limitListed& n = s.lNumbers.at(i);
            QJsonObject o;
            o["id"] = n.sId;
            o["value"] = n.sValue;
            o["confidence"] = n.sConfidence;
            o["checked"] = n.sChecked.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(n.sChecked);
            o["source"] = n.sSource;
            numbers.append(o);
        }
        top["numbers"] = numbers;
        writeOut(QString::fromUtf8(QJsonDocument(top).toJson(QJsonDocument::Indented)));
        return 0;
    }
    QString text;
    text += QString("Limit file: %1\n%2 (%3)\nStandard: %4, edition %5\n\n%6\n\n%7\n\n")
            .arg(s.sPath, s.sName, s.sId, s.sStandard, s.sEdition, s.sEditionNote, s.sDisclaimer);
    text += QString("Filter: Butterworth, order %1, single pass, %2 Hz. Restraint diagram: order %3, %4 Hz.\n")
            .arg(s.iFilterOrder).arg(s.filterCorner.dValue).arg(s.iRestraintFilterOrder).arg(s.restraintFilterCorner.dValue);
    QStringList kinds;
    for(QMap<QString, limitRestraintKind>::const_iterator it = s.lRestraintKinds.constBegin(); it != s.lRestraintKinds.constEnd(); ++it) {
        kinds << QString("%1 (%2: %3)").arg(it.key(), it.value().sName, it.value().sNegXCurve);
    }
    text += "Restraint kinds: " + kinds.join("; ") + "\n\n";
    int counts[4] = { 0, 0, 0, 0 };
    text += "id\tvalue\tconfidence\tchecked\tsource\n";
    for(int i = 0; i < s.lNumbers.size(); ++i) {
        const limitListed& n = s.lNumbers.at(i);
        text += QString("%1\t%2\t%3\t%4\t%5\n").arg(n.sId, n.sValue, n.sConfidence,
                                                   n.sChecked.isEmpty() ? QString("-") : n.sChecked, n.sSource);
        if(!n.sChecked.isEmpty()) counts[3]++;
        else if(n.sConfidence == "high") counts[0]++;
        else if(n.sConfidence == "medium") counts[1]++;
        else counts[2]++;
    }
    text += QString("\n%1 numbers: %2 checked by the owner, %3 high, %4 medium, %5 low confidence (not yet checked).\n")
            .arg(s.lNumbers.size()).arg(counts[3]).arg(counts[0]).arg(counts[1]).arg(counts[2]);
    text += "Sources:\n";
    for(QMap<QString, QString>::const_iterator it = s.lSources.constBegin(); it != s.lSources.constEnd(); ++it) {
        text += QString("  %1  %2\n").arg(it.key(), it.value());
    }
    writeOut(text);
    return 0;
}

static int runLimits(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--extended" << "--no-filter" << "--json",
                     QStringList() << "--file" << "--trace" << "--dt" << "--restraint" << "--samples",
                     positional, flags, values, error) || !positional.isEmpty()) {
        fprintf(stderr, "%s\nusage: %s\n", qPrintable(error.isEmpty() ? QString("unexpected argument") : error), cmdLimits.usage);
        return 1;
    }
    forceLimits set;
    const QString path = values.value("--file", forceLimits::defaultPath());
    if(!forceLimits::load(path, set, &error)) {
        fprintf(stderr, "the limit file is refused: %s\n", qPrintable(error));
        return 1;
    }
    const bool json = flags.contains("--json");
    if(!values.contains("--trace")) {
        if(values.contains("--samples") || values.contains("--dt") || values.contains("--restraint")
           || flags.contains("--extended") || flags.contains("--no-filter")) {
            fprintf(stderr, "those options need --trace\n");
            return 1;
        }
        return showFile(set, json);
    }

    double dt = 0.0;
    if(values.contains("--dt")) {
        bool ok;
        dt = values["--dt"].toDouble(&ok);
        if(!ok || !(dt > 0.0)) { fprintf(stderr, "--dt needs a time step in seconds above zero\n"); return 1; }
    }
    forceTrace trace;
    if(!readTrace(values["--trace"], dt, trace, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    limitOptions options;
    options.sRestraint = values.value("--restraint", "base");
    options.bExtendedNegZ = flags.contains("--extended");
    options.bFilter = !flags.contains("--no-filter");
    const limitResult r = checkForceLimits(trace, set, options);
    if(!r.bValid) {
        fprintf(stderr, "the check did not run: %s\n", qPrintable(r.sError));
        return 1;
    }

    if(values.contains("--samples")) {
        QFile out(values["--samples"]);
        if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fprintf(stderr, "cannot write %s\n", qPrintable(values["--samples"]));
            return 1;
        }
        QString text = "time_s";
        for(int f = 0; f < LIMIT_FAMILIES; ++f) text += "\tshare_" + limitFamilyName(f);
        text += "\tshare_max\trule\trestraint_area\n";
        out.write(text.toUtf8());
        for(int i = 0; i < trace.size(); ++i) {
            text = QString::number(i*trace.dt, 'f', 4);
            for(int f = 0; f < LIMIT_FAMILIES; ++f) text += "\t" + QString::number(r.lUtil[f].at(i), 'f', 4);
            text += "\t" + QString::number(r.lUtilMax.at(i), 'f', 4) + "\t" + r.ruleName(r.lRuleMax.at(i))
                  + "\t" + QString::number(r.lRestraintArea.at(i)) + "\n";
            out.write(text.toUtf8());
        }
    }

    if(json) {
        QJsonObject top;
        top["limit_file"] = set.sPath;
        top["edition"] = set.sEdition;
        top["disclaimer"] = set.sDisclaimer;
        top["samples"] = trace.size();
        top["dt_s"] = trace.dt;
        top["restraint"] = set.restraintKind(options.sRestraint)->sId;
        top["extended_neg_z"] = options.bExtendedNegZ;
        top["filtered"] = options.bFilter;
        top["filter_delay_s"] = rounded(r.dFilterDelay);
        top["pass"] = r.bPass;
        top["worst_share"] = rounded(r.worst.dUtil);
        top["worst_rule"] = r.ruleName(r.worst.iRule);
        top["worst_time_s"] = rounded(r.worst.iSample*trace.dt);
        QJsonObject families;
        for(int f = 0; f < LIMIT_FAMILIES; ++f) {
            QJsonObject o;
            o["share"] = rounded(r.worstFamily[f].dUtil);
            o["rule"] = r.ruleName(r.worstFamily[f].iRule);
            o["time_s"] = rounded(r.worstFamily[f].iSample*trace.dt);
            families[limitFamilyName(f)] = o;
        }
        top["families"] = families;
        top["restraint_area"] = r.iRestraintArea;
        top["restraint_area_time_s"] = rounded(r.iRestraintSample*trace.dt);
        QJsonArray list;
        for(int i = 0; i < r.lFindings.size(); ++i) {
            const limitFinding& f = r.lFindings.at(i);
            QJsonObject o;
            o["rule"] = f.sRule;
            o["family"] = limitFamilyName(f.iFamily);
            o["axis"] = f.sAxis;
            o["direction"] = f.iDirection;
            o["from_time_s"] = rounded(f.iFirst*trace.dt);
            o["to_time_s"] = rounded((f.iLast + 1)*trace.dt);
            o["worst_time_s"] = rounded(f.iWorstSample*trace.dt);
            o["level"] = rounded(f.dLevel);
            o["limit"] = rounded(f.dLimit);
            o["duration_s"] = rounded(f.dDuration);
            o["share"] = rounded(f.dUtilisation);
            o["confidence"] = f.sConfidence;
            o["text"] = f.sText;
            list.append(o);
        }
        top["findings"] = list;
        QJsonArray notes;
        for(int i = 0; i < r.lNotes.size(); ++i) notes.append(r.lNotes.at(i));
        top["notes"] = notes;
        writeOut(QString::fromUtf8(QJsonDocument(top).toJson(QJsonDocument::Indented)));
        return 0;
    }

    QString text;
    text += QString("Force limits (%1, public numbers): %2 samples, %3 s apart; restraint %4%5; %6.\n")
            .arg(set.sStandard).arg(trace.size()).arg(trace.dt).arg(set.restraintKind(options.sRestraint)->sId)
            .arg(options.bExtendedNegZ ? QString(", extended airtime line allowed") : QString())
            .arg(options.bFilter ? QString("filtered at %1 Hz, delay of %2 s taken out").arg(set.filterCorner.dValue).arg(r.dFilterDelay, 0, 'f', 3)
                                 : QString("not filtered"));
    text += QString("Result: %1. Largest share of a limit: %2 (%3, at %4 s).\n")
            .arg(r.bPass ? QString("within the limits") : QString("%1 finding(s)").arg(r.lFindings.size()))
            .arg(r.worst.dUtil, 0, 'f', 2).arg(r.ruleName(r.worst.iRule)).arg(r.worst.iSample*trace.dt, 0, 'f', 2);
    QStringList fam;
    for(int f = 0; f < LIMIT_FAMILIES; ++f) {
        fam << QString("%1 %2").arg(limitFamilyName(f)).arg(r.worstFamily[f].dUtil, 0, 'f', 2);
    }
    text += "Largest share per family: " + fam.join(", ") + "\n";
    text += QString("Restraint area (1 Hz, information): %1, first reached at %2 s.\n")
            .arg(r.iRestraintArea).arg(r.iRestraintSample*trace.dt, 0, 'f', 2);
    for(int i = 0; i < r.lFindings.size(); ++i) {
        text += "  " + r.lFindings.at(i).sText + "\n";
    }
    for(int i = 0; i < r.lNotes.size(); ++i) {
        text += "Note: " + r.lNotes.at(i) + "\n";
    }
    text += set.sDisclaimer + "\n";
    writeOut(text);
    return 0;
}

const command cmdLimits = {
    "limits",
    "fvdcore limits [--file <limits.json>] [--trace <trace.tsv> [--dt S] [--restraint base|otsr|prone] [--extended] [--no-filter] [--samples <out.tsv>]] [--json]",
    "shows the force-limit file (every number with its confidence and source), or checks one rider's force trace against it",
    runLimits
};
