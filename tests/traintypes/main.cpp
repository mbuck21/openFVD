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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

static QString writeDoc(const QTemporaryDir& dir, const QString& file, const QString& entries)
{
    const QString path = dir.filePath(file);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
    {
        f.write(QString("{\"schemaVersion\":1,\"trains\":[%1]}").arg(entries).toUtf8());
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

    check("it holds exactly the 38 seeded entries", trains.size() == OWNER_COUNT && OWNER_COUNT == 38,
          QString("%1 entries").arg(trains.size()));

    QMap<QString, const trainType*> byNl2Name;
    for (int i = 0; i < trains.size(); ++i)
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

    check("a missing file is rejected with a message",
          trainCatalogue::load("this/file/does/not/exist.json", &error).isEmpty() && !error.isEmpty());

    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
