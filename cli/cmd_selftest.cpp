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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../core/command.h"
#include "../core/measure.h"
#include "../core/finding.h"
#include "../core/nolimitsimporter.h"
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

// ---- the session ------------------------------------------------------------

static QByteArray requestLine(const QString& json)
{
    return json.toUtf8() + "\n";
}

static QList<QJsonObject> replies(const QByteArray& output)
{
    QList<QJsonObject> list;
    QList<QByteArray> lines = output.split('\n');
    for(int i = 0; i < lines.size(); ++i) {
        if(lines.at(i).trimmed().isEmpty()) continue;
        list.append(QJsonDocument::fromJson(lines.at(i)).object());
    }
    return list;
}

static QString q(const QString& path)
{
    QString p = path;
    p.replace('\\', '/');
    return "\"" + p + "\"";
}

// The session (cli/cmd_session.cpp) driven on text, and the three importers.
static void checkSessionAndImport(bool& allOk, const QString& repo, const QString& tmp, const QString& project)
{
    QString error;
    QString a = QDir(tmp).filePath("session-start.fvd"), b = QDir(tmp).filePath("session-undone.fvd");
    QString c = QDir(tmp).filePath("session-rolled.fvd"), d = QDir(tmp).filePath("session-changed.fvd");
    QByteArray in;
    in += requestLine("{\"op\":\"describe\"}");                                                             // 0
    in += requestLine("{\"op\":\"save\",\"path\":" + q(a) + "}");                                          // 1
    in += requestLine("{\"op\":\"summary\"}");                                                              // 2
    in += requestLine("{\"op\":\"run\",\"name\":\"track.setFriction\",\"args\":{\"friction\":0.2}}");          // 3
    in += requestLine("{\"op\":\"summary\"}");                                                              // 4
    in += requestLine("{\"op\":\"save\",\"path\":" + q(d) + "}");                                          // 5
    in += requestLine("{\"op\":\"undo\"}");                                                                 // 6
    in += requestLine("{\"op\":\"save\",\"path\":" + q(b) + "}");                                          // 7
    in += requestLine("{\"op\":\"begin\",\"label\":\"two\"}");                                              // 8
    in += requestLine("{\"op\":\"run\",\"name\":\"track.setFriction\",\"args\":{\"friction\":0.3}}");          // 9
    in += requestLine("{\"op\":\"run\",\"name\":\"track.setFriction\",\"args\":{\"friction\":0.4}}");          // 10
    in += requestLine("{\"op\":\"end\",\"commit\":false}");                                                  // 11
    in += requestLine("{\"op\":\"save\",\"path\":" + q(c) + "}");                                          // 12
    in += requestLine("{\"op\":\"history\"}");                                                              // 13
    in += requestLine("this is not json");                                                                  // 14
    in += requestLine("{\"op\":\"summary\",\"sections\":true}");                                            // 15
    in += requestLine("{\"op\":\"sectionEnd\",\"section\":3}");                                              // 16
    in += requestLine("{\"op\":\"save\",\"path\":" + q(a) + "}");                                          // 17 refused
    in += requestLine("{\"op\":\"save\",\"path\":" + q(a) + ",\"force\":true}");                            // 18
    in += requestLine("{\"op\":\"run\",\"name\":\"track.setFriction\",\"args\":{\"friction\":9}}");            // 19 out of range
    in += requestLine("{\"op\":\"recompute\"}");                                                            // 20
    in += requestLine("{\"op\":\"measures\",\"ids\":[\"speed\"],\"node\":0}");                              // 21
    in += requestLine("{\"op\":\"quit\"}");                                                                 // 22
    QByteArray out = sessionRunText(project, 1, in, &error);
    QList<QJsonObject> r = replies(out);
    report(allOk, r.size() == 23, QString("the session answers every request line with one line (%1 replies for 23 requests) %2").arg(r.size()).arg(error));
    if(r.size() != 23) return;

    const QList<commandSpec>& registered = commandRegistry::all();
    QJsonArray listed = r.at(0).value("commands").toArray();
    bool every = r.at(0).value("ok").toBool() && listed.size() == registered.size();
    for(int i = 0; i < registered.size() && every; ++i) {
        bool found = false;
        for(int k = 0; k < listed.size(); ++k) {
            if(listed.at(k).toObject().value("name").toString() == registered.at(i).name) found = true;
        }
        if(!found) every = false;
    }
    report(allOk, every, QString("describe lists all %1 registered commands").arg(registered.size()));

    double length0 = r.at(2).value("summary").toObject().value("duration_s").toDouble();
    double length1 = r.at(4).value("summary").toObject().value("duration_s").toDouble();
    double top0 = r.at(2).value("summary").toObject().value("top_speed").toObject().value("value").toDouble();
    double top1 = r.at(4).value("summary").toObject().value("top_speed").toObject().value("value").toDouble();
    report(allOk, r.at(3).value("ok").toBool() && (length0 != length1 || top0 != top1),
           QString("run track.setFriction changes the summary (duration %1 s to %2 s, top speed %3 to %4 m/s)").arg(length0).arg(length1).arg(top0).arg(top1));
    report(allOk, r.at(6).value("ok").toBool() && r.at(6).value("done").toBool() && r.at(6).value("label").toString() == "Change friction",
           "undo reports that it happened, with the step's label");
    checkSame(allOk, "after undo the saved bytes equal the start", b, a);
    report(allOk, readAll(d) != readAll(QDir(tmp).filePath("session-start.fvd")), "the changed project saves differently from the start");
    report(allOk, r.at(11).value("ok").toBool() && !r.at(11).value("recorded").toBool(),
           "end with commit false leaves no step");
    checkSame(allOk, "begin, two runs, end (rolled back): the saved bytes equal the start", c, QDir(tmp).filePath("session-undone.fvd"));
    report(allOk, r.at(13).value("steps").toArray().size() == 1 && r.at(13).value("position").toInt() == 0,
           "the history holds the one undone step and nothing from the rolled-back scope");
    report(allOk, !r.at(14).value("ok").toBool() && !r.at(14).value("error").toString().isEmpty() && r.at(15).value("ok").toBool(),
           QString("a bad line gets an error reply (\"%1\") and the next request works").arg(r.at(14).value("error").toString()));

    QJsonArray sections = r.at(15).value("summary").toObject().value("section_list").toArray();
    QJsonObject end = r.at(16);
    QJsonObject sheet = sections.size() >= 3 ? sections.at(2).toObject() : QJsonObject();
    bool same = end.value("ok").toBool() && end.value("x").toDouble() == sheet.value("end_x").toDouble()
            && end.value("y").toDouble() == sheet.value("end_y").toDouble() && end.value("z").toDouble() == sheet.value("end_z").toDouble()
            && end.value("heading_deg").toDouble() == sheet.value("end_heading_deg").toDouble()
            && end.value("pitch_deg").toDouble() == sheet.value("end_pitch_deg").toDouble()
            && end.value("bank_deg").toDouble() == sheet.value("end_bank_deg").toDouble()
            && end.value("speed_mps").toDouble() == sheet.value("end_speed_mps").toDouble();
    report(allOk, same, "sectionEnd of section 3 equals the matching entry of summary with sections");
    report(allOk, !r.at(17).value("ok").toBool() && r.at(17).value("error").toString().contains("already exists") && r.at(18).value("ok").toBool(),
           "save refuses to overwrite without force and writes with it");
    report(allOk, !r.at(19).value("ok").toBool() && !r.at(19).value("error").toString().isEmpty() && r.at(20).value("ok").toBool()
           && r.at(21).value("ok").toBool() && r.at(22).value("ok").toBool(),
           "a command refused by its range answers with the reason; recompute, measures and quit work");

    // the importers: the three samples import, broken input is refused with a message
    QString imports = QDir(repo).filePath("samples/import");
    QStringList kinds, files;
    kinds << "nltrack" << "txt" << "csv";
    files << "nl1.nltrack" << "pointlist.txt" << "nl2.csv";
    for(int i = 0; i < 3; ++i) {
        QString outFile = QDir(tmp).filePath(QString("import-%1.fvd").arg(kinds.at(i)));
        int code = cmdImport.run(QStringList() << kinds.at(i) << QDir(imports).filePath(files.at(i)) << outFile);
        coreProject again;
        bool loadedBack = loadProject(outFile, again, error) && !again.tracks.isEmpty() && !again.tracks.at(0)->lSections.isEmpty();
        report(allOk, code == 0 && loadedBack, QString("import of %1 succeeds and the project loads back (exit code %2)").arg(files.at(i)).arg(code));
    }
    QByteArray nl1 = readAll(QDir(imports).filePath("nl1.nltrack"));
    QByteArray csv = readAll(QDir(imports).filePath("nl2.csv"));
    struct bad { const char* kind; QString file; QByteArray data; const char* what; };
    QList<bad> broken;
    broken.append(bad{"txt", QDir(tmp).filePath("empty-points.txt"), QByteArray(), "an empty point list"});
    broken.append(bad{"txt", QDir(tmp).filePath("one-point.txt"), QByteArray("1.0 2.0 3.0"), "a point list with one point"});
    broken.append(bad{"txt", QDir(tmp).filePath("bad-third-line.txt"), QByteArray("1 2 3\n4 5 6\nx y z\n7 8 9\n"), "a point list with a bad third line"});
    broken.append(bad{"txt", QDir(tmp).filePath("words.txt"), QByteArray("not numbers at all"), "a point list of words"});
    broken.append(bad{"csv", QDir(tmp).filePath("short-line.csv"), csv.left(csv.indexOf('\n') + 1) + "1\t2\t3\n", "a CSV with a short line"});
    broken.append(bad{"csv", QDir(tmp).filePath("header-only.csv"), csv.left(csv.indexOf('\n') + 1), "a CSV without data"});
    broken.append(bad{"nltrack", QDir(tmp).filePath("cut.nltrack"), nl1.left(nl1.size()/2), "a truncated nltrack"});
    broken.append(bad{"nltrack", QDir(tmp).filePath("tiny.nltrack"), QByteArray("BEZR"), "an nltrack that is only a tag"});
    for(int i = 0; i < broken.size(); ++i) {
        QFile f(broken.at(i).file);
        f.open(QIODevice::WriteOnly);
        f.write(broken.at(i).data);
        f.close();
        coreProject scratch;
        track* t = scratch.projectNewTrack();
        noLimitsImporter importer(t, broken.at(i).file);
        QString kind = broken.at(i).kind;
        bool ok = kind == "nltrack" ? importer.importAsNlTrack() : (kind == "txt" ? importer.importAsTxt() : importer.importAsCsv());
        QString outFile = QDir(tmp).filePath(QString("refused-%1.fvd").arg(i));
        int code = cmdImport.run(QStringList() << kind << broken.at(i).file << outFile);
        report(allOk, !ok && !importer.error().isEmpty() && code == 1 && !QFileInfo(outFile).exists() && t->lSections.isEmpty(),
               QString("%1 is refused with a message, and the track has no section afterwards: %2").arg(broken.at(i).what).arg(importer.error()));
    }
}

// limits: a real project's forces end to end (measures, then limits --trace),
// and the output when nothing is found (no time of minus one step)
static void checkLimits(bool& allOk, const QString& baseline, const QString& tmp)
{
    const QString captureFile = QDir(tmp).filePath("limits-captured.txt");
    QByteArray printed;
    struct known { const char* project; bool within; };
    const known projects[] = { {"gen-normal-shapes.fvd", true}, {"four-sections.fvd", false} };
    for(unsigned k = 0; k < sizeof(projects)/sizeof(projects[0]); ++k) {
        const QString name = projects[k].project;
        const QString trace = QDir(tmp).filePath(name + ".trace.tsv");
        int code = cmdMeasures.run(QStringList() << QDir(baseline).filePath(name) << "--ids" << "forceAlong,forceLateral,forceNormal"
                                   << "--every-s" << "0.01" << "--out" << trace);
        report(allOk, code == 0, QString("measures writes the forces of %1 every 0.01 s (exit code %2)").arg(name).arg(code));
        code = runCaptured(cmdLimits, QStringList() << "--trace" << trace, captureFile, printed);
        const bool within = printed.contains("Result: within the limits.");
        const bool found = printed.contains(" finding(s).");
        report(allOk, code == 0 && (projects[k].within ? within : found),
               QString("limits --trace on %1: exit 0 and %2 (exit code %3)").arg(name)
               .arg(projects[k].within ? "within the limits" : "findings").arg(code));
        code = runCaptured(cmdLimits, QStringList() << "--trace" << trace << "--json", captureFile, printed);
        QJsonParseError error;
        const QJsonObject doc = QJsonDocument::fromJson(printed, &error).object();
        report(allOk, code == 0 && error.error == QJsonParseError::NoError && doc.value("pass").toBool() == projects[k].within
                      && (doc.value("findings").toArray().isEmpty() == projects[k].within),
               QString("limits --trace --json on %1: a document that parses, pass %2, %3 findings (exit code %4)")
               .arg(name).arg(doc.value("pass").toBool() ? "true" : "false").arg(doc.value("findings").toArray().size()).arg(code));
    }
    // nothing found: a still, weightless trace at 0.6 s steps, not filtered, so
    // no limit applies anywhere and the 1 Hz restraint filter cannot run
    const QString still = QDir(tmp).filePath("still.tsv");
    QFile f(still);
    if(f.open(QIODevice::WriteOnly)) {
        f.write("time_s\tgx\tgy\tgz\n");
        for(int i = 0; i < 20; ++i) f.write(QString("%1\t0\t0\t0\n").arg(i*0.6).toUtf8());
        f.close();
    }
    int code = runCaptured(cmdLimits, QStringList() << "--trace" << still << "--no-filter", captureFile, printed);
    report(allOk, code == 0 && printed.contains("Largest share of a limit: none") && printed.contains("Restraint area (1 Hz, information): not found.")
                  && !printed.contains("-0.60 s"),
           QString("limits on a trace where nothing applies says \"none\" and \"not found\", no negative time (exit code %1)").arg(code));
    code = runCaptured(cmdLimits, QStringList() << "--trace" << still << "--no-filter" << "--json", captureFile, printed);
    const QJsonObject doc = QJsonDocument::fromJson(printed).object();
    report(allOk, code == 0 && doc.value("worst_time_s").isNull() && doc.value("restraint_area_time_s").isNull()
                  && doc.value("restraint_area").isNull() && doc.value("families").toObject().value("z").toObject().value("time_s").isNull(),
           QString("limits --json on that trace gives null times (exit code %1)").arg(code));
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
    report(allOk, code == 0 && printed.contains("\"tracks\"") && printed.contains("\"note\"") && printed.contains("\"length_m\"") && printed.contains("\"floater_airtime_s\"") && printed.contains("\"section_list\""),
           QString("summary --json prints one object with a tracks array and the note (exit code %1)").arg(code));
    code = cmdSummary.run(QStringList() << project << "--track" << "0");
    int code2 = cmdSummary.run(QStringList() << project << "--track" << "-1");
    report(allOk, code == 1 && code2 == 1, QString("summary with --track 0 or -1 exits 1 (exit codes %1, %2)").arg(code).arg(code2));
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

    // an existing output file is not replaced without --force
    code = cmdToText.run(QStringList() << project << textFile);
    c1 = cmdToText.run(QStringList() << project << textFile << "--force");
    report(allOk, code == 1 && c1 == 0, QString("totext refuses an existing file (exit code %1) and writes it with --force (exit code %2)").arg(code).arg(c1));
    code = cmdFromText.run(QStringList() << textFile << backFile);
    c2 = cmdFromText.run(QStringList() << textFile << backFile << "--force");
    report(allOk, code == 1 && c2 == 0, QString("fromtext refuses an existing file (exit code %1) and writes it with --force (exit code %2)").arg(code).arg(c2));
    checkSame(allOk, "the forced fromtext still gives the re-saved project", backFile, QDir(tmp.path()).filePath("four-sections.resaved.fvd"));

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

    checkLimits(allOk, baseline, tmp.path());

    // (gen-normal-shapes.fvd: its sections do not hold the speed, so friction changes the ride sheet)
    checkSessionAndImport(allOk, repo, tmp.path(), QDir(baseline).filePath("gen-normal-shapes.fvd"));

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
    "export, nodes, compare, summary, measures, check, limits and totext/fromtext on the baseline project four-sections.fvd and check the results (also: fvdcore <folder>)",
    runSelftest
};
