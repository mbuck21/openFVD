#ifndef TRACKHISTORY_H
#define TRACKHISTORY_H

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

#include <QList>
#include <QString>
#include <QStringList>

#include "trackstate.h"
#include "command.h"

class track;
class trackHistory;

// One command as it was run: what a step is made of.
struct commandCall
{
    QString name;
    commandTarget target;
    commandArgs args;
};

// One undo step: a label, the command calls that made it (empty for an edit
// made outside a command), the track's state before and after, and the
// selection to show again when the step is undone or redone.
struct historyStep
{
    QString label;
    QList<commandCall> calls;
    trackState before;
    trackState after;
    commandTarget selection;
    QString mergeKey;       // empty: never merged
    qint64 time;            // the history clock when the step last changed (ms)
    qint64 cost;            // bytes this step adds to the history (see trackHistory::bytes())

    historyStep() : time(0), cost(0) {}
};

// Told whenever the list of steps or the position in it changes (a step
// recorded, merged or dropped, an undo, a redo, a clear), so the window can
// enable and label its Undo and Redo entries from one place.
class trackHistoryListener
{
public:
    virtual ~trackHistoryListener() {}
    virtual void historyChanged(trackHistory* which) = 0;
};

// The undo history of one track (docs/decisions/0011-undo-by-snapshot-commands-on-top.md).
// Holds the track (does not own it). Every step keeps the track's whole state
// before and after (trackState); undo and redo put one back with
// restoreTrackState(), which fires the track's own trackAboutToReset and
// trackReset notices. Successive states share the bytes of unchanged sections.
//
// Changes are recorded in one of two ways:
// - a scope (beginStep() ... endStep(), or historyScope): the state is taken
//   at the start and at the end; commandRegistry::run() opens one per command.
// - noteEdit(), after the model was already changed (the window's edit slots
//   until they are converted to commands).
// The head state is the state after the last recorded step (or after
// rebase()): noteEdit() takes it as the state before. A change made outside
// any step and not noted is folded into the next step's state before and can
// never be undone on its own; beginStep() warns (qWarning) when it finds one.
//
// Pointers do not survive a restore. A rollback, an undo and a redo all go
// through restoreTrackState(), which deletes and rebuilds every section,
// function, transition and node: any section*, func* or subfunc* taken before
// is dead afterwards. Find every target again from its numbers
// (commandTarget, resolveTarget()).
//
// How a solver uses a scope: it opens one scope, changes values and
// recomputes directly inside it as many times as it needs, and at the end
// either commits once (after a final full recompute, updateTrack(0, 0), so
// that the committed state is the one undo and redo reproduce) or rolls back
// once. A rollback costs a full rebuild and recompute (about 0.1 s on a large
// track): it is the way to abandon a scope, not something to do per trial value.
class trackHistory
{
public:
    typedef qint64 (*clockFn)();    // milliseconds, any origin, never going back

    explicit trackHistory(track* t);   // takes the track's state as the head state
    ~trackHistory();

    track* getTrack() const { return mTrack; }

    // ---- scopes ----
    // Opens a step. The outermost call keeps the track's state now (sharing the
    // head state's bytes where unchanged) with the label, the selection and the
    // merge key. A nested call joins the outer step: its label and key are
    // ignored, but it keeps the state at its start so that its own rollback
    // can put that back. When the outermost call finds the track different
    // from the head state (a change made outside any step and not noted), it
    // warns with qWarning, naming the label, and goes on: that change becomes
    // part of this step's state before.
    void beginStep(const QString& label, const commandTarget& selection = commandTarget(),
                   const QString& mergeKey = QString());
    // Closes the innermost open scope. commit true: the outermost one records
    // the step (nothing when the state did not change; a merge with the head
    // step when its key allows it). commit false (rollback): the track is put
    // back as it was when this scope began, its calls are forgotten, and
    // nothing is recorded. A rollback that has something to undo rebuilds the
    // whole track (restoreTrackState()): every section*, func* and subfunc*
    // taken before it is dead afterwards; find targets again from their
    // numbers. It is the way to abandon a scope, not a per-trial reset (see
    // the class comment).
    void endStep(bool commit);
    // Adds a call to the open step (commandRegistry::run() does this).
    void recordCall(const commandCall& call);
    bool inScope() const { return !mScopes.isEmpty(); }
    int scopeDepth() const { return mScopes.size(); }

    // ---- outside edits ----
    // The model was changed already: records a step from the head state to
    // the state now. Nothing when nothing changed. Ignored inside a scope (the
    // scope records the change).
    void noteEdit(const QString& label, const commandTarget& selection = commandTarget(),
                  const QString& mergeKey = QString());
    // The track changed outside the history (a load, an import): take its
    // state as the head state without recording a step. Ends a merge run.
    void rebase();
    // The track was changed by an edit that is not a step (a rename, a colour,
    // a dialog): takes its state as the newest applied step's state after (or
    // as the head state when no step is applied), so that the next step's state
    // before includes the change and undoing that step keeps it. Undoing an
    // older step still takes it back. Like a recorded step it ends Redo: the
    // steps that could be redone are dropped. Records no step; nothing when
    // nothing changed. Differs from rebase(), which keeps the steps and the
    // redo branch.
    void foldOutsideChange();
    // Forgets every step and rebases.
    void clear();

    // ---- undo and redo ----
    // Refused (false) while a scope is open or when there is nothing to undo or
    // redo. On success the step's selection is written to selection. Both
    // rebuild the whole track: every section*, func* and subfunc* taken before
    // is dead afterwards; find targets again from their numbers.
    bool undo(commandTarget* selection = NULL);
    bool redo(commandTarget* selection = NULL);
    bool canUndo() const;
    bool canRedo() const;
    QString undoLabel() const;      // empty when there is nothing to undo
    QString redoLabel() const;

    // The steps, oldest first, and how many of them are applied: steps
    // [0, position()) can be undone, [position(), size) redone.
    const QList<historyStep>& steps() const { return mSteps; }
    int position() const { return mPosition; }
    QStringList labels() const;
    const trackState& headState() const { return mHead; }

    // ---- merging ----
    // A step with the same non-empty merge key as the head step, recorded
    // within mergeWindowMs of the head step's last change, extends the head
    // step (its state before stays, its state after and its calls grow).
    // 0 (the default) never merges. A run also ends at endMergeRun(), at an
    // undo or a redo, a rebase, and at a step with another key.
    void setMergeWindow(int ms) { mMergeWindowMs = ms; }
    int mergeWindow() const { return mMergeWindowMs; }
    void endMergeRun() { mMergeOpen = false; }
    void setClock(clockFn clock);   // NULL: the default monotonic clock

    // ---- limits ----
    // When the number of steps or the bytes they hold exceed a cap, the oldest
    // steps are dropped (the newest step is always kept).
    void setMaxSteps(int steps);            // default 50000 (the window's maxUndoChanges)
    void setMaxBytes(qint64 bytes);         // default 128 MB
    int maxSteps() const { return mMaxSteps; }
    qint64 maxBytes() const { return mMaxBytes; }
    // The bytes the steps hold, each shared section counted once: the first
    // step's whole state before, then for each step what its state before does
    // not share with the previous step's state after, and what its state
    // after does not share with its state before.
    qint64 bytes() const { return mBytes; }

    void setListener(trackHistoryListener* listener) { mListener = listener; }

private:
    struct scope
    {
        trackState before;
        int callCount;      // the calls of the open step when this scope began
    };

    void record(const QString& label, const commandTarget& selection, const QString& mergeKey,
                const QList<commandCall>& calls, const trackState& before, const trackState& after);
    void dropRedoBranch();
    qint64 stepCost(int index) const;
    void enforceLimits();
    void headAfterRestore(const trackState& state);
    void notify();

    track* mTrack;
    bool mRestoring;        // an undo or a redo is putting a state back: nothing is recorded
    trackState mHead;
    QList<historyStep> mSteps;
    int mPosition;

    QList<scope> mScopes;
    QString mScopeLabel;
    commandTarget mScopeSelection;
    QString mScopeMergeKey;
    QList<commandCall> mScopeCalls;

    int mMergeWindowMs;
    bool mMergeOpen;
    clockFn mClock;

    int mMaxSteps;
    qint64 mMaxBytes;
    qint64 mBytes;

    trackHistoryListener* mListener;
};

// A scope as an object: begins a step when made, and ends it when it goes out
// of use, rolled back unless commit() was called. So an early return or an
// error leaves no trace. A rollback that has something to undo rebuilds the
// whole track: every section*, func* and subfunc* taken before it is dead
// afterwards; find targets again from their numbers.
class historyScope
{
public:
    historyScope(trackHistory* history, const QString& label,
                 const commandTarget& selection = commandTarget(), const QString& mergeKey = QString());
    ~historyScope();
    void commit();
    void rollback();
private:
    historyScope(const historyScope&);
    historyScope& operator=(const historyScope&);
    trackHistory* mHistory;
    bool mOpen;
};

#endif // TRACKHISTORY_H
