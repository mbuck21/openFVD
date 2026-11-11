#ifndef PROJECTFILE_H
#define PROJECTFILE_H

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

#include <QString>
#include <QList>
#include <fstream>

class track;

// What reading and writing a project file (.fvd) asks of whoever holds the
// project. The reader and the writer call these synchronously, at the points
// noted; they change the model themselves and leave everything else (windows,
// the ground texture, undo) to the client.
// Implemented by projectWidget (ui/projectwidget.h); the batch mode and the core-only
// program use the same reader and writer.
class projectFileClient
{
public:
    virtual ~projectFileClient() {}

    // Reading. The tracks held so far are to be thrown away. Called once the
    // header was read, and again at the end when the file turned out to be
    // unreadable.
    virtual void projectCleared() = 0;

    // Reading. The project's ground texture is the file at path; returns
    // whether it could be loaded (false makes the load end with a warning).
    virtual bool projectGroundTexture(const QString& path) = 0;

    // Reading. A track was found in the file: returns a new, empty track that
    // becomes part of the project and is then read in.
    virtual track* projectNewTrack() = 0;

    // Reading. The roll smoothing entries of a track just read in were
    // brought up to date (track::updateSmoothEntries()); roll smoothing is
    // applied next. Not called for a file of the old version (v0.30).
    virtual void projectTrackSmoothingUpdated(track* which) = 0;

    // Reading. The track finished loading.
    virtual void projectTrackLoaded(track* which) = 0;

    // Reading. Every track in the file was read (or reading stopped at an
    // error); called before the result is returned.
    virtual void projectLoadFinished() = 0;

    // Writing. The track is written next.
    virtual void projectTrackSaving(track* which) = 0;
};

// Writes a project: the header, the ground texture path, the tracks, the end
// mark. Returns "Project Saved!". The client may be NULL.
QString writeProject(std::iostream& file, const QString& texPath, const QList<track*>& tracks, projectFileClient* client);

// Reads a project into new tracks obtained from the client, which must not be
// NULL here (it is the only source of tracks). Returns
// "Load Successfull.", a text starting "Warning: " or a text starting "Error".
QString readProject(std::iostream& file, projectFileClient* client);

// The same, opening the file fileName first ("Error: File is NULL" when it
// cannot be opened).
QString saveProjectFile(const QString& fileName, const QString& texPath, const QList<track*>& tracks, projectFileClient* client);
QString loadProjectFile(const QString& fileName, projectFileClient* client);

#endif // PROJECTFILE_H
