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

// READ THE RULE AT THE TOP OF batchscenarios.h BEFORE CHANGING THIS FILE:
// the scenarios drive only the widgets (by their .ui object names), the public
// slots their menus call, and the Edit menu. They never write the model.

#include "batchscenarios.h"
#include "mainwindow.h"
#include "projectwidget.h"
#include "trackwidget.h"
#include "graphwidget.h"
#include "transitionwidget.h"
#include "trackproperties.h"
#include "smoothui.h"
#include "optionsmenu.h"
#include "trackhandler.h"
#include "track.h"
#include "undohandler.h"
#include "undoaction.h"
#include "smoothhandler.h"
#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// batchDriver: one person at the keyboard
// ---------------------------------------------------------------------------

batchDriver::batchDriver(MainWindow* w)
{
    mWindow = w;
}

void batchDriver::fail(const QString& what)
{
    if(sError.isEmpty()) {
        sError = what;
    }
}

QWidget* batchDriver::find(QWidget* parent, const char* name)
{
    if(!ok()) return NULL;
    if(!parent) {
        fail(QString("no window to look for %1 in").arg(name));
        return NULL;
    }
    QWidget* w = parent->findChild<QWidget*>(QString(name));
    if(!w) {
        fail(QString("widget %1 not found").arg(name));
    }
    return w;
}

QWidget* batchDriver::projectPanel()
{
    return find(mWindow, "projectTab");
}

trackWidget* batchDriver::trackPanel()
{
    QTabWidget* tabs = qobject_cast<QTabWidget*>(find(mWindow, "tabChooser"));
    if(!tabs) return NULL;
    trackWidget* panel = qobject_cast<trackWidget*>(tabs->currentWidget());
    if(!panel) {
        fail("no track is open in a tab");
    }
    return panel;
}

graphWidget* batchDriver::graphPanel()
{
    if(!ok()) return NULL;
    // the open track's plot is the only one in the main window
    QList<graphWidget*> plots = mWindow->findChildren<graphWidget*>();
    if(plots.size() != 1) {
        fail(QString("expected one plot panel, found %1").arg(plots.size()));
        return NULL;
    }
    return plots[0];
}

QWidget* batchDriver::transitionPanel()
{
    return find(graphPanel(), "transitionEditor");
}

// The value boxes: refuse what a person could not do (a disabled box, a value
// outside its range).
static void setSpin(batchDriver* d, QWidget* w, const char* name, double value, QString& error)
{
    Q_UNUSED(d);
    if(!w) return;
    QDoubleSpinBox* box = qobject_cast<QDoubleSpinBox*>(w);
    QSpinBox* intBox = qobject_cast<QSpinBox*>(w);
    if(box) {
        if(!box->isEnabled()) {
            error = QString("%1 is disabled").arg(name);
        } else if(value < box->minimum() - 1e-9 || value > box->maximum() + 1e-9) {
            error = QString("%1: %2 is outside %3 to %4").arg(name).arg(value).arg(box->minimum()).arg(box->maximum());
        } else {
            box->setValue(value);
        }
    } else if(intBox) {
        if(!intBox->isEnabled()) {
            error = QString("%1 is disabled").arg(name);
        } else {
            intBox->setValue((int)floor(value + 0.5));
        }
    } else {
        error = QString("%1 is not a number box").arg(name);
    }
}

static void setCombo(QWidget* w, const char* name, int index, QString& error)
{
    if(!w) return;
    QComboBox* box = qobject_cast<QComboBox*>(w);
    if(!box) {
        error = QString("%1 is not a list box").arg(name);
    } else if(!box->isEnabled()) {
        error = QString("%1 is disabled").arg(name);
    } else if(index < 0 || index >= box->count()) {
        error = QString("%1 has no entry %2").arg(name).arg(index);
    } else {
        box->setCurrentIndex(index);
    }
}

static void setCheck(QWidget* w, const char* name, bool on, QString& error)
{
    if(!w) return;
    QAbstractButton* box = qobject_cast<QAbstractButton*>(w);
    if(!box) {
        error = QString("%1 is not a check box").arg(name);
    } else if(!box->isEnabled()) {
        error = QString("%1 is disabled").arg(name);
    } else {
        box->setChecked(on);
    }
}

static void click(QWidget* w, const char* name, QString& error)
{
    if(!w) return;
    QAbstractButton* button = qobject_cast<QAbstractButton*>(w);
    if(!button) {
        error = QString("%1 is not a button").arg(name);
    } else if(!button->isEnabled()) {
        error = QString("%1 is disabled").arg(name);
    } else {
        button->click();
    }
}

static double spinValue(QWidget* w)
{
    QDoubleSpinBox* box = qobject_cast<QDoubleSpinBox*>(w);
    return box ? box->value() : 0.0;
}

// The dialog a click just opened: the batch mode keeps every window off the
// screen (batchrunner.cpp), so "open" means visible to Qt only.
template<class T> static T* openDialog(MainWindow* w)
{
    QList<T*> all = w->findChildren<T*>();
    T* found = NULL;
    for(int i = 0; i < all.size(); ++i) {
        if(all[i]->isVisible()) {
            if(found) return NULL;
            found = all[i];
        }
    }
    return found;
}

static void clickOk(QWidget* dialog, QString& error)
{
    QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>("buttonBox");
    if(!buttons || !buttons->button(QDialogButtonBox::Ok)) {
        error = "the dialog has no OK button";
        return;
    }
    buttons->button(QDialogButtonBox::Ok)->click();
}

void batchDriver::newProject()
{
    if(!ok()) return;
    // the project tab first, as a person would, then File, New
    QTabWidget* tabs = qobject_cast<QTabWidget*>(find(mWindow, "tabChooser"));
    QTreeWidget* list = qobject_cast<QTreeWidget*>(find(projectPanel(), "trackListWidget"));
    QAction* action = mWindow->findChild<QAction*>("actionNew");
    if(!tabs || !list || !action) {
        fail("File, New not found");
        return;
    }
    tabs->setCurrentIndex(0);
    list->clearSelection();
    action->trigger();
    if(mWindow->project->trackList.size() != 1) {
        fail("File, New did not leave one empty track");
    }
}

void batchDriver::newTrack()
{
    if(!ok()) return;
    projectWidget* project = qobject_cast<projectWidget*>(projectPanel());
    if(!project) return;
    project->newEmptyTrack(); // the project panel's "Add", "Empty Track"
}

void batchDriver::openTrack(int t)
{
    if(!ok()) return;
    QWidget* project = projectPanel();
    QTreeWidget* list = qobject_cast<QTreeWidget*>(find(project, "trackListWidget"));
    if(!list) return;
    if(t < 0 || t >= list->topLevelItemCount()) {
        fail(QString("there is no track %1").arg(t+1));
        return;
    }
    list->setCurrentItem(list->topLevelItem(t));
    QString error;
    click(find(project, "editButton"), "editButton", error);
    if(!error.isEmpty()) fail(error);
    trackWidget* panel = trackPanel();
    if(panel && panel->inTrack != mWindow->project->trackList[t]) {
        fail(QString("track %1 did not open").arg(t+1));
    }
}

void batchDriver::renameTrack(int t, const QString& name)
{
    if(!ok()) return;
    QTreeWidget* list = qobject_cast<QTreeWidget*>(find(projectPanel(), "trackListWidget"));
    if(!list) return;
    if(t < 0 || t >= list->topLevelItemCount()) {
        fail(QString("there is no track %1").arg(t+1));
        return;
    }
    list->topLevelItem(t)->setText(1, name); // typing the name into the list
}

// Track Properties: select the track, click Properties, fill in, OK.
// A NaN leaves the box as the dialog shows it. Resistance is in the box's unit (1e-5).
void batchDriver::trackProperties(int t, double heartline, double friction, double resistance)
{
    if(!ok()) return;
    QWidget* project = projectPanel();
    QTreeWidget* list = qobject_cast<QTreeWidget*>(find(project, "trackListWidget"));
    if(!list) return;
    if(t < 0 || t >= list->topLevelItemCount()) {
        fail(QString("there is no track %1").arg(t+1));
        return;
    }
    list->setCurrentItem(list->topLevelItem(t));
    QString error;
    click(find(project, "propertyButton"), "propertyButton", error);
    if(!error.isEmpty()) {
        fail(error);
        return;
    }
    TrackProperties* dialog = openDialog<TrackProperties>(mWindow);
    if(!dialog) {
        fail("Track Properties did not open");
        return;
    }
    if(heartline == heartline) setSpin(this, find(dialog, "heartlineBox"), "heartlineBox", heartline, error);
    if(error.isEmpty() && friction == friction) setSpin(this, find(dialog, "frictionBox"), "frictionBox", friction, error);
    if(error.isEmpty() && resistance == resistance) setSpin(this, find(dialog, "resistanceBox"), "resistanceBox", resistance, error);
    if(error.isEmpty()) clickOk(dialog, error);
    if(!error.isEmpty()) {
        fail(QString("Track Properties: ") + error);
    } else if(dialog->isVisible()) {
        fail("Track Properties did not close");
    }
}

void batchDriver::addSection(secType type)
{
    trackWidget* panel = trackPanel();
    if(!panel) return;
    int before = sectionCount();
    switch(type) { // the track panel's "Add" menu
    case straight:
        panel->addStraightSec();
        break;
    case curved:
        panel->addCurvedSec();
        break;
    case forced:
        panel->addForceSec();
        break;
    case geometric:
        panel->addGeometricSec();
        break;
    default:
        fail("the Add menu offers only straight, curved, force and geometric sections");
        return;
    }
    if(sectionCount() != before + 1) {
        fail("the section was not added");
    }
}

void batchDriver::selectSection(int s)
{
    trackWidget* panel = trackPanel();
    QTreeWidget* list = qobject_cast<QTreeWidget*>(find(panel, "sectionListWidget"));
    if(!list) return;
    if(s < -1 || s+1 >= list->topLevelItemCount()) {
        fail(QString("there is no section %1").arg(s+1));
        return;
    }
    list->setCurrentItem(list->topLevelItem(s+1));
    if(list->selectedItems().size() != 1 || list->selectedItems().at(0) != list->topLevelItem(s+1)) {
        fail(QString("section %1 could not be selected").arg(s+1));
    }
}

void batchDriver::deleteSection(int s)
{
    selectSection(s);
    QString error;
    click(find(trackPanel(), "deleteButton"), "deleteButton", error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::setPanelValue(const char* name, double value)
{
    QString error;
    setSpin(this, find(trackPanel(), name), name, value, error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::setPanelIndex(const char* name, int index)
{
    QString error;
    setCombo(find(trackPanel(), name), name, index, error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::setPanelChecked(const char* name, bool on)
{
    QString error;
    setCheck(find(trackPanel(), name), name, on, error);
    if(!error.isEmpty()) fail(error);
}

double batchDriver::panelValue(const char* name)
{
    return spinValue(find(trackPanel(), name));
}

bool batchDriver::panelEnabled(const char* name)
{
    QWidget* w = find(trackPanel(), name);
    return w && w->isEnabled();
}

func* batchDriver::function(eFunctype f)
{
    track* t = openTrackData();
    if(!t) return NULL;
    section* sec = t->activeSection;
    if(!sec) {
        fail("no section is selected");
        return NULL;
    }
    func* fn = NULL;
    switch(f) {
    case funcRoll:
        fn = sec->rollFunc;
        break;
    case funcNormal:
        fn = sec->type == forced ? sec->normForce : NULL;
        break;
    case funcLateral:
        fn = sec->type == forced ? sec->latForce : NULL;
        break;
    case funcPitch:
        fn = sec->type == geometric ? sec->normForce : NULL;
        break;
    case funcYaw:
        fn = sec->type == geometric ? sec->latForce : NULL;
        break;
    }
    if(!fn) {
        fail("the selected section has no such function");
    }
    return fn;
}

int batchDriver::transitionCount(eFunctype f)
{
    func* fn = function(f);
    return fn ? fn->funcList.size() : 0;
}

// Picking a transition is clicking its curve in the plot.
void batchDriver::selectTransition(eFunctype f, int index)
{
    func* fn = function(f);
    if(!fn) return;
    if(index < 0 || index >= fn->funcList.size()) {
        fail(QString("the function has no transition %1").arg(index+1));
        return;
    }
    subfunc* target = fn->funcList[index];
    graphWidget* plot = graphPanel();
    transitionWidget* editor = qobject_cast<transitionWidget*>(transitionPanel());
    if(!plot || !editor) return;
    plot->changeSelection(target);
    plot->selectionChanged();
    if(editor->getSelectedFunc() != target) {
        fail(QString("transition %1 could not be picked in the plot").arg(index+1));
    }
}

void batchDriver::setTransitionValue(const char* name, double value)
{
    QString error;
    setSpin(this, find(transitionPanel(), name), name, value, error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::setTransitionIndex(const char* name, int index)
{
    QString error;
    setCombo(find(transitionPanel(), name), name, index, error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::setTransitionChecked(const char* name, bool on)
{
    QString error;
    setCheck(find(transitionPanel(), name), name, on, error);
    if(!error.isEmpty()) fail(error);
}

void batchDriver::clickTransitionButton(const char* name)
{
    QString error;
    click(find(transitionPanel(), name), name, error);
    if(!error.isEmpty()) fail(error);
}

double batchDriver::transitionValue(const char* name)
{
    return spinValue(find(transitionPanel(), name));
}

int batchDriver::transitionIndex(const char* name)
{
    QComboBox* box = qobject_cast<QComboBox*>(find(transitionPanel(), name));
    return box ? box->currentIndex() : -1;
}

bool batchDriver::transitionEnabled(const char* name)
{
    QWidget* w = find(transitionPanel(), name);
    return w && w->isEnabled();
}

bool batchDriver::transitionChecked(const char* name)
{
    QAbstractButton* box = qobject_cast<QAbstractButton*>(find(transitionPanel(), name));
    return box && box->isChecked();
}

// Sets one transition in the editor's order: shape, its variant, length,
// change, time-warp. Shapes that keep the previous shape argument (linear,
// quadratic, cubic, sinusoidal) are reached through quartic, which sets it:
// a new force transition starts as cubic with that argument never set, and the
// saved file would hold whatever was in memory.
void batchDriver::setTransition(eFunctype f, int index, const transitionSpec& spec)
{
    selectTransition(f, index);
    if(!ok()) return;
    section* sec = openTrackData()->activeSection;

    if(spec.shape >= 0) {
        if(spec.shape != 3 && spec.shape != 4 && spec.shape != 6) {
            setTransitionIndex("transitionBox", 3);
        }
        setTransitionIndex("transitionBox", spec.shape);
    }
    int shape = transitionIndex("transitionBox");
    if(spec.variant >= 0) {
        if(shape == 1) {
            setTransitionIndex("quadraticBox", spec.variant);
        } else if(shape == 3) {
            setTransitionIndex("quarticBox", spec.variant);
            if(spec.variantValue != 0.0) setTransitionValue("quarticSpin", spec.variantValue);
        } else if(shape == 4) {
            setTransitionIndex("quinticBox", spec.variant);
            if(spec.variantValue != 0.0) setTransitionValue("quinticSpin", spec.variantValue);
        } else {
            fail("this shape has no variants");
        }
    }
    if(spec.length > 0.0) {
        bool metres = sec->type == straight || (sec->type != curved && sec->bArgument == DISTANCE);
        setTransitionValue("lengthSpin", metres ? len(spec.length) : spec.length);
    }
    bool perMetre = sec->bArgument == DISTANCE && f != funcNormal && f != funcLateral;
    setTransitionValue("changeSpin", perMetre ? perLen(spec.change) : spec.change);
    if(spec.center != 0.0) setTransitionValue("centerSpin", spec.center);
    if(spec.tension != 0.0) setTransitionValue("tensionSpin", spec.tension);
}

void batchDriver::appendTransition(eFunctype f, int after, const transitionSpec& spec)
{
    selectTransition(f, after);
    clickTransitionButton("appendButton");
    setTransition(f, after+1, spec);
}

void batchDriver::prependTransition(eFunctype f, const transitionSpec& spec)
{
    selectTransition(f, 0);
    clickTransitionButton("prependButton");
    setTransition(f, 0, spec);
}

void batchDriver::lockTransition(eFunctype f, int index)
{
    selectTransition(f, index);
    setTransitionChecked("lockCheck", true);
}

void batchDriver::smoothing(int entry, double lengthSeconds, int iterations, bool on)
{
    if(!ok()) return;
    QString error;
    click(find(trackPanel(), "smoothButton"), "smoothButton", error);
    if(!error.isEmpty()) {
        fail(error);
        return;
    }
    smoothUi* dialog = openDialog<smoothUi>(mWindow);
    if(!dialog) {
        fail("the smoothing dialog did not open");
        return;
    }
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(find(dialog, "smoothUnitTree"));
    if(!tree) return;
    QTreeWidgetItem* item = NULL;
    for(int i = 0; i < tree->topLevelItemCount(); ++i) {
        if(tree->topLevelItem(i)->text(0) == QString::number(entry)) {
            item = tree->topLevelItem(i);
            break;
        }
    }
    if(!item) {
        fail(QString("the smoothing dialog has no entry %1").arg(entry));
        return;
    }
    tree->setCurrentItem(item);
    setSpin(this, find(dialog, "lengthBox"), "lengthBox", lengthSeconds, error);
    if(error.isEmpty()) setSpin(this, find(dialog, "iterBox"), "iterBox", iterations, error);
    if(error.isEmpty()) {
        item->setCheckState(6, on ? Qt::Checked : Qt::Unchecked); // the "enabled" tick
        clickOk(dialog, error);
    }
    if(!error.isEmpty()) {
        fail(QString("smoothing: ") + error);
    } else if(dialog->isVisible()) {
        fail("the smoothing dialog did not close");
    }
}

bool batchDriver::undo()
{
    if(!ok()) return false;
    QAction* action = mWindow->findChild<QAction*>("actionUndo");
    if(!action) {
        fail("Edit, Undo not found");
        return false;
    }
    if(!action->isEnabled()) return false;
    action->trigger();
    return true;
}

bool batchDriver::redo()
{
    if(!ok()) return false;
    QAction* action = mWindow->findChild<QAction*>("actionRedo");
    if(!action) {
        fail("Edit, Redo not found");
        return false;
    }
    if(!action->isEnabled()) return false;
    action->trigger();
    return true;
}

double batchDriver::len(double metres)
{
    return metres * mWindow->mOptions->getLengthFactor();
}

double batchDriver::spd(double metresPerSecond)
{
    return metresPerSecond * mWindow->mOptions->getSpeedFactor();
}

double batchDriver::perLen(double perMetre)
{
    return perMetre / mWindow->mOptions->getLengthFactor();
}

trackHandler* batchDriver::openHandler()
{
    trackWidget* panel = trackPanel();
    return panel ? panel->inTrack : NULL;
}

track* batchDriver::openTrackData()
{
    trackHandler* h = openHandler();
    return h ? h->trackData : NULL;
}

int batchDriver::sectionCount()
{
    track* t = openTrackData();
    return t ? t->lSections.size() : 0;
}

secType batchDriver::sectionType(int s)
{
    track* t = openTrackData();
    if(!t || s < 0 || s >= t->lSections.size()) return anchor;
    return t->lSections[s]->type;
}

// ---------------------------------------------------------------------------
// The scenarios
// ---------------------------------------------------------------------------

namespace {

enum { LIN = 0, QUAD = 1, CUB = 2, QUART = 3, QUINT = 4, SINE = 5, PLAT = 6 };

transitionSpec T(double length, double change, int shape = -1, int variant = -1, double variantValue = 0.0,
                 double center = 0.0, double tension = 0.0)
{
    transitionSpec s;
    s.length = length;
    s.change = change;
    s.shape = shape;
    s.variant = variant;
    s.variantValue = variantValue;
    s.center = center;
    s.tension = tension;
    return s;
}

// Holds the force section's normal force where it enters (cubic, no change,
// shape argument set), locked so that the other functions set the length.
void holdNormal(batchDriver& d)
{
    d.setTransition(funcNormal, 0, T(0, 0, CUB));
    d.lockTransition(funcNormal, 0);
}

void physicsStraight(batchDriver& d, double metres)
{
    d.addSection(straight);
    d.setPanelChecked("straightSpeedCheck", false); // unticked: physics carries the speed
    d.setPanelValue("straightLengthBox", d.len(metres));
}

// Every shape the transition editor offers, on normal force, time argument.
void buildNormalShapes(batchDriver& d)
{
    d.openTrack(0);
    d.trackProperties(0, 1.1, 0.03, 2.0);
    d.selectSection(-1);
    d.setPanelValue("anchorSpeedBox", d.spd(20));

    physicsStraight(d, 15);

    // linear, the three quadratics, cubic, the quartic's first two forms
    d.addSection(forced);
    d.lockTransition(funcRoll, 0);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcNormal, 0, T(0.5, 0.6, LIN));
    d.appendTransition(funcNormal, 0, T(0.5, -0.6, QUAD, 0));
    d.appendTransition(funcNormal, 1, T(0.5, -0.5, QUAD, 1));
    d.appendTransition(funcNormal, 2, T(0.6, -0.3, QUAD, 2));
    d.appendTransition(funcNormal, 3, T(0.5, 0.5, CUB));
    d.appendTransition(funcNormal, 4, T(0.6, 0.6, QUART, 0));
    d.appendTransition(funcNormal, 5, T(0.6, 0.3, QUART, 1, 0.4));
    d.appendTransition(funcNormal, 6, T(0.5, -0.3, CUB));

    // quartic overshoot, the three quintics, sinusoidal, plateau; centre and tension
    d.addSection(forced);
    d.lockTransition(funcRoll, 0);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcNormal, 0, T(0.6, 0.3, QUART, 2, 0.3));
    d.appendTransition(funcNormal, 0, T(0.6, -0.3, QUINT, 0));
    d.appendTransition(funcNormal, 1, T(0.8, 0.3, QUINT, 1, 4.0));
    d.appendTransition(funcNormal, 2, T(0.8, -0.3, QUINT, 2, 6.0));
    d.appendTransition(funcNormal, 3, T(0.5, -0.2, SINE, -1, 0.0, 0.5, 0.0));
    d.appendTransition(funcNormal, 4, T(0.8, -0.2, PLAT, -1, 0.0, 0.0, 0.5));
    d.appendTransition(funcNormal, 5, T(0.5, 0.2, LIN));

    d.addSection(curved);
    d.setPanelValue("curvedSpeedBox", d.spd(15));
    d.setPanelValue("curvedDirectionBox", 90);
    d.setPanelValue("curvedRadiusBox", d.len(30));
    d.setPanelValue("curvedAngleBox", 60);

    d.addSection(geometric);
    d.setTransition(funcPitch, 0, T(1.0, 8, QUART, 0));

    physicsStraight(d, 10);
}

// Distance argument, Quaternion geometric, Euler force, prepended and appended
// transitions on pitch, yaw, lateral and roll, a hanging train, the anchor moved.
void buildGeometricDistance(batchDriver& d)
{
    d.openTrack(0);
    d.trackProperties(0, -0.9, 0.02, 3.0);
    d.selectSection(-1);
    d.setPanelValue("xBox", d.len(5));
    d.setPanelValue("yBox", d.len(8));
    d.setPanelValue("zBox", d.len(-3));
    d.setPanelValue("pBox", 4);
    d.setPanelValue("jBox", 30);
    d.setPanelValue("anchorSpeedBox", d.spd(18));

    d.addSection(straight); // fixed speed, the default
    d.setPanelValue("straightSpeedBox", d.spd(18));
    d.setPanelValue("straightLengthBox", d.len(12));

    d.addSection(geometric);
    d.setPanelIndex("argumentBox", 1);    // distance
    d.setPanelIndex("orientationBox", 0); // quaternion
    d.lockTransition(funcRoll, 0);
    d.setTransition(funcPitch, 0, T(12, -0.8, QUART, 0));
    d.appendTransition(funcPitch, 0, T(6, 0.5, CUB));
    d.prependTransition(funcPitch, T(6, -0.5, CUB, -1, 0.0, 0.0, -0.3));
    d.setTransition(funcYaw, 0, T(15, 1.0, QUART, 0));
    d.appendTransition(funcYaw, 0, T(9, -0.8, QUART, 0));

    d.addSection(forced);
    d.setPanelIndex("argumentBox", 1);    // distance
    d.setPanelIndex("orientationBox", 1); // Euler
    holdNormal(d);
    d.setTransition(funcLateral, 0, T(10, 0.15, QUART, 0));
    d.appendTransition(funcLateral, 0, T(10, -0.1, LIN));
    d.prependTransition(funcLateral, T(6, 0.1, LIN));
    d.setTransition(funcRoll, 0, T(13, 2.0, QUART, 0));
    d.appendTransition(funcRoll, 0, T(13, -2.0, QUART, 0));

    d.addSection(curved);
    d.setPanelValue("curvedDirectionBox", -90);
    d.setPanelValue("curvedRadiusBox", d.len(25));
    d.setPanelValue("curvedAngleBox", 45);
    d.setPanelValue("curvedLeadInBox", 10);
    d.setPanelValue("curvedLeadOutBox", 10);
    d.setPanelIndex("orientationBox", 1); // Euler
    d.setPanelValue("curvedSpeedBox", d.spd(16));

    d.addSection(forced);
    holdNormal(d);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcRoll, 0, T(1.0, 30, QUART, 0));
    d.appendTransition(funcRoll, 0, T(1.0, -30, QUART, 0));

    d.addSection(geometric);
    d.setTransition(funcPitch, 0, T(1.0, -3, QUART, 0));
}

// Straight and curved sections: directions, lead in and out, fixed and physics
// speed, roll on both; the anchor rolled; one section smoothed.
void buildCurvesSpeed(batchDriver& d)
{
    d.openTrack(0);
    d.trackProperties(0, 1.1, 0.025, 2.5);
    d.selectSection(-1);
    d.setPanelValue("rBox", 8);
    d.setPanelValue("anchorSpeedBox", d.spd(15));

    physicsStraight(d, 10);
    d.setTransition(funcRoll, 0, T(10, -20, QUART, 0));

    d.addSection(curved);
    d.setPanelChecked("curvedSpeedCheck", false); // physics
    d.setPanelValue("curvedDirectionBox", 90);
    d.setPanelValue("curvedRadiusBox", d.len(30));
    d.setPanelValue("curvedAngleBox", 90);
    d.setPanelValue("curvedLeadInBox", 15);
    d.setPanelValue("curvedLeadOutBox", 15);
    d.setTransition(funcRoll, 0, T(45, 25, QUART, 0, 0.0, -0.4, 0.0));
    d.appendTransition(funcRoll, 0, T(45, -25, QUART, 0));

    d.addSection(curved);
    d.setPanelValue("curvedSpeedBox", d.spd(14));
    d.setPanelValue("curvedDirectionBox", 30);
    d.setPanelValue("curvedRadiusBox", d.len(40));
    d.setPanelValue("curvedAngleBox", 10);

    d.addSection(curved);
    d.setPanelChecked("curvedSpeedCheck", false);
    d.setPanelValue("curvedDirectionBox", -100);
    d.setPanelValue("curvedRadiusBox", d.len(25));
    d.setPanelValue("curvedAngleBox", 70);
    d.setPanelValue("curvedLeadInBox", 20);
    d.setPanelIndex("orientationBox", 1); // Euler

    d.addSection(straight);
    d.setPanelValue("straightSpeedBox", d.spd(14));
    d.setPanelValue("straightLengthBox", d.len(12));

    d.addSection(forced);
    d.lockTransition(funcRoll, 0);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcNormal, 0, T(1.2, 0.4, QUART, 0, 0.0, 0.3, 0.0));

    d.smoothing(2, 0.3, 2, true); // section 2, the banked curve
}

// Roll-heavy sections with the whole track smoothed.
void buildSmoothWhole(batchDriver& d)
{
    d.openTrack(0);
    d.trackProperties(0, 1.1, 0.03, 2.0);
    d.selectSection(-1);
    d.setPanelValue("anchorSpeedBox", d.spd(22));

    d.addSection(forced);
    holdNormal(d);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcRoll, 0, T(0.4, 60, LIN));
    d.appendTransition(funcRoll, 0, T(0.4, -60, LIN));

    d.addSection(geometric);
    d.setPanelIndex("orientationBox", 0); // quaternion
    d.lockTransition(funcPitch, 0);
    d.lockTransition(funcRoll, 0);
    d.setTransition(funcYaw, 0, T(1.5, 20, QUART, 0));

    d.addSection(forced);
    holdNormal(d);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcRoll, 0, T(0.4, -60, LIN));
    d.appendTransition(funcRoll, 0, T(0.4, 60, LIN));

    physicsStraight(d, 10);

    d.addSection(forced);
    d.setPanelIndex("argumentBox", 1); // distance
    d.lockTransition(funcRoll, 0);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcNormal, 0, T(15, 0.8, QUART, 0));
    d.appendTransition(funcNormal, 0, T(15, -0.6, QUART, 0));

    d.addSection(curved);
    d.setPanelChecked("curvedSpeedCheck", false);
    d.setPanelValue("curvedDirectionBox", -90);
    d.setPanelValue("curvedRadiusBox", d.len(60));
    d.setPanelValue("curvedAngleBox", 30);

    d.smoothing(0, 0.5, 2, true); // the whole track
}

// Two tracks in one project.
void buildTwoTracks(batchDriver& d)
{
    d.renameTrack(0, "first");
    d.openTrack(0);
    d.trackProperties(0, 1.1, 0.03, 2.0);
    d.selectSection(-1);
    d.setPanelValue("anchorSpeedBox", d.spd(16));

    d.addSection(straight);
    d.setPanelValue("straightSpeedBox", d.spd(16));
    d.setPanelValue("straightLengthBox", d.len(8));

    d.addSection(curved);
    d.setPanelValue("curvedSpeedBox", d.spd(16));
    d.setPanelValue("curvedDirectionBox", 90);
    d.setPanelValue("curvedRadiusBox", d.len(40));
    d.setPanelValue("curvedAngleBox", 30);

    d.addSection(forced);
    d.lockTransition(funcRoll, 0);
    d.lockTransition(funcLateral, 0);
    d.setTransition(funcNormal, 0, T(1.0, 0.5, QUART, 0));

    d.addSection(geometric);
    d.setTransition(funcYaw, 0, T(1.0, 10, QUART, 0));

    d.newTrack();
    d.renameTrack(1, "second");
    d.openTrack(1);
    d.trackProperties(1, 1.1, 0.03, 2.0);
    d.selectSection(-1);
    d.setPanelValue("xBox", d.len(20));
    d.setPanelValue("zBox", d.len(10));
    d.setPanelValue("jBox", -45);
    d.setPanelValue("anchorSpeedBox", d.spd(12));

    d.addSection(geometric);
    d.setTransition(funcPitch, 0, T(1.0, -6, QUART, 0));

    d.addSection(forced);
    holdNormal(d);
    d.lockTransition(funcRoll, 0);
    d.setTransition(funcLateral, 0, T(0.5, 0.1, LIN));
    d.appendTransition(funcLateral, 0, T(0.5, -0.1, LIN));

    physicsStraight(d, 8);

    d.addSection(curved);
    d.setPanelChecked("curvedSpeedCheck", false);
    d.setPanelValue("curvedDirectionBox", 75);
    d.setPanelValue("curvedRadiusBox", d.len(35));
    d.setPanelValue("curvedAngleBox", 25);
}

} // namespace

QStringList batchScenarios::names()
{
    return QStringList() << "normal-shapes" << "geometric-distance" << "curves-speed" << "smooth-whole" << "two-tracks";
}

QString batchScenarios::description(const QString& name)
{
    if(name == "normal-shapes") return "every transition shape and variant on normal force, time argument";
    if(name == "geometric-distance") return "distance argument, both orientations, prepended and appended transitions, heartline -0.9";
    if(name == "curves-speed") return "curved sections, lead in and out, fixed and physics speed, one section smoothed";
    if(name == "smooth-whole") return "roll transitions with the whole track smoothed";
    if(name == "two-tracks") return "two tracks in one project";
    return QString();
}

void batchScenarios::build(const QString& name, batchDriver& d)
{
    d.newProject();
    if(name == "normal-shapes") buildNormalShapes(d);
    else if(name == "geometric-distance") buildGeometricDistance(d);
    else if(name == "curves-speed") buildCurvesSpeed(d);
    else if(name == "smooth-whole") buildSmoothWhole(d);
    else if(name == "two-tracks") buildTwoTracks(d);
}

// ---------------------------------------------------------------------------
// Statistics and sanity of a computed track (read from the model)
// ---------------------------------------------------------------------------

static bool finite3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static const char* sectionTag(secType type)
{
    switch(type) {
    case straight: return "STR";
    case curved: return "CUR";
    case forced: return "FRC";
    case geometric: return "GEO";
    case bezier: return "BEZ";
    case nolimitscsv: return "CSV";
    default: return "?";
    }
}

// Limits of a sane test track: the train keeps moving, the forces stay in a
// range a ride could have.
static const double MIN_SPEED = 3.0;     // m/s
static const double MIN_FORCE = -1.5;    // g
static const double MAX_FORCE = 5.0;     // g

QString batchScenarios::trackStats(track* t, QString& error)
{
    QStringList tags;
    int nodes = 0;
    double minV = 1e9, maxV = -1e9, minN = 1e9, maxN = -1e9, minL = 1e9, maxL = -1e9;
    double length = 0.0;
    int minVAt = 0, minNAt = 0, maxNAt = 0, minLAt = 0, maxLAt = 0;
    for(int s = 0; s < t->lSections.size(); ++s) {
        section* sec = t->lSections[s];
        tags.append(sectionTag(sec->type));
        for(int i = 0; i < sec->lNodes.size(); ++i) {
            const mnode& n = sec->lNodes[i];
            if(!finite3(n.vPos) || !finite3(n.vDir) || !finite3(n.vLat) || !finite3(n.vNorm)
                    || !std::isfinite(n.fRoll) || !std::isfinite(n.fVel) || !std::isfinite(n.forceNormal)
                    || !std::isfinite(n.forceLateral) || !std::isfinite(n.fTotalLength)) {
                if(error.isEmpty()) error = QString("section %1, node %2: a value is not a finite number").arg(s+1).arg(i);
            }
            if(n.fVel < minV) { minV = n.fVel; minVAt = s+1; }
            maxV = qMax(maxV, (double)n.fVel);
            if(n.forceNormal < minN) { minN = n.forceNormal; minNAt = s+1; }
            if(n.forceNormal > maxN) { maxN = n.forceNormal; maxNAt = s+1; }
            if(n.forceLateral < minL) { minL = n.forceLateral; minLAt = s+1; }
            if(n.forceLateral > maxL) { maxL = n.forceLateral; maxLAt = s+1; }
            length = n.fTotalLength;
        }
        nodes += sec->lNodes.size() - (s == 0 ? 0 : 1);
    }
    if(t->lSections.isEmpty()) {
        error = "no sections";
        return QString();
    }
    if(error.isEmpty() && minV < MIN_SPEED) {
        error = QString("the speed drops to %1 m/s in section %2").arg(minV, 0, 'f', 3).arg(minVAt);
    }
    if(error.isEmpty() && (minN < MIN_FORCE || maxN > MAX_FORCE)) {
        error = QString("normal force %1 g (section %2) to %3 g (section %4)").arg(minN, 0, 'f', 3).arg(minNAt).arg(maxN, 0, 'f', 3).arg(maxNAt);
    }
    if(error.isEmpty() && (minL < MIN_FORCE || maxL > MAX_FORCE)) {
        error = QString("lateral force %1 g (section %2) to %3 g (section %4)").arg(minL, 0, 'f', 3).arg(minLAt).arg(maxL, 0, 'f', 3).arg(maxLAt);
    }
    return QString("%1 sections (%2), %3 nodes, %4 m, speed %5 to %6 m/s, normal %7 to %8 g, lateral %9 to %10 g")
            .arg(t->lSections.size()).arg(tags.join(" ")).arg(nodes).arg(length, 0, 'f', 2)
            .arg(minV, 0, 'f', 2).arg(maxV, 0, 'f', 2).arg(minN, 0, 'f', 3).arg(maxN, 0, 'f', 3)
            .arg(minL, 0, 'f', 3).arg(maxL, 0, 'f', 3);
}

// ---------------------------------------------------------------------------
// The edits of --batch-undo-check
// ---------------------------------------------------------------------------

namespace {

int firstSection(batchDriver& d, secType type)
{
    for(int s = 0; s < d.sectionCount(); ++s) {
        if(d.sectionType(s) == type) return s;
    }
    return -1;
}

// a value a step away, inside the box's range
double nudged(double value, double step, double min, double max)
{
    double up = value + step;
    if(up <= max && up >= min) return up;
    return value - step;
}

void nudgePanel(batchDriver& d, const char* name, double step, double min, double max)
{
    d.setPanelValue(name, nudged(d.panelValue(name), step, min, max));
}

void nudgeTransition(batchDriver& d, const char* name, double step, double min, double max)
{
    d.setTransitionValue(name, nudged(d.transitionValue(name), step, min, max));
}

bool forceSection(batchDriver& d, QString& note, int& s)
{
    s = firstSection(d, forced);
    if(s < 0) {
        note = "no force section";
        return false;
    }
    d.selectSection(s);
    return true;
}

// the first function of the selected section whose first transition's length can be typed
bool pickUnlocked(batchDriver& d, const eFunctype* order, int n)
{
    for(int k = 0; k < n; ++k) {
        d.selectTransition(order[k], 0);
        if(d.transitionEnabled("lengthSpin")) return true;
    }
    return false;
}

const eFunctype FORCE_ORDER[] = { funcNormal, funcRoll, funcLateral };
const eFunctype GEO_ORDER[] = { funcYaw, funcPitch, funcRoll };

// Edits left out of the default list because the undo and redo round trip
// fails on the code as it was when this list was written (2026-11-10), found by
// running every edit on its own on every baseline project (--edits n). Some fail
// only under a condition; there the edit still runs on the other projects.
// --all-edits puts them back, to check a fix. Fix the program, not this list.
enum brokenWhen { ALWAYS, SMOOTHED_TRACK, DISTANCE_SECTION, ROLLED_ANCHOR };
struct knownBroken { int edit; eActionType type; brokenWhen when; secType section; const char* reason; };
const knownBroken KNOWN_BROKEN[] = {
    { 4, changeAnchorPitchChange, ALWAYS, anchor, "undo of the anchor's pitch change rate recomputes the anchor's normal and lateral force from the rate "
                         "(trackWidget::on_pitchChangeBox_valueChanged) instead of restoring them; the saved forces differ in the last digits" },
    { 5, changeAnchorYawChange, ALWAYS, anchor, "undo of the anchor's yaw change rate: as for the pitch change rate (on_yawChangeBox_valueChanged)" },
    { 9, changeAnchorRoll, ROLLED_ANCHOR, anchor, "undo of the anchor roll applies the old roll as a difference to the current one (mnode::setRoll); "
                                "with a rolled anchor the restored roll differs in the last digits" },
    { 29, onLengthSpin, ALWAYS, anchor, "undo of a transition length on the first force section does not restore the track "
                          "(gen-normal-shapes: exports differ; gen-smooth-whole: exports differ)" },
    { 41, onTensionSpin, ALWAYS, anchor, "undo of a tension change restores undoHandler::oldTensionSpinValue, which nothing sets from the selected "
                          "transition (transitionWidget::changeSubfunc sets the other four); the restored tension is whatever was "
                          "left there (seen: minus infinity)" },
    { 42, appendSubFunction, SMOOTHED_TRACK, anchor, "on a smoothed track, undo of appending a transition leaves the track different" },
    { 43, appendSubFunction, SMOOTHED_TRACK, anchor, "on a smoothed track, undo of prepending a transition leaves the track different" },
    { 44, removeSubFunction, SMOOTHED_TRACK, anchor, "on a smoothed track, undo and redo of removing a transition leave the track different" },
    { 45, appendSubFunction, ALWAYS, anchor, "a new normal-force transition starts as cubic with its shape argument (subfunc::arg1) never set; "
                          "redo creates it again with another leftover value, so the saved file differs" },
    { 46, appendSubFunction, ALWAYS, anchor, "prepend on normal force: the same unset shape argument as edit 45" },
    { 49, changeSegmentArgument, DISTANCE_SECTION, forced, "undo of switching a force section from distance to time does not restore the saved project" },
    { 52, changeSegmentArgument, DISTANCE_SECTION, geometric, "undo of switching a geometric section from distance to time does not restore the saved project" },
    { 56, removeSegment, ALWAYS, anchor, "undo of deleting a section recreates its smoothing entry without the name the smoothing dialog gave it "
                          "(smoothHandler::saveSmooth writes the list row's text), so the saved file is shorter; "
                          "the section's own smoothing settings are lost too" },
    { -1, onLengthSpin, ALWAYS, anchor, NULL }
};

const int EDIT_COUNT = 57;

} // namespace

int batchScenarios::editCount()
{
    return EDIT_COUNT;
}

bool batchScenarios::editIsKnownBroken(int i, batchDriver& d, QString& reason, int& type)
{
    for(int k = 0; KNOWN_BROKEN[k].reason; ++k) {
        const knownBroken& kb = KNOWN_BROKEN[k];
        if(kb.edit != i+1) continue; // numbered from 1, as in the report
        track* t = d.openTrackData();
        bool applies = false;
        switch(kb.when) {
        case ALWAYS:
            applies = true;
            break;
        case SMOOTHED_TRACK: // read only: is any smoothing entry switched on
            for(int n = 0; t && n < t->smoothList.size(); ++n) {
                if(t->smoothList[n]->active) applies = true;
            }
            break;
        case DISTANCE_SECTION: {
            int s = firstSection(d, kb.section);
            applies = s >= 0 && t->lSections[s]->bArgument == DISTANCE;
            break;
        }
        case ROLLED_ANCHOR:
            applies = t && t->anchorNode->fRoll != 0.f;
            break;
        }
        if(applies) {
            reason = kb.reason;
            type = kb.type;
            return true;
        }
    }
    return false;
}

bool batchScenarios::applyEdit(int i, batchDriver& d, QString& desc, QString& expected, QString& note)
{
    int s = -1;
    switch(i) {
    // the anchor; the first edit recomputes the whole track (see the last one)
    case 0:
        desc = "anchor: start speed"; expected = "changeAnchorSpeed";
        d.selectSection(-1); nudgePanel(d, "anchorSpeedBox", d.spd(2), d.spd(1), d.spd(100));
        return true;
    case 1:
        desc = "anchor: normal force"; expected = "changeAnchorNormal";
        d.selectSection(-1); nudgePanel(d, "normalBox", 0.1, -2, 4);
        return true;
    case 2:
        desc = "anchor: lateral force"; expected = "changeAnchorLateral";
        d.selectSection(-1); nudgePanel(d, "lateralBox", 0.05, -1, 1);
        return true;
    case 3:
        desc = "anchor: pitch change rate"; expected = "changeAnchorPitchChange";
        d.selectSection(-1); nudgePanel(d, "pitchChangeBox", 1.0, -100, 100);
        return true;
    case 4:
        desc = "anchor: yaw change rate"; expected = "changeAnchorYawChange";
        d.selectSection(-1); nudgePanel(d, "yawChangeBox", 1.0, -100, 100);
        return true;
    case 5:
        desc = "anchor: x"; expected = "changeAnchorPosX";
        d.selectSection(-1); nudgePanel(d, "xBox", d.len(1.5), -9000, 9000);
        return true;
    case 6:
        desc = "anchor: y"; expected = "changeAnchorPosY";
        d.selectSection(-1); nudgePanel(d, "yBox", d.len(0.5), -9000, 9000);
        return true;
    case 7:
        desc = "anchor: z"; expected = "changeAnchorPosZ";
        d.selectSection(-1); nudgePanel(d, "zBox", d.len(-1.0), -9000, 9000);
        return true;
    case 8:
        desc = "anchor: roll"; expected = "changeAnchorRoll+changeAnchorPosZ+changeAnchorPosY+changeAnchorPosX";
        d.selectSection(-1); nudgePanel(d, "rBox", 5, -170, 170);
        return true;
    case 9:
        desc = "anchor: pitch"; expected = "changeAnchorPitch+changeAnchorPosZ+changeAnchorPosY+changeAnchorPosX";
        d.selectSection(-1); nudgePanel(d, "pBox", 3, -80, 80);
        return true;
    case 10:
        desc = "anchor: yaw"; expected = "changeAnchorYaw+changeAnchorPosZ+changeAnchorPosY+changeAnchorPosX";
        d.selectSection(-1); nudgePanel(d, "jBox", 10, -170, 170);
        return true;
    // Track Properties (OK also records friction and resistance when the
    // boxes' rounding differs from the stored float; see the report)
    case 11: {
        desc = "track properties: heartline"; expected = "changeTrackHeartline+changeAnchorPosZ+changeAnchorPosY+changeAnchorPosX";
        track* t = d.openTrackData();
        if(!t) return true;
        double nan = std::numeric_limits<double>::quiet_NaN();
        d.trackProperties(0, nudged(t->fHeart, 0.1, -3, 3), nan, nan);
        return true;
    }
    case 12: {
        desc = "track properties: friction"; expected = "changeTrackFriction";
        track* t = d.openTrackData();
        if(!t) return true;
        double nan = std::numeric_limits<double>::quiet_NaN();
        d.trackProperties(0, nan, nudged(t->fFriction, 0.005, 0, 1), nan);
        return true;
    }
    case 13: {
        desc = "track properties: resistance"; expected = "changeTrackResistance";
        track* t = d.openTrackData();
        if(!t) return true;
        double nan = std::numeric_limits<double>::quiet_NaN();
        d.trackProperties(0, nan, nan, nudged(floor(t->fResistance*1e5*10+0.5)/10, 1.0, 0, 50));
        return true;
    }
    // the first straight section
    case 14:
        desc = "straight: length"; expected = "changeSegmentLength";
        if((s = firstSection(d, straight)) < 0) { note = "no straight section"; return false; }
        d.selectSection(s); nudgePanel(d, "straightLengthBox", d.len(2), d.len(1), d.len(400));
        return true;
    case 15:
        desc = "straight: fixed speed value"; expected = "changeSegmentSpeed";
        if((s = firstSection(d, straight)) < 0) { note = "no straight section"; return false; }
        d.selectSection(s);
        if(!d.panelEnabled("straightSpeedBox")) { note = "the straight section has physics speed"; return false; }
        nudgePanel(d, "straightSpeedBox", d.spd(1), d.spd(1), d.spd(100));
        return true;
    case 16:
        desc = "straight: speed tick"; expected = "changeSpeedState";
        if((s = firstSection(d, straight)) < 0) { note = "no straight section"; return false; }
        d.selectSection(s); d.setPanelChecked("straightSpeedCheck", !d.panelEnabled("straightSpeedBox"));
        return true;
    // the first curved section
    case 17:
        desc = "curved: radius"; expected = "changeCurveRadius";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s); nudgePanel(d, "curvedRadiusBox", d.len(3), d.len(5), d.len(400));
        return true;
    case 18:
        desc = "curved: angle"; expected = "changeSegmentLength";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s); nudgePanel(d, "curvedAngleBox", 10, 5, 300);
        return true;
    case 19:
        desc = "curved: direction"; expected = "changeCurveDirection";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s); nudgePanel(d, "curvedDirectionBox", 15, -180, 180);
        return true;
    case 20:
        desc = "curved: lead in"; expected = "changeCurveLeadIn";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s);
        nudgePanel(d, "curvedLeadInBox", 5, 0, d.panelValue("curvedAngleBox") - d.panelValue("curvedLeadOutBox"));
        return true;
    case 21:
        desc = "curved: lead out"; expected = "changeCurveLeadOut";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s);
        nudgePanel(d, "curvedLeadOutBox", 5, 0, d.panelValue("curvedAngleBox") - d.panelValue("curvedLeadInBox"));
        return true;
    case 22:
        desc = "curved: fixed speed value"; expected = "changeSegmentSpeed";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s);
        if(!d.panelEnabled("curvedSpeedBox")) { note = "the curved section has physics speed"; return false; }
        nudgePanel(d, "curvedSpeedBox", d.spd(1), d.spd(1), d.spd(100));
        return true;
    case 23:
        desc = "curved: speed tick"; expected = "changeSpeedState";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s); d.setPanelChecked("curvedSpeedCheck", !d.panelEnabled("curvedSpeedBox"));
        return true;
    case 24:
        desc = "curved: orientation"; expected = "changeSegmentOrientation";
        if((s = firstSection(d, curved)) < 0) { note = "no curved section"; return false; }
        d.selectSection(s); d.setPanelIndex("orientationBox", d.openTrackData()->lSections[s]->bOrientation ? 0 : 1);
        return true;
    // the first force section
    case 25:
        desc = "force: speed tick"; expected = "changeSpeedState";
        if(!forceSection(d, note, s)) return false;
        d.setPanelChecked("advancedSpeedCheck", !d.panelEnabled("advancedSpeedBox"));
        return true;
    case 26:
        desc = "force: fixed speed value"; expected = "changeSegmentSpeed";
        if(!forceSection(d, note, s)) return false;
        if(!d.panelEnabled("advancedSpeedBox")) { note = "the force section has physics speed"; return false; }
        nudgePanel(d, "advancedSpeedBox", d.spd(1), d.spd(1), d.spd(100));
        return true;
    case 27:
        desc = "force: orientation"; expected = "changeSegmentOrientation";
        if(!forceSection(d, note, s)) return false;
        d.setPanelIndex("orientationBox", d.openTrackData()->lSections[s]->bOrientation ? 0 : 1);
        return true;
    // the first force section's transitions (normal force unless said otherwise)
    case 28:
        desc = "force transition: length"; expected = "onLengthSpin";
        if(!forceSection(d, note, s)) return false;
        if(!pickUnlocked(d, FORCE_ORDER, 3)) { note = "every first transition is locked"; return false; }
        nudgeTransition(d, "lengthSpin", 0.2, 0.2, 1000);
        return true;
    case 29:
        desc = "force transition: change"; expected = "onChangeSpin";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); nudgeTransition(d, "changeSpin", 0.2, -3, 3);
        return true;
    case 30:
        desc = "force transition: shape to sinusoidal"; expected = "onTransitionBox+onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("transitionBox", d.transitionIndex("transitionBox") == 5 ? 0 : 5);
        return true;
    case 31:
        desc = "force transition: shape to quadratic"; expected = "onTransitionBox+onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("transitionBox", 1);
        return true;
    case 32:
        desc = "force transition: quadratic form"; expected = "onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("quadraticBox", d.transitionIndex("quadraticBox") == 0 ? 1 : 0);
        return true;
    case 33:
        desc = "force transition: shape to quartic"; expected = "onTransitionBox+onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("transitionBox", 3);
        return true;
    case 34:
        desc = "force transition: quartic form"; expected = "onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("quarticBox", 2);
        return true;
    case 35:
        desc = "force transition: quartic parameter"; expected = "onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); nudgeTransition(d, "quarticSpin", 0.5, 0, 10);
        return true;
    case 36:
        desc = "force transition: shape to quintic"; expected = "onTransitionBox+onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("transitionBox", 4);
        return true;
    case 37:
        desc = "force transition: quintic form"; expected = "onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.setTransitionIndex("quinticBox", 1);
        return true;
    case 38:
        desc = "force transition: quintic parameter"; expected = "onArg1";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); nudgeTransition(d, "quinticSpin", 1.0, 2, 8);
        return true;
    case 39:
        desc = "force transition: centre"; expected = "onCenterSpin";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); nudgeTransition(d, "centerSpin", 0.5, -10, 10);
        return true;
    case 40:
        desc = "force transition: tension"; expected = "onTensionSpin";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); nudgeTransition(d, "tensionSpin", 0.5, -10, 10);
        return true;
    case 41:
        desc = "force transition: append after roll transition 1"; expected = "appendSubFunction";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcRoll, 0); d.clickTransitionButton("appendButton");
        return true;
    case 42:
        desc = "force transition: prepend before roll transition 1"; expected = "appendSubFunction";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcRoll, 0); d.clickTransitionButton("prependButton");
        return true;
    case 43:
        desc = "force transition: remove roll transition 1"; expected = "removeSubFunction";
        if(!forceSection(d, note, s)) return false;
        if(d.transitionCount(funcRoll) < 2) { note = "roll has one transition"; return false; }
        d.selectTransition(funcRoll, 0); d.clickTransitionButton("removeButton");
        return true;
    case 44:
        desc = "force transition: append after normal transition 1"; expected = "appendSubFunction";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.clickTransitionButton("appendButton");
        return true;
    case 45:
        desc = "force transition: prepend before normal transition 1"; expected = "appendSubFunction";
        if(!forceSection(d, note, s)) return false;
        d.selectTransition(funcNormal, 0); d.clickTransitionButton("prependButton");
        return true;
    case 46:
        desc = "force transition: remove normal transition 1"; expected = "removeSubFunction";
        if(!forceSection(d, note, s)) return false;
        if(d.transitionCount(funcNormal) < 2) { note = "normal force has one transition"; return false; }
        d.selectTransition(funcNormal, 0); d.clickTransitionButton("removeButton");
        return true;
    case 47: {
        desc = "force transition: lock or unlock the last transition"; expected = "changeFunctionStatus";
        if(!forceSection(d, note, s)) return false;
        eFunctype order[] = { funcLateral, funcNormal, funcRoll };
        for(int k = 0; k < 3; ++k) {
            d.selectTransition(order[k], d.transitionCount(order[k]) - 1);
            if(d.transitionEnabled("lockCheck")) {
                d.setTransitionChecked("lockCheck", !d.transitionChecked("lockCheck"));
                return true;
            }
        }
        note = "no transition can be locked or unlocked";
        return false;
    }
    case 48:
        desc = "force: argument"; expected = "changeSegmentArgument";
        if(!forceSection(d, note, s)) return false;
        d.setPanelIndex("argumentBox", d.openTrackData()->lSections[s]->bArgument ? 0 : 1);
        return true;
    // the first geometric section
    case 49:
        desc = "geometric transition: pitch change"; expected = "onChangeSpin";
        if((s = firstSection(d, geometric)) < 0) { note = "no geometric section"; return false; }
        d.selectSection(s); d.selectTransition(funcPitch, 0); nudgeTransition(d, "changeSpin", 2, -30, 30);
        return true;
    case 50:
        desc = "geometric transition: length"; expected = "onLengthSpin";
        if((s = firstSection(d, geometric)) < 0) { note = "no geometric section"; return false; }
        d.selectSection(s);
        if(!pickUnlocked(d, GEO_ORDER, 3)) { note = "every first transition is locked"; return false; }
        nudgeTransition(d, "lengthSpin", 0.2, 0.2, 1000);
        return true;
    case 51:
        desc = "geometric: argument"; expected = "changeSegmentArgument";
        if((s = firstSection(d, geometric)) < 0) { note = "no geometric section"; return false; }
        d.selectSection(s); d.setPanelIndex("argumentBox", d.openTrackData()->lSections[s]->bArgument ? 0 : 1);
        return true;
    case 52:
        desc = "straight transition: roll change"; expected = "onChangeSpin";
        if((s = firstSection(d, straight)) < 0) { note = "no straight section"; return false; }
        d.selectSection(s); d.selectTransition(funcRoll, 0); nudgeTransition(d, "changeSpin", 5, -50, 50);
        return true;
    // structure
    case 53:
        desc = "add a straight section"; expected = "appendSegment";
        d.addSection(straight);
        return true;
    case 54:
        desc = "add a geometric section"; expected = "appendSegment";
        d.addSection(geometric);
        return true;
    case 55:
        desc = "delete a section"; expected = "removeSegment";
        if((s = firstSection(d, curved)) < 0) s = 0;
        if(d.sectionCount() < 2) { note = "only one section"; return false; }
        d.deleteSection(s);
        return true;
    // last, like the first: an anchor edit recomputes the whole track, so the
    // end states B and B2 do not depend on where earlier recomputations started
    case 56:
        desc = "anchor: start speed again"; expected = "changeAnchorSpeed";
        d.selectSection(-1); nudgePanel(d, "anchorSpeedBox", d.spd(-1.5), d.spd(1), d.spd(100));
        return true;
    }
    note = "no such edit";
    return false;
}

// ---------------------------------------------------------------------------
// Undo action types (core/undoaction.h), for the report only
// ---------------------------------------------------------------------------

static const char* ACTION_NAMES[] = {
    "onLengthSpin", "onChangeSpin", "onArg1", "onCenterSpin", "onTensionSpin", "onTransitionBox",
    "appendSubFunction", "removeSubFunction", "changeSegmentLength", "changeCurveDirection",
    "changeCurveLeadIn", "changeCurveLeadOut", "changeCurveRadius", "changeSpeedState",
    "changeSegmentSpeed", "changeAnchorPosX", "changeAnchorPosY", "changeAnchorPosZ",
    "changeAnchorRoll", "changeAnchorPitch", "changeAnchorYaw", "changeAnchorNormal",
    "changeAnchorLateral", "changeAnchorPitchChange", "changeAnchorYawChange", "changeAnchorSpeed",
    "changeTrackHeartline", "changeTrackFriction", "changeTrackResistance", "appendSegment",
    "removeSegment", "changeSegmentArgument", "changeSegmentOrientation", "changeFunctionStatus",
    "newTrack", "deleteTrack"
};

int batchScenarios::actionTypeCount()
{
    return (int)(sizeof(ACTION_NAMES)/sizeof(ACTION_NAMES[0]));
}

QString batchScenarios::actionTypeName(int type)
{
    if(type < 0 || type >= actionTypeCount()) return QString("type%1").arg(type);
    return QString(ACTION_NAMES[type]);
}

bool batchScenarios::actionTypeHandled(int type)
{
    return type != newTrack && type != deleteTrack; // no case in undoAction::doUndo()
}

int batchScenarios::undoDepth(trackHandler* h)
{
    return h ? h->mUndoHandler->lActions.size() : 0;
}

QString batchScenarios::recordedActions(trackHandler* h, int steps, QList<int>& types)
{
    QStringList list;
    if(!h) return QString();
    QList<undoAction*>& actions = h->mUndoHandler->lActions;
    for(int k = steps-1; k >= 0; --k) {
        if(k >= actions.size()) continue;
        QStringList chain;
        for(undoAction* a = actions[k]; a; a = a->nextAction) {
            chain.append(actionTypeName(a->type));
            types.append(a->type);
        }
        list.append(chain.join("+"));
    }
    return list.join(", ");
}
