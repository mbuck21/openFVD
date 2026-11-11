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

#include "transitionwidget.h"
#include "optionsmenu.h"
#include "ui_transitionwidget.h"
#include "undohandler.h"
#include "mainwindow.h"
#include "lenassert.h"
#include "trackwidget.h"
#include "trackhandler.h"
#include "command.h"

extern MainWindow* gloParent;

transitionWidget::transitionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::transitionWidget)
{
    ui->setupUi(this);
    selectedFunc = NULL;
    phantomChanges = false;
}

transitionWidget::~transitionWidget()
{
    delete ui;
}

void transitionWidget::changeSubfunc(subfunc *_newSubfunc)
{
    phantomChanges = true;
    selectedFunc = _newSubfunc;
    gloParent->selectedFunc = selectedFunc;
    if(selectedFunc == NULL) {
        phantomChanges = false;
        return;
    }
    if(_newSubfunc->parent->secParent->type == curved) {
        ui->lengthSpin->setValue(selectedFunc->maxArgument-selectedFunc->minArgument);
        ui->lengthSpin->setSuffix(QString("%1").arg(QChar(0xb0)));
    } else if(_newSubfunc->parent->secParent->bArgument == TIME && _newSubfunc->parent->secParent->type != straight) {
        ui->lengthSpin->setValue(selectedFunc->maxArgument-selectedFunc->minArgument);
        ui->lengthSpin->setSuffix(QString("s"));
    } else {
        ui->lengthSpin->setValue((selectedFunc->maxArgument-selectedFunc->minArgument)*gloParent->mOptions->getLengthFactor());
        ui->lengthSpin->setSuffix(gloParent->mOptions->getLengthString());
    }

    if(_newSubfunc->parent->secParent->bArgument == TIME) {
        ui->changeSpin->setValue(selectedFunc->symArg);
        switch(_newSubfunc->parent->type) {
        case funcNormal:
        case funcLateral:
            ui->changeSpin->setSuffix("g");
            break;
        case funcRoll:
        case funcPitch:
        case funcYaw:
            ui->changeSpin->setSuffix(QString("%1/s").arg(QChar(0xb0)));
            break;
        default:
            lenAssert(0 && "unhandeled case");
            break;
        }
    } else {
        switch(_newSubfunc->parent->type) {
        case funcNormal:
        case funcLateral:
            ui->changeSpin->setValue(selectedFunc->symArg);
            ui->changeSpin->setSuffix("g");
            break;
        case funcRoll:
        case funcPitch:
        case funcYaw:
            ui->changeSpin->setValue(selectedFunc->symArg / gloParent->mOptions->getLengthFactor());
            ui->changeSpin->setSuffix(QString("%1/").arg(QChar(0xb0)).append(gloParent->mOptions->getLengthString()));
            break;
        default:
            lenAssert(0 && "unhandeled case");
            break;
        }
    }

    // HACK: is there a more stable way to do this?
	/*if((_newSubfunc->parent->secParent->type == forced || _newSubfunc->parent->secParent->type == geometric)
      && _newSubfunc->parent->type == funcRoll
      && _newSubfunc->parent->secParent->bArgument == TIME
      && ui->transitionBox->count() == 7)
    {
        ui->transitionBox->addItem("ToZero (experimental)");
    }

    if((_newSubfunc->parent->secParent->type == curved || _newSubfunc->parent->secParent->type == straight || _newSubfunc->parent->type != funcRoll || _newSubfunc->parent->secParent->bArgument == DISTANCE)
      && ui->transitionBox->count() == 8)
    {
        ui->transitionBox->removeItem(7);
	}*/


    ui->buttonFrame->show();
    ui->changeSpin->setEnabled(true);
    switch(selectedFunc->degree) {
    case linear:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->show();
        break;
    case quadratic:
        ui->quadraticFrame->show();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->show();
        if(selectedFunc->isSymmetric()) {
            ui->quadraticBox->setCurrentIndex(2);
        } else if(selectedFunc->arg1 > 0.f) {
            ui->quadraticBox->setCurrentIndex(0);
        } else {
            ui->quadraticBox->setCurrentIndex(1);
        }
        break;
    case cubic:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->show();
        break;
    case quartic:
        ui->quadraticFrame->hide();
        ui->quarticFrame->show();
        ui->quinticFrame->hide();
        ui->timewarpFrame->show();
        if(selectedFunc->isSymmetric()) {
            ui->quarticBox->setCurrentIndex(0);
            ui->quarticSpin->setVisible(false);
        } else if(selectedFunc->arg1 > 0.5) {
            ui->quarticSpin->setValue(  (0.5f/(selectedFunc->arg1 - 0.5f)-1)/5.f  );
            ui->quarticBox->setCurrentIndex(2);
            ui->quarticSpin->setVisible(true);
        } else if(selectedFunc->arg1 >= 0) {
            ui->quarticSpin->setValue((0.5f/(0.5f - selectedFunc->arg1)-1)/5.f);
            ui->quarticBox->setCurrentIndex(1);
            ui->quarticSpin->setVisible(true);
        }
        break;
    case quintic:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->show();
        ui->timewarpFrame->show();
        if(!selectedFunc->isSymmetric()) {
            ui->quinticSpin->setVisible(false);
            ui->quinticBox->setCurrentIndex(0);
        } else if(selectedFunc->arg1 < 0) {
            ui->quinticSpin->setVisible(true);
            ui->quinticSpin->setValue(fabs(selectedFunc->arg1));
            ui->quinticBox->setCurrentIndex(1);
        } else {
            ui->quinticSpin->setVisible(true);
            ui->quinticSpin->setValue(fabs(selectedFunc->arg1));
            ui->quinticBox->setCurrentIndex(2);
        }
        break;
    case sinusoidal:
    case plateau:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->show();
        break;
    case freeform:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->hide();
        break;
    case tozero:
        ui->quadraticFrame->hide();
        ui->quarticFrame->hide();
        ui->quinticFrame->hide();
        ui->timewarpFrame->hide();
        ui->changeSpin->setEnabled(false);
        break;
    }
    ui->transitionBox->setCurrentIndex(selectedFunc->degree);
    ui->centerSpin->setValue(selectedFunc->centerArg);
    ui->tensionSpin->setValue(selectedFunc->tensionArg);

    if(selectedFunc->parent->secParent->isLockable(selectedFunc->parent)) {
        ui->lockCheck->setEnabled(true);
        ui->lockCheck->setChecked(selectedFunc->locked);
    } else {
        ui->lockCheck->setEnabled(false);
        ui->lockCheck->setChecked(false);
    }

    if(ui->lockCheck->isChecked()) {
        ui->lengthSpin->setEnabled(false);
    } else {
        ui->lengthSpin->setEnabled(true);
    }

    if(selectedFunc->parent->funcList.size() == 1) {
        ui->removeButton->setEnabled(false);
    } else {
        ui->removeButton->setEnabled(true);
    }

    switch(selectedFunc->parent->type) {
    case funcRoll:
        ui->changeSpin->setSingleStep(1.);
        break;
    case funcNormal:
        ui->changeSpin->setSingleStep(0.1);
        break;
    case funcLateral:
        ui->changeSpin->setSingleStep(0.1);
        break;
    case funcPitch:
        ui->changeSpin->setSingleStep(1.);
        break;
    case funcYaw:
        ui->changeSpin->setSingleStep(1.);
        break;
    }

    float diff = selectedFunc->maxArgument - mParent->selTrack->trackData->activeSection->getMaxArgument();

    if(diff > 0) {
        ui->errLabel->setText(QString("Warning: Transition is ").append(QString().number(diff+0.0001, 'f', 2)).append(" longer than Section!"));
        ui->errLabel->show();
    } else {
        ui->errLabel->hide();
    }


    gloParent->updateInfoPanel();
    phantomChanges = false;
}

// Every edit below is a named command (core/commands_transition.cpp): the
// slot works out the value in model units, runs the command, which changes the
// model, recomputes the track and records the undo step, and then refreshes the
// panels. The target is the selected transition, by numbers.
static commandTarget targetOf(trackHandler* inTrack, subfunc* sub)
{
    func* f = sub->parent;
    return commandTarget::forTransition(inTrack->getID()-1, inTrack->trackData->lSections.indexOf(f->secParent),
                                        f->type, f->getSubfuncNumber(sub));
}

static bool runTransitionCommand(trackHandler* inTrack, const char* name, const commandTarget& target, const commandArgs& args)
{
    QString error;
    if(commandRegistry::run(inTrack->mHistory, name, target, args, &error)) return true;
    qWarning("Transition command %s failed: %s", name, qPrintable(error));
    return false;
}

void transitionWidget::on_lengthSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    const bool need_length_factor = (selectedFunc->parent->secParent->bArgument == DISTANCE && (selectedFunc->parent->secParent->type == forced || selectedFunc->parent->secParent->type == geometric)) || selectedFunc->parent->secParent->type == straight;

    commandArgs args;
    if(need_length_factor) {
        args.insert("length", arg1/gloParent->mOptions->getLengthFactor());
    } else {
        args.insert("length", arg1);
    }
    if(!runTransitionCommand(inTrack, "transition.setLength", targetOf(inTrack, selectedFunc), args)) return;

    float diff = selectedFunc->maxArgument - mParent->selTrack->trackData->activeSection->getMaxArgument();

    if(diff > 0) {
        if(need_length_factor) {
            diff*=gloParent->mOptions->getLengthFactor();
        }
        ui->errLabel->setText(QString("Warning: Transition is ").append(QString().number(diff+0.0001, 'f', 2)).append(" longer than Section!"));
        ui->errLabel->show();
    } else {
        ui->errLabel->hide();
    }

    mParent->redrawGraphs();

    if(inTrack->trackData->activeSection->type == straight) {
        inTrack->trackWidgetItem->setupStraightFrame();
    } else if(inTrack->trackData->activeSection->type == curved) {
        inTrack->trackWidgetItem->setupCurvedFrame();
    }

    gloParent->updateInfoPanel();
}

void transitionWidget::on_transitionBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    static const char* shapes[] = { "linear", "quadratic", "cubic", "quartic", "quintic", "sinusoidal", "plateau" };
    if(index < 0 || index >= (int)(sizeof(shapes)/sizeof(shapes[0]))) {
        qWarning("Bad transition shape index %d", index);
        return;
    }
    commandArgs args;
    args.insert("shape", QString(shapes[index]));
    if(!runTransitionCommand(inTrack, "transition.setShape", targetOf(inTrack, selectedFunc), args)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();

    changeSubfunc(selectedFunc); // to make sure the frames are up to date (after undo settings because that would fuck them up)
}

void transitionWidget::on_changeSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    commandArgs args;
    if(selectedFunc->parent->secParent->bArgument == DISTANCE && selectedFunc->parent->type != funcLateral && selectedFunc->parent->type != funcNormal) {
        args.insert("change", arg1*gloParent->mOptions->getLengthFactor());
    } else {
        args.insert("change", arg1);
    }
    if(!runTransitionCommand(inTrack, "transition.setChange", targetOf(inTrack, selectedFunc), args)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

// Runs transition.setShapeParameter for the five boxes below, then refreshes.
// parameter is the value the box's slot worked out for the model's arg1.
static bool setShapeParameter(trackHandler* inTrack, subfunc* sub, double parameter)
{
    commandArgs args;
    args.insert("parameter", parameter);
    return runTransitionCommand(inTrack, "transition.setShapeParameter", targetOf(inTrack, sub), args);
}

void transitionWidget::on_quadraticBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;
    double value = selectedFunc->arg1;
    switch(index) {
    case 0:
        value = 1.f;
        break;
    case 1:
        value = -1.f;
        break;
    case 2:
        value = 0.f;
        break;
    default:
        lenAssert(0 && "unhandled default case");
        break;
    }
    if(!setShapeParameter(inTrack, selectedFunc, value)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_quarticBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;
    double value = selectedFunc->arg1;
    if(index > 0)
    {
        ui->quarticSpin->setVisible(true);
        if(selectedFunc->arg1 < -1)
        {
            switch(index)
            {
            case 1:
                value = 0.5f - 0.5f/(1+5*ui->quarticSpin->value());
                break;
            case 2:
                value = 0.5f + 0.5f/(1+5*ui->quarticSpin->value());
                break;
            }
        }
        else
        {
            value = 1-selectedFunc->arg1;
        }
    }
    else
    {
        ui->quarticSpin->setVisible(false);
        value = -10;
    }
    if(!setShapeParameter(inTrack, selectedFunc, value)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_quarticSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;
    double value;
    int key = ui->quarticBox->currentIndex();
    switch (key)
    {
    case 1:
        value = 0.5f - 0.5f/(1+5*arg1);
        break;
    case 2:
        value = 0.5f + 0.5f/(1+5*arg1);
        break;
    default:
        qWarning("Bad Quartic Spin Change");
        return;
    }
    if(!setShapeParameter(inTrack, selectedFunc, value)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_quinticBox_currentIndexChanged(int index)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;
    double value = selectedFunc->arg1;
    switch(index)
    {
    case 0:
        ui->quinticSpin->setVisible(false);
        value = 0.f;
        break;
    case 1:
        ui->quinticSpin->setVisible(true);
        value = fabs(selectedFunc->arg1) < 0.005 ? -ui->quinticSpin->value() : -fabs(selectedFunc->arg1);
        break;
    case 2:
        ui->quinticSpin->setVisible(true);
        value = fabs(selectedFunc->arg1) < 0.005 ? ui->quinticSpin->value() : fabs(selectedFunc->arg1);
        break;
    }
    if(!setShapeParameter(inTrack, selectedFunc, value)) return;

    if(index == 1 || index == 2) {
        phantomChanges = true;
        ui->quinticSpin->setValue(fabs(selectedFunc->arg1));
        phantomChanges = false;
    }

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_quinticSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;
    double value;
    int key = ui->quinticBox->currentIndex();
    switch (key)
    {
    case 1:
        value = -arg1;
        break;
    case 2:
        value = arg1;
        break;
    default:
        qWarning("Bad Quintic Spin Call");
        return;
    }
    if(!setShapeParameter(inTrack, selectedFunc, value)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_centerSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    commandArgs args;
    args.insert("center", arg1);
    if(!runTransitionCommand(inTrack, "transition.setCenter", targetOf(inTrack, selectedFunc), args)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

void transitionWidget::on_tensionSpin_valueChanged(double arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    commandArgs args;
    args.insert("tension", arg1);
    if(!runTransitionCommand(inTrack, "transition.setTension", targetOf(inTrack, selectedFunc), args)) return;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}

subfunc* transitionWidget::getSelectedFunc()
{
    return selectedFunc;
}

void transitionWidget::on_appendButton_released()
{
    trackHandler* inTrack = mParent->selTrack;
    func* parentFunc = selectedFunc->parent;
    int atIndex = parentFunc->getSubfuncNumber(selectedFunc);

    commandArgs args;
    args.insert("where", QString("after"));
    if(!runTransitionCommand(inTrack, "transition.add", targetOf(inTrack, selectedFunc), args)) return;

    ui->removeButton->setEnabled(true);
    mParent->changeSelection(parentFunc->funcList[atIndex+1]);
    mParent->redrawGraphs();


    if(inTrack->trackData->activeSection->type == straight)
    {
        inTrack->trackWidgetItem->setupStraightFrame();
    }
    else if(inTrack->trackData->activeSection->type == curved)
    {
        inTrack->trackWidgetItem->setupCurvedFrame();
    }

    gloParent->updateInfoPanel();
}

void transitionWidget::on_prependButton_released()
{
    trackHandler* inTrack = mParent->selTrack;
    func* parentFunc = selectedFunc->parent;
    int atIndex = parentFunc->getSubfuncNumber(selectedFunc)-1;

    commandArgs args;
    args.insert("where", QString("before"));
    if(!runTransitionCommand(inTrack, "transition.add", targetOf(inTrack, selectedFunc), args)) return;

    ui->removeButton->setEnabled(true);
    mParent->changeSelection(parentFunc->funcList[atIndex+1]);
    mParent->redrawGraphs();

    if(mParent->selTrack->trackData->activeSection->type == straight)
    {
        mParent->selTrack->trackWidgetItem->setupStraightFrame();
    }
    else if(mParent->selTrack->trackData->activeSection->type == curved)
    {
        mParent->selTrack->trackWidgetItem->setupCurvedFrame();
    }

    gloParent->updateInfoPanel();
}

void transitionWidget::on_removeButton_released()
{
    trackHandler* inTrack = mParent->selTrack;
    func* parentFunc = selectedFunc->parent;
    int pos = parentFunc->getSubfuncNumber(selectedFunc);

    // the command removes the transition, so selectedFunc is gone afterwards
    if(!runTransitionCommand(inTrack, "transition.delete", targetOf(inTrack, selectedFunc), commandArgs())) return;

    if(pos == parentFunc->funcList.size())
    {
        mParent->selFunc = parentFunc->funcList[pos-1];
        this->changeSubfunc(parentFunc->funcList[pos-1]);
    }
    else
    {
        mParent->selFunc = parentFunc->funcList[pos];
        this->changeSubfunc(parentFunc->funcList[pos]);
    }
    mParent->redrawGraphs();

    if(inTrack->trackData->activeSection->type == straight)
    {
        inTrack->trackWidgetItem->setupStraightFrame();
    }
    else if(inTrack->trackData->activeSection->type == curved)
    {
        inTrack->trackWidgetItem->setupCurvedFrame();
    }

    gloParent->updateInfoPanel();
}

void transitionWidget::adjustLengthSteps(secType _type, bool _argument)
{
    switch(_type)
    {
    case straight:
        ui->lengthSpin->setSingleStep(1.);
        break;
    case curved:
        ui->lengthSpin->setSingleStep(10.);
        break;
    case forced:
    case geometric:
        if(_argument == TIME)
        {
            ui->lengthSpin->setSingleStep(0.1);
        }
        else
        {
            ui->lengthSpin->setSingleStep(1.);
        }
        break;
    default:
        qWarning("Bad Call to adjust Length Steps");
        return;
    }
}

void transitionWidget::on_lockCheck_stateChanged(int arg1)
{
    if(phantomChanges) return;
    trackHandler* inTrack = mParent->selTrack;

    // the command locks or unlocks, refreshes the length of a locked
    // transition and recomputes the track
    commandArgs args;
    args.insert("locked", arg1 != 0);
    if(!runTransitionCommand(inTrack, "transition.setLocked", targetOf(inTrack, selectedFunc), args)) {
        // the box shows what the model does not have: put it back
        bool oldPhantom = phantomChanges;
        phantomChanges = true;
        ui->lockCheck->setChecked(selectedFunc->locked);
        phantomChanges = oldPhantom;
        return;
    }

    bool oldP = phantomChanges;
    phantomChanges = true;
    if(ui->lockCheck->isChecked())
    {
        ui->lengthSpin->setEnabled(false);
        ui->lengthSpin->setValue(selectedFunc->maxArgument-selectedFunc->minArgument);
    }
    else
    {
        ui->lengthSpin->setEnabled(true);
        ui->lengthSpin->setValue(selectedFunc->maxArgument-selectedFunc->minArgument);
    }
    phantomChanges = oldP;

    mParent->redrawGraphs();
    gloParent->updateInfoPanel();
}
