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

#include "track.h"
#include "mainwindow.h"
#include "projectwidget.h"
#include "trackhandler.h"
#include "traintype.h"
#include "trackhistory.h"
#include "command.h"

#include "trackproperties.h"
#include "ui_trackproperties.h"
#include <QColorDialog>
#include <QAbstractItemView>
#include <algorithm>


extern MainWindow* gloParent;

TrackProperties::TrackProperties(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TrackProperties)
{
    ui->setupUi(this);
    curTrack = NULL;
    stepTrack = NULL;
    bFilling = false;
    fFilledFriction = fFilledResistance = fFilledHeart = 0.;
    // blocks the main window while open: the step open on the track's history
    // must not meet another edit of the track
    setWindowModality(Qt::WindowModal);
    colorPicker = new QColorDialog(this);
    colorPicker->setOption(QColorDialog::ShowAlphaChannel, false);
    bSettingHeartline = false;
    fillTrainList();
}

TrackProperties::~TrackProperties()
{
    delete colorPicker;
    delete ui;
}


void TrackProperties::on_frictionBox_valueChanged(double)
{
}

void TrackProperties::on_heartlineBox_valueChanged(double)
{
    // A number typed by hand no longer belongs to the train that was picked.
    if(!bSettingHeartline && ui->trainTypeBox->currentIndex() != 0)
        ui->trainTypeBox->setCurrentIndex(0);
}

// The heartline box of this dialog is always in metres (suffix " m", raw
// fHeart), whatever the "measures" option says, so the list is in metres too.
void TrackProperties::fillTrainList()
{
    const QList<trainType>& trains = trainCatalogue::defaultCatalogue();

    QList<const trainType*> usable;
    for(int i = 0; i < trains.size(); ++i) {
        if(trains[i].heartline.bPresent)
            usable.append(&trains[i]);
    }
    // By name: the real models carry their maker in the name ("Mack Hyper
    // Coaster"), so they group themselves; the NoLimits 2 styles sort between.
    std::sort(usable.begin(), usable.end(), [](const trainType* a, const trainType* b) {
        return a->sName.compare(b->sName, Qt::CaseInsensitive) < 0;
    });

    ui->trainTypeBox->blockSignals(true);
    int widest = 0;
    for(int i = 0; i < usable.size(); ++i) {
        const trainType* t = usable[i];
        // Two decimals, or three where the value has a third (1.295 m).
        QString number = QString::number(t->heartline.fValue, 'f', 3);
        if(number.endsWith('0'))
            number.chop(1);
        QString label = t->sName + QString("  (%1 m)").arg(number);
        ui->trainTypeBox->addItem(label, t->sId);
        widest = qMax(widest, ui->trainTypeBox->fontMetrics().horizontalAdvance(label));
    }
    ui->trainTypeBox->blockSignals(false);
    ui->trainTypeBox->view()->setMinimumWidth(widest + 40);
    ui->trainTypeBox->setEnabled(!usable.isEmpty());
    ui->trainSourceLabel->clear();
}

void TrackProperties::on_trainTypeBox_currentIndexChanged(int index)
{
    ui->trainSourceLabel->clear();
    if(index <= 0)
        return;

    const trainType* t = trainCatalogue::findById(trainCatalogue::defaultCatalogue(),
                                                  ui->trainTypeBox->itemData(index).toString());
    if(t == nullptr)
        return;

    bSettingHeartline = true;
    ui->heartlineBox->setValue(t->heartline.fValue);
    bSettingHeartline = false;

    QString conf = t->heartline.sConfidence;
    if(conf == "verified")
        conf = "Verified";
    else if(conf == "likely")
        conf = "Likely right";
    else if(conf == "estimate")
        conf = "An estimate";
    else
        conf = "Not verified";
    ui->trainSourceLabel->setText("Source: " + t->heartline.sSource + ". " + conf + ".");
}

void TrackProperties::on_defaultColorButton_released()
{
    colorPicker->setCurrentColor(curTrack->trackData->display.colors[0]);
    colorPicker->open(this, SLOT(ondefaultColor_received()));
}

void TrackProperties::ondefaultColor_received()
{
    if(!runCommand("track.setColor", QVariantMap{{"which", QString("default")}, {"color", colorPicker->selectedColor().name()}})) return;
    QPalette palette = ui->defaultColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[0]);
    ui->defaultColorButton->setPalette(palette);
}

void TrackProperties::on_sectionColorButton_released()
{
    colorPicker->setCurrentColor(curTrack->trackData->display.colors[1]);
    colorPicker->open(this, SLOT(onsectionColor_received()));
}

void TrackProperties::onsectionColor_received()
{
    if(!runCommand("track.setColor", QVariantMap{{"which", QString("section")}, {"color", colorPicker->selectedColor().name()}})) return;
    QPalette palette = ui->sectionColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[1]);
    ui->sectionColorButton->setPalette(palette);
}

void TrackProperties::on_transitionColorButton_released()
{
    colorPicker->setCurrentColor(curTrack->trackData->display.colors[2]);
    colorPicker->open(this, SLOT(ontransitionColor_received()));
}

void TrackProperties::ontransitionColor_received()
{
    if(!runCommand("track.setColor", QVariantMap{{"which", QString("transition")}, {"color", colorPicker->selectedColor().name()}})) return;
    QPalette palette = ui->transitionColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[2]);
    ui->transitionColorButton->setPalette(palette);
}

// The drawing mode shows at once (inside the dialog's step: Cancel takes it back).
void TrackProperties::on_drawBox_currentIndexChanged(int index)
{
    static const char* modes[] = { "everything", "trackOnly", "heartlineOnly" };
    if(bFilling || index < 0 || index > 2) return;
    if(curTrack->trackData->drawHeartline == index) return;
    runCommand("track.setHeartlineDrawing", QVariantMap{{"mode", QString(modes[index])}});
}

void TrackProperties::on_styleBox_currentIndexChanged(int)
{
}

void TrackProperties::on_wireframeBox_stateChanged(int)
{
}

// Runs a command on the track inside the dialog's step (it joins the step).
// False, with the reason in the log, when there is no step or the command
// fails; a failed command puts the track back as it was before it.
bool TrackProperties::runCommand(const QString& name, const QVariantMap& args)
{
    if(stepTrack == NULL) return false;
    QString error;
    if(!commandRegistry::run(stepTrack->mHistory, name, commandTarget::forTrack(stepTrack->getID()-1), args, &error)) {
        qWarning("Track Properties: %s", qPrintable(error));
        return false;
    }
    return true;
}

// OK: the boxes that differ from what they were filled with, each as a
// command, in the order the dialog always applied them. A box left alone
// changes nothing, so OK without a change records nothing (issues 31 and 41).
void TrackProperties::applyBoxes()
{
    if(ui->frictionBox->value() != fFilledFriction) {
        runCommand("track.setFriction", QVariantMap{{"friction", ui->frictionBox->value()}});
    }
    if(ui->resistanceBox->value() != fFilledResistance) {
        runCommand("track.setResistance", QVariantMap{{"resistance", ui->resistanceBox->value()*1e-5}});
    }
    if(ui->heartlineBox->value() != fFilledHeart) {
        runCommand("track.setHeartline", QVariantMap{{"height", ui->heartlineBox->value()}});
    }

    static const char* styles[] = { "generic", "genericflat", "vekoma", "bm", "triangle", "box", "smallflat", "doublespine" };
    int style = ui->styleBox->currentIndex();
    if(style >= 0 && style < 8 && curTrack->trackData->style != (enum trackStyle)style) {
        runCommand("track.setStyle", QVariantMap{{"style", QString(styles[style])}});
    }
    if(curTrack->trackData->display.bWireframe != ui->wireframeBox->isChecked()) {
        runCommand("track.setWireframe", QVariantMap{{"on", ui->wireframeBox->isChecked()}});
    }
}

// Every way the dialog closes comes here: OK (accept()), Cancel, Escape and
// the window's close button (reject()), and cancelFor(). The step is marked
// closed before it ends, so that the refill a rollback causes (trackReset())
// runs no command.
void TrackProperties::done(int r)
{
    if(stepTrack != NULL) {
        trackHandler* handler = stepTrack;
        if(r == QDialog::Accepted) {
            applyBoxes();
        }
        stepTrack = NULL;
        handler->mHistory->endStep(r == QDialog::Accepted);
        gloParent->setUndoButtons();
    }
    QDialog::done(r);
}

// Opens the dialog for a track and opens the step its changes go into. Open
// for another track already (not possible by hand: the dialog is modal), that
// one is cancelled first.
void TrackProperties::openForTrack(trackHandler* _curTrack)
{
    if(_curTrack == NULL) return;
    if(stepTrack != NULL) {
        if(stepTrack == _curTrack) {
            raise();
            activateWindow();
            return;
        }
        reject();
    }
    curTrack = _curTrack;
    fillBoxes();
    curTrack->mHistory->beginStep("Change track properties", commandTarget::forTrack(curTrack->getID()-1));
    stepTrack = curTrack;
    gloParent->setUndoButtons();   // Undo and Redo wait while the step is open
    this->show();
}

void TrackProperties::cancelFor(trackHandler* _track)
{
    if(stepTrack != NULL && stepTrack == _track) {
        reject();
    }
}

// The track was put back to another state (an undo or redo, or a command
// that failed inside the step): an open dialog shows the values it has now,
// so OK does not write the old ones back.
void TrackProperties::trackReset(trackHandler* _track)
{
    if(_track == curTrack && isVisible()) {
        fillBoxes();
    }
}

// Every box from the track, every time the dialog opens (not only for another
// track: an undo since it was last open may have changed the values).
void TrackProperties::fillBoxes()
{
    bFilling = true;
    {
        ui->frictionBox->setValue(curTrack->trackData->fFriction);
        ui->resistanceBox->setValue(curTrack->trackData->fResistance*1e5);

        QPalette palette = ui->defaultColorButton->palette();
        palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[0]);
        ui->defaultColorButton->setPalette(palette);
        palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[1]);
        ui->sectionColorButton->setPalette(palette);
        palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[2]);
        ui->transitionColorButton->setPalette(palette);

        ui->drawBox->setCurrentIndex(curTrack->trackData->drawHeartline);
        ui->styleBox->setCurrentIndex(curTrack->trackData->style);
        ui->wireframeBox->setChecked(curTrack->trackData->display.bWireframe);

    }
    // Every time the dialog opens, not only for another track: a train picked
    // and then cancelled must not still be in the box at the next OK.
    bSettingHeartline = true;
    ui->heartlineBox->setValue(curTrack->trackData->fHeart);
    bSettingHeartline = false;
    ui->trainTypeBox->setCurrentIndex(0);
    ui->trainSourceLabel->clear();

    // what the boxes show now, rounded to their decimals
    fFilledFriction = ui->frictionBox->value();
    fFilledResistance = ui->resistanceBox->value();
    fFilledHeart = ui->heartlineBox->value();
    bFilling = false;
}
