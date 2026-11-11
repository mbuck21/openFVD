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

#include "trackhistory.h"
#include "track.h"

#include <QElapsedTimer>
#include <QtGlobal>

namespace {

qint64 defaultClock()
{
    static QElapsedTimer timer;
    if(!timer.isValid()) timer.start();
    return timer.elapsed();
}

}

trackHistory::trackHistory(track* t)
    : mTrack(t),
      mPosition(0),
      mMergeWindowMs(0),
      mMergeOpen(false),
      mClock(defaultClock),
      mMaxSteps(50000),
      mMaxBytes(qint64(128)*1024*1024),
      mBytes(0),
      mListener(NULL)
{
    mHead = captureTrackState(mTrack);
}

trackHistory::~trackHistory()
{
}

// ---- scopes -----------------------------------------------------------------

void trackHistory::beginStep(const QString& label, const commandTarget& selection, const QString& mergeKey)
{
    scope s;
    if(mScopes.isEmpty()) {
        trackState now = captureTrackState(mTrack, &mHead);
        if(now == mHead) {
            s.before = mHead;
        } else {
            // Changed outside any step and not noted: the change becomes part
            // of this step's state before and can never be undone on its own.
            // Kept as it is, but said, so that a missing noteEdit() shows.
            qWarning("trackHistory: the track changed outside the history before step \"%s\"; "
                     "that change is folded into this step and cannot be undone on its own",
                     label.toUtf8().constData());
            s.before = now;
        }
        s.callCount = 0;
        mScopeLabel = label;
        mScopeSelection = selection;
        mScopeMergeKey = mergeKey;
        mScopeCalls.clear();
    } else {
        // joins the outer step; keeps its own start for its own rollback
        s.before = captureTrackState(mTrack, &mScopes.last().before);
        s.callCount = mScopeCalls.size();
    }
    mScopes.append(s);
}

void trackHistory::endStep(bool commit)
{
    if(mScopes.isEmpty()) {
        qWarning("trackHistory::endStep() without beginStep()");
        return;
    }
    scope s = mScopes.takeLast();
    bool outermost = mScopes.isEmpty();

    if(!commit) {
        // Rollback: put back the state this scope began with, unless nothing
        // changed (a restore recomputes the whole track; not needed then).
        // A restore deletes and rebuilds every section, function and
        // transition: pointers into the track taken before are dead after it.
        trackState now = captureTrackState(mTrack, &s.before);
        bool restored = now != s.before;
        if(restored) {
            restoreTrackState(mTrack, s.before);
        }
        while(mScopeCalls.size() > s.callCount) mScopeCalls.removeLast();
        if(outermost) {
            // The head is the track as it now is: on some tracks a restore's
            // full recompute leaves live values a little different from the
            // state put back (trackstate.h), which is not an outside change.
            if(restored) {
                headAfterRestore(s.before);
            } else {
                mHead = s.before;
            }
            mScopeCalls.clear();
        }
        return;
    }

    if(!outermost) {
        return;     // a nested scope joins the outer step
    }

    // DEPENDENTS: here, after the step's edits and before its state after is
    // taken, whatever depends on the track (elements, pinned points) will
    // re-solve in later stages, so that the edit and its consequences are one
    // step. Nothing happens here now.

    trackState after = captureTrackState(mTrack, &s.before);
    QList<commandCall> calls = mScopeCalls;
    mScopeCalls.clear();
    if(after == s.before) {
        mHead = s.before;
        return;     // nothing changed: nothing recorded
    }
    record(mScopeLabel, mScopeSelection, mScopeMergeKey, calls, s.before, after);
}

void trackHistory::recordCall(const commandCall& call)
{
    if(mScopes.isEmpty()) {
        qWarning("trackHistory::recordCall() outside a scope");
        return;
    }
    mScopeCalls.append(call);
}

// ---- outside edits ----------------------------------------------------------

void trackHistory::noteEdit(const QString& label, const commandTarget& selection, const QString& mergeKey)
{
    if(inScope()) return;
    trackState after = captureTrackState(mTrack, &mHead);
    if(after == mHead) return;
    record(label, selection, mergeKey, QList<commandCall>(), mHead, after);
}

void trackHistory::rebase()
{
    if(inScope()) return;
    mHead = captureTrackState(mTrack, &mHead);
    mMergeOpen = false;
}

void trackHistory::clear()
{
    if(inScope()) return;
    mSteps.clear();
    mPosition = 0;
    mBytes = 0;
    rebase();
    notify();
}

// ---- recording --------------------------------------------------------------

void trackHistory::record(const QString& label, const commandTarget& selection, const QString& mergeKey,
                          const QList<commandCall>& calls, const trackState& before, const trackState& after)
{
    qint64 now = mClock();
    bool merge = mMergeWindowMs > 0 && mMergeOpen && !mergeKey.isEmpty()
            && mPosition > 0 && mPosition == mSteps.size()
            && mSteps[mPosition-1].mergeKey == mergeKey
            && now - mSteps[mPosition-1].time <= mMergeWindowMs;

    if(merge) {
        historyStep& head = mSteps[mPosition-1];
        head.after = after;
        head.calls += calls;
        head.selection = selection;
        head.time = now;
        mBytes -= head.cost;
        if(head.after == head.before) {
            // typed back to where the run started: the step is gone
            mSteps.removeAt(mPosition-1);
            --mPosition;
            mMergeOpen = false;
        } else {
            head.cost = stepCost(mPosition-1);
            mBytes += head.cost;
        }
    } else {
        dropRedoBranch();
        historyStep step;
        step.label = label;
        step.calls = calls;
        step.before = before;
        step.after = after;
        step.selection = selection;
        step.mergeKey = mergeKey;
        step.time = now;
        mSteps.append(step);
        mPosition = mSteps.size();
        mSteps.last().cost = stepCost(mPosition-1);
        mBytes += mSteps.last().cost;
        mMergeOpen = !mergeKey.isEmpty();
    }
    enforceLimits();
    mHead = after;
    notify();
}

void trackHistory::dropRedoBranch()
{
    while(mSteps.size() > mPosition) {
        mBytes -= mSteps.last().cost;
        mSteps.removeLast();
    }
}

qint64 trackHistory::stepCost(int index) const
{
    const historyStep& s = mSteps[index];
    qint64 cost = s.after.bytesNotSharedWith(s.before);
    if(index == 0) {
        cost += s.before.byteSize();
    } else {
        cost += s.before.bytesNotSharedWith(mSteps[index-1].after);
    }
    return cost;
}

void trackHistory::enforceLimits()
{
    // Only steps already applied are dropped (a step to redo needs the one
    // before it), and the newest step is always kept.
    bool dropped = false;
    while(mSteps.size() > 1 && mPosition > 0 && (mSteps.size() > mMaxSteps || mBytes > mMaxBytes)) {
        mBytes -= mSteps.first().cost;
        mSteps.removeFirst();
        --mPosition;
        dropped = true;
    }
    if(dropped && !mSteps.isEmpty()) {
        // the new first step now holds its whole state before
        mBytes -= mSteps.first().cost;
        mSteps.first().cost = stepCost(0);
        mBytes += mSteps.first().cost;
        if(mBytes > mMaxBytes) enforceLimits();
    }
}

void trackHistory::setMaxSteps(int steps)
{
    mMaxSteps = qMax(1, steps);
    int before = mSteps.size();
    enforceLimits();
    if(mSteps.size() != before) notify();
}

void trackHistory::setMaxBytes(qint64 bytes)
{
    mMaxBytes = bytes;
    int before = mSteps.size();
    enforceLimits();
    if(mSteps.size() != before) notify();
}

void trackHistory::setClock(clockFn clock)
{
    mClock = clock != NULL ? clock : defaultClock;
}

// The head after restoreTrackState(state): the track as it now is. On some
// tracks the restore's full recompute leaves live values a little different
// from state (trackstate.h); that is not an outside change, so the next
// beginStep() must not warn about it.
void trackHistory::headAfterRestore(const trackState& state)
{
    trackState now = captureTrackState(mTrack, &state);
    mHead = now == state ? state : now;
}

void trackHistory::notify()
{
    if(mListener != NULL) mListener->historyChanged(this);
}

// ---- undo and redo ----------------------------------------------------------

bool trackHistory::canUndo() const
{
    return !inScope() && mPosition > 0;
}

bool trackHistory::canRedo() const
{
    return !inScope() && mPosition < mSteps.size();
}

QString trackHistory::undoLabel() const
{
    return mPosition > 0 ? mSteps[mPosition-1].label : QString();
}

QString trackHistory::redoLabel() const
{
    return mPosition < mSteps.size() ? mSteps[mPosition].label : QString();
}

QStringList trackHistory::labels() const
{
    QStringList out;
    for(int i = 0; i < mSteps.size(); ++i) out.append(mSteps[i].label);
    return out;
}

bool trackHistory::undo(commandTarget* selection)
{
    if(!canUndo()) return false;
    mMergeOpen = false;
    trackState state = mSteps[mPosition-1].before;
    commandTarget sel = mSteps[mPosition-1].selection;
    --mPosition;
    restoreTrackState(mTrack, state);   // rebuilds the track: old pointers are dead
    headAfterRestore(state);
    if(selection != NULL) *selection = sel;
    notify();
    return true;
}

bool trackHistory::redo(commandTarget* selection)
{
    if(!canRedo()) return false;
    mMergeOpen = false;
    trackState state = mSteps[mPosition].after;
    commandTarget sel = mSteps[mPosition].selection;
    ++mPosition;
    restoreTrackState(mTrack, state);   // rebuilds the track: old pointers are dead
    headAfterRestore(state);
    if(selection != NULL) *selection = sel;
    notify();
    return true;
}

// ---- historyScope -----------------------------------------------------------

historyScope::historyScope(trackHistory* history, const QString& label,
                           const commandTarget& selection, const QString& mergeKey)
    : mHistory(history), mOpen(true)
{
    mHistory->beginStep(label, selection, mergeKey);
}

historyScope::~historyScope()
{
    if(mOpen) mHistory->endStep(false);
}

void historyScope::commit()
{
    if(!mOpen) return;
    mOpen = false;
    mHistory->endStep(true);
}

void historyScope::rollback()
{
    if(!mOpen) return;
    mOpen = false;
    mHistory->endStep(false);
}
