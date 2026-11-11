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

// fvdcore totext and fromtext: a project as text (core/projecttext.h,
// docs/project-text-format.md) and back. "-" as the text file means stdout
// (totext) or stdin (fromtext). Warnings go to stderr; a refused text prints
// the message that names the place and exits 1.

#include "commands.h"
#include "project.h"

#include "../core/projecttext.h"

#include <QFile>
#include <cstdio>

static int runToText(const QStringList& args)
{
    if(args.size() != 2) {
        fprintf(stderr, "usage: %s\n", cmdToText.usage);
        return 1;
    }
    coreProject project;
    QString error;
    QStringList warnings;
    if(!loadProject(args.at(0), project, error, &warnings)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    for(int i = 0; i < warnings.size(); ++i) fprintf(stderr, "%s\n", qPrintable(warnings.at(i)));
    QByteArray out;
    QString result = writeProjectText(out, project.texPath, project.tracks, &project);
    if(result.startsWith("Error")) {
        fprintf(stderr, "%s\n", qPrintable(result));
        return 1;
    }
    if(args.at(1) == "-") {
        fwrite(out.constData(), 1, out.size(), stdout);
        return 0;
    }
    QFile file(args.at(1));
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(out) != out.size()) {
        fprintf(stderr, "cannot write %s\n", qPrintable(args.at(1)));
        return 1;
    }
    return 0;
}

static int runFromText(const QStringList& args)
{
    if(args.size() != 2) {
        fprintf(stderr, "usage: %s\n", cmdFromText.usage);
        return 1;
    }
    QByteArray in;
    if(args.at(0) == "-") {
        char buffer[65536];
        size_t got;
        while((got = fread(buffer, 1, sizeof(buffer), stdin)) > 0) in.append(buffer, (int)got);
    } else {
        QFile file(args.at(0));
        if(!file.open(QIODevice::ReadOnly)) {
            fprintf(stderr, "cannot read %s\n", qPrintable(args.at(0)));
            return 1;
        }
        in = file.readAll();
    }
    coreProject project;
    QString result = readProjectText(in, &project);
    if(result.startsWith("Error")) {
        fprintf(stderr, "%s\n", qPrintable(result));
        return 1;
    }
    if(result.startsWith("Warning")) fprintf(stderr, "%s\n", qPrintable(result));
    result = saveProjectFile(args.at(1), project.texPath, project.tracks, &project);
    if(result.contains("Error")) {
        fprintf(stderr, "%s\n", qPrintable(result));
        return 1;
    }
    return 0;
}

const command cmdToText = {
    "totext",
    "fvdcore totext <project.fvd> <out.json|->",
    "write a project as text (JSON); - means standard output",
    runToText
};

const command cmdFromText = {
    "fromtext",
    "fvdcore fromtext <in.json|-> <out.fvd>",
    "read a project from its text form and save it as .fvd; - means standard input",
    runFromText
};
