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

// fvdcore import: the three importers of the project panel (a NoLimits 1
// track, a point list, a NoLimits 2 CSV) on the window-less program. The
// import itself is core/nolimitsimporter.*; this file makes the project
// around it and saves it.

#include "commands.h"
#include "project.h"

#include "../core/nolimitsimporter.h"

#include <QDir>
#include <QFileInfo>
#include <cstdio>

static int runImport(const QStringList& args)
{
    QStringList positional, flags;
    QMap<QString, QString> values;
    QString error;
    if(!parseOptions(args, QStringList() << "--force" << "--window-project", QStringList() << "--export-dir",
                     positional, flags, values, error) || positional.size() != 3) {
        if(!error.isEmpty()) fprintf(stderr, "%s\n", qPrintable(error));
        fprintf(stderr, "usage: %s\n", cmdImport.usage);
        return 1;
    }
    QString kind = positional.at(0), input = positional.at(1), output = positional.at(2);
    if(kind != "nltrack" && kind != "txt" && kind != "csv") {
        fprintf(stderr, "the kind of file is nltrack, txt or csv, not %s\n", qPrintable(kind));
        return 1;
    }
    if(!QFileInfo(input).isFile()) {
        fprintf(stderr, "input file not found: %s\n", qPrintable(input));
        return 1;
    }
    if(!flags.contains("--force") && QFileInfo(output).exists()) {
        fprintf(stderr, "%s already exists; give --force to replace it\n", qPrintable(output));
        return 1;
    }

    coreProject project;
    if(flags.contains("--window-project")) {
        project.projectNewTrack();      // the empty track the windowed program starts with
    }
    track* t = project.projectNewTrack();
    noLimitsImporter importer(t, input);
    bool ok = kind == "nltrack" ? importer.importAsNlTrack()
            : (kind == "txt" ? importer.importAsTxt() : importer.importAsCsv());
    if(!ok) {
        fprintf(stderr, "import refused: %s\n", qPrintable(importer.error()));
        return 1;
    }
    if(values.contains("--export-dir")) {
        // What FVD.exe --batch-import writes: the exports of the imported
        // track(s) and the project saved straight from memory, named from the
        // input file (<stem>.track2.nl2elem, <stem>.resaved.fvd ...). A saved
        // import that is opened again does not always come back the same
        // (cli/README.md), so these are written before anything is reloaded.
        QString dirName = values["--export-dir"];
        if(!QDir().mkpath(dirName)) {
            fprintf(stderr, "cannot create %s\n", qPrintable(dirName));
            return 1;
        }
        QStringList written;
        if(!writeProjectExports(project, QDir(dirName), QFileInfo(input).completeBaseName(), written, error)) {
            fprintf(stderr, "%s\n", qPrintable(error));
            return 1;
        }
    }
    QString result = saveProjectFile(output, project.texPath, project.tracks, &project);
    if(result.contains("Error")) {
        fprintf(stderr, "saving %s: %s\n", qPrintable(output), qPrintable(result));
        return 1;
    }
    return 0;
}

const command cmdImport = {
    "import",
    "fvdcore import <nltrack|txt|csv> <file> <out.fvd> [--force] [--window-project] [--export-dir <dir>]",
    "import a NoLimits 1 track, a point list or a NoLimits 2 CSV into a new project and save it",
    runImport
};
