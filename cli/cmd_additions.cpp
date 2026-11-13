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

// fvdcore additions: shows or edits the additions a project keeps in the
// appendix after the end of its file (core/projectfile.h). Nothing in the
// program uses an addition yet; this exists so that a test file can be made
// by hand. A key belongs either to the project (--project) or to one track
// (--track N); exactly one of the two is given with --set and --remove.

#include "commands.h"
#include "project.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <cstdio>

// The document the appendix would hold, for --show.
static QJsonObject appendixDocument(const coreProject& project)
{
    QJsonArray tracks;
    for(int i = 0; i < project.tracks.size(); ++i) {
        tracks.append(project.tracks.at(i)->additions);
    }
    QJsonObject root;
    root["needs"] = QJsonArray();
    root["project"] = project.additions;
    root["tracks"] = tracks;
    return root;
}

// Any JSON value (an object, a list, a number, a text in quotes, true, false,
// null): Qt reads only an object or a list as a document, so it is read as
// the one item of a list.
static bool parseValue(const QString& text, QJsonValue* value, QString* error)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray("[") + text.toUtf8() + "]", &parseError);
    if(doc.isNull() || !doc.isArray() || doc.array().size() != 1) {
        *error = QString("the value \"%1\" is not one JSON value (%2); a text needs its quotes, for example key=\"\\\"text\\\"\"")
                 .arg(text, doc.isNull() ? parseError.errorString() : QString("more than one value"));
        return false;
    }
    *value = doc.array().at(0);
    return true;
}

static int runAdditions(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--show" << "--project" << "--force",
                     QStringList() << "--track" << "--set" << "--remove" << "--out",
                     positional, flags, values, error) || positional.size() != 1) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdAdditions.usage);
        return 1;
    }
    bool show = flags.contains("--show");
    bool set = values.contains("--set");
    bool remove = values.contains("--remove");
    if((show ? 1 : 0) + (set ? 1 : 0) + (remove ? 1 : 0) != 1) {
        fprintf(stderr, "give exactly one of --show, --set and --remove\nusage: %s\n", cmdAdditions.usage);
        return 1;
    }
    bool forProject = flags.contains("--project");
    bool forTrack = values.contains("--track");
    if(!show && forProject == forTrack) {
        fprintf(stderr, "say whose key it is: --project for the project's own, or --track N for track N\n");
        return 1;
    }

    const QString file = positional.at(0);
    coreProject project;
    QStringList warnings;
    if(!loadProject(file, project, error, &warnings)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    for(int i = 0; i < warnings.size(); ++i) fprintf(stderr, "%s\n", qPrintable(warnings.at(i)));

    if(show) {
        QByteArray text = QJsonDocument(appendixDocument(project)).toJson(QJsonDocument::Indented);
        fwrite(text.constData(), 1, text.size(), stdout);
        return 0;
    }

    // whose additions
    QJsonObject* target = &project.additions;
    QString whose("the project");
    if(forTrack) {
        bool ok = false;
        int number = values.value("--track").toInt(&ok);
        if(!ok) {
            fprintf(stderr, "--track needs a track number, not %s\n", qPrintable(values.value("--track")));
            return 1;
        }
        track* t = trackByNumber(project, number, error);
        if(!t) {
            fprintf(stderr, "%s\n", qPrintable(error));
            return 1;
        }
        target = &t->additions;
        whose = QString("track %1").arg(number);
    }

    if(set) {
        QString pair = values.value("--set");
        int eq = pair.indexOf('=');
        if(eq <= 0) {
            fprintf(stderr, "--set takes key=<json>, for example --set note=\"\\\"hello\\\"\"\n");
            return 1;
        }
        QJsonValue value;
        if(!parseValue(pair.mid(eq+1), &value, &error)) {
            fprintf(stderr, "%s\n", qPrintable(error));
            return 1;
        }
        target->insert(pair.left(eq), value);
    } else {
        QString key = values.value("--remove");
        if(!target->contains(key)) {
            fprintf(stderr, "%s has no key \"%s\"; nothing written\n", qPrintable(whose), qPrintable(key));
            return 1;
        }
        target->remove(key);
    }

    // Written to --out, or back over the project itself; either way an
    // existing file is replaced only with --force.
    QString out = values.contains("--out") ? values.value("--out") : file;
    // A file whose appendix could not be read still holds those additions:
    // never written over, not even with --force.
    if(refuseOverwritingDropped(project, out, error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    if(QFileInfo(out).exists() && !flags.contains("--force")) {
        fprintf(stderr, "%s already exists; give --force to replace it\n", qPrintable(out));
        return 1;
    }
    QString result = saveProjectFile(out, project.texPath, project.tracks, &project);
    if(result.contains("Error")) {
        fprintf(stderr, "%s\n", qPrintable(result));
        return 1;
    }
    return 0;
}

const command cmdAdditions = {
    "additions",
    "fvdcore additions <project.fvd> --show | (--project | --track N) (--set key=<json> | --remove key) [--out <file>] [--force]",
    "show, set or remove a project's or a track's additions (the appendix after the end of the file); "
    "writes to --out or over the project, an existing file needs --force",
    runAdditions
};
