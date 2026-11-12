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
#include "command.h"
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

    // The project stores the start forces, not these rates: a typed rate
    // becomes the two forces, and the box then shows the rate the stored forces
    // give (about 2 percent more; core/commands_anchor.cpp). It is rewritten
    // once editing ends, not while it is being typed in.
    QString rateTip("The project stores the start forces, not this rate. A typed rate sets the forces; "
                    "when you leave the box it shows the rate those forces give, as after reopening.");
    ui->pitchChangeBox->setToolTip(rateTip);
    ui->yawChangeBox->setToolTip(rateTip);
    connect(ui->pitchChangeBox, &QDoubleSpinBox::editingFinished, this, [this]() { setupAnchorFrame(); });
    connect(ui->yawChangeBox, &QDoubleSpinBox::editingFinished, this, [this]() { setupAnchorFrame(); });

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

// The name section.add knows a kind by; "" for the kinds only an importer makes.
static const char* sectionTypeName(secType type)
{
    switch(type) {
    case straight: return "straight";
    case curved: return "curved";
    case forced: return "force";
    case geometric: return "geometric";
    default: return "";
    }
}

// Runs one section command (core/commands_section.cpp) on this panel's track;
// it records its own undo step. The values are in model units, each worked out
// with the expression the slot had before it became a command.
static bool runSectionCommand(trackHandler* handler, const char* name, const commandTarget& target, const commandArgs& args)
{
    QString error;
    bool ok = commandRegistry::run(handler->mHistory, QString(name), target, args, &error);
    if(!ok) {
        qWarning("%s refused: %s", name, error.toLocal8Bit().constData());
    }
    return ok;
}

// The selected section's command target: what the undo step selects again.
static commandTarget activeSectionTarget(trackHandler* handler)
{
    return commandTarget::forSection(handler->getID()-1, handler->trackData->getSectionNumber(handler->trackData->activeSection));
}

// Adds a section after the one with the number "after" (0: first) with the
// command; the section is then in the track and the panel builds its row.
// Null when the command refused.
static section* runAddCommand(trackHandler* handler, secType type, int after)
{
    commandArgs args{{"type", QString(sectionTypeName(type))}, {"after", after}};
    if(!runSectionCommand(handler, "section.add", commandTarget::forTrack(handler->getID()-1), args)) return NULL;
    return handler->trackData->lSections[after];
}

// Runs a command that edits the selected section's values. When the command is
// refused the boxes are put back to what the model holds, so no box shows a
// number the model does not have.
bool trackWidget::runSectionEdit(const char* name, const commandArgs& args)
{
    bool ok = runSectionCommand(inTrack, name, activeSectionTarget(inTrack), args);
    if(!ok && selSection && selSection->sectionData) {
        switch(selSection->type) {
        case straight:
            setupStraightFrame();
            break;
        case curved:
            setupCurvedFrame();
            break;
        case forced:
        case geometric:
            setupAdvFrame();
            break;
        default:
            break;
        }
        updateOptionsFrame();
    }
    return ok;
}

void trackWidget::addSection(secType _type)
{
    if(*sectionTypeName(_type) == 0) {
        // a NoLimits CSV import (projectWidget::importNoLimitsCSVFile()), which
        // starts the track's undo list afresh afterwards: no command, no step
        sectionHandler* newSec = new sectionHandler(inTrack->trackData, _type, sectionList.size());
        this->inTrack->trackData->updateTrack(newSec->sectionData, 0);

        showNewSection(newSec);
        return;
    }
    section* added = runAddCommand(inTrack, _type, inTrack->trackData->lSections.size());
    if(!added) return;
    sectionHandler* newSec = new sectionHandler(added, sectionList.size());

    showNewSection(newSec);
}

// A section the track added itself (track::appendSectionFromFile(), told
// through trackHandler) is shown as addSection() shows the one it creates.
void trackWidget::showAddedSection(int index)
{
    section* added = inTrack->trackData->lSections[index];
    sectionHandler* newSec = new sectionHandler(added, sectionList.size());

    showNewSection(newSec);
}

// The panel's part of adding a section at the end: its row, selected; the
// graphs. (The undo step is the command's, not the panel's.)
void trackWidget::showNewSection(sectionHandler* newSec)
{
    sectionList.append(newSec);
    ui->sectionListWidget->addTopLevelItem(newSec->listItem);
    ui->sectionListWidget->clearSelection();
    newSec->listItem->setSelected(true);

    inTrack->graphWidgetItem->redrawGraphs();
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
    section* added = runAddCommand(inTrack, _type, index-1);
    if(!added) return;
    sectionHandler* newSec = new sectionHandler(added, index);
    sectionList.insert(index, newSec);
    ui->sectionListWidget->insertTopLevelItem(index, newSec->listItem);
    this->inTrack->trackData->activeSection = newSec->sectionData;
    selSection = newSec;

    //emit done();

    inTrack->graphWidgetItem->redrawGraphs();
    updateSectionIDs();
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

    if(!runSectionCommand(inTrack, "section.delete", commandTarget::forSection(inTrack->getID()-1, index-1), commandArgs())) return;
    delete sectionList[index];
    sectionList.removeAt(index);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();
    updateSectionIDs();
    on_sectionListWidget_itemSelectionChanged();
    inTrack->graphWidgetItem->selectionChanged();
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

void trackWidget::setupAnchorFrame(QWidget* keep)
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    mnode* anchor = inTrack->trackData->anchorNode;
    float yaw = inTrack->trackData->startYaw*F_PI/180;
    glm::vec3 temp = anchor->vPosHeart(inTrack->trackData->fHeart);

    if(keep != ui->xBox) ui->xBox->setValue( (-cos(yaw)*temp.z+sin(yaw)*temp.x+inTrack->trackData->startPos.x)  * gloParent->mOptions->getLengthFactor());
    if(keep != ui->yBox) ui->yBox->setValue((temp.y+inTrack->trackData->startPos.y) * gloParent->mOptions->getLengthFactor());
    if(keep != ui->zBox) ui->zBox->setValue((cos(yaw)*temp.x+sin(yaw)*temp.z+inTrack->trackData->startPos.z) * gloParent->mOptions->getLengthFactor());
    if(keep != ui->rBox) ui->rBox->setValue(anchor->fRoll);
    if(keep != ui->pBox) ui->pBox->setValue(anchor->getPitch());
    if(keep != ui->jBox) ui->jBox->setValue(inTrack->trackData->startYaw);
    if(keep != ui->anchorSpeedBox) ui->anchorSpeedBox->setValue(anchor->fVel*gloParent->mOptions->getSpeedFactor());
    ui->anchorSpeedBox->setSuffix(gloParent->mOptions->getSpeedString());
    ui->xBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->yBox->setSuffix(gloParent->mOptions->getLengthString());
    ui->zBox->setSuffix(gloParent->mOptions->getLengthString());
    if(keep != ui->normalBox) ui->normalBox->setValue(anchor->forceNormal);
    if(keep != ui->lateralBox) ui->lateralBox->setValue(anchor->forceLateral);
    if(keep != ui->pitchChangeBox) ui->pitchChangeBox->setValue(anchor->fPitchFromLast*F_HZ);
    if(keep != ui->yawChangeBox) ui->yawChangeBox->setValue(anchor->fYawFromLast*F_HZ);

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

// Runs one anchor command (core/commands_anchor.cpp) on this panel's track; it
// records its own undo step. The values are in model units, each worked out
// with the expression the slot had before it became a command.
static bool runAnchorCommand(trackHandler* handler, const char* name, const commandArgs& args)
{
    QString error;
    bool ok = commandRegistry::run(handler->mHistory, QString(name), commandTarget::forTrack(handler->getID()-1), args, &error);
    if(!ok) {
        qWarning("%s refused: %s", name, error.toLocal8Bit().constData());
    }
    return ok;
}

// The point the roll, pitch and yaw commands keep in place: what the X, Y and
// Z boxes show, in metres.
static void addHoldArgs(commandArgs& args, double x, double y, double z)
{
    args.insert("holdX", x);
    args.insert("holdY", y);
    args.insert("holdZ", z);
}

void trackWidget::on_xBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    bool ok = runAnchorCommand(inTrack, "anchor.setPositionX", commandArgs{{"x", arg1/gloParent->mOptions->getLengthFactor()}});
    setupAnchorFrame(ok ? ui->xBox : NULL);

    gloParent->updateInfoPanel();
}

void trackWidget::on_yBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    bool ok = runAnchorCommand(inTrack, "anchor.setPositionY", commandArgs{{"y", arg1/gloParent->mOptions->getLengthFactor()}});
    setupAnchorFrame(ok ? ui->yBox : NULL);

    inTrack->mMesh->buildMeshes(0);

    gloParent->updateInfoPanel();
}

void trackWidget::on_zBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    bool ok = runAnchorCommand(inTrack, "anchor.setPositionZ", commandArgs{{"z", arg1/gloParent->mOptions->getLengthFactor()}});
    setupAnchorFrame(ok ? ui->zBox : NULL);

    gloParent->updateInfoPanel();
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
    commandArgs args{{"roll", arg1}};
    addHoldArgs(args, ui->xBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->yBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->zBox->value()/gloParent->mOptions->getLengthFactor());
    bool ok = runAnchorCommand(inTrack, "anchor.setRoll", args);

    setupAnchorFrame(ok ? ui->rBox : NULL);

    gloParent->updateInfoPanel();
}

void trackWidget::on_pBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    commandArgs args{{"pitch", arg1}};
    addHoldArgs(args, ui->xBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->yBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->zBox->value()/gloParent->mOptions->getLengthFactor());
    bool ok = runAnchorCommand(inTrack, "anchor.setPitch", args);

    setupAnchorFrame(ok ? ui->pBox : NULL);

    gloParent->updateInfoPanel();
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

    commandArgs args{{"yaw", arg1}};
    addHoldArgs(args, ui->xBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->yBox->value()/gloParent->mOptions->getLengthFactor(),
                ui->zBox->value()/gloParent->mOptions->getLengthFactor());
    bool ok = runAnchorCommand(inTrack, "anchor.setYaw", args);

    setupAnchorFrame(ok ? ui->jBox : NULL);

    gloParent->updateInfoPanel();
}

void trackWidget::on_anchorSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    bool ok = runAnchorCommand(inTrack, "anchor.setSpeed", commandArgs{{"speed", arg1/gloParent->mOptions->getSpeedFactor()}});

    setupAnchorFrame(ok ? ui->anchorSpeedBox : NULL);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_straightSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->straightSpeedBox->setEnabled(arg1);
    inTrack->mHistory->endMergeRun();
    runSectionEdit("section.setSpeedFixed", commandArgs{{"fixed", arg1 != 0}});
    inTrack->mHistory->endMergeRun();
    phantomChanges = true;
    ui->straightSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_straightSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("section.setSpeed", commandArgs{{"speed", arg1/gloParent->mOptions->getSpeedFactor()}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_straightLengthBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("straight.setLength", commandArgs{{"length", arg1/gloParent->mOptions->getLengthFactor()}});
    if(inTrack->trackData->activeSection->bSpeed) {
        phantomChanges = true;
        ui->straightSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
        phantomChanges = false;
    }
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->curvedSpeedBox->setEnabled(arg1);
    inTrack->mHistory->endMergeRun();
    runSectionEdit("section.setSpeedFixed", commandArgs{{"fixed", arg1 != 0}});
    inTrack->mHistory->endMergeRun();
    phantomChanges = true;
    ui->curvedSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("section.setSpeed", commandArgs{{"speed", arg1/gloParent->mOptions->getSpeedFactor()}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedRadiusBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("curve.setRadius", commandArgs{{"radius", arg1/gloParent->mOptions->getLengthFactor()}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedAngleBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("curve.setAngle", commandArgs{{"angle", arg1}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedDirectionBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("curve.setDirection", commandArgs{{"direction", arg1}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedLeadInBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    // the command cuts a lead-in that is too large; the box shows what is left
    bool cut = arg1+inTrack->trackData->activeSection->fLeadOut > inTrack->trackData->activeSection->fAngle;
    runSectionEdit("curve.setLeadIn", commandArgs{{"leadIn", arg1}});
    if(cut) {
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->curvedLeadInBox->setValue(inTrack->trackData->activeSection->fLeadIn);
        phantomChanges = oldP;
    }
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_curvedLeadOutBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    bool cut = arg1+inTrack->trackData->activeSection->fLeadIn > inTrack->trackData->activeSection->fAngle;
    runSectionEdit("curve.setLeadOut", commandArgs{{"leadOut", arg1}});
    if(cut) {
        bool oldP = phantomChanges;
        phantomChanges = true;
        ui->curvedLeadOutBox->setValue(inTrack->trackData->activeSection->fLeadOut);
        phantomChanges = oldP;
    }
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_advancedSpeedCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;

    ui->advancedSpeedBox->setEnabled(arg1);
    inTrack->mHistory->endMergeRun();
    runSectionEdit("section.setSpeedFixed", commandArgs{{"fixed", arg1 != 0}});
    inTrack->mHistory->endMergeRun();
    phantomChanges = true;
    ui->advancedSpeedBox->setValue(inTrack->trackData->activeSection->getSpeed()*gloParent->mOptions->getSpeedFactor());
    phantomChanges = false;
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_advancedSpeedBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    runSectionEdit("section.setSpeed", commandArgs{{"speed", arg1/gloParent->mOptions->getSpeedFactor()}});
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
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

    inTrack->mHistory->endMergeRun();
    runSectionEdit("section.setOrientation", commandArgs{{"orientation", QString(index ? "euler" : "quaternion")}});
    inTrack->mHistory->endMergeRun();
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_argumentBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;

    inTrack->mHistory->endMergeRun();
    runSectionEdit("section.setArgument", commandArgs{{"argument", QString(index ? "distance" : "time")}});
    inTrack->mHistory->endMergeRun();
    gloParent->updateInfoPanel();
    updateSectionFrame();

    inTrack->graphWidgetItem->curSectionChanged(selSection);
    inTrack->graphWidgetItem->redrawGraphs(true);
}

void trackWidget::on_normalBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    bool ok = runAnchorCommand(inTrack, "anchor.setNormalForce", commandArgs{{"force", arg1}});

    setupAnchorFrame(ok ? ui->normalBox : NULL);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::on_lateralBox_valueChanged(double arg1)
{
    if(phantomChanges) return;

    bool ok = runAnchorCommand(inTrack, "anchor.setLateralForce", commandArgs{{"force", arg1}});

    setupAnchorFrame(ok ? ui->lateralBox : NULL);

    gloParent->updateInfoPanel();

    inTrack->graphWidgetItem->redrawGraphs();
}

void trackWidget::updateAnchorGeometrics()
{
    inTrack->trackData->updateAnchorGeometrics();
}

void trackWidget::on_smoothButton_released()
{
    if(!smoothScreen) {
        smoothScreen = new smoothUi(inTrack, gloParent);
        // "a smoothing dialog exists" (track::bAutoSmooth): not saved and not
        // a step; it stays when the dialog is cancelled, as the dialog does
        inTrack->trackData->bAutoSmooth = true;
        inTrack->mUndoHandler->takeOutsideChange();
    }

    // the dialog's step: the entries are brought up to date inside it
    smoothScreen->openStep();
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
    bool ok = runAnchorCommand(inTrack, "anchor.setPitchRate", commandArgs{{"rate", arg1}});

    setupAnchorFrame(ok ? ui->pitchChangeBox : NULL);
}

void trackWidget::on_yawChangeBox_valueChanged(double arg1)
{
    if(phantomChanges) return;
    bool ok = runAnchorCommand(inTrack, "anchor.setYawRate", commandArgs{{"rate", arg1}});

    setupAnchorFrame(ok ? ui->yawChangeBox : NULL);
}

void trackWidget::on_sectionListWidget_itemChanged(QTreeWidgetItem *item, int column)
{
    if(phantomChanges) return;

    if(column == 1) {
        int index = getSection(item);
        if(index < 1) return;
        runSectionCommand(inTrack, "section.setName", commandTarget::forSection(inTrack->getID()-1, index-1),
                          commandArgs{{"name", item->text(1)}});
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
