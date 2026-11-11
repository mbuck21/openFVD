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

#include "elementfile.h"
#include "track.h"
#include "exportfuncs.h"
#include <QByteArray>
#include <cstdio>
#include <fstream>

using namespace std;

// Was exportUi::doNL2Export() and batchRunner::exportNL2(). The file is
// opened in text mode, as it always was.
bool writeNL2Element(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex)
{
    QByteArray localFileName = fileName.toLocal8Bit();
    FILE* fout = fopen(localFileName.constData(), "w");
    if(fout == NULL) {
        return false;
    }

    fprintf(fout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fout, "<root>\n");
    fprintf(fout, "\t<element>\n");
    fprintf(fout, "\t\t<description>FVD++ Export Data</description>\n");

    tTrack->exportNL2Track(fout, fPerNode, fromIndex, toIndex);

    fprintf(fout, "\t</element>\n");
    fprintf(fout, "</root>\n");
    fclose(fout);
    return true;
}

// Was exportUi::doExport() and doExport2() and batchRunner::exportNL1(): a
// "MELE" header with two lengths filled in at the end, the track's export,
// 69 zero bytes.
static bool writeNL1Element(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex, float fRollThresh, bool bNoHeartline, bool bTangent)
{
    QByteArray localFileName = fileName.toLocal8Bit();
    fstream* fout = new fstream(localFileName.constData(), ios::out | ios::binary);
    if(!fout->is_open()) {
        delete fout;
        return false;
    }

    writeBytes(fout, (const char*)"MELE", 4);
    writeNulls(fout, 4); // will be replaced with length of data
    writeNulls(fout, 64);
    writeNulls(fout, 4); // will be replaced with No of NL beziers

    float oldHeartLine = tTrack->fHeart;
    if(bNoHeartline) {
        tTrack->fHeart = 0.f;
    }
    int iNodes;
    if(bTangent) {
        iNodes = tTrack->exportTrack4(fout, fPerNode, fromIndex, toIndex, fRollThresh);
    } else {
        iNodes = tTrack->exportTrack3(fout, fPerNode, fromIndex, toIndex, fRollThresh);
    }

    tTrack->fHeart = oldHeartLine;

    int iDataLength = iNodes*50+132;

    writeNulls(fout, 69);

    fout->seekp(4);
    writeBytes(fout, (const char*)&iDataLength, 4); // replaced with length of data
    fout->seekp(72);
    writeBytes(fout, (const char*)&iNodes, 4); // replaced with no of NL beziers

    bool ok = !fout->fail();
    fout->close();
    delete fout;
    return ok;
}

bool writeNL1TangentElement(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex, float fRollThresh, bool bNoHeartline)
{
    return writeNL1Element(fileName, tTrack, fPerNode, fromIndex, toIndex, fRollThresh, bNoHeartline, true);
}

bool writeNL1SplineElement(const QString& fileName, track* tTrack, float fPerNode, int fromIndex, int toIndex, float fRollThresh, bool bNoHeartline)
{
    return writeNL1Element(fileName, tTrack, fPerNode, fromIndex, toIndex, fRollThresh, bNoHeartline, false);
}
