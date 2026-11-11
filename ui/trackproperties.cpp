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
#include "undoaction.h"
#include "undohandler.h"
#include "trackmesh.h"
#include "traintype.h"

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
    curTrack->trackData->display.colors[0] = colorPicker->selectedColor();
    QPalette palette = ui->defaultColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[0]);
    ui->defaultColorButton->setPalette(palette);
    curTrack->trackData->hasChanged = true;
    curTrack->mUndoHandler->takeOutsideChange();
}

void TrackProperties::on_sectionColorButton_released()
{
    colorPicker->setCurrentColor(curTrack->trackData->display.colors[1]);
    colorPicker->open(this, SLOT(onsectionColor_received()));
}

void TrackProperties::onsectionColor_received()
{
    curTrack->trackData->display.colors[1] = colorPicker->selectedColor();
    QPalette palette = ui->sectionColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[1]);
    ui->sectionColorButton->setPalette(palette);
    curTrack->trackData->hasChanged = true;
    curTrack->mUndoHandler->takeOutsideChange();
}

void TrackProperties::on_transitionColorButton_released()
{
    colorPicker->setCurrentColor(curTrack->trackData->display.colors[2]);
    colorPicker->open(this, SLOT(ontransitionColor_received()));
}

void TrackProperties::ontransitionColor_received()
{
    curTrack->trackData->display.colors[2] = colorPicker->selectedColor();
    QPalette palette = ui->transitionColorButton->palette();
    palette.setColor(QPalette::ButtonText, curTrack->trackData->display.colors[2]);
    ui->transitionColorButton->setPalette(palette);
    curTrack->trackData->hasChanged = true;
    curTrack->mUndoHandler->takeOutsideChange();
}

void TrackProperties::on_drawBox_currentIndexChanged(int index)
{
    curTrack->trackData->drawHeartline = index;
    curTrack->mUndoHandler->takeOutsideChange();
}

void TrackProperties::on_styleBox_currentIndexChanged(int)
{
}

void TrackProperties::on_wireframeBox_stateChanged(int)
{
}

void TrackProperties::on_buttonBox_accepted()
{
    if(curTrack->trackData->fFriction != ui->frictionBox->value()) {
        curTrack->trackData->fFriction = ui->frictionBox->value();
        curTrack->trackData->updateTrack(0, 0);

        if(!curTrack->mUndoHandler->busy) {
            undoAction* temp = new undoAction(curTrack, changeTrackFriction);
            curTrack->mUndoHandler->addAction(temp);
            gloParent->setUndoButtons();
        }
    }

    if(curTrack->trackData->fResistance != ui->resistanceBox->value()*1e-5) {
        curTrack->trackData->fResistance = ui->resistanceBox->value()*1e-5;
        curTrack->trackData->updateTrack(0, 0);

        if(!curTrack->mUndoHandler->busy) {
            undoAction* temp = new undoAction(curTrack, changeTrackResistance);
            curTrack->mUndoHandler->addAction(temp);
            gloParent->setUndoButtons();
        }
    }


    if(curTrack->trackData->fHeart != ui->heartlineBox->value()) {
        mnode* anchor = curTrack->trackData->anchorNode;
        glm::vec3 temp = anchor->vPosHeart(curTrack->trackData->fHeart);
        float roll = anchor->fRoll*F_PI/180;
        float pitch = anchor->getPitch()*F_PI/180;
        float yaw = curTrack->trackData->startYaw*F_PI/180;
        float x = -cos(yaw)*temp.z+sin(yaw)*temp.x+curTrack->trackData->startPos.x;
        float y = temp.y+curTrack->trackData->startPos.y;
        float z = cos(yaw)*temp.x+sin(yaw)*temp.z+curTrack->trackData->startPos.z;

        undoAction *tempx = NULL, *tempy = NULL, *tempz = NULL;
        if(!curTrack->mUndoHandler->busy) {
            tempx = new undoAction(curTrack, changeAnchorPosX);

            tempy = new undoAction(curTrack, changeAnchorPosY);
            tempy->nextAction = tempx;

            tempz = new undoAction(curTrack, changeAnchorPosZ);
            tempz->nextAction = tempy;
        }

        curTrack->trackData->fHeart = ui->heartlineBox->value();

        curTrack->trackData->startPos.x = -(cos(yaw)*sin(pitch)*cos(roll)+sin(yaw)*sin(roll))*curTrack->trackData->fHeart + x;
        curTrack->trackData->startPos.y = (cos(pitch)*cos(roll))*curTrack->trackData->fHeart + y;
        curTrack->trackData->startPos.z = (sin(yaw)*sin(pitch)*cos(roll)-cos(yaw)*sin(roll))*curTrack->trackData->fHeart + z;

        curTrack->trackData->updateTrack(0, 0);
        if(!curTrack->mUndoHandler->busy) {
            undoAction* temp = new undoAction(curTrack, changeTrackHeartline);
            temp->nextAction = tempz;
            curTrack->mUndoHandler->addAction(temp);
            gloParent->setUndoButtons();
        }
    }


    if(curTrack->trackData->style != (enum trackStyle)ui->styleBox->currentIndex()) {
        curTrack->trackData->style = (enum trackStyle)ui->styleBox->currentIndex();
        curTrack->mMesh->buildMeshes(0);
        curTrack->trackData->hasChanged = true;
    }

    if(curTrack->trackData->display.bWireframe != ui->wireframeBox->isChecked()) {
        curTrack->trackData->display.bWireframe = ui->wireframeBox->isChecked();
        curTrack->mMesh->buildMeshes(0);
        curTrack->trackData->hasChanged = true;
    }

    // style and wireframe are not undo steps
    curTrack->mUndoHandler->takeOutsideChange();

}

void TrackProperties::on_buttonBox_rejected()
{
    return;
}

void TrackProperties::openForTrack(trackHandler* _curTrack)
{
    curTrack = _curTrack;
    fillBoxes();
    this->show();
}

// The track was put back to another state (an undo or redo): an open dialog
// shows the values it has now, so OK does not write the old ones back.
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
}
