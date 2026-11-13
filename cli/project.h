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

#ifndef CLI_PROJECT_H
#define CLI_PROJECT_H

#include "../core/track.h"
#include "../core/projectfile.h"
#include "../core/ridesummary.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <glm/glm.hpp>

// What the windowed program's projectWidget does for the file reader, without
// the windows: it owns the tracks, makes new ones the way trackHandler's
// constructor does (ui/trackhandler.cpp), and keeps the ground texture path.
class coreProject : public projectFileClient
{
public:
    coreProject();
    ~coreProject();

    QList<track*> tracks;
    QString texPath;
    QJsonObject additions;  // the project's own additions (the file's appendix)
    QString loadedFrom;     // the file loadProject() read, empty for none
    bool additionsDropped;  // that load dropped the file's appendix (projectAdditionsDropped())
    QString droppedReason;

    void clear();

    void projectCleared();
    bool projectGroundTexture(const QString& path);
    track* projectNewTrack();
    void projectTrackSmoothingUpdated(track*) {}
    void projectTrackLoaded(track*) {}
    void projectLoadFinished() {}
    void projectTrackSaving(track*) {}
    QJsonObject projectAdditions() { return additions; }
    void projectAdditionsRead(const QJsonObject& read) { additions = read; }
    void projectAdditionsDropped(const QString& reason) { additionsDropped = true; droppedReason = reason; }
};

// True, with the refusal in error, when target is the very file the project
// was loaded from and that load dropped the file's appendix: the file still
// holds the additions, so nothing writes over it, --force or not. Every
// command that can write a project to a path asks this first.
bool refuseOverwritingDropped(const coreProject& project, const QString& target, QString& error);

// Loads a project file into project. On failure returns false and error holds
// the text (the same texts the export report uses: "project file not found: ..."
// or "loading <file>: <message>"). A warning from the reader goes into
// *warnings when given.
bool loadProject(const QString& file, coreProject& project, QString& error, QStringList* warnings = NULL);

// Track number (1-based, as the user counts) of a loaded project; NULL and an
// error text when the project has no such track.
track* trackByNumber(coreProject& project, int number, QString& error);

// `placement` (where a track's nodes are in the world) lives in
// core/ridesummary.h, shared with the ride sheet.

// Sorts args into plain words (positional), option flags that take no value
// (flags) and options that take one (values). Returns false and sets error on
// an unknown option or a missing value.
bool parseOptions(const QStringList& args, const QStringList& flags, const QStringList& values,
                  QStringList& positional, QStringList& givenFlags, QMap<QString, QString>& givenValues,
                  QString& error);

#endif
