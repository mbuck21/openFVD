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

#include "project.h"

#include <QFileInfo>
#include <QImage>
#include <glm/gtc/matrix_transform.hpp>

#include "../lenassert.h"

coreProject::coreProject() : texPath(":/background.png"), additionsDropped(false) {}
coreProject::~coreProject() { clear(); }

void coreProject::clear()
{
    while(!tracks.isEmpty()) {
        delete tracks.takeFirst();
    }
}

void coreProject::projectCleared()
{
    clear();
    additions = QJsonObject();
    additionsDropped = false;
    droppedReason.clear();
    loadedFrom.clear();
}

// The image resources of resources.qrc: this program does not link them, so
// they are accepted by name.
static bool canLoad(const QString& path)
{
    if(path.startsWith(":/")) {
        static const char* resources[] = { ":/background.png", ":/metal.png", ":/sky/negx.jpg", ":/sky/negy.jpg",
                                           ":/sky/negz.jpg", ":/sky/posx.jpg", ":/sky/posy.jpg", ":/sky/posz.jpg" };
        for(unsigned i = 0; i < sizeof(resources)/sizeof(resources[0]); ++i) {
            if(path == resources[i]) return true;
        }
        return false;
    }
    return !QImage(path).isNull();
}

// The windowed program loads the image into the view and falls back to the
// built-in one when that fails.
bool coreProject::projectGroundTexture(const QString& path)
{
    bool loaded = true;
    texPath = path;
    if(!canLoad(texPath)) {
        texPath = QString(":/background.png");
        loaded = false;
    }
    return loaded;
}

track* coreProject::projectNewTrack()
{
    track* t = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
    t->name = QString("unnamed Track");
    t->display.colors[0] = QColor(20, 20, 130);
    t->display.colors[1] = QColor(255, 51, 51);
    t->display.colors[2] = QColor(51, 255, 51);
    tracks.append(t);
    return t;
}

bool loadProject(const QString& file, coreProject& project, QString& error, QStringList* warnings)
{
    if(!QFileInfo(file).isFile()) {
        error = QString("project file not found: ") + file;
        return false;
    }
    QString loadResult = loadProjectFile(file, &project);
    // told by how the text starts: a warning can quote words from the file
    if(loadResult.startsWith("Error")) {
        error = QString("loading ") + file + ": " + loadResult;
        return false;
    }
    project.loadedFrom = file;
    if(loadResult.startsWith("Warning") && warnings) {
        warnings->append(loadResult);
    }
    return true;
}

bool refuseOverwritingDropped(const coreProject& project, const QString& target, QString& error)
{
    if(!project.additionsDropped || project.loadedFrom.isEmpty()) return false;
    QFileInfo to(target), from(project.loadedFrom);
    if(!to.exists() || to.canonicalFilePath() != from.canonicalFilePath()) return false;
    error = QString("%1 was loaded without its additions (%2); writing over it would lose them for good, "
                    "even with --force. Give another output file.").arg(target, project.droppedReason);
    return true;
}

track* trackByNumber(coreProject& project, int number, QString& error)
{
    if(number < 1 || number > project.tracks.size()) {
        error = QString("the project has %1 track(s); there is no track %2").arg(project.tracks.size()).arg(number);
        return NULL;
    }
    return project.tracks.at(number-1);
}

bool parseOptions(const QStringList& args, const QStringList& flags, const QStringList& values,
                  QStringList& positional, QStringList& givenFlags, QMap<QString, QString>& givenValues,
                  QString& error)
{
    for(int i = 0; i < args.size(); ++i) {
        const QString& a = args.at(i);
        if(flags.contains(a)) {
            givenFlags.append(a);
        } else if(values.contains(a)) {
            if(i+1 >= args.size()) {
                error = QString("option %1 needs a value").arg(a);
                return false;
            }
            givenValues[a] = args.at(++i);
        } else if(a.startsWith("--")) {
            error = QString("unknown option %1").arg(a);
            return false;
        } else {
            positional.append(a);
        }
    }
    return true;
}
