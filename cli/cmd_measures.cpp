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

// fvdcore measures: the measure registry (core/measure.h) as a table of values
// per node, tab-separated text, for Python. Rows are chosen as `nodes` chooses
// them, and time_s is node index / F_HZ as in `nodes` and the plot.

#include "commands.h"
#include "project.h"

#include "../core/measure.h"
#include "../core/mnode.h"
#include "../lenassert.h"

#include <QFile>
#include <cmath>
#include <cstdio>

static QString num(double v)
{
    return QString::number(v, 'g', 9);
}

// The node indexes to print: always the first and the last, none twice.
// (The same selection as cli/cmd_nodes.cpp.)
static QList<int> pickIndexes(track* t, bool all, bool perMetre, double step)
{
    int last = t->getNumPoints();
    QList<int> list;
    if(all) {
        for(int i = 0; i <= last; ++i) list.append(i);
        return list;
    }
    list.append(0);
    double total = t->getPoint(last)->fTotalLength;
    for(long k = 1; ; ++k) {
        int index;
        if(perMetre) {
            double dist = k*step;
            if(dist > total) break;
            index = t->getIndexFromDist((float)dist);
        } else {
            double at = floor(k*step*F_HZ + 0.5);
            if(at > last) break;
            index = (int)at;
        }
        if(index > last) index = last;
        if(index > list.last()) list.append(index);
    }
    if(list.last() != last) list.append(last);
    return list;
}

static int runMeasures(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--all" << "--list",
                     QStringList() << "--track" << "--ids" << "--every-m" << "--every-s" << "--out",
                     positional, flags, values, error)) {
        fprintf(stderr, "%s\nusage: %s\n", qPrintable(error), cmdMeasures.usage);
        return 1;
    }
    if(flags.contains("--list")) {
        const QVector<measureDef>& table = measureRegistry::all();
        QString text = "id\tunit\tname\tdescription\n";
        for(int i = 0; i < table.size(); ++i) {
            const measureDef& d = table.at(i);
            text += d.id + "\t" + d.unit + "\t" + d.name + "\t" + d.description + "\n";
        }
        QByteArray bytes = text.toUtf8();
        fwrite(bytes.constData(), 1, bytes.size(), stdout);
        return 0;
    }
    if(positional.size() != 1) {
        fprintf(stderr, "usage: %s\n", cmdMeasures.usage);
        return 1;
    }
    int trackNumber = 1;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    int spacings = (flags.contains("--all") ? 1 : 0) + (values.contains("--every-m") ? 1 : 0) + (values.contains("--every-s") ? 1 : 0);
    if(spacings > 1) {
        fprintf(stderr, "give only one of --all, --every-m and --every-s\n");
        return 1;
    }
    bool all = flags.contains("--all");
    bool perMetre = !values.contains("--every-s");
    double step = 1.0;
    if(values.contains("--every-m") || values.contains("--every-s")) {
        bool ok;
        step = values.contains("--every-m") ? values["--every-m"].toDouble(&ok) : values["--every-s"].toDouble(&ok);
        if(!ok || !(step > 0.0)) { fprintf(stderr, "the spacing must be a number above zero\n"); return 1; }
    }
    QStringList ids;
    if(values.contains("--ids")) {
        ids = values["--ids"].split(',');
        ids.removeAll(QString());
        for(int i = 0; i < ids.size(); ++i) {
            ids[i] = ids.at(i).trimmed();
            if(!measureRegistry::find(ids.at(i))) {
                fprintf(stderr, "unknown measure %s (fvdcore measures --list names them)\n", qPrintable(ids.at(i)));
                return 1;
            }
        }
        if(ids.isEmpty()) { fprintf(stderr, "--ids needs at least one measure id\n"); return 1; }
    } else {
        const QVector<measureDef>& table = measureRegistry::all();
        for(int i = 0; i < table.size(); ++i) ids << table.at(i).id;
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

    QString text = "index\ttime_s\tsection\tdist_m\t" + ids.join("\t") + "\n";
    if(!t->lSections.isEmpty()) {
        QVector<QVector<float> > data = measureRegistry::values(t, ids);
        QList<int> indexes = pickIndexes(t, all, perMetre, step);
        for(int i = 0; i < indexes.size(); ++i) {
            int index = indexes.at(i);
            int nodeInSection, section;
            t->getSecNode(index, &nodeInSection, &section);
            text += QString::number(index) + "\t" + num(index/F_HZ) + "\t" + QString::number(section+1)
                    + "\t" + num(t->getPoint(index)->fTotalLength);
            for(int k = 0; k < data.size(); ++k) text += "\t" + num(data.at(k).at(index));
            text += "\n";
        }
    }

    QByteArray bytes = text.toUtf8();
    if(values.contains("--out")) {
        QFile out(values["--out"]);
        if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size()) {
            fprintf(stderr, "cannot write %s\n", qPrintable(values["--out"]));
            return 1;
        }
    } else {
        fwrite(bytes.constData(), 1, bytes.size(), stdout);
    }
    return 0;
}

const command cmdMeasures = {
    "measures",
    "fvdcore measures <project.fvd> [--track N] [--ids a,b,c] [--every-m X | --every-s X | --all] [--out <file>]   |   fvdcore measures --list",
    "measure values per node as tab-separated text (--list names every measure)",
    runMeasures
};
