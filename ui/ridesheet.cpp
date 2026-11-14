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

#include "ridesheet.h"

#include "mainwindow.h"
#include "optionsmenu.h"
#include "trackhandler.h"
#include "track.h"
#include "ridesummary.h"

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QRegExp>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

extern MainWindow* gloParent;

static const int REFRESH_DELAY_MS = 150;

// Fixed decimals; a value that rounds to zero loses its minus sign (as `fvdcore summary` does).
static QString num(double v, int decimals)
{
    QString text = QString::number(v, 'f', decimals);
    if(text.startsWith('-') && text.indexOf(QRegExp("[1-9]")) < 0) text.remove(0, 1);
    return text;
}

// The tool tip of a value that has a place.
static QString placeTip(const rideSpot& s, double lengthFactor, const QString& lengthUnit)
{
    return QObject::tr("at %1 s, %2 %3 along the track, section %4")
            .arg(num(s.time_s, 1)).arg(num(s.dist_m * lengthFactor, 1)).arg(lengthUnit).arg(s.section);
}

rideSheet::rideSheet(QWidget* parent) :
    QWidget(parent),
    mTrack(NULL),
    bStale(true)
{
    setObjectName("rideSheet");
    setMinimumWidth(150);

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setObjectName("rideSheetScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    QWidget* inner = new QWidget();
    scroll->setWidget(inner);
    QVBoxLayout* column = new QVBoxLayout(inner);
    column->setContentsMargins(6, 6, 6, 6);

    mName = new QLabel(inner);
    mName->setObjectName("rideNameValue");
    QFont bold = mName->font();
    bold.setBold(true);
    mName->setFont(bold);
    mName->setWordWrap(true);
    column->addWidget(mName);

    mNonFinite = new QLabel(inner);
    mNonFinite->setObjectName("rideNonFiniteLabel");
    mNonFinite->setWordWrap(true);
    mNonFinite->setStyleSheet("color: #b00000; font-weight: bold;");
    mNonFinite->setText(tr("This track has values that are not numbers (a stalled train). The numbers below skip them."));
    mNonFinite->hide();
    column->addWidget(mNonFinite);

    mGrid = new QGridLayout();
    mGrid->setContentsMargins(0, 0, 0, 0);
    mGrid->setHorizontalSpacing(8);
    mGrid->setVerticalSpacing(3);
    mGrid->setColumnStretch(0, 5);
    mGrid->setColumnStretch(1, 4);
    column->addLayout(mGrid);

    // the labels that carry a threshold are set from the options on every refresh
    addRow("Length", tr("Length"));
    addRow("Highest", tr("Highest point"));
    addRow("Lowest", tr("Lowest point"));
    addRow("HeightDiff", tr("Height difference"));
    addRow("Drop", tr("Biggest drop"));
    addRow("Climb", tr("Biggest climb"));
    addRow("Duration", tr("Duration"));
    addRow("TopSpeed", tr("Top speed"));
    addRow("LowSpeed", tr("Lowest speed"));
    addRow("Floater", tr("Floater airtime"));
    addRow("Ejector", tr("Ejector airtime"));
    addRow("Airtime", tr("Total airtime"));
    addRow("NormalMax", tr("Vertical force, highest"));
    addRow("NormalMin", tr("Vertical force, lowest"));
    addRow("LateralLeft", tr("Lateral, rider pressed left"));
    addRow("LateralRight", tr("Lateral, rider pressed right"));
    addRow("AlongMax", tr("Along track, pressed back"));
    addRow("AlongMin", tr("Along track, thrown forward"));
    addRow("Inversions", tr("Inversions"));
    addRow("Tilt", tr("Largest angle from upright"));
    addRow("ForceLimits", tr("Force limits (ASTM F2291, public numbers)"));
    addRow("Findings", tr("Findings"));

    mFindingList = new QLabel(inner);
    mFindingList->setObjectName("rideFindingsList");
    mFindingList->setWordWrap(true);
    QFont small = mFindingList->font();
    small.setPointSizeF(small.pointSizeF() > 1 ? small.pointSizeF() - 1 : 8);
    mFindingList->setFont(small);
    mFindingList->hide();
    column->addWidget(mFindingList);

    mNote = new QLabel(inner);
    mNote->setObjectName("rideNoteLabel");
    mNote->setWordWrap(true);
    mNote->setFont(small);
    mNote->setStyleSheet("color: #606060;");
    mNote->setText(rideSheetNote());
    column->addWidget(mNote);

    mLimitsNote = new QLabel(inner);
    mLimitsNote->setObjectName("rideForceLimitsNoteLabel");
    mLimitsNote->setWordWrap(true);
    mLimitsNote->setFont(small);
    mLimitsNote->setStyleSheet("color: #606060;");
    mLimitsNote->setText(rideForceLimitsNote(designLimitsInfo()));
    column->addWidget(mLimitsNote);
    column->addStretch(1);

    mTimer.setSingleShot(true);
    mTimer.setInterval(REFRESH_DELAY_MS);
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(timerFired()));

    showDashes();
}

void rideSheet::addRow(const QString& key, const QString& labelText)
{
    row r;
    r.key = key;
    r.label = new QLabel(labelText);
    r.label->setObjectName("ride" + key + "Label");
    r.label->setWordWrap(true);
    r.label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    r.value = new QLabel("-");
    r.value->setObjectName("ride" + key + "Value");
    r.value->setWordWrap(true);
    r.value->setAlignment(Qt::AlignRight | Qt::AlignTop);
    int at = lRows.size();
    mGrid->addWidget(r.label, at, 0);
    mGrid->addWidget(r.value, at, 1);
    lRows.append(r);
}

void rideSheet::setValue(const QString& key, const QString& text, const QString& tip)
{
    for(int i = 0; i < lRows.size(); ++i) {
        if(lRows[i].key == key) {
            lRows[i].value->setText(text);
            lRows[i].value->setToolTip(tip);
            lRows[i].label->setToolTip(tip);
            return;
        }
    }
}

void rideSheet::setLabel(const QString& key, const QString& text)
{
    for(int i = 0; i < lRows.size(); ++i) {
        if(lRows[i].key == key) {
            lRows[i].label->setText(text);
            return;
        }
    }
}

void rideSheet::showDashes()
{
    mName->setText("-");
    mNonFinite->hide();
    mFindingList->hide();
    for(int i = 0; i < lRows.size(); ++i) {
        lRows[i].value->setText("-");
        lRows[i].value->setToolTip(QString());
        lRows[i].label->setToolTip(QString());
    }
}

void rideSheet::setTrack(track* t)
{
    mTrack = t;
    markStale();
}

void rideSheet::trackChanged(track* t)
{
    if(t != NULL && t == mTrack) {
        markStale();
    }
}

void rideSheet::markStale()
{
    bStale = true;
    mTimer.start();     // restarts a running timer
}

void rideSheet::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if(bStale) {
        mTimer.start();
    }
}

void rideSheet::timerFired()
{
    if(!isVisible() || !bStale) {
        return;     // shown again later: showEvent() starts the timer
    }
    refreshNow();
}

// The track pointer is only used if it is still one of the project's tracks.
bool rideSheet::trackIsOpen() const
{
    if(mTrack == NULL || gloParent == NULL) {
        return false;
    }
    QList<trackHandler*> list = gloParent->getTrackList();
    for(int i = 0; i < list.size(); ++i) {
        if(list[i]->trackData == mTrack) {
            return true;
        }
    }
    return false;
}

void rideSheet::refreshNow()
{
    mTimer.stop();
    bStale = false;
    if(!trackIsOpen()) {
        mTrack = NULL;
        showDashes();
        return;
    }

    // units: read on every refresh, nothing pushes a change of the option
    double lf = 1.0, sf = 1.0;
    QString lu = "m", su = "m/s";
    if(gloParent->mOptions != NULL) {
        lf = gloParent->mOptions->getLengthFactor();
        sf = gloParent->mOptions->getSpeedFactor();
        lu = gloParent->mOptions->getLengthString().trimmed();
        su = gloParent->mOptions->getSpeedString().trimmed();
    }

    rideSummaryOptions options;
    rideSummary r = computeRideSummary(mTrack, options);

    mName->setText(mTrack->name);
    setLabel("Floater", rideFloaterLabel(options));
    setLabel("Ejector", rideEjectorLabel(options));
    setLabel("Inversions", rideInversionLabel(options));
    setLabel("ForceLimits", r.forceLimits.label());
    mLimitsNote->setText(rideForceLimitsNote(r.forceLimits));
    mNonFinite->setVisible(r.nonFinite);

    if(r.empty) {
        for(int i = 0; i < lRows.size(); ++i) {
            lRows[i].value->setText("-");
            lRows[i].value->setToolTip(QString());
            lRows[i].label->setToolTip(QString());
        }
        mFindingList->hide();
        return;
    }

    // a value with its place: "31.2 m/s at 42.0 s"; the tip adds the distance and section
    #define SPOT(key, spot, factor, unit, decimals) \
        if((spot).valid) setValue(key, num((spot).value * (factor), decimals) + QChar(0xa0) + (unit) + tr(" at\302\240%1\302\240s").arg(num((spot).time_s, 1)), placeTip(spot, lf, lu)); \
        else setValue(key, tr("none"));

    setValue("Length", num(r.length_m * lf, 1) + " " + lu, tr("Heartline: %1 %2").arg(num(r.heartLength_m * lf, 1)).arg(lu));
    SPOT("Highest", r.highest, lf, lu, 1)
    SPOT("Lowest", r.lowest, lf, lu, 1)
    setValue("HeightDiff", num(r.heightRange_m * lf, 1) + " " + lu);

    if(r.biggestDrop.valid) {
        setValue("Drop", num(r.biggestDrop.amount_m * lf, 1) + QChar(0xa0) + lu + tr(" at\302\240%1\302\240s").arg(num(r.biggestDrop.start.time_s, 1)),
                 tr("From height %1 %3 (%2 s) down to %4 %3 (%5 s)")
                 .arg(num(r.biggestDrop.start.value * lf, 1)).arg(num(r.biggestDrop.start.time_s, 1)).arg(lu)
                 .arg(num(r.biggestDrop.end.value * lf, 1)).arg(num(r.biggestDrop.end.time_s, 1)));
    } else {
        setValue("Drop", tr("none"));
    }
    if(r.biggestClimb.valid) {
        setValue("Climb", num(r.biggestClimb.amount_m * lf, 1) + QChar(0xa0) + lu + tr(" at\302\240%1\302\240s").arg(num(r.biggestClimb.start.time_s, 1)),
                 tr("From height %1 %3 (%2 s) up to %4 %3 (%5 s)")
                 .arg(num(r.biggestClimb.start.value * lf, 1)).arg(num(r.biggestClimb.start.time_s, 1)).arg(lu)
                 .arg(num(r.biggestClimb.end.value * lf, 1)).arg(num(r.biggestClimb.end.time_s, 1)));
    } else {
        setValue("Climb", tr("none"));
    }

    setValue("Duration", num(r.duration_s, 2) + " s");
    SPOT("TopSpeed", r.topSpeed, sf, su, 1)
    SPOT("LowSpeed", r.lowSpeed, sf, su, 1)

    setValue("Floater", num(r.floaterAirtime_s, 2) + " s");
    setValue("Ejector", num(r.ejectorAirtime_s, 2) + " s");
    setValue("Airtime", num(r.totalAirtime_s, 2) + " s");

    SPOT("NormalMax", r.normalMax, 1.0, QString("g"), 2)
    SPOT("NormalMin", r.normalMin, 1.0, QString("g"), 2)
    SPOT("LateralLeft", r.lateralLeft, 1.0, QString("g"), 2)
    SPOT("LateralRight", r.lateralRight, 1.0, QString("g"), 2)
    SPOT("AlongMax", r.alongMax, 1.0, QString("g"), 2)
    SPOT("AlongMin", r.alongMin, 1.0, QString("g"), 2)
    #undef SPOT

    setValue("Inversions", QString::number(r.inversions));
    if(r.maxTilt.valid) {
        setValue("Tilt", num(r.maxTilt.value, 1) + QString(QChar(0xb0)) + tr(" at\302\240%1\302\240s").arg(num(r.maxTilt.time_s, 1)), placeTip(r.maxTilt, lf, lu));
    } else {
        setValue("Tilt", tr("none"));
    }

    setValue("ForceLimits", r.forceLimits.value(), r.forceLimits.sConfidence.isEmpty() ? QString() : tr("The numbers behind these findings are of %1 confidence at the lowest").arg(r.forceLimits.sConfidence));

    int problems = r.findingCount[findingProblem];
    int warnings = r.findingCount[findingWarning];
    int infos = r.findingCount[findingInformation];
    QStringList sentences;
    for(int i = 0; i < r.findings.size(); ++i) {
        sentences.append(r.findings.at(i).text);
    }
    setValue("Findings", tr("%1\302\240problems, %2\302\240warnings, %3\302\240information").arg(problems).arg(warnings).arg(infos), sentences.join("\n"));
    if(sentences.isEmpty()) {
        mFindingList->hide();
    } else {
        const int SHOWN = 4;
        QStringList shown = sentences.mid(0, SHOWN);
        if(sentences.size() > SHOWN) {
            shown.append(tr("... and %1 more (hover over Findings)").arg(sentences.size() - SHOWN));
        }
        mFindingList->setText(shown.join("\n"));
        mFindingList->show();
    }
}

QStringList rideSheet::textLines() const
{
    QStringList lines;
    lines.append("rideNameValue | " + mName->text());
    if(!mNonFinite->isHidden()) {
        lines.append("rideNonFiniteLabel | " + mNonFinite->text());
    }
    for(int i = 0; i < lRows.size(); ++i) {
        lines.append(lRows[i].value->objectName() + " | " + lRows[i].label->text() + " | " + QString(lRows[i].value->text()).replace(QChar(0xa0), QChar(0x20)));
    }
    if(!mFindingList->isHidden()) {
        lines.append("rideFindingsList | " + QString(mFindingList->text()).replace("\n", " / "));
    }
    lines.append("rideNoteLabel | " + mNote->text());
    lines.append("rideForceLimitsNoteLabel | " + mLimitsNote->text());
    return lines;
}
