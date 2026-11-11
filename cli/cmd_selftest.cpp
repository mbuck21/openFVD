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

// fvdcore selftest <repository folder>: the quick test tools/run-tests.sh runs
// (through tests/coreonly/, which builds this same program). It uses
// samples/baseline/four-sections.fvd and the 2016 binary's files next to it.

#include "commands.h"
#include "project.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

static QByteArray readAll(const QString& fileName)
{
    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

static void report(bool& allOk, bool ok, const QString& text)
{
    printf("%s: %s\n", ok ? "ok" : "FAIL", qPrintable(text));
    if(!ok) allOk = false;
}

static void checkSame(bool& allOk, const QString& label, const QString& made, const QString& reference)
{
    QByteArray a = readAll(made);
    QByteArray b = readAll(reference);
    bool ok = !b.isEmpty() && a == b;
    printf("%s: %s (%d bytes made, %d bytes in the reference)\n", ok ? "ok" : "FAIL", qPrintable(label), a.size(), b.size());
    if(!ok) allOk = false;
}

static int lineCount(const QByteArray& bytes)
{
    return bytes.isEmpty() ? 0 : bytes.count('\n');
}

// Exports samples/baseline/four-sections.fvd and compares the Spline export
// and the re-saved project with the 2016 binary's files, byte for byte; then
// tries the other commands on the same project.
static int runSelftest(const QStringList& args)
{
    if(args.size() != 1) {
        fprintf(stderr, "usage: %s\n", cmdSelftest.usage);
        return 1;
    }
    QString repo = args.at(0);
    QTemporaryDir tmp;
    if(!tmp.isValid()) {
        printf("FAIL: cannot make a temporary folder\n");
        return 1;
    }
    QString baseline = QDir(repo).filePath("samples/baseline");
    QString project = QDir(baseline).filePath("four-sections.fvd");
    bool allOk = true;

    int code = cmdExport.run(QStringList() << project << tmp.path());
    report(allOk, code == 0, QString("export of four-sections.fvd (exit code %1)").arg(code));
    checkSame(allOk, "spline export", QDir(tmp.path()).filePath("four-sections.spline.nlelem"),
              QDir(baseline).filePath("four-sections.2016.spline.nlelem"));
    checkSame(allOk, "re-saved project", QDir(tmp.path()).filePath("four-sections.resaved.fvd"),
              QDir(baseline).filePath("four-sections.2016.resaved.fvd"));

    // nodes: --all writes the header and one line per node
    coreProject loaded;
    QString error;
    int points = -1;
    if(loadProject(project, loaded, error) && !loaded.tracks.isEmpty()) {
        points = loaded.tracks.at(0)->getNumPoints();
    }
    QString allFile = QDir(tmp.path()).filePath("nodes-all.tsv");
    code = cmdNodes.run(QStringList() << project << "--all" << "--out" << allFile);
    int lines = lineCount(readAll(allFile));
    report(allOk, code == 0 && points > 0 && lines == points + 2,
           QString("nodes --all writes %1 lines (header + %2 nodes, exit code %3)").arg(lines).arg(points + 1).arg(code));

    // nodes: the default spacing starts at node 0, ends at the last node, and is shorter
    QString defFile = QDir(tmp.path()).filePath("nodes-default.tsv");
    code = cmdNodes.run(QStringList() << project << "--out" << defFile);
    QList<QByteArray> rows = readAll(defFile).split('\n');
    while(!rows.isEmpty() && rows.last().isEmpty()) rows.removeLast();
    bool ends = rows.size() >= 3 && rows.at(1).startsWith("0\t")
                && rows.last().startsWith(QByteArray::number(points) + "\t");
    report(allOk, code == 0 && ends && rows.size() < lines,
           QString("nodes at the default spacing: %1 lines, from node 0 to node %2").arg(rows.size()).arg(points));

    // compare of a project with itself: zero everywhere
    bool zero = !loaded.tracks.isEmpty();
    int rowCount = 0;
    if(zero) {
        compareResult same = compareTracks(loaded.tracks.at(0), loaded.tracks.at(0));
        rowCount = same.rows.size();
        for(int i = 0; i < same.rows.size(); ++i) {
            const compareRow& r = same.rows.at(i);
            if(r.distance != 0.0 || r.dHeading != 0.0 || r.dPitch != 0.0 || r.dRoll != 0.0 || r.dSpeed != 0.0) zero = false;
        }
        if(same.lengthA != same.lengthB || same.durationA != same.durationB) zero = false;
    }
    report(allOk, zero && rowCount >= 2, QString("compare of a project with itself: zero in all %1 rows").arg(rowCount));

    // the dispatcher: an unknown command exits 1 (its usage text goes nowhere here)
    code = runCommand(QStringList() << "no-such-command", NULL);
    report(allOk, code == 1, QString("an unknown command exits 1 (exit code %1)").arg(code));

    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}

const command cmdSelftest = {
    "selftest",
    "fvdcore selftest <repository folder>",
    "export, nodes and compare the baseline project four-sections.fvd and check the results (also: fvdcore <folder>)",
    runSelftest
};
