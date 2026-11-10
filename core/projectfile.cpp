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

using namespace std;

// Was projectWidget::saveProject(); the section names typed into the track
// panel are copied into the track by the client, just before it is written.
QString writeProject(std::fstream& file, const QString& texPath, const QList<track*>& tracks, projectFileClient* client)
{
    file << "FVD";
    file << "v0.77";

    int namelength = texPath.length();
    std::string stdName = texPath.toStdString();

    writeBytes(&file, (const char*)&namelength, sizeof(int));
    file << stdName;


    for(int i = 0; i < tracks.size(); ++i) {
        if(client) client->projectTrackSaving(tracks[i]);
        tracks[i]->saveTrack(file);
    }

    file << "EOP";
    return QString("Project Saved!");
}

// Was projectWidget::loadProject(); what it did to the windows is done by the
// client, at the same points.
QString readProject(std::fstream& file, projectFileClient* client)
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
                client->projectTrackLoaded(newTrack);
            }
            else if(temp == "EOP") {
                break;
            } else {
                errType = 11;
                break;
            }
        }
    } else {
        errType = 10;
    }

    client->projectLoadFinished();

    switch(errType) {
    case -1:
        return QString("Load Successfull.");
    case 0:
        return QString("Warning: Loaded old File Version. Please save to convert to new File version!");
    case 1:
        return QString("Warning: Could Not find corresponding texture. Loaded default texture instead.");
    case 10:
        client->projectCleared();
        return QString("Error: Unsupported File Version!");
    case 11:
        client->projectCleared();
        return QString("Error: File Corrupted!");
    }
    return QString();
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
