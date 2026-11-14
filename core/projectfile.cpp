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

#include "projectfile.h"
#include "track.h"
#include "exportfuncs.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <algorithm>
#include <sstream>

using namespace std;

// ---- the appendix after "EOP" ----------------------------------------------

QByteArray additionsBytes(const QJsonObject& additions)
{
    if(additions.isEmpty()) return QByteArray();
    return QJsonDocument(additions).toJson(QJsonDocument::Compact);
}

QJsonObject additionsFromBytes(const QByteArray& bytes)
{
    if(bytes.isEmpty()) return QJsonObject();
    QJsonDocument doc = QJsonDocument::fromJson(bytes);
    return doc.isObject() ? doc.object() : QJsonObject();
}

QStringList knownAppendixNeeds()
{
    return QStringList();
}

namespace {

// Nothing to add: the project and every track have no additions.
bool nothingToAdd(const QJsonObject& project, const QList<track*>& tracks)
{
    if(!project.isEmpty()) return false;
    for(int i = 0; i < tracks.size(); ++i) {
        if(!tracks[i]->additions.isEmpty()) return false;
    }
    return true;
}

void writeAppendix(std::iostream& file, const QJsonObject& project, const QList<track*>& tracks)
{
    QJsonArray trackArray;
    for(int i = 0; i < tracks.size(); ++i) {
        trackArray.append(tracks[i]->additions);
    }
    QJsonObject root;
    root["needs"] = QJsonArray();   // nothing a reader must understand, yet
    root["project"] = project;
    root["tracks"] = trackArray;
    QByteArray document = QJsonDocument(root).toJson(QJsonDocument::Compact);

    int version = APPENDIX_VERSION;
    int length = document.size();
    file << "FVX";
    writeBytes(&file, (const char*)&version, sizeof(int));
    writeBytes(&file, (const char*)&length, sizeof(int));
    file.write(document.constData(), document.size());   // the text as it is, first byte first
    file << "EOX";
}

enum appendixState { APPENDIX_NONE, APPENDIX_READ, APPENDIX_DROPPED, APPENDIX_REFUSED };

struct appendixResult
{
    appendixState state;
    QString reason;                 // DROPPED: why; REFUSED: the whole message
    QJsonObject project;            // READ: the project's additions
    QList<QJsonObject> tracks;      // READ: one per track

    appendixResult() : state(APPENDIX_NONE) {}
};

appendixResult dropped(const QString& reason)
{
    appendixResult r;
    r.state = APPENDIX_DROPPED;
    r.reason = reason;
    return r;
}

// An int at the given place, read as readInt() reads one from the file.
int intAt(const std::string& bytes, size_t at)
{
    std::stringstream s(bytes.substr(at, 4), ios::in | ios::binary);
    return readInt(&s);
}

// Everything after "EOP", checked from the first byte to the last. Any fault
// drops the whole appendix (the tracks are already read and stay); only an
// unknown name under "needs" refuses the load.
appendixResult readAppendix(std::iostream& file, int trackCount)
{
    std::string rest;
    char buffer[4096];
    while(true) {
        file.read(buffer, sizeof(buffer));
        std::streamsize got = file.gcount();
        if(got > 0) rest.append(buffer, (size_t)got);
        if(!file) break;
    }
    if(rest.empty()) return appendixResult();   // no appendix: a project as before

    const qint64 size = (qint64)rest.size();
    const size_t head = std::min(rest.size(), (size_t)3);
    if(rest.compare(0, head, std::string("FVX"), 0, head) != 0) {
        return dropped(QString("%1 bytes after the end of the project are not an appendix").arg(size));
    }
    if(size < 3) return dropped(QString("the appendix is cut off inside its tag FVX"));
    if(size < 7) return dropped(QString("the appendix is cut off inside its version number"));
    int version = intAt(rest, 3);
    if(version > APPENDIX_VERSION) {
        return dropped(QString("they were saved by a newer version of FVD++, appendix version %1, and this version reads version %2")
                       .arg(version).arg(APPENDIX_VERSION));
    }
    if(version < 1) return dropped(QString("the appendix version %1 is not a valid version").arg(version));
    if(size < 11) return dropped(QString("the appendix is cut off inside its length"));
    qint64 length = intAt(rest, 7);
    if(length < 0) return dropped(QString("the appendix gives a negative length (%1)").arg(length));
    if(size < 11 + length) {
        return dropped(QString("the appendix is cut off inside its document (its length says %1 bytes, the file holds %2)")
                       .arg(length).arg(size - 11));
    }
    if(size == 11 + length) return dropped(QString("the appendix has no end mark EOX"));
    if(size < 11 + length + 3) return dropped(QString("the appendix is cut off inside its end mark EOX"));
    if(rest.compare((size_t)(11 + length), 3, "EOX") != 0) {
        return dropped(QString("the appendix's end mark EOX is not where its length (%1 bytes) says").arg(length));
    }
    if(size > 11 + length + 3) {
        return dropped(QString("%1 bytes follow the appendix's end mark EOX").arg(size - (11 + length + 3)));
    }

    QByteArray document(rest.data() + 11, (int)length);
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(document, &parseError);
    if(doc.isNull()) {
        return dropped(QString("the appendix's document is not valid JSON (%1 at byte %2)")
                       .arg(parseError.errorString()).arg(parseError.offset));
    }
    if(!doc.isObject()) return dropped(QString("the appendix's document is not a JSON object"));
    QJsonObject root = doc.object();

    // "needs" first: a project that needs something this program does not
    // understand is refused whatever else is wrong with it.
    if(!root.value("needs").isArray()) {
        return dropped(QString("the appendix's document has no list \"needs\""));
    }
    QJsonArray needs = root.value("needs").toArray();
    QStringList known = knownAppendixNeeds();
    QStringList unknown;
    for(int i = 0; i < needs.size(); ++i) {
        if(!needs[i].isString()) return dropped(QString("the appendix's \"needs\" holds something that is not a name"));
        if(!known.contains(needs[i].toString())) unknown.append(QString("\"%1\"").arg(needs[i].toString()));
    }
    if(!unknown.isEmpty()) {
        appendixResult r;
        r.state = APPENDIX_REFUSED;
        r.reason = QString("Error: This project needs %1, which this version of FVD++ does not understand. "
                           "It was saved by a newer version; open it there.").arg(unknown.join(", "));
        return r;
    }

    QStringList keys = root.keys();
    for(int i = 0; i < keys.size(); ++i) {
        if(keys[i] != "needs" && keys[i] != "project" && keys[i] != "tracks") {
            return dropped(QString("the appendix's document has the unknown key \"%1\" at its top").arg(keys[i]));
        }
    }
    if(!root.value("project").isObject()) {
        return dropped(QString("the appendix's document has no object \"project\""));
    }
    if(!root.value("tracks").isArray()) {
        return dropped(QString("the appendix's document has no list \"tracks\""));
    }
    QJsonArray trackArray = root.value("tracks").toArray();
    if(trackArray.size() != trackCount) {
        return dropped(QString("the appendix has additions for %1 track(s), the project has %2")
                       .arg(trackArray.size()).arg(trackCount));
    }

    appendixResult r;
    r.state = APPENDIX_READ;
    r.project = root.value("project").toObject();
    for(int i = 0; i < trackArray.size(); ++i) {
        if(!trackArray[i].isObject()) {
            return dropped(QString("the appendix's entry for track %1 is not an object").arg(i+1));
        }
        r.tracks.append(trackArray[i].toObject());
    }
    return r;
}

} // namespace

// Was projectWidget::saveProject(); the section names typed into the track
// panel are copied into the track by the client, just before it is written.
QString writeProject(std::iostream& file, const QString& texPath, const QList<track*>& tracks, projectFileClient* client)
{
    file << "FVD";
    file << "v0.77";

    int namelength = texPath.toUtf8().length(); // bytes, as written below
    std::string stdName = texPath.toStdString();

    writeBytes(&file, (const char*)&namelength, sizeof(int));
    file << stdName;


    for(int i = 0; i < tracks.size(); ++i) {
        if(client) client->projectTrackSaving(tracks[i]);
        tracks[i]->saveTrack(file);
    }

    file << "EOP";

    QJsonObject projectAdditions = client ? client->projectAdditions() : QJsonObject();
    if(!nothingToAdd(projectAdditions, tracks)) {
        writeAppendix(file, projectAdditions, tracks);
    }
    return QString("Project Saved!");
}

// Was projectWidget::loadProject(); what it did to the windows is done by the
// client, at the same points.
QString readProject(std::iostream& file, projectFileClient* client)
{
    int errType = -1;
    std::string temp = readString(&file, 3);
    if(temp != "FVD") return QString("Error while Loading: No FVD File!");
    temp = readString(&file, 5);
    int legacy;
    if(temp == "v0.30") {
        legacy = 1;
    } else if(temp == "v0.77") {
        legacy = 0;
    } else {
        legacy = -1;
    }

    client->projectCleared();
    QList<track*> loaded;       // told projectTrackLoaded() once the appendix is read
    bool sawEnd = false;
    if(legacy > -1) { // supported versions
        int namelength = readInt(&file);
        QString texPath = QString(readString(&file, namelength).c_str());

        if(!client->projectGroundTexture(texPath)) { // error while Loading
            errType = 1;
        }

        while(1) {
            temp = readString(&file, 3);
            if(temp == "TRC") {
                track* newTrack = client->projectNewTrack();
                if(legacy == 1) {
                    newTrack->legacyLoadTrack(file);
                    errType = 0;
                } else {
                    newTrack->loadTrack(file);

                    // Roll smoothing is switched on, its entries brought up to
                    // date and applied, as the smoothing dialog made for the
                    // loaded track used to do.
                    newTrack->bAutoSmooth = true;
                    char customChar;
                    newTrack->updateSmoothEntries(&customChar);
                    client->projectTrackSmoothingUpdated(newTrack);
                    newTrack->applyRollSmooth();
                }
                loaded.append(newTrack);
            }
            else if(temp == "EOP") {
                sawEnd = true;
                break;
            } else {
                errType = 11;
                break;
            }
        }
    } else {
        errType = 10;
    }

    // The appendix, when the file reached its end mark: each track's
    // additions go to it before the client hears that it was loaded.
    appendixResult appendix;
    if(sawEnd) appendix = readAppendix(file, loaded.size());
    if(appendix.state == APPENDIX_READ) {
        for(int i = 0; i < loaded.size(); ++i) {
            loaded[i]->additions = appendix.tracks[i];
        }
    }
    client->projectAdditionsRead(appendix.state == APPENDIX_READ ? appendix.project : QJsonObject());
    if(appendix.state == APPENDIX_DROPPED) {
        client->projectAdditionsDropped(appendix.reason);
    }
    for(int i = 0; i < loaded.size(); ++i) {
        client->projectTrackLoaded(loaded[i]);
    }

    client->projectLoadFinished();

    if(appendix.state == APPENDIX_REFUSED) {
        client->projectCleared();
        return appendix.reason;
    }
    QString droppedText;
    if(appendix.state == APPENDIX_DROPPED) {
        droppedText = QString("The additions saved after the end of the project could not be read: %1. "
                              "The tracks are loaded without them. The original file is not written over, "
                              "so that it keeps its additions: save the project under another name.")
                      .arg(appendix.reason);
        qWarning("%s", qPrintable(droppedText));
    }

    switch(errType) {
    case -1:
        if(!droppedText.isEmpty()) return QString("Warning: ") + droppedText;
        return QString("Load Successfull.");
    case 0:
        return QString("Warning: Loaded old File Version. Please save to convert to new File version!")
                + (droppedText.isEmpty() ? QString() : QString(" ") + droppedText);
    case 1:
        return QString("Warning: Could Not find corresponding texture. Loaded default texture instead.")
                + (droppedText.isEmpty() ? QString() : QString(" ") + droppedText);
    case 10:
        client->projectCleared();
        return QString("Error: Unsupported File Version!");
    case 11:
        client->projectCleared();
        return QString("Error: File Corrupted!");
    }
    return QString();
}

projectFileAdditions readProjectFileAdditions(const QString& fileName)
{
    projectFileAdditions out;
    fstream file(fileName.toLocal8Bit().data(), ios::in | ios::binary);
    if(!file) {
        out.problem = QString("the file %1 could not be opened").arg(fileName);
        return out;
    }
    if(readString(&file, 3) != "FVD") {
        out.problem = QString("the file is not an FVD project");
        return out;
    }
    std::string version = readString(&file, 5);
    if(version == "v0.30") return out;   // the old version has no appendix (none is ever written to it)
    if(version != "v0.77") {
        out.problem = QString("the project's version is not known");
        return out;
    }
    int namelength = readInt(&file);
    readString(&file, namelength);
    // the tracks, read but not computed, only to find where the project ends
    while(file) {
        std::string tag = readString(&file, 3);
        if(tag == "TRC") {
            out.trackStarts.append((qint64)file.tellg());
            track scratch(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);   // as a reader's client makes one
            scratch.loadTrackHeader(file);
            QString error = scratch.loadTrackSections(file, false);
            if(!error.isEmpty() || readString(&file, 3) != "EOT") {
                out.problem = QString("track %1 of the project could not be read").arg(out.trackStarts.size());
                break;
            }
        } else if(tag == "EOP") {
            out.projectEnd = (qint64)file.tellg() - 3;
            break;
        } else {
            out.problem = QString("the project is damaged");
            break;
        }
    }
    if(out.projectEnd < 0 && out.problem.isEmpty()) out.problem = QString("the project has no end mark EOP");
    if(out.problem.isEmpty()) {
        appendixResult appendix = readAppendix(file, out.trackStarts.size());
        if(appendix.state == APPENDIX_READ) {
            out.tracks = appendix.tracks;
            out.project = appendix.project;
        } else if(appendix.state != APPENDIX_NONE) {
            // damaged, or a "needs" this program does not understand: the
            // tracks come without additions
            out.problem = appendix.reason;
            if(out.problem.startsWith("Error: ")) out.problem.remove(0, 7);
        }
    }
    while(out.tracks.size() < out.trackStarts.size()) out.tracks.append(QJsonObject());
    file.close();
    return out;
}

// Was saver::doSave().
QString saveProjectFile(const QString& fileName, const QString& texPath, const QList<track*>& tracks, projectFileClient* client)
{
    fstream fout(fileName.toLocal8Bit().data(), ios::out | ios::binary);
    if(!fout) {
        return QString("Error: File is NULL");
    }

    QString temp = writeProject(fout, texPath, tracks, client);

    fout.close();
    return temp;
}

// Was saver::doLoad().
QString loadProjectFile(const QString& fileName, projectFileClient* client)
{
    fstream fin(fileName.toLocal8Bit().data(), ios::in | ios::binary);
    if(!fin) {
        return QString("Error: File is NULL");
    }

    QString temp = readProject(fin, client);

    fin.close();
    return temp;
}
