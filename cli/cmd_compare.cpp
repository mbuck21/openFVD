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

// fvdcore compare: how far one version of a project moved from another, section
// end by section end. This is the table behind the planned knock-on view
// (docs/research/r5-fvd-workflow.md, direction C5): change a section early in
// the track and see how far every later section end moved.
// Differences are b minus a. Positions are the centre line in the world frame.

#include "commands.h"
#include "project.h"

#include "../core/mnode.h"
#include "../core/section.h"
#include "../lenassert.h"

#include <cmath>
#include <cstdio>

double wrap180(double angle)
{
    while(angle > 180.0) angle -= 360.0;
    while(angle <= -180.0) angle += 360.0;
    return angle;
}

static compareRow rowFor(const QString& label, mnode* a, const placement& pa, float heartA,
                         mnode* b, const placement& pb, float heartB)
{
    compareRow row;
    row.label = label;
    glm::vec3 d = pb.point(b->vPosHeart(heartB)) - pa.point(a->vPosHeart(heartA));
    row.distance = glm::length(d);
    row.dHeading = wrap180((double)pb.heading(b->getDirection()) - (double)pa.heading(a->getDirection()));
    row.dPitch = (double)b->getPitch() - (double)a->getPitch();
    row.dRoll = wrap180((double)b->fRoll - (double)a->fRoll);
    row.dSpeed = (double)b->fVel - (double)a->fVel;
    return row;
}

compareResult compareTracks(track* a, track* b)
{
    compareResult result;
    placement pa(a, true), pb(b, true);
    result.sectionsA = a->lSections.size();
    result.sectionsB = b->lSections.size();
    int common = qMin(result.sectionsA, result.sectionsB);
    for(int i = 0; i < common; ++i) {
        result.rows.append(rowFor(QString("section %1").arg(i+1),
                                  &a->lSections.at(i)->lNodes.last(), pa, a->fHeart,
                                  &b->lSections.at(i)->lNodes.last(), pb, b->fHeart));
    }
    int lastA = a->getNumPoints(), lastB = b->getNumPoints();
    mnode* endA = a->getPoint(lastA);
    mnode* endB = b->getPoint(lastB);
    result.rows.append(rowFor("track end", endA, pa, a->fHeart, endB, pb, b->fHeart));
    result.lengthA = endA->fTotalLength;
    result.lengthB = endB->fTotalLength;
    result.durationA = lastA/F_HZ;
    result.durationB = lastB/F_HZ;
    return result;
}

static QString num(double v)
{
    return QString::number(v, 'f', 4);
}

static int runCompare(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--tsv", QStringList() << "--track",
                     positional, flags, values, error) || positional.size() != 2) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdCompare.usage);
        return 1;
    }
    int trackNumber = 1;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    bool tsv = flags.contains("--tsv");

    coreProject projectA, projectB;
    if(!loadProject(positional.at(0), projectA, error) || !loadProject(positional.at(1), projectB, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    track* a = trackByNumber(projectA, trackNumber, error);
    if(!a) { fprintf(stderr, "%s: %s\n", qPrintable(positional.at(0)), qPrintable(error)); return 1; }
    track* b = trackByNumber(projectB, trackNumber, error);
    if(!b) { fprintf(stderr, "%s: %s\n", qPrintable(positional.at(1)), qPrintable(error)); return 1; }

    compareResult r = compareTracks(a, b);

    QString text;
    if(!tsv) {
        text += QString("a: %1 (track %2, %3 sections)\nb: %4 (track %2, %5 sections)\n")
                .arg(positional.at(0)).arg(trackNumber).arg(r.sectionsA).arg(positional.at(1)).arg(r.sectionsB);
        if(r.sectionsA != r.sectionsB) {
            text += QString("the section counts differ: comparing the first %1 sections and the track end\n").arg(qMin(r.sectionsA, r.sectionsB));
        }
        text += "differences are b minus a; distance is between the two end positions (world frame, centre line)\n\n";
        text += QString("%1 %2 %3 %4 %5 %6\n").arg("end of", -12).arg("distance m", 11).arg("heading deg", 12)
                .arg("pitch deg", 10).arg("bank deg", 10).arg("speed m/s", 10);
        for(int i = 0; i < r.rows.size(); ++i) {
            const compareRow& row = r.rows.at(i);
            text += QString("%1 %2 %3 %4 %5 %6\n").arg(row.label, -12).arg(num(row.distance), 11).arg(num(row.dHeading), 12)
                    .arg(num(row.dPitch), 10).arg(num(row.dRoll), 10).arg(num(row.dSpeed), 10);
        }
        text += QString("\ntotal length   a %1 m   b %2 m   difference %3 m\n").arg(num(r.lengthA)).arg(num(r.lengthB)).arg(num(r.lengthB-r.lengthA));
        text += QString("total duration a %1 s   b %2 s   difference %3 s\n").arg(num(r.durationA)).arg(num(r.durationB)).arg(num(r.durationB-r.durationA));
    } else {
        text += "end_of\tdistance_m\td_heading_deg\td_pitch_deg\td_bank_deg\td_speed_mps\n";
        for(int i = 0; i < r.rows.size(); ++i) {
            const compareRow& row = r.rows.at(i);
            text += row.label + "\t" + num(row.distance) + "\t" + num(row.dHeading) + "\t" + num(row.dPitch)
                    + "\t" + num(row.dRoll) + "\t" + num(row.dSpeed) + "\n";
        }
        // the totals: b minus a, in the distance_m column
        text += "total_length_m\t" + num(r.lengthB-r.lengthA) + "\t\t\t\t\n";
        text += "total_duration_s\t" + num(r.durationB-r.durationA) + "\t\t\t\t\n";
    }
    fwrite(text.toUtf8().constData(), 1, text.toUtf8().size(), stdout);
    return 0;
}

const command cmdCompare = {
    "compare",
    "fvdcore compare <a.fvd> <b.fvd> [--track N] [--tsv]",
    "how far the section ends of project b moved from those of project a (b minus a)",
    runCompare
};
