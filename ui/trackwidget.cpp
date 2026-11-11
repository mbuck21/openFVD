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

#include "trackwidget.h"
#include "graphwidget.h"
#include "ui_trackwidget.h"
#include "optionsmenu.h"
#include "undohandler.h"
#include "mainwindow.h"
#include "trackmesh.h"
#include <QMenu>
#include <QKeyEvent>
#include <QPushButton>

extern MainWindow* gloParent;

trackWidget::trackWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::trackWidget)
{
    phantomChanges = false;
    ui->setupUi(this);
    //track = new trackHandler("blubb", 255);

    ui->sectionListWidget->setItemDelegateForColumn(0, new NoEditDelegate(this));
    ui->sectionListWidget->setItemDelegateForColumn(2, new NoEditDelegate(this));

    ui->sectionListWidget->setColumnWidth(0, 30);
    ui->sectionListWidget->setColumnWidth(1, 140);
    ui->sectionListWidget->setColumnWidth(2, 120);
    selSection = NULL;

    //sectionHandler* newSec = new sectionHandler(track->trackData, anchor, sectionList.size());
    //sectionList.append(newSec);
    //ui->sectionListWidget->addTopLevelItem(newSec->listItem);

    ui->rLabel->setText(QChar(0x03a6));
    ui->pLabel->setText(QChar(0x0398));
    ui->jLabel->setText(QChar(0x03a8));

    smoothScreen = NULL;

    //ui->sectionListWidget->sizePolicy().setVerticalPolicy((QSizePolicy::Policy)15);//(QSizePolicy::GrowFlag | QSizePolicy::ShrinkFlag));

   // on_sectionListWidget_itemSelectionChanged();
}

trackWidget::trackWidget(QWidget *parent, trackHandler* _track) :
    QWidget(parent),
    ui(new Ui::trackWidget)
{
    phantomChanges = false;
    ui->setupUi(this);
    inTrack = _track;

    ui->sectionListWidget->setItemDelegateForColumn(0, new NoEditDelegate(this));
    ui->sectionListWidget->setItemDelegateForColumn(2, new NoEditDelegate(this));

    ui->sectionListWidget->setColumnWidth(0, 30);
    ui->sectionListWidget->setColumnWidth(1, 140);
    ui->sectionListWidget->setColumnWidth(2, 120);
    selSection = NULL;

    sectionHandler* newSec = new sectionHandler(inTrack->trackData, anchor, sectionList.size());
    sectionList.append(newSec);
    ui->sectionListWidget->addTopLevelItem(newSec->listItem);

    ui->rLabel->setText(QChar(0x03a6));
    ui->pLabel->setText(QChar(0x0398));
    ui->jLabel->setText(QChar(0x03a8));

    ui->pitchChangeLabel->setText(QString("d%1/dt").arg(QChar(0x0398)));
    ui->yawChangeLabel->setText(QString("d%1/dt").arg(QChar(0x03a8)));

    smoothScreen = NULL;
    inTrack->trackData->bAutoSmooth = smoothScreen != NULL;

    //connect(this, SIGNAL(done()), this, SLOT(update()));

    //ui->smoothButton->hide();

    on_sectionListWidget_itemSelectionChanged();
}

trackWidget::~trackWidget()
{
    inTrack->trackWidgetItem = NULL;
    inTrack->tabId = -1;
    delete ui->sectionListWidget->itemDelegateForColumn(0);
    delete ui->sectionListWidget->itemDelegateForColumn(2);
    delete ui;
}

void trackWidget::on_addButton_released()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction("Straight Section", this, SLOT(addStraightSec()));
    menu->addAction("Curved Section", this, SLOT(addCurvedSec()));
    menu->addAction("Force Section", this, SLOT(addForceSec()));
    menu->addAction("Geometric Section", this, SLOT(addGeometricSec()));

    menu->popup(this->cursor().pos());
}

void trackWidget::addStraightSec()
{
    /*undoAction* temp = new undoAction(inTrack, appendSegment);
    temp->sectionNumber++;
    gloParent->project->trackWorker->queueAction(temp);*/


    addSection(straight);
}

void trackWidget::addCurvedSec()
{
    addSection(curved);
}

void trackWidget::addForceSec()
{
    addSection(forced);
}

void trackWidget::addGeometricSec()
{
    addSection(geometric);
}

void trackWidget::addSection(secType _type)
{
    sectionHandler* newSec = new sectionHandler(inTrack->trackData, _type, sectionList.size());
    this->inTrack->trackData->updateTrack(newSec->sectionData, 0);

    showNewSection(newSec, _type, true);
}

// A section the track added itself (track::appendSectionFromFile(), told
// through trackHandler) is shown as addSection() shows the one it creates.
void trackWidget::showAddedSection(int index)
{
    section* added = inTrack->trackData->lSections[index];
    sectionHandler* newSec = new sectionHandler(added, sectionList.size());

    showNewSection(newSec, added->type, false);
}

// The panel's part of adding a section at the end: its row, selected; the
// graphs; an undo step when the person added it (not for a load or an import,
// whose caller starts the undo list afresh).
void trackWidget::showNewSection(sectionHandler* newSec, secType _type, bool record)
{
    sectionList.append(newSec);
    ui->sectionListWidget->addTopLevelItem(newSec->listItem);
    ui->sectionListWidget->clearSelection();
    newSec->listItem->setSelected(true);

    inTrack->graphWidgetItem->redrawGraphs();

    Q_UNUSED(_type);
    if(record && !inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, appendSegment);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::appendStraightSec()
{
    appendSection(straight);
}

void trackWidget::appendCurvedSec()
{
    appendSection(curved);
}

void trackWidget::appendForceSec()
{
    appendSection(forced);
}

void trackWidget::appendGeometricSec()
{
    appendSection(geometric);
}

void trackWidget::appendSection(secType _type)
{
    int index;
    if(!selSection) index = 1;
    else index =selSection->type == anchor ? 1 : inTrack->trackData->getSectionNumber(selSection->sectionData)+2;
    sectionHandler* newSec = new sectionHandler(inTrack->trackData, _type, index);
    sectionList.insert(index, newSec);
    ui->sectionListWidget->insertTopLevelItem(index, newSec->listItem);
    this->inTrack->trackData->updateTrack(newSec->sectionData, 0);
    this->inTrack->trackData->activeSection = newSec->sectionData;
    selSection = newSec;

    //emit done();

    inTrack->graphWidgetItem->redrawGraphs();
    updateSectionIDs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, appendSegment);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::updateSectionIDs()
{
    for(int i = 0; i < sectionList.size(); ++i) {
        sectionList[i]->updateID(i);
    }
}

void trackWidget::clearSelection()
{
    ui->sectionListWidget->clearSelection();
}

void trackWidget::setNames()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    for(int i = 1; i < sectionList.size(); ++i) {
        ui->sectionListWidget->topLevelItem(i)->setText(1, sectionList[i]->sectionData->sName);
    }
    phantomChanges = oldP;
}

void trackWidget::writeNames()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    for(int i = 1; i < sectionList.size(); ++i) {
        sectionList[i]->sectionData->sName = ui->sectionListWidget->topLevelItem(i)->text(1);
    }
    phantomChanges = oldP;
}

void trackWidget::on_sectionListWidget_itemSelectionChanged()
{
    if(!ui->sectionListWidget->selectedItems().size()) {
        selSection = NULL;
        inTrack->trackData->activeSection = NULL;
        inTrack->trackData->hasChanged = true;

        inTrack->graphWidgetItem->curSectionChanged(sectionList[0]);
        inTrack->graphWidgetItem->redrawGraphs();

        ui->deleteButton->setEnabled(false);

        ui->anchorFrame->hide();
        ui->straightFrame->hide();
        ui->curvedFrame->hide();
        ui->advancedFrame->hide();
        ui->sectionFrame->hide();
        ui->optionsFrame->hide();
    } else {
        QTreeWidgetItem* selected = ui->sectionListWidget->selectedItems().at(0);
        int index = getSection(selected);

        bool otherArgument = false;

        if(selSection && selSection->sectionData) {
            otherArgument = selSection->sectionData->bArgument;
        } else if(sectionList[index]->sectionData) {
            otherArgument = sectionList[index]->sectionData->bArgument;
        }

        selSection = sectionList[index];

        if(selSection->sectionData) {
            otherArgument = (otherArgument != selSection->sectionData->bArgument);
        } else {
            otherArgument = false;
        }

        switch(selSection->type) {
        case anchor:
            setupAnchorFrame();
            updateSectionFrame();
            ui->deleteButton->setEnabled(false);
            ui->anchorFrame->show();
            ui->straightFrame->hide();
            ui->curvedFrame->hide();
            ui->advancedFrame->hide();
            ui->sectionFrame->hide();
            ui->optionsFrame->hide();
            break;
        case straight:
            setupStraightFrame();
            updateSectionFrame();
            updateOptionsFrame();
            ui->deleteButton->setEnabled(true);
            ui->anchorFrame->hide();
            ui->straightFrame->show();
            ui->curvedFrame->hide();
            ui->advancedFrame->hide();
            ui->sectionFrame->show();
            ui->optionsFrame->show();
            break;
        case curved:
            setupCurvedFrame();
            updateSectionFrame();
            updateOptionsFrame();
            ui->deleteButton->setEnabled(true);
            ui->anchorFrame->hide();
            ui->straightFrame->hide();
            ui->curvedFrame->show();
            ui->advancedFrame->hide();
            ui->sectionFrame->show();
            ui->optionsFrame->show();
            break;
        case forced:
            setupAdvFrame();
            updateSectionFrame();
            updateOptionsFrame();
            ui->deleteButton->setEnabled(true);
            ui->anchorFrame->hide();
            ui->straightFrame->hide();
            ui->curvedFrame->hide();
            ui->advancedFrame->show();
            ui->sectionFrame->show();
            ui->optionsFrame->show();
            break;
        case geometric:
            setupAdvFrame();
            updateSectionFrame();
            updateOptionsFrame();
            ui->deleteButton->setEnabled(true);
            ui->anchorFrame->hide();
            ui->straightFrame->hide();
            ui->curvedFrame->hide();
            ui->advancedFrame->show();
            ui->sectionFrame->show();
            ui->optionsFrame->show();
            break;
        case bezier:
        case nolimitscsv:
            ui->deleteButton->setEnabled(true);
            ui->anchorFrame->hide();
            ui->straightFrame->hide();
            ui->curvedFrame->hide();
            ui->advancedFrame->hide();
            ui->sectionFrame->hide();
            ui->optionsFrame->hide();
            break;
        }

        inTrack->trackData->activeSection = selSection->sectionData;
        inTrack->trackData->hasChanged = true;
        gloParent->updateInfoPanel();
        gloParent->sectionChanged();
        inTrack->graphWidgetItem->curSectionChanged(sectionList[index]);

        inTrack->graphWidgetItem->redrawGraphs(otherArgument);
    }


    ui->scrollAreaWidgetContents->adjustSize();
    int height = ui->scrollAreaWidgetContents->height() + 5;
    int maxheight = this->height() - ui->addButton->height() - 100;

    ui->scrollArea->setMaximumHeight(height);
    ui->scrollArea->setMinimumHeight(maxheight > height ? height : maxheight);
    return;
}

int trackWidget::getSection(QTreeWidgetItem *item)
{
    for(int i = 0; i < sectionList.size(); ++i) {
        if(sectionList[i]->listItem == item) return i;
    }
    return -1;
}

void trackWidget::on_deleteButton_released()
{
    // the Delete key reaches here with nothing or the anchor selected
    if(ui->sectionListWidget->selectedItems().isEmpty()) return;
    QTreeWidgetItem* selected = ui->sectionListWidget->selectedItems().at(0);

    int index = getSection(selected);
    if(index < 1) return;

    // made now, for the number of the section it deletes; recorded once the
    // section is gone
    undoAction* temp = NULL;
    if(!inTrack->mUndoHandler->busy) {
        temp = new undoAction(inTrack, removeSegment);
    }

    inTrack->trackData->removeSection(index-1);
    delete sectionList[index];
    sectionList.removeAt(index);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();
    updateSectionIDs();
    on_sectionListWidget_itemSelectionChanged();
    inTrack->graphWidgetItem->selectionChanged();

    if(temp) {
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

// Before the track's model is replaced (trackHandler::trackAboutToReset()):
// no selection, and no rows for the sections, which are about to be deleted.
void trackWidget::dropSections()
{
    ui->sectionListWidget->clearSelection();
    selSection = NULL;
    inTrack->trackData->activeSection = NULL;

    bool oldP = phantomChanges;
    phantomChanges = true;
    while(sectionList.size() > 1) {
        delete sectionList.takeLast();
    }
    phantomChanges = oldP;
}

// After the model was replaced (trackHandler::trackReset()): one row per
// section, nothing selected, no undo step.
void trackWidget::showSections()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    for(int i = 0; i < inTrack->trackData->lSections.size(); ++i) {
        sectionHandler* row = new sectionHandler(inTrack->trackData->lSections[i], sectionList.size());
        sectionList.append(row);
        ui->sectionListWidget->addTopLevelItem(row->listItem);
    }
    updateSectionIDs();
    phantomChanges = oldP;
}

void trackWidget::setupAnchorFrame()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    mnode* anchor = inTrack->trackData->anchorNode;
    float yaw = inTrack->trackData->startYaw*F_PI/180;
    glm::vec3 temp = anchor->vPosHeart(inTrack->trackData->fHeart);

    ui->xBox->setValue( (-cos(yaw)*temp.z+sin(yaw)*temp.x+inTrack->trackData->startPos.x)  * gloParent->mOptions->getLengthFactor());
    ui->yBox->setValue((temp.y+inTrack->trackData->startPos.y) * gloParent->mOptions->getLengthFactor());
    ui->zBox->setValue((cos(yaw)*temp.x+sin(yaw)*temp.z+inTrack->trackData->startPos.z) * gloParent->mOptions->getLengthFactor());
    ui->rBox->setValue(anchor->fRoll);
    ui->pBox->setValue(anchor->getPitch());
    ui->jBox->setValue(inTrack->trackData->startYaw);
    ui->anchorSpeedBox->setValue(anchor->fVel*gloParent->mOptions->getSpeedFactor());
    ui->anchorSpeedBox->setSuffix(gloParent->mOptions->getSpeedString());
    ui->xBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->yBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->zBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->normalBox->setValue(anchor->forceNormal);
    ui->lateralBox->setValue(anchor->forceLateral);
    ui->pitchChangeBox->setValue(anchor->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(anchor->fYawFromLast*F_HZ);

    phantomChanges = oldP;
}

void trackWidget::setupStraightFrame()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->straightLengthBox->setValue(selSection->sectionData->fHLength*gloParent->mOptions->getLengthFactor());
    ui->straightLengthBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->straightSpeedBox->setEnabled(!selSection->sectionData->bSpeed);
    ui->straightSpeedCheck->setChecked(!selSection->sectionData->bSpeed);
    ui->straightSpeedBox->setValue(selSection->sectionData->getSpeed()*gloParent->mOptions->getSpeedFactor());
    ui->straightSpeedBox->setSuffix(gloParent->mOptions->getSpeedString());

    phantomChanges = oldP;
}

void trackWidget::setupCurvedFrame()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->curvedRadiusBox->setValue(selSection->sectionData->fRadius*gloParent->mOptions->getLengthFactor());
    ui->curvedRadiusBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->curvedAngleBox->setValue(selSection->sectionData->fAngle);
    ui->curvedDirectionBox->setValue(selSection->sectionData->fDirection);
    ui->curvedLeadInBox->setValue(selSection->sectionData->fLeadIn);
    ui->curvedLeadOutBox->setValue(selSection->sectionData->fLeadOut);
    ui->curvedSpeedBox->setEnabled(!selSection->sectionData->bSpeed);
    ui->curvedSpeedCheck->setChecked(!selSection->sectionData->bSpeed);
    ui->curvedSpeedBox->setValue(selSection->sectionData->getSpeed()*gloParent->mOptions->getSpeedFactor());
    ui->curvedSpeedBox->setSuffix(gloParent->mOptions->getSpeedString());

    phantomChanges = oldP;
}

void trackWidget::setupAdvFrame()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->advancedSpeedBox->setEnabled(!selSection->sectionData->bSpeed);
    ui->advancedSpeedCheck->setChecked(!selSection->sectionData->bSpeed);
    ui->advancedSpeedBox->setValue(selSection->sectionData->getSpeed()*gloParent->mOptions->getSpeedFactor());
    ui->advancedSpeedBox->setSuffix(gloParent->mOptions->getSpeedString());
    phantomChanges = oldP;
}

void trackWidget::updateSectionFrame()
{
    if(!selSection || selSection->type == anchor) return;
    ui->timeLabel->setText(QString().number((selSection->sectionData->lNodes.size()-1)/F_HZ, 'f', 3).append(" s"));
    ui->lengthLabel->setText(QString().number(selSection->sectionData->length*gloParent->mOptions->getLengthFactor(), 'f', 2).append(" ").append(gloParent->mOptions->getLengthString()));
}

void trackWidget::updateOptionsFrame()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->argumentBox->setCurrentIndex(selSection->sectionData->bArgument);
    ui->orientationBox->setCurrentIndex(selSection->sectionData->bOrientation);
    if(selSection->sectionData->type == straight) {
        ui->argumentBox->setEnabled(false);
        ui->orientationBox->setEnabled(false);
    } else if(selSection->sectionData->type == curved) {
        ui->argumentBox->setEnabled(false);
        ui->orientationBox->setEnabled(true);
    } else {
        ui->argumentBox->setEnabled(true);
        ui->orientationBox->setEnabled(true);
    }
    phantomChanges = oldP;
}

void trackWidget::on_xBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    float a = inTrack->trackData->startYaw * F_PI/180.f;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.x = -(cos(a)*sin(b)*cos(y)+sin(a)*sin(y))*inTrack->trackData->fHeart + arg1/gloParent->mOptions->getLengthFactor();

    gloParent->updateInfoPanel();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorPosX);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_yBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.y = (cos(b)*cos(y))*inTrack->trackData->fHeart + arg1/gloParent->mOptions->getLengthFactor();

    inTrack->mMesh->buildMeshes(0);

    inTrack->trackData->hasChanged = true;

    gloParent->updateInfoPanel();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorPosY);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_zBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    float a = inTrack->trackData->startYaw * F_PI/180.f;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.z = (sin(a)*sin(b)*cos(y)-cos(a)*sin(y))*inTrack->trackData->fHeart + arg1/gloParent->mOptions->getLengthFactor();

    gloParent->updateInfoPanel();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorPosZ);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_rBox_valueChanged(double arg1)
{
    int arg = arg1-0.0001;
    if(arg/180) {
        arg1 -= (float)((abs(arg/180)+1)/2)*360.f * (arg > 0 ? 1 : -1);
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->rBox->setValue(arg1);
        phantomChanges = oldP;
    }
    if(phantomChanges) return;
    inTrack->trackData->anchorNode->setRoll(arg1-inTrack->trackData->anchorNode->fRoll);

    undoAction *tempx = NULL, *tempy = NULL, *tempz = NULL;
    if(!inTrack->mUndoHandler->busy) {
        tempx = new undoAction(inTrack, changeAnchorPosX);

        tempy = new undoAction(inTrack, changeAnchorPosY);
        tempy->nextAction = tempx;

        tempz = new undoAction(inTrack, changeAnchorPosZ);
        tempz->nextAction = tempy;
    }

    float a = inTrack->trackData->startYaw * F_PI/180.f;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.x = -(cos(a)*sin(b)*cos(y)+sin(a)*sin(y))*inTrack->trackData->fHeart + ui->xBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.y = (cos(b)*cos(y))*inTrack->trackData->fHeart + ui->yBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.z = (sin(a)*sin(b)*cos(y)-cos(a)*sin(y))*inTrack->trackData->fHeart + ui->zBox->value()/gloParent->mOptions->getLengthFactor();

    updateAnchorGeometrics();

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->pitchChangeBox->setValue(inTrack->trackData->anchorNode->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(inTrack->trackData->anchorNode->fYawFromLast*F_HZ);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();


    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorRoll);
        temp->nextAction = tempz;
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_pBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    undoAction *tempx = NULL, *tempy = NULL, *tempz = NULL;
    if(!inTrack->mUndoHandler->busy) {
        tempx = new undoAction(inTrack, changeAnchorPosX);

        tempy = new undoAction(inTrack, changeAnchorPosY);
        tempy->nextAction = tempx;

        tempz = new undoAction(inTrack, changeAnchorPosZ);
        tempz->nextAction = tempy;
    }

    inTrack->trackData->startPitch = arg1;

    inTrack->trackData->anchorNode->changePitch(arg1-inTrack->trackData->anchorNode->getPitch(), fabs(inTrack->trackData->anchorNode->fRoll) >= 90.f);

    float a = inTrack->trackData->startYaw * F_PI/180.f;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.x = -(cos(a)*sin(b)*cos(y)+sin(a)*sin(y))*inTrack->trackData->fHeart + ui->xBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.y = (cos(b)*cos(y))*inTrack->trackData->fHeart + ui->yBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.z = (sin(a)*sin(b)*cos(y)-cos(a)*sin(y))*inTrack->trackData->fHeart + ui->zBox->value()/gloParent->mOptions->getLengthFactor();

    updateAnchorGeometrics();

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->pitchChangeBox->setValue(inTrack->trackData->anchorNode->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(inTrack->trackData->anchorNode->fYawFromLast*F_HZ);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();

    if(!inTrack->mUndoHandler->busy) {

        undoAction* temp = new undoAction(inTrack, changeAnchorPitch);
        temp->nextAction = tempz;
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_jBox_valueChanged(double arg1)
{
    int arg = arg1-0.0001;
    if(arg/180) {
        arg1 -= (float)((abs(arg/180)+1)/2)*360.f * (arg > 0 ? 1 : -1);
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->jBox->setValue(arg1);
        phantomChanges = oldP;
    }
    if(phantomChanges) return;

    undoAction *tempx = NULL, *tempy = NULL, *tempz = NULL;
    if(!inTrack->mUndoHandler->busy) {
        tempx = new undoAction(inTrack, changeAnchorPosX);

        tempy = new undoAction(inTrack, changeAnchorPosY);
        tempy->nextAction = tempx;

        tempz = new undoAction(inTrack, changeAnchorPosZ);
        tempz->nextAction = tempy;
    }

    inTrack->trackData->startYaw = arg1;

    float a = inTrack->trackData->startYaw * F_PI/180.f;
    float b = inTrack->trackData->startPitch * F_PI/180.f;
    float y = inTrack->trackData->anchorNode->fRoll * F_PI/180.f;
    inTrack->trackData->startPos.x = -(cos(a)*sin(b)*cos(y)+sin(a)*sin(y))*inTrack->trackData->fHeart + ui->xBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.y = (cos(b)*cos(y))*inTrack->trackData->fHeart + ui->yBox->value()/gloParent->mOptions->getLengthFactor();
    inTrack->trackData->startPos.z = (sin(a)*sin(b)*cos(y)-cos(a)*sin(y))*inTrack->trackData->fHeart + ui->zBox->value()/gloParent->mOptions->getLengthFactor();

    updateAnchorGeometrics();

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();

    if(!inTrack->mUndoHandler->busy) {

        undoAction* temp = new undoAction(inTrack, changeAnchorYaw);
        temp->nextAction = tempz;
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_anchorSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->anchorNode->fVel = arg1/gloParent->mOptions->getSpeedFactor();
    inTrack->trackData->anchorNode->fEnergy = 0.5*inTrack->trackData->anchorNode->fVel*inTrack->trackData->anchorNode->fVel + F_G*inTrack->trackData->anchorNode->fPosHearty(0.9*inTrack->trackData->fHeart);

    updateAnchorGeometrics();

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->pitchChangeBox->setValue(inTrack->trackData->anchorNode->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(inTrack->trackData->anchorNode->fYawFromLast*F_HZ);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorSpeed);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_straightSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->straightSpeedBox->setEnabled(arg1);
    inTrack->trackData->activeSection->bSpeed = !arg1;
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    phantomChanges = true;
    ui->straightSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSpeedState);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_straightSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->fVel = arg1/gloParent->mOptions->getSpeedFactor();
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentSpeed);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_straightLengthBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->rollFunc->setMaxArgument(arg1/gloParent->mOptions->getLengthFactor());
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    if(inTrack->trackData->activeSection->bSpeed) {
        phantomChanges = true;
        ui->straightSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
        phantomChanges = false;
    }
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentLength);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->curvedSpeedBox->setEnabled(arg1);
    inTrack->trackData->activeSection->bSpeed = !arg1;
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    phantomChanges = true;
    ui->curvedSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSpeedState);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->fVel = arg1/gloParent->mOptions->getSpeedFactor();
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentSpeed);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedRadiusBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->fRadius = arg1/gloParent->mOptions->getLengthFactor();

    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeCurveRadius);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedAngleBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->rollFunc->setMaxArgument(arg1);

    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentLength);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedDirectionBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->fDirection = arg1;

    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeCurveDirection);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedLeadInBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    if(arg1+inTrack->trackData->activeSection->fLeadOut > inTrack->trackData->activeSection->fAngle) {
        arg1 = inTrack->trackData->activeSection->fAngle - inTrack->trackData->activeSection->fLeadOut;
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->curvedLeadInBox->setValue(arg1);
        phantomChanges = oldP;
    }

    inTrack->trackData->activeSection->fLeadIn = arg1;

    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeCurveLeadIn);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_curvedLeadOutBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    if(arg1+inTrack->trackData->activeSection->fLeadIn > inTrack->trackData->activeSection->fAngle) {
        arg1 = inTrack->trackData->activeSection->fAngle - inTrack->trackData->activeSection->fLeadIn;
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->curvedLeadOutBox->setValue(arg1);
        phantomChanges = oldP;
    }

    inTrack->trackData->activeSection->fLeadOut = arg1;

    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeCurveLeadOut);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_advancedSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->advancedSpeedBox->setEnabled(arg1);
    inTrack->trackData->activeSection->bSpeed = !arg1;
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    phantomChanges = true;
    ui->advancedSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSpeedState);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_advancedSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->fVel = arg1/gloParent->mOptions->getSpeedFactor();
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentSpeed);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_sectionListWidget_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(ui->sectionListWidget->itemAt(pos) == NULL) {
        menu->addAction("Add Straight Section", this, SLOT(addStraightSec()));
        menu->addAction("Add Curved Section", this, SLOT(addCurvedSec()));
        menu->addAction("Add Force Section", this, SLOT(addForceSec()));
        menu->addAction("Add Geometric Section", this, SLOT(addGeometricSec()));
    } else {
        menu->addAction("Append Straight Section", this, SLOT(appendStraightSec()));
        menu->addAction("Append Curved Section", this, SLOT(appendCurvedSec()));
        menu->addAction("Append Force Section", this, SLOT(appendForceSec()));
        menu->addAction("Append Geometric Section", this, SLOT(appendGeometricSec()));
        if(selSection != NULL && selSection->type != anchor) {
            menu->addAction("Remove Section", this, SLOT(on_deleteButton_released()));
        }
    }

    menu->popup(ui->sectionListWidget->mapToGlobal(pos));
}

void trackWidget::on_orientationBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->bOrientation = index;
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentOrientation);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_argumentBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;

    inTrack->trackData->activeSection->bArgument = index;
    inTrack->trackData->updateTrack(inTrack->trackData->activeSection, 0);
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->curSectionChanged(selSection);
    inTrack->graphWidgetItem->redrawGraphs(true);

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeSegmentArgument);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_normalBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->anchorNode->forceNormal = arg1;

    updateAnchorGeometrics();


    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->pitchChangeBox->setValue(inTrack->trackData->anchorNode->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(inTrack->trackData->anchorNode->fYawFromLast*F_HZ);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorNormal);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_lateralBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    inTrack->trackData->anchorNode->forceLateral = arg1;

    updateAnchorGeometrics();

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->pitchChangeBox->setValue(inTrack->trackData->anchorNode->fPitchFromLast*F_HZ);
    ui->yawChangeBox->setValue(inTrack->trackData->anchorNode->fYawFromLast*F_HZ);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorLateral);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::updateAnchorGeometrics()
{
    inTrack->trackData->updateAnchorGeometrics();
}

void trackWidget::on_smoothButton_released()
{
    if(!smoothScreen) {
        smoothScreen = new smoothUi(inTrack, gloParent);
        inTrack->trackData->bAutoSmooth = true;
    }

    smoothScreen->updateUi();
    inTrack->mUndoHandler->takeOutsideChange();
    smoothScreen->show();
}

void trackWidget::setSelection(int index)
{
    ui->sectionListWidget->clearSelection();
    ui->sectionListWidget->topLevelItem(index+1)->setSelected(true);
    //selSection = sectionList[index+1];
}

void trackWidget::on_pitchChangeBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    mnode* anchor = inTrack->trackData->anchorNode;
    anchor->fPitchFromLast = arg1/F_HZ;

    float temp = cos(fabs(anchor->getPitch())*F_PI/180.f);
    float forceAngle = sqrt(temp*temp*anchor->fYawFromLast*anchor->fYawFromLast + anchor->fPitchFromLast*anchor->fPitchFromLast);//deltaAngle;
    float dirFromLast = glm::atan(anchor->fYawFromLast, anchor->fPitchFromLast) - TO_RAD(anchor->fRoll);

    glm::vec3 forceVec;
    if(fabs(forceAngle) < std::numeric_limits<float>::epsilon()) {
        forceVec = glm::vec3(0.f, 1.f, 0.f);
    } else {
        forceVec = glm::vec3(0.f, 1.f, 0.f) + (float)((anchor->fVel*anchor->fVel) / (9.80665 * anchor->fVel/forceAngle * 0.18f/F_PI)) * glm::normalize(glm::vec3(glm::rotate(dirFromLast, -anchor->vDir)*glm::vec4(-anchor->vNorm, 0.f)));
    }
    anchor->forceNormal = - glm::dot(forceVec, glm::normalize(anchor->vNorm));
    anchor->forceLateral = - glm::dot(forceVec, glm::normalize(anchor->vLat));

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->normalBox->setValue(anchor->forceNormal);
    ui->lateralBox->setValue(anchor->forceLateral);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorPitchChange);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_yawChangeBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    mnode* anchor = inTrack->trackData->anchorNode;
    anchor->fYawFromLast = arg1/F_HZ;

    float temp = cos(fabs(anchor->getPitch())*F_PI/180.f);
    float forceAngle = sqrt(temp*temp*anchor->fYawFromLast*anchor->fYawFromLast + anchor->fPitchFromLast*anchor->fPitchFromLast);//deltaAngle;
    float dirFromLast = glm::atan(anchor->fYawFromLast, anchor->fPitchFromLast) - TO_RAD(anchor->fRoll);

    glm::vec3 forceVec;
    if(fabs(forceAngle) < std::numeric_limits<float>::epsilon()) {
        forceVec = glm::vec3(0.f, 1.f, 0.f);
    } else {
        forceVec = glm::vec3(0.f, 1.f, 0.f) + (float)((anchor->fVel*anchor->fVel) / (9.80665 * anchor->fVel/forceAngle * 0.18f/F_PI)) * glm::normalize(glm::vec3(glm::rotate(dirFromLast, -anchor->vDir)*glm::vec4(-anchor->vNorm, 0.f)));
    }
    anchor->forceNormal = - glm::dot(forceVec, glm::normalize(anchor->vNorm));
    anchor->forceLateral = - glm::dot(forceVec, glm::normalize(anchor->vLat));

    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->normalBox->setValue(anchor->forceNormal);
    ui->lateralBox->setValue(anchor->forceLateral);
    phantomChanges = oldP;

    inTrack->trackData->updateTrack(0, 0);

    if(!inTrack->mUndoHandler->busy) {
        undoAction* temp = new undoAction(inTrack, changeAnchorYawChange);
        inTrack->mUndoHandler->addAction(temp);
        gloParent->setUndoButtons();
    }
}

void trackWidget::on_sectionListWidget_itemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    if(phantomChanges) return;

    if(column == 1) {
        writeNames();
        inTrack->mUndoHandler->takeOutsideChange();
    }
}

void trackWidget::update()
{
    sectionHandler* curSec = selSection;
    ui->sectionListWidget->clearSelection();
    curSec->listItem->setSelected(true);
    inTrack->graphWidgetItem->redrawGraphs();
    updateSectionIDs();
}

void trackWidget::keyPressEvent(QKeyEvent* event) {
    switch(event->key()) {
    case Qt::Key_Delete:
        on_deleteButton_released();
        event->accept();
        break;
    default:
        event->ignore();
        break;
    }
}
