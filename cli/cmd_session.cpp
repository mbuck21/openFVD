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

// fvdcore session: a project held open in the window-less program and driven
// by the model's named commands (core/command.h). One request per line on
// standard input, as a JSON object with an "op"; one reply per line on
// standard output, always with "ok" true or false (and "error" when false).
// A bad line gets an error reply and the session goes on. cli/README.md has
// the table of operations; tools/fvdsession.py is the Python side.

#include "commands.h"
#include "project.h"

#include "../core/command.h"
#include "../core/finding.h"
#include "../core/measure.h"
#include "../core/mnode.h"
#include "../core/projecttext.h"
#include "../core/ridesummary.h"
#include "../core/section.h"
#include "../core/trackhistory.h"
#include "../lenassert.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

QJsonValue numberOrNull(double v)
{
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

QJsonObject errorReply(const QString& text)
{
    QJsonObject o;
    o["ok"] = false;
    o["error"] = text;
    return o;
}

QJsonObject okReply()
{
    QJsonObject o;
    o["ok"] = true;
    return o;
}

class session
{
public:
    session(const QString& file, int defaultTrack);
    ~session();

    bool load(QString* error);

    // One request in, one reply out. *quit is set by the "quit" operation.
    QJsonObject handle(const QByteArray& line, bool* quit);

private:
    QJsonObject dispatch(const QJsonObject& req, bool* quit);
    bool pickTrack(const QJsonObject& req, int* number, track** t, trackHistory** h, QString* error);
    trackHistory* historyFor(int number);

    QJsonObject opRun(const QJsonObject& req);
    QJsonObject opUndoRedo(const QJsonObject& req, bool undo);
    QJsonObject opHistory(const QJsonObject& req);
    QJsonObject opBegin(const QJsonObject& req);
    QJsonObject opEnd(const QJsonObject& req);
    QJsonObject opRecompute(const QJsonObject& req);
    QJsonObject opSummary(const QJsonObject& req);
    QJsonObject opSectionEnd(const QJsonObject& req);
    QJsonObject opMeasures(const QJsonObject& req);
    QJsonObject opCheck(const QJsonObject& req);
    QJsonObject opSave(const QJsonObject& req);
    QJsonObject opToText(const QJsonObject& req);
    QJsonObject opExport(const QJsonObject& req);
    QJsonObject opStats();

    QString file;
    int defaultTrack;
    coreProject project;
    QMap<int, trackHistory*> histories;

    qint64 requests;
    qint64 runCount, recomputeCount;
    qint64 runNs, recomputeNs;
};

session::session(const QString& f, int t)
    : file(f), defaultTrack(t), requests(0), runCount(0), recomputeCount(0), runNs(0), recomputeNs(0)
{
}

session::~session()
{
    qDeleteAll(histories);      // before the tracks they point to
    histories.clear();
}

bool session::load(QString* error)
{
    QString problem;
    if(!loadProject(file, project, problem)) {
        if(error) *error = problem;
        return false;
    }
    if(!trackByNumber(project, defaultTrack, problem)) {
        if(error) *error = problem;
        return false;
    }
    return true;
}

trackHistory* session::historyFor(int number)
{
    trackHistory* h = histories.value(number, NULL);
    if(!h) {
        h = new trackHistory(project.tracks.at(number-1));
        histories[number] = h;
    }
    return h;
}

// The track of a request: its "track" key (1-based) or the session's default.
bool session::pickTrack(const QJsonObject& req, int* number, track** t, trackHistory** h, QString* error)
{
    int n = defaultTrack;
    if(req.contains("track")) {
        QJsonValue v = req.value("track");
        double d = v.toDouble(-1.0);
        if(!v.isDouble() || std::floor(d) != d || d < 1.0 || d > 1.0e6) {
            *error = "\"track\" must be a whole number from 1";
            return false;
        }
        n = (int)d;
    }
    if(!trackByNumber(project, n, *error)) {
        return false;
    }
    if(number) *number = n;
    if(t) *t = project.tracks.at(n-1);
    if(h) *h = historyFor(n);
    return true;
}

QJsonObject session::handle(const QByteArray& rawLine, bool* quit)
{
    ++requests;
    QByteArray line = rawLine.trimmed();
    if(line.isEmpty()) {
        return errorReply("empty line: a request is one JSON object per line");
    }
    QJsonParseError parse;
    QJsonDocument doc = QJsonDocument::fromJson(line, &parse);
    if(parse.error != QJsonParseError::NoError || !doc.isObject()) {
        return errorReply(parse.error != QJsonParseError::NoError
                          ? QString("not JSON: ") + parse.errorString() + QString(" (offset %1)").arg(parse.offset)
                          : QString("a request is a JSON object, such as {\"op\":\"summary\"}"));
    }
    return dispatch(doc.object(), quit);
}

QJsonObject session::dispatch(const QJsonObject& req, bool* quit)
{
    if(!req.value("op").isString()) {
        return errorReply("the request has no \"op\" (a text), such as \"run\", \"summary\" or \"quit\"");
    }
    const QString op = req.value("op").toString();
    if(op == "describe") {
        QJsonObject o = okReply();
        o["commands"] = commandRegistry::describeJson();
        return o;
    }
    if(op == "run") return opRun(req);
    if(op == "undo") return opUndoRedo(req, true);
    if(op == "redo") return opUndoRedo(req, false);
    if(op == "history") return opHistory(req);
    if(op == "begin") return opBegin(req);
    if(op == "end") return opEnd(req);
    if(op == "recompute") return opRecompute(req);
    if(op == "summary") return opSummary(req);
    if(op == "sectionEnd") return opSectionEnd(req);
    if(op == "measures") return opMeasures(req);
    if(op == "check") return opCheck(req);
    if(op == "save") return opSave(req);
    if(op == "totext") return opToText(req);
    if(op == "export") return opExport(req);
    if(op == "stats") return opStats();
    if(op == "quit") {
        *quit = true;
        return okReply();
    }
    return errorReply(QString("unknown op \"%1\" (describe, run, undo, redo, history, begin, end, recompute, summary, "
                              "sectionEnd, measures, check, save, totext, export, stats, quit)").arg(op));
}

QJsonObject session::opRun(const QJsonObject& req)
{
    QString error;
    int number;
    trackHistory* h;
    if(!pickTrack(req, &number, NULL, &h, &error)) return errorReply(error);
    if(!req.value("name").isString()) return errorReply("\"run\" needs \"name\", the command's name (op describe lists them)");
    commandTarget target(number-1);
    if(req.contains("target")) {
        if(!req.value("target").isObject()) return errorReply("\"target\" must be an object such as {\"section\":2}");
        if(!commandTarget::fromJson(req.value("target").toObject(), &target, &error)) return errorReply(error);
        if(target.track < 0) target.track = number-1;
    }
    commandArgs args;
    if(req.contains("args")) {
        if(!req.value("args").isObject()) return errorReply("\"args\" must be an object such as {\"change\":1.5}");
        args = commandArgsFromJson(req.value("args").toObject());
    }
    QElapsedTimer timer;
    timer.start();
    bool ok = commandRegistry::run(h, req.value("name").toString(), target, args, &error);
    runNs += timer.nsecsElapsed();
    ++runCount;
    return ok ? okReply() : errorReply(error);
}

QJsonObject session::opUndoRedo(const QJsonObject& req, bool undo)
{
    QString error;
    trackHistory* h;
    if(!pickTrack(req, NULL, NULL, &h, &error)) return errorReply(error);
    QString label = undo ? h->undoLabel() : h->redoLabel();
    QJsonObject o = okReply();
    bool done;
    if(h->inScope()) {
        done = false;
        o["reason"] = "a step is open (begin without end)";
    } else {
        done = undo ? h->undo() : h->redo();
        if(!done) o["reason"] = undo ? "nothing to undo" : "nothing to redo";
    }
    o["done"] = done;
    o["label"] = done ? label : QString();
    o["position"] = h->position();
    return o;
}

QJsonObject session::opHistory(const QJsonObject& req)
{
    QString error;
    trackHistory* h;
    if(!pickTrack(req, NULL, NULL, &h, &error)) return errorReply(error);
    QJsonObject o = okReply();
    QJsonArray steps;
    QStringList labels = h->labels();
    for(int i = 0; i < labels.size(); ++i) steps.append(labels.at(i));
    o["steps"] = steps;
    o["position"] = h->position();
    o["can_undo"] = h->canUndo();
    o["can_redo"] = h->canRedo();
    o["open_scopes"] = h->scopeDepth();
    return o;
}

QJsonObject session::opBegin(const QJsonObject& req)
{
    QString error;
    trackHistory* h;
    if(!pickTrack(req, NULL, NULL, &h, &error)) return errorReply(error);
    QString label = req.value("label").isString() ? req.value("label").toString() : QString("Session step");
    h->beginStep(label);
    QJsonObject o = okReply();
    o["depth"] = h->scopeDepth();
    return o;
}

QJsonObject session::opEnd(const QJsonObject& req)
{
    QString error;
    trackHistory* h;
    if(!pickTrack(req, NULL, NULL, &h, &error)) return errorReply(error);
    if(!h->inScope()) return errorReply("no step is open: \"end\" needs an earlier \"begin\"");
    bool commit = true;
    if(req.contains("commit")) {
        if(!req.value("commit").isBool()) return errorReply("\"commit\" must be true or false");
        commit = req.value("commit").toBool();
    }
    int stepsBefore = h->steps().size(), positionBefore = h->position();
    qint64 bytesBefore = h->bytes();
    h->endStep(commit);
    bool recorded = false;
    if(!h->inScope()) {
        recorded = h->steps().size() != stepsBefore || h->position() != positionBefore || h->bytes() != bytesBefore;
        if(!commit) recorded = false;
    }
    QJsonObject o = okReply();
    o["recorded"] = recorded;
    o["depth"] = h->scopeDepth();
    return o;
}

QJsonObject session::opRecompute(const QJsonObject& req)
{
    QString error;
    track* t;
    trackHistory* h;
    if(!pickTrack(req, NULL, &t, &h, &error)) return errorReply(error);
    QElapsedTimer timer;
    timer.start();
    t->updateTrack(0, 0);
    recomputeNs += timer.nsecsElapsed();
    ++recomputeCount;
    // Outside a step the recompute is a change nobody recorded: fold it into
    // the history's state so that the next step does not warn about it.
    if(!h->inScope()) h->foldOutsideChange();
    return okReply();
}

QJsonObject session::opSummary(const QJsonObject& req)
{
    QString error;
    int number;
    track* t;
    if(!pickTrack(req, &number, &t, NULL, &error)) return errorReply(error);
    bool withSections = req.value("sections").toBool(false);
    rideSummary r = computeRideSummary(t);
    QJsonObject o = okReply();
    o["summary"] = summarySheetJson(r, number, t->name, withSections);
    return o;
}

QJsonObject session::opSectionEnd(const QJsonObject& req)
{
    QString error;
    track* t;
    if(!pickTrack(req, NULL, &t, NULL, &error)) return errorReply(error);
    QJsonValue sv = req.value("section");
    double sd = sv.toDouble(-1.0);
    if(!sv.isDouble() || std::floor(sd) != sd || sd < 1.0) return errorReply("\"section\" must be a whole number from 1");
    int s = (int)sd - 1;
    if(s >= t->lSections.size()) {
        return errorReply(QString("the track has %1 section(s); there is no section %2").arg(t->lSections.size()).arg(s+1));
    }
    // the global index of the section's last node (as track::getPoint counts)
    int index = 0;
    for(int i = 0; i <= s; ++i) index += t->lSections.at(i)->lNodes.size() - 1;
    mnode* end = t->getPoint(index);

    // the three forces, roll smoothing included, as the measures give them
    measureNodes n = measureRegistry::collect(t);
    QVector<QVector<float> > m = measureRegistry::values(n, QStringList() << "forceNormal" << "forceLateral" << "forceAlong");

    bool nonFinite = false;
    for(int i = 0; i <= index && !nonFinite; ++i) {
        const mnode* node = n.nodes.at(i);
        if(!std::isfinite(node->vPos.x) || !std::isfinite(node->vPos.y) || !std::isfinite(node->vPos.z)
                || !std::isfinite(node->fVel) || !std::isfinite(node->fTotalLength)
                || !std::isfinite(m.at(0).at(i)) || !std::isfinite(m.at(1).at(i)) || !std::isfinite(m.at(2).at(i))) {
            nonFinite = true;
        }
    }

    placement place(t, true);
    glm::vec3 pos = place.point(end->vPosHeart(t->fHeart));
    glm::vec3 norm = place.direction(end->vNorm);
    QJsonObject o = okReply();
    o["section"] = s + 1;
    o["node"] = index;
    o["x"] = numberOrNull(pos.x);
    o["y"] = numberOrNull(pos.y);
    o["z"] = numberOrNull(pos.z);
    o["heading_deg"] = numberOrNull(place.heading(end->getDirection()));
    o["pitch_deg"] = numberOrNull(end->getPitch());
    o["bank_deg"] = numberOrNull(end->fRoll);
    o["speed_mps"] = numberOrNull(end->fVel);
    o["normal_g"] = numberOrNull(m.at(0).at(index));
    o["lateral_g"] = numberOrNull(m.at(1).at(index));
    o["along_g"] = numberOrNull(m.at(2).at(index));
    o["norm_x"] = numberOrNull(norm.x);
    o["norm_y"] = numberOrNull(norm.y);
    o["norm_z"] = numberOrNull(norm.z);
    o["dist_m"] = numberOrNull(end->fTotalLength);
    o["non_finite"] = nonFinite;
    return o;
}

QJsonObject session::opMeasures(const QJsonObject& req)
{
    QString error;
    track* t;
    if(!pickTrack(req, NULL, &t, NULL, &error)) return errorReply(error);
    if(!req.value("ids").isArray() || req.value("ids").toArray().isEmpty()) {
        return errorReply("\"ids\" must be a list of measure ids, such as [\"speed\",\"forceNormal\"] (fvdcore measures --list)");
    }
    QStringList ids;
    QJsonArray idArray = req.value("ids").toArray();
    for(int i = 0; i < idArray.size(); ++i) {
        if(!idArray.at(i).isString() || !measureRegistry::find(idArray.at(i).toString())) {
            return errorReply(QString("unknown measure %1 (fvdcore measures --list)").arg(idArray.at(i).toString()));
        }
        ids << idArray.at(i).toString();
    }
    if(t->lSections.isEmpty()) return errorReply("the track has no sections");
    int last = t->getNumPoints();
    int given = (req.contains("node") ? 1 : 0) + (req.contains("at_m") ? 1 : 0) + (req.contains("at_s") ? 1 : 0);
    if(given != 1) return errorReply("give exactly one of \"node\" (0-based, as `nodes` prints it), \"at_m\" or \"at_s\"");
    int index;
    if(req.contains("node")) {
        double d = req.value("node").toDouble(-1.0);
        if(!req.value("node").isDouble() || std::floor(d) != d || d < 0.0 || d > last) {
            return errorReply(QString("\"node\" must be a whole number from 0 to %1").arg(last));
        }
        index = (int)d;
    } else if(req.contains("at_m")) {
        double d = req.value("at_m").toDouble(-1.0);
        if(!req.value("at_m").isDouble() || !(d >= 0.0)) return errorReply("\"at_m\" must be a number of metres, 0 or more");
        index = t->getIndexFromDist((float)d);
        if(index > last) index = last;
        if(index < 0) index = 0;
    } else {
        double d = req.value("at_s").toDouble(-1.0);
        if(!req.value("at_s").isDouble() || !(d >= 0.0)) return errorReply("\"at_s\" must be a number of seconds, 0 or more");
        double at = std::floor(d*F_HZ + 0.5);
        index = at > last ? last : (int)at;
    }
    QVector<QVector<float> > data = measureRegistry::values(t, ids);
    QJsonObject values;
    for(int k = 0; k < ids.size(); ++k) values[ids.at(k)] = numberOrNull(data.at(k).at(index));
    int nodeInSection, sec;
    t->getSecNode(index, &nodeInSection, &sec);
    QJsonObject o = okReply();
    o["node"] = index;
    o["time_s"] = index/F_HZ;
    o["section"] = sec + 1;
    o["dist_m"] = numberOrNull(t->getPoint(index)->fTotalLength);
    o["values"] = values;
    return o;
}

QJsonObject session::opCheck(const QJsonObject& req)
{
    QString error;
    track* t;
    if(!pickTrack(req, NULL, &t, NULL, &error)) return errorReply(error);
    QList<finding> found;
    if(req.contains("check")) {
        if(!req.value("check").isString() || !checkRegistry::find(req.value("check").toString())) {
            return errorReply(QString("unknown check (fvdcore check --list names them)"));
        }
        found = checkRegistry::run(t, req.value("check").toString());
    } else {
        found = checkRegistry::runAll(t);
    }
    QJsonObject o = checkFindingsJson(t, found);
    o["ok"] = true;
    return o;
}

// A path to write: needs "path", and an existing file is not replaced
// without "force": true.
static bool writablePath(const QJsonObject& req, QString* path, QString* error)
{
    if(!req.value("path").isString() || req.value("path").toString().isEmpty()) {
        *error = "\"path\" must be the file to write";
        return false;
    }
    *path = req.value("path").toString();
    if(QFileInfo(*path).exists() && !req.value("force").toBool(false)) {
        *error = *path + " already exists; give \"force\":true to replace it";
        return false;
    }
    return true;
}

QJsonObject session::opSave(const QJsonObject& req)
{
    QString path, error;
    if(!writablePath(req, &path, &error)) return errorReply(error);
    QString result = saveProjectFile(path, project.texPath, project.tracks, &project);
    if(result.contains("Error")) return errorReply(QString("saving ") + path + ": " + result);
    return okReply();
}

QJsonObject session::opToText(const QJsonObject& req)
{
    QString path, error;
    if(!writablePath(req, &path, &error)) return errorReply(error);
    QByteArray out;
    QString result = writeProjectText(out, project.texPath, project.tracks, &project);
    if(result.startsWith("Error")) return errorReply(result);
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(out) != out.size()) {
        return errorReply(QString("cannot write ") + path);
    }
    return okReply();
}

QJsonObject session::opExport(const QJsonObject& req)
{
    if(!req.value("dir").isString() || req.value("dir").toString().isEmpty()) {
        return errorReply("\"dir\" must be the folder to write into");
    }
    QString dirName = req.value("dir").toString();
    bool force = req.value("force").toBool(false);
    QString stem = QFileInfo(file).fileName();
    if(stem.endsWith(".fvd", Qt::CaseInsensitive)) stem.chop(4);

    // the names the export would write, so that nothing is replaced unasked
    QDir dir(dirName);
    QStringList names;
    for(int i = 0; i < project.tracks.size(); ++i) {
        if(project.tracks.at(i)->lSections.isEmpty()) continue;
        QString base = i > 0 ? stem + QString(".track%1").arg(i+1) : stem;
        names << base + ".nl2elem" << base + ".tangent.nlelem" << base + ".spline.nlelem";
    }
    names << stem + ".resaved.fvd";
    if(!force) {
        for(int i = 0; i < names.size(); ++i) {
            if(QFileInfo(dir.filePath(names.at(i))).exists()) {
                return errorReply(dir.filePath(names.at(i)) + " already exists; give \"force\":true to replace it");
            }
        }
    }
    if(!QDir().mkpath(dirName)) return errorReply(QString("cannot create ") + dirName);
    QStringList report;
    QString error;
    if(!writeProjectExports(project, dir, stem, report, error)) return errorReply(error);
    QJsonArray written;
    for(int i = 0; i < report.size(); ++i) written.append(report.at(i));
    QJsonObject o = okReply();
    o["report"] = written;
    return o;
}

QJsonObject session::opStats()
{
    QJsonObject o = okReply();
    o["requests"] = (double)requests;
    o["run_count"] = (double)runCount;
    o["run_ms"] = runNs/1.0e6;
    o["recompute_count"] = (double)recomputeCount;
    o["recompute_ms"] = recomputeNs/1.0e6;
    return o;
}

QByteArray replyLine(const QJsonObject& reply)
{
    return QJsonDocument(reply).toJson(QJsonDocument::Compact) + "\n";
}

} // namespace

// The loop on standard input and output, or on any pair of files (the test).
int runSession(const QString& projectFile, int trackNumber, FILE* in, FILE* out, QString* error)
{
    session s(projectFile, trackNumber);
    QString problem;
    if(!s.load(&problem)) {
        if(error) *error = problem;
        return 1;
    }
    bool quit = false;
    QByteArray line;
    char buffer[65536];
    while(!quit && fgets(buffer, sizeof(buffer), in)) {
        size_t got = strlen(buffer);
        line.append(buffer, (int)got);
        if(got == sizeof(buffer)-1 && buffer[got-1] != '\n') continue;     // a long line: keep reading
        QByteArray reply = replyLine(s.handle(line, &quit));
        fwrite(reply.constData(), 1, reply.size(), out);
        fflush(out);
        line.clear();
    }
    return 0;
}

QByteArray sessionRunText(const QString& projectFile, int trackNumber, const QByteArray& input, QString* error)
{
    session s(projectFile, trackNumber);
    QString problem;
    if(!s.load(&problem)) {
        if(error) *error = problem;
        return QByteArray();
    }
    QByteArray output;
    bool quit = false;
    QList<QByteArray> lines = input.split('\n');
    if(!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
    for(int i = 0; i < lines.size() && !quit; ++i) {
        output += replyLine(s.handle(lines.at(i), &quit));
    }
    return output;
}

static int runSessionCommand(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList(), QStringList() << "--track", positional, flags, values, error)
            || positional.size() != 1) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdSession.usage);
        return 1;
    }
    int trackNumber = 1;
    if(values.contains("--track")) {
        bool ok;
        trackNumber = values["--track"].toInt(&ok);
        if(!ok) { fprintf(stderr, "--track needs a whole number\n"); return 1; }
    }
    int result = runSession(positional.at(0), trackNumber, stdin, stdout, &error);
    if(result != 0) fprintf(stderr, "%s\n", qPrintable(error));
    return result;
}

const command cmdSession = {
    "session",
    "fvdcore session <project.fvd> [--track N]",
    "hold a project open and run commands from standard input, one JSON request and one JSON reply per line",
    runSessionCommand
};
