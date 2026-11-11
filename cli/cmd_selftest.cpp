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
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../core/measure.h"
#include "../core/finding.h"
#include "../core/ridesummary.h"

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

// Runs a command with its standard output going to a file, returns the exit
// code and puts what it printed in out.
static int runCaptured(const command& cmd, const QStringList& args, const QString& file, QByteArray& out)
{
    fflush(stdout);
    int saved = dup(fileno(stdout));
    FILE* redirected = fopen(qPrintable(file), "wb");
    if(!redirected || saved < 0) {
        if(redirected) fclose(redirected);
        return -1;
    }
    dup2(fileno(redirected), fileno(stdout));
    int code = cmd.run(args);
    fflush(stdout);
    dup2(saved, fileno(stdout));
    close(saved);
    fclose(redirected);
    out = readAll(file);
    return code;
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

    // summary: the ride sheet of four-sections.fvd, as text and as JSON
    QByteArray printed;
    QString captureFile = QDir(tmp.path()).filePath("captured.txt");
    code = runCaptured(cmdSummary, QStringList() << project << "--sections", captureFile, printed);
    report(allOk, code == 0 && printed.contains("Length (centre line)") && printed.contains("Top speed") && printed.contains("force"),
           QString("summary of four-sections.fvd prints the ride sheet and its sections (exit code %1, %2 bytes)").arg(code).arg(printed.size()));
    code = runCaptured(cmdSummary, QStringList() << project << "--json", captureFile, printed);
    report(allOk, code == 0 && printed.contains("\"length_m\"") && printed.contains("\"floater_airtime_s\"") && printed.contains("\"section_list\""),
           QString("summary --json prints one JSON object (exit code %1)").arg(code));
    if(!loaded.tracks.isEmpty()) {
        rideSummary sheet = computeRideSummary(loaded.tracks.at(0));
        report(allOk, !sheet.empty && sheet.sections.size() == loaded.tracks.at(0)->lSections.size()
               && sheet.length_m == (double)loaded.tracks.at(0)->getPoint(points)->fTotalLength,
               QString("the ride sheet of four-sections.fvd: %1 sections, %2 m, %3 s").arg(sheet.sections.size()).arg(sheet.length_m, 0, 'f', 1).arg(sheet.duration_s, 0, 'f', 1));
    }

    // totext then fromtext gives back the bytes of a re-save
    QString textFile = QDir(tmp.path()).filePath("four-sections.json");
    QString backFile = QDir(tmp.path()).filePath("four-sections.back.fvd");
    int c1 = cmdToText.run(QStringList() << project << textFile);
    int c2 = cmdFromText.run(QStringList() << textFile << backFile);
    checkSame(allOk, QString("totext then fromtext (exit codes %1, %2) gives the re-saved project").arg(c1).arg(c2), backFile,
              QDir(tmp.path()).filePath("four-sections.resaved.fvd"));

    // measures --list names every measure of the registry; the table has a column for each
    code = runCaptured(cmdMeasures, QStringList() << "--list", captureFile, printed);
    const QVector<measureDef>& registry = measureRegistry::all();
    bool listed = code == 0;
    for(int i = 0; i < registry.size(); ++i) {
        if(!printed.contains(("\n" + registry.at(i).id + "\t").toUtf8()) || !printed.contains(registry.at(i).name.toUtf8())) listed = false;
    }
    report(allOk, listed && !registry.isEmpty(), QString("measures --list names all %1 measures (exit code %2)").arg(registry.size()).arg(code));
    QString measuresFile = QDir(tmp.path()).filePath("measures.tsv");
    code = cmdMeasures.run(QStringList() << project << "--all" << "--out" << measuresFile);
    QList<QByteArray> measureRows = readAll(measuresFile).split('\n');
    bool header = !measureRows.isEmpty() && measureRows.at(0).startsWith("index\ttime_s\tsection\tdist_m\t");
    for(int i = 0; i < registry.size() && header; ++i) {
        if(!measureRows.at(0).contains(("\t" + registry.at(i).id).toUtf8())) header = false;
    }
    report(allOk, code == 0 && header && lineCount(readAll(measuresFile)) == points + 2,
           QString("measures --all writes a header with a column per measure and %1 rows (exit code %2)").arg(points + 1).arg(code));
    code = cmdMeasures.run(QStringList() << project << "--ids" << "nonsense");
    report(allOk, code == 1, QString("measures with an unknown id exits 1 (exit code %1)").arg(code));

    // check: exit 0 when it ran, 3 when --fail-on finds one that bad, 1 on a load error
    code = runCaptured(cmdCheck, QStringList() << project, captureFile, printed);
    bool counts = printed.contains("problem(s)") && printed.contains("warning(s)");
    report(allOk, code == 0 && counts, QString("check of four-sections.fvd exits 0 and ends with the counts (exit code %1)").arg(code));
    code = runCaptured(cmdCheck, QStringList() << project << "--fail-on" << "problem", captureFile, printed);
    report(allOk, code == 3, QString("check --fail-on problem exits 3 when a problem exists (exit code %1)").arg(code));
    code = runCaptured(cmdCheck, QStringList() << QDir(baseline).filePath("gen-curves-speed.fvd") << "--fail-on" << "information", captureFile, printed);
    report(allOk, code == 0, QString("check --fail-on information exits 0 when there is no finding (exit code %1)").arg(code));
    code = cmdCheck.run(QStringList() << QDir(baseline).filePath("no-such-project.fvd"));
    report(allOk, code == 1, QString("check of a missing project exits 1 (exit code %1)").arg(code));

    // angle changes go the short way round: -179 to +179 degrees is 2, not 358
    double turn = wrap180(179.0 - (-179.0));
    report(allOk, turn > -2.0001 && turn < -1.9999, QString("a change from -179 to 179 degrees wraps to %1").arg(turn));

    // the dispatcher: an unknown command exits 1 (its usage text goes nowhere here)
    code = runCommand(QStringList() << "no-such-command", NULL);
    report(allOk, code == 1, QString("an unknown command exits 1 (exit code %1)").arg(code));

    printf("%s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}

const command cmdSelftest = {
    "selftest",
    "fvdcore selftest <repository folder>",
    "export, nodes, compare, summary, measures, check and totext/fromtext on the baseline project four-sections.fvd and check the results (also: fvdcore <folder>)",
    runSelftest
};
