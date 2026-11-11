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

// fvdcore nodes: one track as a table of points, tab-separated text, for
// experiments in Python. The world frame uses the maths of
// MainWindow::updateInfoPanel (ui/mainwindow.cpp), so the numbers equal what
// the program shows in its info panel.

#include "commands.h"
#include "project.h"

#include "../core/mnode.h"
#include "../lenassert.h"

#include <QFile>
#include <cmath>
#include <cstdio>

static QString num(double v)
{
    return QString::number(v, 'g', 9);
}

static QString vec(const glm::vec3& v)
{
    return num(v.x) + "\t" + num(v.y) + "\t" + num(v.z);
}

// The node indexes to print: always the first and the last, none twice.
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

static int runNodes(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--all",
                     QStringList() << "--track" << "--every-m" << "--every-s" << "--frame" << "--out",
                     positional, flags, values, error) || positional.size() != 1) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdNodes.usage);
        return 1;
    }
    int trackNumber = 1;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    bool world = true;
    if(values.contains("--frame")) {
        if(values["--frame"] == "anchor") world = false;
        else if(values["--frame"] != "world") { fprintf(stderr, "--frame is world or anchor\n"); return 1; }
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

    placement place(t, world);
    float heart = t->fHeart;
    QList<int> indexes = pickIndexes(t, all, perMetre, step);

    QString text;
    // One header line only, so pandas and numpy read it without options.
    // The columns are explained in cli/README.md.
    text += "index\ttime_s\tsection\tdist_m\theart_dist_m\theart_x\theart_y\theart_z\tpos_x\tpos_y\tpos_z"
            "\tdir_x\tdir_y\tdir_z\tlat_x\tlat_y\tlat_z\tnorm_x\tnorm_y\tnorm_z"
            "\troll\tpitch\theading\tspeed_mps\tnormal_g\tlateral_g\tenergy\n";
    for(int i = 0; i < indexes.size(); ++i) {
        int index = indexes.at(i);
        mnode* n = t->getPoint(index);
        int nodeInSection, section;
        t->getSecNode(index, &nodeInSection, &section);
        text += QString::number(index) + "\t" + num(index/F_HZ) + "\t" + QString::number(section+1)
                + "\t" + num(n->fTotalLength) + "\t" + num(n->fTotalHeartLength)
                + "\t" + vec(place.point(n->vPos))
                + "\t" + vec(place.point(n->vPosHeart(heart)))
                + "\t" + vec(place.direction(n->vDir))
                + "\t" + vec(place.direction(n->vLat))
                + "\t" + vec(place.direction(n->vNorm))
                + "\t" + num(n->fRoll) + "\t" + num(n->getPitch()) + "\t" + num(place.heading(n->getDirection()))
                + "\t" + num(n->fVel)
                + "\t" + num(n->forceNormal + n->smoothNormal) + "\t" + num(n->forceLateral + n->smoothLateral)
                + "\t" + num(n->fEnergy) + "\n";
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

const command cmdNodes = {
    "nodes",
    "fvdcore nodes <project.fvd> [--track N] [--every-m X | --every-s X | --all] [--frame world|anchor] [--out <file>]",
    "print a track as a table of points (tab-separated text, norm points from the rider toward the rails)",
    runNodes
};
