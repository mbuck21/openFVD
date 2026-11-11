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

// The smoothing group of commands (core/command.h): what the smoothing
// dialog edits (ui/smoothui.cpp).
//
// A track's smoothing entries (track::smoothList): entry 0 is the whole
// track, entries 1 to n belong to the n sections, and the entries after them
// are custom regions (the dialog labels them a, b, c, ...). An entry's length
// and range are stored in nodes (1/F_HZ s, so milliseconds); the commands take
// seconds, as the dialog shows them, and convert as the dialog does.
//
// Changing an entry reshapes the track only when the track is smoothed
// again: setEntry, addRegion and removeRegion do it themselves when their
// "apply" is true (what a script passes), with the full recompute an undo or
// a redo runs, so the track a lone command leaves is exactly the one undo and
// redo reproduce. The dialog passes false for each change and smooths once,
// at OK (smoothing.apply, the dialog's own arithmetic: smoothing applied over
// the track as it is), all of it in one undo step.
//
// Every change tells the entries' listener (the dialog's list), so a change
// made from outside the dialog shows in it.

#include "command.h"
#include "track.h"
#include "smoothhandler.h"

namespace {

// seconds as the dialog's boxes give them, to nodes as the dialog stores them
int nodesFromSeconds(double seconds)
{
    return (int)(seconds*1000.+0.5);
}

QString entryRangeText(track* t)
{
    return QString("the track has entries 0 to %1").arg(t->smoothList.size()-1);
}

// What the dialog's OK does, after the dialog had brought every entry up to
// date from the track when it opened: the same refresh (for the dialog it
// changes nothing, nothing having changed the sections since; for a script it
// takes in any section edits), then the roll smoothing applied. The track is
// marked as one whose smoothing is re-applied by every recompute
// (bAutoSmooth, which a loaded track and a track with a smoothing dialog have
// already), so that a recompute (and so undo and redo) keeps the smoothing.
void smoothNow(track* t)
{
    t->bAutoSmooth = true;
    char customChar;
    t->updateSmoothEntries(&customChar);
    t->applyRollSmooth();
}

// "apply" of the other commands: the same refresh and switch, then the whole
// track recomputed and smoothed (track::updateTrack(0, 0), which re-applies
// the smoothing because bAutoSmooth is on), as restoreTrackState() does for
// an undo or a redo. Applying the smoothing again over a track that is
// smoothed already (smoothNow()) can end a last digit away from that.
void recomputeSmoothed(track* t)
{
    t->bAutoSmooth = true;
    char customChar;
    t->updateSmoothEntries(&customChar);
    t->updateTrack(0, 0);
}

// The dialog's edits of one entry, each its own slot there: the length box,
// the passes box (setIterations), the "enabled" tick, and for a custom region
// the from and to boxes and the name typed into the list. Only the values that
// differ are written, so an edit of one of them writes that one alone, as the
// slot did. A whole-track or section entry takes its range and name from the
// track (smoothHandler::update()); from, to and name are not used for it.
bool applySetEntry(track* t, const commandTarget&, const commandArgs& args, QString* error)
{
    int entry = args.value("entry").toInt();
    if(entry < 0 || entry >= t->smoothList.size()) {
        if(error) *error = QString("\"entry\": there is no smoothing entry %1 (%2)").arg(entry).arg(entryRangeText(t));
        return false;
    }
    smoothHandler* cur = t->smoothList[entry];
    int length = nodesFromSeconds(args.value("length").toDouble());
    int passes = args.value("passes").toInt();
    bool on = args.value("on").toBool();
    bool custom = cur->sec == NULL;
    int from = nodesFromSeconds(args.value("from").toDouble());
    int to = nodesFromSeconds(args.value("to").toDouble());
    QString name = args.value("name").toString();

    // the dialog's from and to boxes stop at the track's end
    if(custom) {
        const int max = t->getNumPoints();
        if(from != cur->getFrom() && from > max) {
            if(error) *error = QString("\"from\": %1 s is past the track's end (%2 s)").arg(from/1000.).arg(max/1000.);
            return false;
        }
        if(to != cur->getTo() && to > max) {
            if(error) *error = QString("\"to\": %1 s is past the track's end (%2 s)").arg(to/1000.).arg(max/1000.);
            return false;
        }
    }

    if(cur->getLength() != length) cur->setLength(length);
    if(cur->getIterations() != passes) cur->setIterations(passes);
    if(custom) {
        if(cur->getFrom() != from) cur->setFrom(from);
        if(cur->getTo() != to) cur->setTo(to);
        if(cur->sName != name) cur->setName(name);
    }
    if(cur->active != on) cur->setActive(on);
    if(args.value("apply").toBool()) recomputeSmoothed(t);
    return true;
}

// The dialog's "New" button: a custom region appended to the list, then every
// entry brought up to date (track::updateSmoothEntries(), which also labels
// the custom regions a, b, c, ...), as the dialog's updateUi() did. The
// dialog's region is 0 to 1 s, 0.4 s long, one pass, on, "custom Region". The
// region gets the listener of the track's other entries (the dialog's list,
// or none), so the list learns of it when the refresh tells it.
bool applyAddRegion(track* t, const commandTarget&, const commandArgs& args, QString*)
{
    char customChar = 'a';
    smoothHandler* region = new smoothHandler(t, -2, &customChar,
                                              nodesFromSeconds(args.value("length").toDouble()),
                                              args.value("passes").toInt(),
                                              nodesFromSeconds(args.value("from").toDouble()),
                                              nodesFromSeconds(args.value("to").toDouble()));
    QString name = args.value("name").toString();
    if(region->sName != name) region->sName = name;
    region->active = args.value("on").toBool();
    region->mListener = t->smoothList.isEmpty() ? NULL : t->smoothList[0]->mListener;
    t->smoothList.append(region);
    t->updateSmoothEntries(&customChar);
    if(args.value("apply").toBool()) recomputeSmoothed(t);
    return true;
}

// The dialog's "Remove" button (only for a custom region): the entry leaves
// the list first, then is deleted (its destructor tells the dialog, which must
// not find it in the list any more).
bool applyRemoveRegion(track* t, const commandTarget&, const commandArgs& args, QString* error)
{
    int region = args.value("region").toInt();
    int index = t->lSections.size() + region;
    int regions = t->smoothList.size() - t->lSections.size() - 1;
    if(index >= t->smoothList.size() || t->smoothList[index]->sec != NULL) {
        if(error) *error = QString("\"region\": there is no custom region %1 (the track has %2)").arg(region).arg(regions);
        return false;
    }
    smoothHandler* entry = t->smoothList.takeAt(index);
    delete entry;
    if(args.value("apply").toBool()) recomputeSmoothed(t);
    return true;
}

// The dialog's OK.
bool applyApply(track* t, const commandTarget&, const commandArgs&, QString*)
{
    smoothNow(t);
    return true;
}

bool hasCustomRegion(track* t, int trackIndex, commandTarget* out)
{
    if(t->smoothList.size() <= t->lSections.size() + 1) return false;
    *out = commandTarget::forTrack(trackIndex);
    return true;
}

commandParam applyParam()
{
    return commandParam::boolParam("apply", "True to reshape the track now (recompute it and smooth it as its entries say); "
                                            "the smoothing dialog passes false for each change and smooths once, at OK (smoothing.apply).");
}

}

QList<commandSpec> smoothingCommands()
{
    QList<commandSpec> list;

    {
        commandSpec s;
        s.name = "smoothing.setEntry";
        s.description = "Sets the values of one roll-smoothing entry (the whole track, a section, or a custom region), and with apply smooths the track as the entries now say.";
        s.target = targetTrack;
        s.params << commandParam::intParam("entry", "none",
                                           "The entry: 0 is the whole track, 1 to n the track's n sections, n+1 and on the custom regions (a, b, ...).")
                 << commandParam::floatParam("length", "s",
                                             "The smoothing filter's length (0.4 s for a new entry; the time is rounded to whole milliseconds).",
                                             0.0, 6.0)
                 << commandParam::intParam("passes", "none", "How many times the filter runs over the entry.", 1, 16)
                 << commandParam::boolParam("on", "True to smooth this entry, false to leave it as it is.")
                 << commandParam::floatParam("from", "s",
                                             "Where a custom region starts, as time from the track's start (rounded to whole milliseconds; not used for the whole track or a section, which follow the track).",
                                             0.0, 900000.0)
                 << commandParam::floatParam("to", "s",
                                             "Where a custom region ends, as time from the track's start, at most the track's end (not used for the whole track or a section).",
                                             0.0, 900000.0)
                 << commandParam::textParam("name", "A custom region's name (not used for the whole track or a section, which carry the track's or the section's name).")
                 << applyParam();
        s.label = "Change smoothing";
        s.example.insert("entry", 0);
        s.example.insert("length", 0.437);
        s.example.insert("passes", 3);
        s.example.insert("on", true);
        s.example.insert("from", 0.0);
        s.example.insert("to", 1.0);
        s.example.insert("name", QString("not used for entry 0"));
        s.example.insert("apply", true);
        s.apply = applySetEntry;
        list << s;
    }

    {
        commandSpec s;
        s.name = "smoothing.addRegion";
        s.description = "Adds a custom roll-smoothing region, a span of the track given by time, after the existing entries, and with apply smooths the track as the entries now say.";
        s.target = targetTrack;
        s.params << commandParam::floatParam("from", "s",
                                             "Where the region starts, as time from the track's start (the dialog's New makes 0 s).",
                                             0.0, 900000.0)
                 << commandParam::floatParam("to", "s",
                                             "Where the region ends, as time from the track's start (the dialog's New makes 1 s).",
                                             0.0, 900000.0)
                 << commandParam::floatParam("length", "s", "The smoothing filter's length (the dialog's New makes 0.4 s).", 0.0, 6.0)
                 << commandParam::intParam("passes", "none", "How many times the filter runs over the region (the dialog's New makes 1).", 1, 16)
                 << commandParam::boolParam("on", "True to smooth the region (the dialog's New makes it on).")
                 << commandParam::textParam("name", "The region's name (the dialog's New makes \"custom Region\").")
                 << applyParam();
        s.label = "Add smoothing region";
        s.example.insert("from", 0.0);
        s.example.insert("to", 1.0);
        s.example.insert("length", 0.4);
        s.example.insert("passes", 1);
        s.example.insert("on", true);
        s.example.insert("name", QString("custom Region"));
        s.example.insert("apply", true);
        s.apply = applyAddRegion;
        list << s;
    }

    {
        commandSpec s;
        s.name = "smoothing.removeRegion";
        s.description = "Removes a custom roll-smoothing region, and with apply smooths the track as the remaining entries say.";
        s.target = targetTrack;
        s.params << commandParam::intParam("region", "none",
                                           "The custom region, counted from 1 (the first region, labelled a, is entry n+1 of a track with n sections).", 1, 1000)
                 << applyParam();
        s.label = "Remove smoothing region";
        s.example.insert("region", 1);
        s.example.insert("apply", true);
        s.exampleTarget = hasCustomRegion;
        s.apply = applyRemoveRegion;
        list << s;
    }

    {
        commandSpec s;
        s.name = "smoothing.apply";
        s.description = "Smooths the track's roll as its smoothing entries say (the smoothing dialog's OK).";
        s.target = targetTrack;
        s.label = "Apply smoothing";
        s.exampleMayChangeNothing = true;   // a loaded project is smoothed already
        s.apply = applyApply;
        list << s;
    }

    return list;
}
