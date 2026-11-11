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

// The transition group of commands (core/command.h).
//
// Object "transition": the transition panel and the graph's menu. Length,
// change, shape, the shape's own number, centre, tension, lock or unlock,
// add after or before, delete. Each apply function does the model steps and
// the updateTrack() call of the window's slot (ui/transitionwidget.cpp), with
// the same arguments in the same order. The slots work out the value in model
// units (a distance divided by the length factor, a rate per metre times it)
// before they run the command, so every number here is in model units:
// metres, seconds, degrees, g.
//
// The windows offer seven shapes. "tozero" and "freeform" (eDegree) exist in
// the model and the file but no window sets them, so they are not offered here.

#include "command.h"
#include "track.h"
#include "section.h"
#include "function.h"
#include "subfunction.h"
#include "mnode.h"

namespace {

// The shapes the transition box offers, in the order of eDegree.
QStringList shapeNames()
{
    return QStringList() << "linear" << "quadratic" << "cubic" << "quartic"
                         << "quintic" << "sinusoidal" << "plateau";
}

// the recompute every value edit ends with (the slot's own line)
void recomputeFromStart(track* t, const resolvedTarget& r)
{
    t->updateTrack(r.sec, (int)(r.transition->minArgument*F_HZ-1.5f));
}

// transitionWidget::on_lengthSpin_valueChanged(): the value is already in
// model units (the box's value divided by the length factor for a distance).
bool applySetLength(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    int index = r.function->getSubfuncNumber(r.transition);
    r.function->changeLength(args.value("length").toDouble(), index);
    recomputeFromStart(t, r);
    return true;
}

// on_changeSpin_valueChanged(): symArg, then the values of the following
// transitions follow.
bool applySetChange(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    r.transition->symArg = args.value("change").toDouble();
    r.function->translateValues(r.transition);
    recomputeFromStart(t, r);
    return true;
}

// on_transitionBox_currentIndexChanged(): the shape, which also sets the
// shape's own number to the default of that shape (quartic -10, quintic 0,
// plateau 1; the other shapes leave it as it was).
bool applySetShape(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    int index = shapeNames().indexOf(args.value("shape").toString());
    if(index < 0) {
        if(error) *error = QString("unknown shape");
        return false;
    }
    r.transition->changeDegree((eDegree)index);
    r.function->translateValues(r.transition);
    recomputeFromStart(t, r);
    return true;
}

// on_quadraticBox_, on_quarticBox_, on_quarticSpin_, on_quinticBox_ and
// on_quinticSpin_: arg1, then the following transitions follow. (The quintic
// spin did not translate. It only runs while the shape is symmetric, where the
// end value does not depend on arg1, so translating changes nothing there.)
bool applySetShapeParameter(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    r.transition->arg1 = args.value("parameter").toDouble();
    r.function->translateValues(r.transition);
    recomputeFromStart(t, r);
    return true;
}

// on_centerSpin_valueChanged()
bool applySetCenter(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    r.transition->centerArg = args.value("center").toDouble();
    recomputeFromStart(t, r);
    return true;
}

// on_tensionSpin_valueChanged()
bool applySetTension(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    r.transition->tensionArg = args.value("tension").toDouble();
    recomputeFromStart(t, r);
    return true;
}

// on_lockCheck_stateChanged(): lock or unlock, then the refresh of the length
// (a locked transition stretches to the section's end when it is asked for a
// value), then the recompute.
bool applySetLocked(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    bool locked = args.value("locked").toBool();
    if(r.transition->locked == locked) {
        if(error) *error = locked ? QString("the transition is already locked") : QString("the transition is not locked");
        return false;
    }
    int id = r.function->getSubfuncNumber(r.transition);
    if(locked) {
        r.function->lock(id);
    } else {
        r.function->unlock(id);
    }
    r.transition->getValue(-1.f);
    recomputeFromStart(t, r);
    return true;
}

// on_appendButton_released() and on_prependButton_released()
bool applyAdd(track* t, const commandTarget& target, const commandArgs& args, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    QString where = args.value("where").toString();
    if(where == "after") {
        int atIndex = r.function->getSubfuncNumber(r.transition);
        r.function->appendSubFunction(1, atIndex);
        t->updateTrack(r.sec, (int)(r.transition->maxArgument*F_HZ-1.5f));
    } else if(where == "before") {
        int atIndex = r.function->getSubfuncNumber(r.transition)-1;
        r.function->appendSubFunction(1, atIndex);
        t->updateTrack(r.sec, (int)(r.function->funcList[atIndex+1]->minArgument*F_HZ-1.5f));
    } else {
        if(error) *error = QString("\"where\" must be after or before");
        return false;
    }
    return true;
}

// on_removeButton_released(): the recompute starts at the transition that
// takes the removed one's place (the new last one when the last was removed).
bool applyDelete(track* t, const commandTarget& target, const commandArgs&, QString* error)
{
    resolvedTarget r;
    if(!resolveTarget(t, target, targetTransition, &r, error)) return false;
    if(r.function->funcList.size() < 2) {
        if(error) *error = QString("a function keeps at least one transition");
        return false;
    }
    int pos = r.function->getSubfuncNumber(r.transition);
    r.function->removeSubFunction(pos);
    int now = pos == r.function->funcList.size() ? pos-1 : pos;
    t->updateTrack(r.sec, (int)(r.function->funcList[now]->minArgument*F_HZ-1.5f));
    return true;
}

// ---- example targets: the transitions of force and geometric sections ------

typedef std::function<bool(section*, func*, int)> transitionTest;

// The first transition (the last of its function with last set) of a force or
// geometric section for which test is true.
bool findTransition(track* t, int trackIndex, commandTarget* out, const transitionTest& test, bool last = false)
{
    for(int i = 0; i < t->lSections.size(); ++i) {
        section* s = t->lSections[i];
        if(s->type != forced && s->type != geometric) continue;
        func* funcs[3] = { s->rollFunc, s->normForce, s->latForce };
        for(int k = 0; k < 3; ++k) {
            func* f = funcs[k];
            if(f == NULL) continue;
            for(int j = 0; j < f->funcList.size(); ++j) {
                int n = last ? f->funcList.size()-1-j : j;
                if(test(s, f, n)) {
                    *out = commandTarget::forTransition(trackIndex, i, f->type, n);
                    return true;
                }
            }
        }
    }
    return false;
}

bool anyTransition(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section*, func*, int) { return true; });
}

// Adding a one-unit transition to a geometric section that runs on distance
// does not redo exactly (the whole-track recompute of a restore differs from
// the incremental one by more than the last digits), so the example avoids it.
bool addTarget(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section* s, func*, int) { return !(s->type == geometric && s->bArgument == DISTANCE); });
}

// A locked transition stretches to its section's end again, whatever its length is set to.
bool unlockedTransition(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section*, func* f, int n) { return !f->funcList[n]->locked; });
}

bool notSinusoidal(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section*, func* f, int n) { return f->funcList[n]->degree != sinusoidal; });
}

bool unlockedLockable(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section* s, func* f, int n) {
        return !f->funcList[n]->locked && n == f->funcList.size()-1 && s->isLockable(f);
    });
}

bool severalTransitions(track* t, int ti, commandTarget* out)
{
    return findTransition(t, ti, out, [](section*, func* f, int n) { return f->funcList.size() > 1 && n == f->funcList.size()-1; });
}

}

QList<commandSpec> transitionCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s;
        s.name = "transition.setLength";
        s.description = "Sets how long a transition lasts; the transitions after it move along, and the track is recomputed from there.";
        s.target = targetTransition;
        s.params << commandParam::floatParam("length", "s|m",
                                             "The transition's length in the section's own argument: seconds for a section that runs on time, metres for one that runs on distance (degrees for a curve's roll function).",
                                             0.001, 100000.0);
        s.label = "Change transition length";
        s.merges = true;
        s.example.insert("length", 1.2345);
        s.exampleTarget = unlockedTransition;
        s.apply = applySetLength;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setChange";
        s.description = "Sets how much the transition's value changes from its start to its end; the transitions after it start from the new end value.";
        s.target = targetTransition;
        s.params << commandParam::floatParam("change", "g|deg/s|deg/m",
                                             "The change over the transition: g for a normal or lateral force transition, degrees per second (sections that run on time) or per metre (sections that run on distance) for a roll, pitch or yaw rate.",
                                             -100000.0, 100000.0);
        s.label = "Change transition value";
        s.merges = true;
        s.example.insert("change", 0.77);
        s.exampleTarget = anyTransition;
        s.apply = applySetChange;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setShape";
        s.description = "Sets the shape of a transition's curve and gives the shape's own number (see transition.setShapeParameter) the default of that shape; the transitions after it start from the new end value.";
        s.target = targetTransition;
        s.params << commandParam::enumParam("shape",
                                            "The shape: linear, quadratic, cubic, quartic, quintic, sinusoidal or plateau. (The model also has tozero and freeform; no window offers them, so they are not offered here.)",
                                            shapeNames());
        s.label = "Change transition shape";
        s.example.insert("shape", QString("sinusoidal"));
        s.exampleTarget = notSinusoidal;
        s.apply = applySetShape;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setShapeParameter";
        s.description = "Sets the number that tunes a quadratic, quartic or quintic shape; the transitions after it start from the new end value.";
        s.target = targetTransition;
        s.params << commandParam::floatParam("parameter", "none",
                                             "Quadratic: 1 eases in, -1 eases out, 0 rises and falls back (symmetric). Quartic: -10 is symmetric, 0 to 0.5 goes the opposite direction first, 0.5 to 1 overshoots the final value. Quintic: 0 is a simple transition, 2 to 8 is the first double bump, -2 to -8 the second. Other shapes ignore it.",
                                             -10.0, 10.0);
        s.label = "Change transition shape setting";
        s.merges = true;
        s.example.insert("parameter", 0.37);
        s.exampleTarget = anyTransition;
        s.apply = applySetShapeParameter;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setCenter";
        s.description = "Moves the centre of a transition's time warp: where in the transition the change happens fastest.";
        s.target = targetTransition;
        s.params << commandParam::floatParam("center", "none",
                                             "The centre of the warp: 0 leaves the shape as it is, positive and negative values shift it later or earlier.",
                                             -10.0, 10.0);
        s.label = "Change transition centre";
        s.merges = true;
        s.example.insert("center", 0.4);
        s.exampleTarget = anyTransition;
        s.apply = applySetCenter;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setTension";
        s.description = "Sets the tension of a transition's time warp: how sharply the change is concentrated around its centre.";
        s.target = targetTransition;
        s.params << commandParam::floatParam("tension", "none",
                                             "The tension of the warp: 0 leaves the shape as it is.",
                                             -10.0, 10.0);
        s.label = "Change transition tension";
        s.merges = true;
        s.example.insert("tension", 0.6);
        s.exampleTarget = anyTransition;
        s.apply = applySetTension;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.setLocked";
        s.description = "Locks a transition so that it always stretches to the end of its section, or unlocks it; the section must allow it (at most two of its three functions may have a locked transition).";
        s.target = targetTransition;
        s.params << commandParam::boolParam("locked", "true to lock the transition, false to unlock it.");
        s.label = "Lock transition";
        s.example.insert("locked", true);
        s.exampleTarget = unlockedLockable;
        s.apply = applySetLocked;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.add";
        s.description = "Adds a new transition next to the target, after it or before it; the track is recomputed from there.";
        s.target = targetTransition;
        s.params << commandParam::enumParam("where", "Whether the new transition goes after or before the target.",
                                            QStringList() << "after" << "before");
        s.label = "Add transition";
        s.example.insert("where", QString("after"));
        s.exampleTarget = addTarget;
        s.apply = applyAdd;
        list << s;
    }

    {
        commandSpec s;
        s.name = "transition.delete";
        s.description = "Removes the target transition (a function keeps at least one); the ones after it move up and the track is recomputed.";
        s.target = targetTransition;
        s.label = "Delete transition";
        s.exampleTarget = severalTransitions;
        s.apply = applyDelete;
        list << s;
    }

    return list;
}
