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

// Checks data/traintypes.json and the loader in core/traintype.*.
// Prints one line per check and a last line PASS or FAIL; exit code 0 or 1.

#include "../../core/traintype.h"
#include "../../core/trainspec.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>

static int failures = 0;

static void check(const QString& what, bool ok, const QString& detail = QString())
{
    printf("%s  %s%s\n", ok ? "ok  " : "FAIL", qPrintable(what),
           (!ok && !detail.isEmpty()) ? qPrintable("  (" + detail + ")") : "");
    if (!ok)
        ++failures;
}

struct ownerRow { const char* name; double meters; };

// The owner's table (docs/feature-requests.md, FR-1), Meters column.
static const ownerRow OWNER_TABLE[] = {
        {"4D Coaster", 0.95},
        {"Classic Steel Looping Coaster", 1.1},
        {"Classic Steel Looping Coaster (Modern)", 1.1},
        {"Corkscrew Coaster", 0.65},
        {"Gerstlauer Bobsled Coaster", 1.1},
        {"Gerstlauer Euro-Fighter", 1.1},
        {"Gerstlauer Euro-Fighter 2", 1.1},
        {"Gerstlauer Spinning Coaster", 1.1},
        {"Gravity Group Timberliner", 1.0},
        {"Hyper Coaster", 0.9},
        {"Hyper Coaster (4 Seats Across)", 1.1},
        {"Hyper Coaster (4 Seats Across with Scoops)", 1.1},
        {"Hyper Coaster (4 Seats Staggered)", 1.1},
        {"Inverted Coaster (2 Seats Across)", -0.9},
        {"Inverted Coaster (4 Seats Across)", -1.1},
        {"Inverted Face-to-Face Coaster", -0.9},
        {"Inverted Impulse Coaster", -1.16},
        {"LIM Launched Coaster", 1.0},
        {"Mack Launched Coaster", 1.5},
        {"Mauer Söhne Spinning Coaster", 1.15},
        {"Mauer Söhne X-Car Coaster", 1.295},
        {"Rocket Coaster", 1.1},
        {"Suspended Coaster", -1.7},
        {"Twisted Dive Coaster", 1.8},
        {"Twisted Floorless Coaster", 1.3},
        {"Twisted Flying Coaster", -1.1},
        {"Twisted Sitdown Coaster", 1.2},
        {"Twisted Stand-Up Coaster", 1.6},
        {"Twisted Wing Coaster", 0.5},
        {"Vekoma Flying Dutchman", 0.8},
        {"Vekoma Minetrain Coaster", 1.2},
        {"Vekoma Minetrain Coaster with Locomotive", 1.2},
        {"Vekoma Motorbike Coaster", 1.1},
        {"Wooden Coaster (Classic 4-Seat)", 1.0},
        {"Wooden Coaster (Classic 6-Seat)", 1.0},
        {"Wooden Coaster (Trailered 2-Seat)", 1.0},
        {"Wooden Coaster (Trailered 4-Seat)", 1.0},
        {"Zamperla Twister Coaster", 1.1},
};
static const int OWNER_COUNT = (int)(sizeof(OWNER_TABLE) / sizeof(OWNER_TABLE[0]));

static const char* GOOD_ENTRY =
    "{\"id\":\"%1\",\"name\":\"X\",\"heartline\":{\"value\":1.0,\"source\":\"s\",\"confidence\":\"%2\"}}";

static QString writeDoc(const QTemporaryDir& dir, const QString& file, const QString& entries, int schema = 1)
{
    const QString path = dir.filePath(file);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
    {
        f.write(QString("{\"schemaVersion\":%2,\"trains\":[%1]}").arg(entries).arg(schema).toUtf8());
        f.close();
    }
    return path;
}

static void checkRejected(const QString& what, const QString& path, const QString& mustMention)
{
    QString error;
    const QList<trainType> list = trainCatalogue::load(path, &error);
    check(what + " is rejected", list.isEmpty() && !error.isEmpty());
    check(what + " message names the problem", error.contains(mustMention), error);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // The real file.
    QString error;
    if (argc < 2)
    {
        printf("usage: traintypes <repository folder>\nFAIL\n");
        return 1;
    }
    const QString jsonPath = QDir(QString::fromLocal8Bit(argv[1])).filePath("data/traintypes.json");
    const QList<trainType> trains = trainCatalogue::load(jsonPath, &error);
    check("data/traintypes.json loads with no error", !trains.isEmpty() && error.isEmpty(), error);

    check("it holds at least the 38 seeded entries", trains.size() >= OWNER_COUNT && OWNER_COUNT == 38,
          QString("%1 entries").arg(trains.size()));

    QMap<QString, const trainType*> byNl2Name;
    for (int i = 0; i < trains.size(); ++i)
        if (!byNl2Name.contains(trains.at(i).sNl2Name)) // the seeded 38 come first; real-model rows may reuse a name
            byNl2Name.insert(trains.at(i).sNl2Name, &trains.at(i));

    int missing = 0, wrongValue = 0;
    QStringList problems;
    for (int i = 0; i < OWNER_COUNT; ++i)
    {
        const QString name = QString::fromUtf8(OWNER_TABLE[i].name);
        const trainType* t = byNl2Name.value(name, nullptr);
        if (!t)
        {
            ++missing;
            problems << name + " missing";
        }
        else if (std::fabs(t->heartline.fValue - OWNER_TABLE[i].meters) > 1e-4)
        {
            ++wrongValue;
            problems << QString("%1: %2 instead of %3").arg(name).arg(t->heartline.fValue).arg(OWNER_TABLE[i].meters);
        }
    }
    check("every owner's-table name is present by nl2Name", missing == 0, problems.join("; "));
    check("every heartline equals the owner's metre value", wrongValue == 0, problems.join("; "));

    QStringList ids;
    bool idsUnique = true;
    for (int i = 0; i < trains.size(); ++i)
    {
        if (ids.contains(trains.at(i).sId))
            idsUnique = false;
        ids << trains.at(i).sId;
    }
    check("ids are unique", idsUnique);

    bool heartlinesOk = true, confidenceOk = true, lookupOk = !trains.isEmpty();
    for (int i = 0; i < trains.size(); ++i)
    {
        const sourcedValue& h = trains.at(i).heartline;
        if (!h.bPresent || !std::isfinite(h.fValue) || h.fValue < -3.0f || h.fValue > 3.0f)
            heartlinesOk = false;
        const QString& c = h.sConfidence;
        if (c != "verified" && c != "likely" && c != "estimate" && c != "unverified")
            confidenceOk = false;
        if (trainCatalogue::findById(trains, trains.at(i).sId) != &trains.at(i))
            lookupOk = false;
    }
    check("every heartline is finite and between -3 and 3 m", heartlinesOk);
    check("every confidence is one of the four words", confidenceOk);
    check("findById finds every entry", lookupOk);
    check("findById returns nothing for an unknown id", trainCatalogue::findById(trains, "no-such-train") == nullptr);

    // Broken documents.
    QTemporaryDir dir;
    check("temporary directory created", dir.isValid());
    if (dir.isValid())
    {
        const QString dup = QString(GOOD_ENTRY).arg("same", "verified") + "," + QString(GOOD_ENTRY).arg("same", "verified");
        checkRejected("a duplicate id", writeDoc(dir, "dup.json", dup), "same");

        checkRejected("a bad confidence word", writeDoc(dir, "conf.json", QString(GOOD_ENTRY).arg("bad-conf", "certain")), "confidence");

        checkRejected("a missing heartline",
                      writeDoc(dir, "nohl.json", "{\"id\":\"no-heartline\",\"name\":\"X\"}"), "heartline");

        QString good;
        check("a small valid document loads",
              trainCatalogue::load(writeDoc(dir, "ok.json", QString(GOOD_ENTRY).arg("fine", "likely")), &good).size() == 1
              && good.isEmpty(), good);
    }

    // Schema 2: car pitch, row and seat offsets, friction and drag per type.
    {
        QFile f(jsonPath);
        QJsonObject root;
        if (f.open(QIODevice::ReadOnly))
            root = QJsonDocument::fromJson(f.readAll()).object();
        check("data/traintypes.json is schema 2", root.value("schemaVersion").toDouble() == 2.0);
        const QJsonArray arr = root.value("trains").toArray();
        bool allKeys = !arr.isEmpty();
        for (int i = 0; i < arr.size(); ++i)
        {
            const QJsonObject o = arr.at(i).toObject();
            const char* keys[] = { "carPitch", "rowOffsets", "seatOffsets", "friction", "drag" };
            for (int k = 0; k < 5; ++k)
                if (!o.contains(keys[k]))
                    allKeys = false;
        }
        check("every entry has carPitch, rowOffsets, seatOffsets, friction and drag (null until Stage 4)", allKeys);
        bool noneFilled = true;
        for (int i = 0; i < trains.size(); ++i)
        {
            const trainType& t = trains.at(i);
            if (t.carPitch.bPresent || t.rowOffsets.bPresent || t.seatOffsets.bPresent || t.friction.bPresent || t.drag.bPresent)
                noneFilled = false;
        }
        check("none of the five is filled in yet (Stage 4 brings the numbers)", noneFilled);
    }
    if (dir.isValid())
    {
        const QString src = "\"source\":\"s\",\"confidence\":\"estimate\"";
        const QString full = QString("{\"id\":\"full\",\"name\":\"Full\",\"rowsPerCar\":2,\"seatsAcross\":4,"
                                     "\"heartline\":{\"value\":1.1,%1},\"carPitch\":{\"value\":2.35,%1},"
                                     "\"rowOffsets\":{\"value\":[0.4,-0.45],%1},\"seatOffsets\":{\"value\":[-0.9,-0.3,0.3,0.9],%1},"
                                     "\"friction\":{\"value\":0.026,%1},\"drag\":{\"value\":0.0001,%1},"
                                     "\"carLength\":{\"value\":3.0,%1}}").arg(src);
        QString why;
        const QList<trainType> two = trainCatalogue::load(writeDoc(dir, "two.json", full, 2), &why);
        check("a schema 2 document with every new field loads", two.size() == 1 && why.isEmpty(), why);
        if (two.size() == 1)
        {
            const trainType& t = two.at(0);
            check("its new fields are read", t.carPitch.bPresent && t.carPitch.fValue == 2.35f
                  && t.rowOffsets.lValues == (QList<float>() << 0.4f << -0.45f)
                  && t.seatOffsets.lValues.size() == 4 && t.seatOffsets.sConfidence == "estimate"
                  && t.friction.fValue == 0.026f && t.drag.fValue == 0.0001f);
            QStringList assumed;
            const trainSpec spec = trainSpec::fromType(t, &assumed);
            check("fromType() takes the car pitch before the car length", spec.fCarPitch == 2.35f);
            check("fromType() takes the row and seat offsets as given", spec.lRowOffsets == t.rowOffsets.lValues
                  && spec.lSeatOffsets == t.seatOffsets.lValues);
            check("fromType() takes friction and drag, so not the project's", !spec.resistance.bFromProject
                  && spec.resistance.fRolling == 0.026f && spec.resistance.fDrag == 0.0001f);
            check("and does not call them assumed", !assumed.contains("fCarPitch") && !assumed.contains("lRowOffsets")
                  && !assumed.contains("lSeatOffsets") && !assumed.contains("rowCount") && !assumed.contains("seatCount"),
                  assumed.join(", "));
        }
        // null fields (the shipped file's state): the stand-ins as before
        const QString nulls = QString("{\"id\":\"nulls\",\"name\":\"N\",\"heartline\":{\"value\":1.0,%1},"
                                      "\"carPitch\":null,\"rowOffsets\":null,\"seatOffsets\":null,\"friction\":null,\"drag\":null}").arg(src);
        const QList<trainType> withNulls = trainCatalogue::load(writeDoc(dir, "nulls.json", nulls, 2), &why);
        check("a schema 2 document with the new fields null loads", withNulls.size() == 1 && why.isEmpty(), why);
        const QList<trainType> old = trainCatalogue::load(writeDoc(dir, "old.json", QString(GOOD_ENTRY).arg("old", "likely"), 1), &why);
        check("the old file shape (schema 1, without the new fields) still loads", old.size() == 1 && why.isEmpty(), why);
        if (withNulls.size() == 1 && old.size() == 1)
        {
            QStringList a1, a2;
            trainSpec s1 = trainSpec::fromType(withNulls.at(0), &a1);
            trainSpec s2 = trainSpec::fromType(old.at(0), &a2);
            s1.sTypeId = s2.sTypeId;
            check("a type with the new fields null gives the same train and stand-ins as the old shape", s1 == s2 && a1 == a2
                  && s1.resistance.bFromProject && a1.contains("fCarPitch") && a1.contains("lRowOffsets"));
        }
        checkRejected("schema 3", writeDoc(dir, "three.json", QString(GOOD_ENTRY).arg("x", "likely"), 3), "schemaVersion");
        const QString wrongRows = QString(full).replace("[0.4,-0.45]", "[0.4,0,-0.45]");
        checkRejected("row offsets that disagree with rowsPerCar", writeDoc(dir, "rows.json", wrongRows, 2), "rowOffsets");
        const QString badList = QString(full).replace("[-0.9,-0.3,0.3,0.9]", "[-0.9,\"a\",0.3,0.9]");
        checkRejected("a seat offset that is not a number", writeDoc(dir, "seats.json", badList, 2), "seatOffsets");
        const QString badPitch = QString(full).replace("\"carPitch\":{\"value\":2.35", "\"carPitch\":{\"value\":0");
        checkRejected("a car pitch of 0", writeDoc(dir, "pitch.json", badPitch, 2), "carPitch");
    }

    check("a missing file is rejected with a message",
          trainCatalogue::load("this/file/does/not/exist.json", &error).isEmpty() && !error.isEmpty());

    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
