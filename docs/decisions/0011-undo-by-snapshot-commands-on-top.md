# 0011: Undo by snapshot; named commands on top

- Status: accepted
- Date: 2026-11-11
- Needs owner approval: no for the mechanism (how to build it; the architect's call under record 0006). **Four changes the owner will notice were approved with the Stage 2 plan on 2026-11-11** and are listed below.

## Context

- Stage 2's piece T1 replaces today's undo with the command list (record 0009; roadmap, Stage 2).
- An inventory of 2026-11-11 found 34 kinds of edit with undo, each re-applied by its own code in two large `switch` statements (`ui/undoaction.cpp`). Eight kinds come back wrong (issue 31). About twenty kinds of edit have no undo at all: the whole smoothing dialog, track colours and style, renaming a section or a track.
- The model has no setters that can be reversed. Anchor roll and pitch are applied as differences (`mnode::setRoll`, `changePitch`); a length change cascades into the transitions after it. "Store the old value and set it back through the same function" is what produces the defects.
- A track cannot be copied in memory (raw pointers), but its saved bytes are small, about 0.5 kB per section, and writing them takes microseconds.
- Later pieces need more than undo of single values: a solve must be one step with its trial runs not recorded; an element that re-solves several sections after an upstream edit must be one step; an outside caller must be able to run the same edits the window runs.
- Two defects of the original program were found on the way. Changing the start speed, roll, pitch or forces of a track that already has sections does not reach the track until the project is saved and reopened: the first section keeps a private copy of the start point (`core/section.cpp`, the constructor) and nothing refreshes it. And a name with a non-ASCII letter corrupts the saved file: the length written is in characters, the bytes are UTF-8.

## Decision

1. **Undo restores a snapshot.** Before an edit the program keeps the track's state: the bytes `track::saveTrack` writes, plus four live values those bytes do not hold (the anchor node as it is in memory, the first section's start node, the speeds of Bezier points, and the two smoothing flags `smoothedUntil` and `bAutoSmooth`). Undo puts the state back in place and recomputes the whole track; redo restores the state after the edit. Every edit of a track is then undoable and exact by construction, including the kinds with no undo today.
2. **A restore is not a load.** It puts back every input, runs the same full recompute an anchor edit runs, and writes the smoothing entries' saved fields back as they were. A load rewrites smoothing ranges and names from the track, so "restore by loading" would not land on the state that was captured.
3. **Whole track, not per section.** A function cannot be loaded into a used function, the window's pointers into the model die either way, and a partial recompute brings back last-digit differences that depend on the history of edits. Memory is kept down by sharing the bytes of sections that did not change between steps, and by a byte cap beside the step count.
4. **Mechanism first.** The snapshot goes in under the existing undo list, so the existing check (`--batch-undo-check`) proves it with unchanged test code. Both `switch` statements and the thirty "old value" fields go at that point, and the list of edits left out of the check ends empty.
5. **Named commands on top, group by group.** Each edit's model logic moves from the widget slot into a core function registered as a command: a name (`transition.setChange`, `anchor.setSpeed`, `section.add`), typed parameters in model units (m, m/s, degrees, g, s), a description, and a target given by numbers (track, section, function, transition), never by pointer. The window's controls call the commands. A step in the history carries a label and the commands that made it. A core-only test runs every command with undo and redo and fails if a command or a parameter has no description.
6. **The history lives in core** (`trackHistory`), one per track. It offers scopes (several commands, one step), rollback (restore and record nothing), and a merge rule. The windows learn of a restore through two notices on `trackListener`, "about to reset" and "reset", and rebuild what they hold; the same refresh serves the automation interface later.
7. **What is one step.** A run of changes to the same value through the same control is one step; it ends when the box loses focus, the selection changes, another command runs, or after a second without a change. A dialog's OK is one step, and nothing is recorded when nothing changed. In batch mode there is no merging, so scripted checks stay one step per edit.
8. **A new field of the model must be saved, declared as a live extra of the state, or derived.** Otherwise undo loses it. `core/CLAUDE.md` carries the rule.
9. **Not in Stage 2: undo for adding, deleting or importing a track.** There is no undo for these today, and the list of tracks lives in a window (`projectWidget`). It needs a project object in core and gets its own issue.

## Changes the owner will notice

1. Undo and redo work for every edit of a track and restore the design exactly.
2. Changing the start speed, roll, pitch or start forces of a track with sections takes effect at once.
3. Cancel in Track Properties and in the smoothing dialog discards the dialog's changes. Both dialogs block the main window while open.
4. Typing a number digit by digit is one undo step. A name with accented letters no longer corrupts the file (files with plain names are byte for byte as before).

After an undo the track is the one a full recompute gives. On a track without smoothing that differs from the state before the edit by the last digit at most; on a smoothed track the partial recompute of the original program can differ by more (issue 31), and undo now always shows the full one.

## Where enforced

- `core/trackstate.*`, `core/trackhistory.*`, `core/command.*`, `core/commands_*.cpp`; `tests/trackstate/`, `tests/commands/`.
- `py -3 tools/run-baseline.py --undo-check`, with no edit left out.
- `core/CLAUDE.md` and `ui/CLAUDE.md` (rewritten by the closing unit); root `CLAUDE.md`, "Conventions".
- Issue 31.
