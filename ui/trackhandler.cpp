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
#include "mainwindow.h"
#include "trackmesh.h"
#include "trackwidget.h"
#include "transitionwidget.h"
#include "smoothui.h"
#include "projectwidget.h"
#include "glviewwidget.h"
#include <QTreeWidgetItem>

extern MainWindow* gloParent;
extern glViewWidget* glView;

int trackHandler::mergeWindowMs = 1000;

trackHandler::trackHandler(QString _name, int _id, int _maxUndoChanges)
{
    id = _id;
    restoring = false;
    mHistory = NULL;    // made below, after the panels, which may ask for it (endMergeRun())
    mMesh = NULL;
    resetHasSelection = false;
    resetSection = -2;
    resetFunc = -1;
    resetSubfunc = 0;
    resetArgument = -1;
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


    // The steps are capped by number (maxUndoChanges) and also by the bytes
    // they hold (trackHistory's default, 128 MB; decision record 0011): the
    // oldest steps are dropped when either limit is passed. States share the
    // bytes of sections that did not change.
    mHistory = new trackHistory(trackData);
    mHistory->setMaxSteps(_maxUndoChanges);
    mHistory->setListener(this);
    // a number typed digit by digit is one step (decision record 0011, point 7)
    mHistory->setMergeWindow(mergeWindowMs);
    mMesh = new trackMesh(trackData);
}

trackHandler::~trackHandler()
{
    delete trackWidgetItem;
    delete graphWidgetItem;
    delete listItem;
    delete mHistory;
    delete trackData;
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
    resetArgument = -1;
    // the unit the plot's horizontal axis is in (time or distance)
    if(trackData->activeSection != NULL) resetArgument = (int)trackData->activeSection->bArgument;
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

    // the step's selection (anchor and track edits have none: what was selected stays)
    // (section -1 is the anchor: undoing the only section's add selects it)
    if(resetHasSelection) {
        selectInPanel(resetSelection.section, resetSelection.function, resetSelection.transition);
    } else {
        selectInPanel(resetSection, resetFunc, resetSubfunc);
    }
    // The panel's selection slot converts the plot's horizontal range between
    // seconds and metres when the argument changes, judged from the section
    // selected before; that was dropped above, so the conversion is done here.
    if(trackData->activeSection != NULL && resetArgument >= 0
            && (int)trackData->activeSection->bArgument != resetArgument) {
        graphWidgetItem->redrawGraphs(true);
    }
    // after the selection, which the mesh shows (the selected section and transition)
    if(mMesh != NULL) mMesh->buildMeshes(0);

    if(glView != NULL && gloParent->curTrack() == trackData) {
        // the rider stays at the track's end when the track got shorter
        // (moveCamera() would send a position past the end to the start)
        if(glView->povPos > trackData->getNumPoints()) glView->povPos = trackData->getNumPoints();
        if(glView->povMode) glView->povNode = trackData->getPoint(glView->povPos);
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

void trackHandler::historyChanged(trackHistory* which)
{
    Q_UNUSED(which);
    gloParent->setUndoButtons();
}

// An undone add or a redone delete has one section (or transition) fewer, so
// the one before it is selected. The step is recognised by the name of its
// first command; any other step is selected as its selection says.
bool trackHandler::selectionAfter(const historyStep& step, bool undone, commandTarget* out) const
{
    commandTarget sel = step.selection;
    QString name = step.calls.isEmpty() ? QString() : step.calls.first().name;
    // section.add has the track as its target. The new section's 0-based place
    // is its "after" argument: redone it is selected, undone the section before
    // it (the anchor, -1, when it was first).
    if(name == "section.add" && sel.track >= 0) {
        int added = step.calls.first().args.value("after").toInt();
        *out = commandTarget::forSection(sel.track, undone ? added-1 : added);
        return true;
    }
    if(sel.section < 0) return false;

    bool addsSection = name == "section.add";
    bool deletesSection = name == "section.delete";
    bool addsTransition = name == "transition.add";
    bool deletesTransition = name == "transition.delete";
    int sec = sel.section;
    int sub = sel.transition;
    // transition.add "after" has the transition it started from as its target;
    // the window selects the new one after it (one further on)
    if(addsTransition && step.calls.first().args.value("where").toString() == "after") ++sub;
    if(addsSection && undone) --sec;
    if(deletesSection && !undone) --sec;
    if(addsTransition && undone && sub > 0) --sub;
    if(deletesTransition && !undone && sub > 0) --sub;

    // a transition is only kept on the section it was edited on
    int func = sec == sel.section ? sel.function : -1;
    *out = commandTarget(sel.track, sec, func, sub);
    return true;
}

void trackHandler::putBack(bool undo)
{
    // the step that is undone is the newest applied one, the one redone the
    // first after it
    int index = undo ? mHistory->position()-1 : mHistory->position();
    commandTarget sel;
    bool hasSelection = selectionAfter(mHistory->steps()[index], undo, &sel);

    gloParent->setUpdatesEnabled(false);
    restoring = true;
    // the panel selects the step's section and transition again once the
    // track is rebuilt (trackReset())
    selectAfterReset(hasSelection ? &sel : NULL);
    if(undo) mHistory->undo();
    else mHistory->redo();
    selectAfterReset(NULL);
    restoring = false;
    gloParent->setUpdatesEnabled(true);
}

// The bounds are checked here, not only by the menu entries (issue 31).
void trackHandler::undo()
{
    if(restoring || !mHistory->canUndo()) return;
    putBack(true);
}

void trackHandler::redo()
{
    if(restoring || !mHistory->canRedo()) return;
    putBack(false);
}

void trackHandler::clearHistory()
{
    mHistory->clear();
}

void trackHandler::takeOutsideChange()
{
    if(restoring) return;   // a restore in progress is not an edit
    mHistory->foldOutsideChange();
}

void trackHandler::endMergeRun()
{
    if(mHistory != NULL) mHistory->endMergeRun();
}

void trackHandler::selectAfterReset(const commandTarget* selection)
{
    resetHasSelection = selection != NULL;
    if(selection != NULL) resetSelection = *selection;
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
