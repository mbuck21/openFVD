/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
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

#include "nolimitsimporter.h"
#include <fstream>
#include "exportfuncs.h"
#include "secbezier.h"
#include "secnlcsv.h"
#include "track.h"
#include <QFile>

using namespace std;

noLimitsImporter::noLimitsImporter(track* _track, QString _fileName)
{
    inTrack = _track;
    fileName = _fileName;
}

bool noLimitsImporter::fail(const QString& text)
{
    mError = text;
    return false;
}

// The NoLimits 1 reader trusts the counts in the file: a vertex count larger
// than the file, a segment block before the vertices or a tube that names a
// vertex that is not there makes it read past the end of a list or loop for
// ever. This walks the file as the reader does (a tag can be found at any byte)
// and checks every count against the bytes that are left and every index
// against the list it picks from, before the track is touched. A good file
// passes unchanged.
bool noLimitsImporter::checkNlTrack(const QByteArray& data)
{
    const int length = data.size();
    const unsigned char* d = (const unsigned char*)data.constData();
    #define NL_INT(at) ((int)(((unsigned)d[(at)] << 24) | ((unsigned)d[(at)+1] << 16) | ((unsigned)d[(at)+2] << 8) | (unsigned)d[(at)+3]))
    qint64 bezTotal = 0, fundTotal = 0, freeTotal = 0;
    int lineSize = 0;
    bool sawBezier = false;

    for(int i = 0; i + 4 <= length; ++i)
    {
        if(d[i] != 'S' && d[i] != 'B' && d[i] != 'F' && d[i] != 'T') continue;
        QByteArray tag = data.mid(i, 4);
        if(tag == "SEGM")
        {
            if(i + 29 > length) return fail("the file ends inside a segment block (SEGM)");
            qint64 segCount = NL_INT(i+8);
            if(segCount < 0 || segCount > bezTotal) {
                return fail(QString("the segment block (SEGM) holds %1 segments but only %2 vertices were read before it").arg(segCount).arg(bezTotal));
            }
            qint64 pos = i + 29;
            for(qint64 s = 0; s < segCount; ++s)
            {
                if(pos + 33 > length) return fail("the file ends inside a segment block (SEGM)");
                int type = NL_INT(pos);
                qint64 extra = 0;
                if(type == 0) extra = 4;
                else if(type == 1) extra = 54 + 4 + 72;
                else if(type == 2) extra = 8 + 4 + 28;
                else if(type == 3) extra = 4 + 4 + 24;
                else if(type == 4) extra = 4 + 4 + 46;
                pos += 33 + extra;
                if(pos > length) return fail("the file ends inside a segment block (SEGM)");
            }
        }
        else if(tag == "BEZR" || tag == "FUND" || tag == "FREN" || tag == "TUBE")
        {
            if(i + 28 > length) return fail(QString("the file ends inside a ") + QString(tag) + " block");
            qint64 count = NL_INT(i+24);
            qint64 record = tag == "BEZR" ? 60 : (tag == "FUND" ? 92 : (tag == "FREN" ? 28 : 48));
            if(count < 0 || (qint64)i + 28 + count*record > length) {
                return fail(QString("the %1 block says it holds %2 entries, which is more than the file has bytes for").arg(QString(tag)).arg(count));
            }
            if(tag == "BEZR") { bezTotal += count; sawBezier = true; }
            else if(tag == "FUND") fundTotal += count;
            else if(tag == "FREN") freeTotal += count;
            else
            {
                qint64 pos = i + 28;
                for(qint64 b = 0; b < count; ++b, pos += 48)
                {
                    int type = NL_INT(pos), index = NL_INT(pos+8);
                    if(type != 3)
                    {
                        if((type != 1 && type != 2) || index < 0 || index >= (type == 1 ? fundTotal : freeTotal))
                            return fail(QString("a tube names a support point (type %1, number %2) that the file does not have").arg(type).arg(index));
                        ++lineSize;
                    }
                    type = NL_INT(pos+16);
                    index = NL_INT(pos+24);
                    if(type != 3 && lineSize%2)
                    {
                        if((type != 1 && type != 2) || index < 0 || index >= (type == 1 ? fundTotal : freeTotal))
                            return fail(QString("a tube names a support point (type %1, number %2) that the file does not have").arg(type).arg(index));
                        ++lineSize;
                    }
                    else if(type == 3 && lineSize%2)
                    {
                        --lineSize;
                    }
                }
            }
        }
    }
    #undef NL_INT
    if(!sawBezier || bezTotal < 2) {
        return fail("the file holds fewer than two track vertices (no usable BEZR block)");
    }
    return true;
}

bool noLimitsImporter::importAsNlTrack()
{
    QFile source(fileName);
    if(!source.open(QIODevice::ReadOnly))
    {
        return fail(QString("cannot read ") + fileName);
    }
    QByteArray bytes = source.readAll();
    source.close();
    if(!checkNlTrack(bytes))
    {
        mError = fileName + ": " + mError;
        return false;
    }

    fstream fin(fileName.toLocal8Bit().data(), ios::in | ios::binary);
    if(!fin)
    {
        return fail(QString("cannot read ") + fileName);
    }

    inTrack->fHeart = 0.f;
    inTrack->appendSectionFromFile(bezier);

    fin.seekg (0, ios::end);

    int length = fin.tellg();
    string temp;
    QList<bezier_t*> *bList = &inTrack->activeSection->bezList;

    QList<glm::vec3> *lineList = &inTrack->activeSection->supList;

    QList<glm::vec3> TubeNodes[3];

    int bezCount, fundCount, freeCount, tubeCount;
    glm::vec3 anchor;
    bool closeTrack = false;

    for(int i = 0; i < length; ++i)
    {
        fin.seekg(i);
        temp = readString(&fin, 4);
        if(temp == "SEGM")
        {
            readNulls(&fin, 4);
            int segCount = readInt(&fin);
            closeTrack = readBool(&fin);
            readNulls(&fin, 16);
            for(int i = 0; i < segCount; ++i)
            {
                int type = readInt(&fin);
                readNulls(&fin, 29);
                if(type == 0)
                {
                    readNulls(&fin, 4);
                    bList->at(i)->fVel = 0.f;
                }
                else if(type == 1)   // station
                {
                    readNulls(&fin, 54);
                    bList->at(i)->fVel = readFloat(&fin);
                    readNulls(&fin, 72);
                }
                else if(type == 2) // lift
                {
                    readNulls(&fin, 8);
                    bList->at(i)->fVel = readFloat(&fin);
                    readNulls(&fin, 28);
                }
                else if(type == 3) // transport
                {
                    readNulls(&fin, 4);
                    bList->at(i)->fVel = readFloat(&fin);
                    readNulls(&fin, 24);
                }
                else if(type == 4) // brake
                {
                    readNulls(&fin, 4);
                    bList->at(i)->fVel = readFloat(&fin);
                    readNulls(&fin, 46);
                }
                else
                {
                    qWarning("something wrong importing NL Track");
                }
                if(bList->at(i)->fVel != bList->at(i)->fVel)
                {
                    qWarning("read nan while importing NL Track");
                }

            }
        }
        if(temp == "BEZR")
        {
            readInt(&fin);  // size
            readNulls(&fin, 16);
            bezCount = readInt(&fin);
            for(int b = 0; b < bezCount; ++b)
            {
                bList->append(new bezier_t);
                bList->at(b)->P1.x = readFloat(&fin);
                bList->at(b)->P1.y = readFloat(&fin);
                bList->at(b)->P1.z = readFloat(&fin);
                bList->at(b)->Kp1.x = readFloat(&fin);
                bList->at(b)->Kp1.y = readFloat(&fin);
                bList->at(b)->Kp1.z = readFloat(&fin);
                bList->at(b)->Kp2.x = readFloat(&fin);
                bList->at(b)->Kp2.y = readFloat(&fin);
                bList->at(b)->Kp2.z = readFloat(&fin);
                bList->at(b)->roll = readFloat(&fin);
                bList->at(b)->contRoll = readBool(&fin);
                bList->at(b)->equalDist = readBool(&fin);
                bList->at(b)->relRoll = readBool(&fin);
                readNulls(&fin, 17);
                if(b == 0) anchor = bList->at(b)->P1;
                bList->at(b)->P1 -= anchor;
                bList->at(b)->Kp1 -= anchor;
                bList->at(b)->Kp2 -= anchor;

                bList->at(b)->ptf = 0.f;
                bList->at(b)->fvdRoll = 0.f;
                bList->at(b)->fVel = 0.f;
            }
        }
        if(temp == "FUND")
        {
            readInt(&fin);  //size
            readNulls(&fin, 16);
            fundCount = readInt(&fin);
            for(int b = 0; b < fundCount; ++b)
            {
                glm::vec3 temp;
                /*for(int i = 0; i < 4; ++i)
                {
                    temp.x = readFloat(&fin);
                }*/
                readNulls(&fin, 16);
                temp.x = readFloat(&fin);
                temp.y = readFloat(&fin);
                temp.z = readFloat(&fin);
                readNulls(&fin, 64);
                TubeNodes[0].append(temp);
                /*for(int i = 0; i < 12; ++i)
                {
                    temp.x = readFloat(&fin);
                }*/
            }
        }
        if(temp == "FREN")
        {
            readInt(&fin);  // size
            readNulls(&fin, 16);
            freeCount = readInt(&fin);
            for(int b = 0; b < freeCount; ++b)
            {
                glm::vec3 temp;
                temp.x = readFloat(&fin);
                temp.y = readFloat(&fin);
                temp.z = readFloat(&fin);
                TubeNodes[1].append(temp);
                readNulls(&fin, 16);
            }
        }
        if(temp == "TUBE")
        {
            int type, index;
            readInt(&fin);  // size
            readNulls(&fin, 16);
            tubeCount = readInt(&fin);
            for(int b = 0; b < tubeCount; ++b)
            {
                type = readInt(&fin);
                readInt(&fin);  // segment
                index = readInt(&fin);
                if(type != 3)
                {
                    lineList->append(TubeNodes[type-1][index] - anchor);
                }
                readNulls(&fin, 4);
                type = readInt(&fin);
                readInt(&fin);  // segment
                index = readInt(&fin);
                if(type != 3 && lineList->size()%2)
                {
                    lineList->append(TubeNodes[type-1][index] - anchor);
                }
                else if(type == 3 && lineList->size()%2)
                {
                    lineList->removeLast();
                }
                readNulls(&fin, 20);
            }
        }
    }

    if(closeTrack)
    {
        bList->append(new bezier_t);
        bList->last()->P1 = bList->at(0)->P1;
        bList->last()->Kp1 = bList->at(0)->Kp1;
        bList->last()->Kp2 = bList->at(0)->Kp2;
        bList->last()->roll = bList->at(0)->roll;
        bList->last()->contRoll = bList->at(0)->contRoll;
        bList->last()->equalDist = bList->at(0)->equalDist;
        bList->last()->relRoll = bList->at(0)->relRoll;
        bList->last()->ptf = 0.f;
        bList->last()->fvdRoll = 0.f;
    }

    inTrack->updateTrack(0, 0);
    fin.close();
    return true;
}

bool noLimitsImporter::importAsTxt()
{
    fstream fin(fileName.toLocal8Bit().data(), ios::in);
    if(!fin)
    {
        return fail(QString("cannot read ") + fileName);
    }

    inTrack->fHeart = 0.f;
    inTrack->appendSectionFromFile(bezier);

    fin.seekg (0, ios::end);

    QList<bezier_t*> *bList = &inTrack->activeSection->bezList;

    fin.seekg(0);

    float x, y, z;
    int b = 0;

    while(!fin.eof())
    {
        fin >> z;
        fin >> x;
        fin >> y;
        if(fin.fail() && !fin.eof())
        {
            // not a number: the loop would never reach the end of the file
            return fail(fileName + ": the point list holds something that is not a number (each line is three numbers: z x y)");
        }
        bList->append(new bezier_t);
        bList->at(b)->P1.x = x;
        bList->at(b)->P1.y = y;
        bList->at(b)->P1.z = z;
        b++;
    }

    if(bList->size() < 2)
    {
        return fail(fileName + ": the point list needs at least two points (each line is three numbers: z x y)");
    }

    glm::vec3 anchor = bList->at(0)->P1;
    bList->at(0)->Kp2 = 2.f/3.f * bList->at(0)->P1 + 1.f/3.f * bList->at(1)->P1;
    bList->at(0)->Kp1 = 2.f*bList->at(0)->P1 - bList->at(0)->Kp2;

    bList->last()->Kp1 = 2.f/3.f * bList->last()->P1 + 1.f/3.f * bList->at(bList->size()-2)->P1;
    bList->last()->Kp2 = 2.f*bList->last()->P1 - bList->last()->Kp1;

    for(b = 1; b < bList->size()-1; ++b)
    {
        bList->at(b)->Kp1 = 2.f/3.f * bList->at(b)->P1 + 1.f/3.f * bList->at(b-1)->P1;
        bList->at(b)->Kp2 = 2.f/3.f * bList->at(b)->P1 + 1.f/3.f * bList->at(b+1)->P1;
        bList->at(b)->roll = 0.f;
        bList->at(b)->contRoll = true;
        bList->at(b)->equalDist = true;
        bList->at(b)->relRoll = false;
        bList->at(b)->ptf = 0.f;
        bList->at(b)->fvdRoll = 0.f;
        bList->at(b)->fVel = 0.f;
    }

    for(b = 0; b < bList->size(); ++b)
    {
        //bList->at(b)->P1 = 0.5f*(bList->at(b)->Kp1 + bList->at(b)->Kp2);
        bList->at(b)->P1 -= anchor;
        bList->at(b)->Kp1 -= anchor;
        bList->at(b)->Kp2 -= anchor;
    }

    inTrack->updateTrack(0, 0);
    fin.close();
    return true;
}

// The NoLimits 2 CSV: the model steps the window's import entry used to do
// itself (heartline 0, a NoLimits CSV section, the file loaded into it).
bool noLimitsImporter::importAsCsv()
{
    // checked first, so that a refused file leaves the track as it was
    QList<mnode> nodes;
    QString problem;
    if(!secnlcsv::readNodes(fileName, nodes, &problem))
    {
        return fail(problem);
    }

    inTrack->fHeart = 0.f;
    section* added = inTrack->appendSectionFromFile(nolimitscsv);
    secnlcsv* csv = dynamic_cast<secnlcsv*>(added);
    if(!csv)
    {
        return fail("could not make a NoLimits 2 CSV section");
    }
    if(!csv->loadTrack(fileName, &problem))
    {
        return fail(problem);
    }
    return true;
}
