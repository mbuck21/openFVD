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

#include "trackhandler.h"
#include "graphwidget.h"
#include "undohandler.h"
#include "mainwindow.h"
#include "trackmesh.h"
#include "trackwidget.h"
#include "transitionwidget.h"
#include "undoaction.h"
#include "smoothui.h"
#include "projectwidget.h"
#include "glviewwidget.h"
#include <QTreeWidgetItem>

extern MainWindow* gloParent;
extern glViewWidget* glView;

trackHandler::trackHandler(QString _name, int _id, int _maxUndoChanges)
{
    id = _id;
    mMesh = NULL;
    resetStep = NULL;
    resetUndone = false;
    resetSection = -2;
    resetFunc = -1;
    resetSubfunc = 0;
    this->trackData = new track(glm::vec3(0.f, 5.f, 0.f), 0, 1.1);
    trackData->mListener = this;
    trackData->name = _name;
    this->listItem = new QTreeWidgetItem(id);
    listItem->setFlags(listItem->flags() | Qt::ItemIsEditable);
    listItem->setText(0, QString().number(id));
    listItem->setTextAlignment(0, Qt::AlignHCenter | Qt::AlignVCenter);
    listItem->setText(1, trackData->name);
    listItem->setCheckState(2, Qt::Checked);
    graphWidgetItem = new graphWidget(NULL, this);
    graphWidgetItem->setPalette(gloParent->palette());
    graphWidgetItem->hide();
    trackWidgetItem = new trackWidget(NULL, this);
    trackWidgetItem->setPalette(gloParent->palette());
    trackWidgetItem->hide();

    tabId = -1;

    trackData->display.colors[0] = QColor(20, 20, 130);
    trackData->display.colors[1] = QColor(255, 51, 51);
    trackData->display.colors[2] = QColor(51, 255, 51);


    mUndoHandler = new undoHandler(trackData, _maxUndoChanges);
    mMesh = new trackMesh(trackData);
}

trackHandler::~trackHandler()
{
    delete trackWidgetItem;
    delete graphWidgetItem;
    delete listItem;
    delete trackData;
    delete mUndoHandler;
    delete mMesh;
}

void trackHandler::changeID(int _id)
{
    id = _id;
    listItem->setText(0, QString().number(_id));
}

int trackHandler::getID()
{
    return id;
}

void trackHandler::trackGeometryChanged(track* which, int fromNode)
{
    if(mMesh != NULL)
        mMesh->buildMeshes(fromNode);
    gloParent->rideSheetChanged(which);
}

void trackHandler::trackStatusMessage(const QString& text, int timeoutMs)
{
    gloParent->showMessage(text, timeoutMs);
}

void trackHandler::trackSmoothingApplied(track* which)
{
    Q_UNUSED(which);
    graphWidgetItem->redrawGraphs();
}

void trackHandler::trackSectionAdded(track* which, int index)
{
    Q_UNUSED(which);
    trackWidgetItem->showAddedSection(index);
}

void trackHandler::trackLoaded(track* which)
{
    trackWidgetItem->clearSelection();
    trackWidgetItem->setNames();
    gloParent->rideSheetChanged(which);
}

// A restore (core/trackstate.h) deletes every section, transition and
// smoothing entry of the track and makes new ones. Everything in the windows
// that points into the track lets go here, before the old model is deleted,
// and is rebuilt in trackReset(). Undo and redo come through here; so will
// anything else that replaces the model under the windows.
void trackHandler::trackAboutToReset(track* which)
{
    Q_UNUSED(which);

    // what to select again, when no undo step says
    resetSection = -2;
    resetFunc = -1;
    resetSubfunc = 0;
    sectionHandler* sel = trackWidgetItem->selSection;
    if(sel != NULL) {
        resetSection = sel->sectionData ? trackData->getSectionNumber(sel->sectionData) : -1;
    }
    subfunc* selFunc = graphWidgetItem->selFunc;
    if(selFunc != NULL && trackData->activeSection != NULL && selFunc->parent->secParent == trackData->activeSection) {
        resetFunc = selFunc->parent->type;
        resetSubfunc = selFunc->parent->getSubfuncNumber(selFunc);
    }

    // the 3D view's highlight: MainWindow::selectedFunc may point into this
    // track (trackMesh reads it)
    subfunc* global = gloParent->selectedFunc;
    bool globalHere = false;
    for(int i = 0; global != NULL && i < trackData->lSections.size(); ++i) {
        if(global->parent->secParent == trackData->lSections[i]) globalHere = true;
    }

    // the panel: no selection (selSection, activeSection, the graphs'
    // and the transition editor's subfunc), then no section rows
    trackWidgetItem->dropSections();
    graphWidgetItem->transitionHandler->changeSubfunc(NULL);
    graphWidgetItem->selFunc = NULL;
    gloParent->selectedFunc = globalHere ? NULL : global;

    // the smoothing dialog: its rows go with their entries
    if(trackWidgetItem->smoothScreen) {
        trackWidgetItem->smoothScreen->dropSelection();
    }

    // the ride view: its node is put back in trackReset()
    if(glView != NULL && glView->povNode != NULL && gloParent->curTrack() == trackData) {
        glView->povNode = trackData->anchorNode;
    }
}

void trackHandler::trackReset(track* which)
{

    // the restore leaves the last section active; the panel selects below
    trackData->activeSection = NULL;

    trackWidgetItem->showSections();
    graphWidgetItem->redrawGraphs();   // the plots drop their pointers to the old transitions

    if(trackWidgetItem->smoothScreen) {
        trackWidgetItem->smoothScreen->showRows();
    }

    listItem->setText(1, trackData->name);
    if(tabId >= 0) gloParent->renameTab(this);

    if(resetStep == NULL || !resetStep->reselect(resetUndone)) {
        selectInPanel(resetSection, resetFunc, resetSubfunc);
    }
    // after the selection, which the mesh shows (the selected section and transition)
    if(mMesh != NULL) mMesh->buildMeshes(0);

    if(glView != NULL && glView->povMode && gloParent->curTrack() == trackData) {
        glView->povNode = trackData->getPoint(glView->povPos);
    }

    // the dialogs that show values of the track
    gloParent->updateBoxes();
    if(gloParent->project != NULL) {
        gloParent->project->trackReset(this);
    }

    gloParent->updateInfoPanel();
    gloParent->setUndoButtons();
    gloParent->rideSheetChanged(which);
}

void trackHandler::selectAfterReset(undoAction* step, bool undone)
{
    resetStep = step;
    resetUndone = undone;
}

void trackHandler::selectInPanel(int sec, int funcType, int sub)
{
    if(sec < -1) {
        graphWidgetItem->redrawGraphs();
        return;
    }
    if(sec >= trackData->lSections.size()) sec = trackData->lSections.size()-1;
    trackWidgetItem->setSelection(sec);   // -1: the anchor

    // the section first: transitionWidget::changeSubfunc() reads activeSection
    section* selected = trackData->activeSection;
    if(funcType < 0 || selected == NULL) return;

    func* parentFunc = NULL;
    switch(funcType) {
    case funcRoll:
        parentFunc = selected->rollFunc;
        break;
    case funcNormal:
    case funcPitch:
        parentFunc = selected->normForce;
        break;
    case funcLateral:
    case funcYaw:
        parentFunc = selected->latForce;
        break;
    default:
        break;
    }
    if(parentFunc == NULL || parentFunc->funcList.isEmpty()) return;
    if(sub >= parentFunc->funcList.size()) sub = parentFunc->funcList.size()-1;
    if(sub < 0) sub = 0;

    graphWidgetItem->changeSelection(parentFunc->funcList[sub]);
    graphWidgetItem->selectionChanged();
}
