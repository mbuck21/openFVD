# 0013: Every command ends in the state that saving and reopening gives

- Status: accepted
- Date: 2026-11-11
- Needs owner approval: no for the rule (it is the principle the owner approved for the start point, applied to every edit; the architect's call under record 0006). The owner was told the same day, with the measurements. It changes what is on screen straight after some edits; it does not change what a saved project computes.

## Context

- The original program recomputes a track from the edited point onward and keeps what it had before that point. Stage 2 found, in one place after another, that the track left on screen is then not the track the saved file describes:
  - an edit of the start point did not reach the track at all until the project was reopened (fixed in pull request 71);
  - a recompute left traces of old roll smoothing in the nodes (fixed in pull request 66);
  - adding a transition to a geometric section on distance left the end of the track 19.8 mm off (measured in pull request 69);
  - **adding a section in the middle of a smoothed track left the end of the track up to 12.3 m off and its end speed 3.45 m/s off** (measured in pull request 74); 15 mm on a project without smoothing but with a rolled start.
- In every measured case the project saved and reopened, and the track after an undo and a redo, agree with each other exactly. The running track is the odd one.
- Record 0011 expected such differences to be "the last digit at most". They are not.
- Nothing saved or exported was wrong: files hold the settings, and opening a file computes the whole track. But the designer works on the running track, and the owner's first complaint is that an early change disturbs the rest of the ride. Part of that disturbance was this.
- The goal-seeking solver of Stage 3 recomputes from the middle of a track thousands of times. It cannot aim at a stale track.

## Decision

1. **After every command, the running track is the track a save and a reopen give:** same nodes, same exports. This holds for every registered command, on every kind of section, with and without roll smoothing.
2. **A test proves it:** for every command on every baseline project, at several places in the track, the running track is compared with the saved and reopened one (node count, the three exports byte for byte) and with the track after undo and redo. A command that fails it is a defect. Where opening a file itself moves a stored value (the last-digit effect of issue 63, item 7), the test says so by name and bounds it.
3. **How:** the simplest thing that is exact. A command finishes with a whole-track recompute; where a whole-track recompute of the running track is itself not what a fresh one gives, the cause is fixed in the model (`track::newSection`, `track::updateTrack`, the smoothing ranges). Rebuilding the track from its saved state, as undo does, is the fallback for a command that cannot be made exact otherwise.
4. **Cost is measured, not assumed.** A whole-track recompute after each edit costs at most about twice what the original program spends on an edit near the start of a ride. If typing in a box becomes slow on a long ride, a run of typed changes to one value recomputes from the edited point while it lasts and settles to the exact track when it ends; undo, redo, save and export always see the settled track.
5. **What a file load computes does not change.** The comparison with the 2016 binary stays exact with no tolerance.
6. **Committed test projects that were saved from a stale running track are re-recorded** when this rule makes the scripted scenario produce the file a reopen would: only where the differences are confined to values the program recalculates on load, the new file equals its own re-save, and the 2016 binary itself, driven by the lead, gives the committed 2016 exports and re-save from the new file. The first was `gen-curves-speed.fvd` (pull request 71).
7. This supersedes the sentence in record 0011 that a redo may differ from the state before the undo: after this, it does not.

## Where enforced

- `core/command.cpp` (`commandRegistry::run`) and the model fixes it needs; `tests/commands/` ("running equals reopened").
- `FVD.exe --batch-undo-check ... --each`: a single-edit redo that differs is a failure, not a note.
- `core/CLAUDE.md`: a new command must pass the test; a new kind of recompute must be exact.
- Issue 63, item 14.
