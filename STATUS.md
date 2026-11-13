# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-13, written by the session that planned Stage 3. **Stage 3 is planned and not started; no code has changed since Stage 2.** Stage 2 is complete and in `master`: it was built on the integration branch `claude/stage-2` and merged through pull request #46 by the lead session at the owner's word. Stage 1 has been in `master` since 2026-11-10.

## Focus

- **Stage 2 of the roadmap is finished: "reach and measure".** The program can be driven and measured without a person, every edit of a track is a named command, and the questions behind the newer requests were answered by experiment. It was built in 30 pull requests into the integration branch (#47 to #81) under decision records 0011, 0012 and 0013.
- **Saved files did not change, and every project still computes exactly as the 2016 program does.** The comparison with the 2016 binary passes with no tolerance, the owner's own project included.
- **Stage 3 is planned, not built: the train.** The owner set its priorities on 2026-11-13: a whole-train simulation with friction as in NoLimits 2, the number of cars and which car the design is for, the ASTM force check at every seat shown at a glance, and English units by default. The plan is three documents: decision record 0014 (the design, provisional, ending in eight choices for the owner), `docs/roadmap.md` (reworked: Stage 3 shows the train and changes no design, Stage 4 is the train's real numbers and the design following a car, the old Stage 3's other pieces are Stage 5) and `docs/stage-3-plan.md` (the build plan, written so a fresh session can start from it). Another session builds the stage at the owner's word.
- **Two research notes came with the plan.** R12: NoLimits 2 has no mass anywhere and uses the same two resistance numbers as this program; how it couples its cars and what a lift crest does are not published and need a short session in NoLimits 2. R13: of the twelve uncertain force-limit items of R3, two are now corroborated, six still rest on one source and one is contradicted between editions.

## Milestone now

**Milestone 4, Stage 2, is reached.** What it delivers:

- **Undo that works for every edit of a track.** Every edit a person can make to a track is a named command in the model (`core/command.h`, 45 of them; `fvdcore commands` lists them with units and ranges). A command records its own undo step; undo and redo put back a snapshot of the whole track, exact to the last digit. The kinds of edit that had no undo, or a wrong one, in the 2016 program (issue #31) are fixed: the smoothing dialog, colours, renaming, Track Properties, and the eight kinds the round-trip check had found. The old undo classes are gone. The Edit menu names the step ("Undo Change transition length"), and a number typed digit by digit is one step. Adding, deleting or importing a whole track is not an undo step yet (issue #64).
- **The track on screen is always the track a reopen gives** (decision record 0013). In the 2016 program an edit could leave the running track different from the one that saving and reopening produced (measured: up to 12.3 m after adding a section in the middle of a smoothed track). Now every command ends in the reopened state, and a test runs every command on every baseline project to hold it there. Cancel in Track Properties and in the smoothing dialog really cancels. A change of the start speed, roll, pitch or start forces takes effect at once.
- **A command-line program, `fvdcore`,** on the model without the windows: export, a table of points, compare two versions of a project, the ride sheet, measures, checks, the project as text and back, the three importers, and `session`, which holds a project open and runs named commands from another program (`cli/README.md`, `tools/fvdsession.py`).
- **The project as text:** `fvdcore totext` and `fromtext` write and read a project as readable JSON; the round trip gives back the same bytes (`docs/project-text-format.md`). The binary `.fvd` is unchanged.
- **Measures and findings** (decision record 0012): numbers worked out along the track that never change it, the along-track force first; new curves on the plot, colour by measure on the 3D track, findings marked on both.
- **The ride sheet** (FR-18): length, height, drop, duration, top speed, seconds of airtime, peak g each way, inversions and open findings, on a panel that updates while editing and as text from `fvdcore summary`.
- **Research and experiments** in `docs/research/`: friction, resistance and margins (R8), special track (R9), comfort (R10), a whole train against a point (R7), and the solver, elements and pins tried on the real program (E1). What they found is under "What Stage 2 found" in `docs/roadmap.md`.

Open issues:
- #80 (`claude`): time what an edit costs in the 3D view with measures and findings on; the final review of pull request #46 asked.
- #72 (`owner`): the two start-rate boxes come back about 2 percent larger. Leave or make them agree (below).
- #65 (`owner`): NoLimits 2 tracks as CSV, for the deferred import experiment (R11).
- #64 (`claude`): undo for adding, deleting and importing a track (a project-level history). Not in the plan yet.
- #63 (`claude`, `bug`): defects and quirks of the original program found during Stage 2, 21 items. None fixed here; most belong to pieces of the stages after the train. The one to know: a track imported from NoLimits 1 (a Bezier section) does not reopen repeatably, so decision 0013's rule does not hold for it yet (item 16).
- #41 (`claude`, `bug`): defects in the importers, the export and smoothing dialogs.
- #30, #28 (`claude`): FR-11 to FR-18 and parametric elements. Their Stage 2 experiments are written up (R7 to R10, E1); R11 is deferred.
- #18 (`claude`): three latent defects in the core. #13 (`owner`): three checks deferred from the baseline. #3 (`claude`): Qt 5.15 or Qt 6.

## Waiting on the owner

The full list, with when each answer is needed, is the last section of `docs/roadmap.md`. The ones that matter now:

- **The eight choices of decision record 0014,** each with a recommendation: the split between Stage 3 and Stage 4; how the seat check is shown; no train by default; the train's centre as the default reference; buying ASTM F2291 (edition 26b, US$121); train settings saved in an appendix the 2016 program ignores, with rule 4 in `CLAUDE.md` reworded; a short session in NoLimits 2; the order after the train. `docs/stage-3-plan.md` says which piece waits for which answer; only the Train panel waits before it can be built.
- **The word for a session to build Stage 3.**
- **Try Stage 2,** if not done yet. Pull request #46's "For the owner" part says what to try first.
- **#72, the start-rate boxes:** recommendation, leave it. No answer means it stays.
- **#65, one or two NoLimits 2 tracks exported as CSV,** when convenient; needed before Stage 6.
- **A slower edit near the end of a very long ride.** Because every edit now ends in the reopened state, an edit computes the whole track once: about 13 ms on a normal ride (14,000 nodes), about 77 ms on a 150,000-node ride, wherever the edit is. Before, an edit of the last section of a long ride took 1 ms and an early one 78 ms. The limit set for typing is 150 ms. Say so if a long ride feels slow to edit; the next step would be to settle when typing stops.
- **Before "hold the ride together" (now Stage 6), not before:** the questions at the end of R5 and R6, now with E1's answers beside them.
- #13, when convenient.

## Verified

Nothing was built or verified for the Stage 3 plan: it is documents only. What follows is the verification of Stage 2, which is still the state of the code.

**The whole verification, from an empty `build` folder,** on the closing piece's commit `9026da7` (2026-11-12; the commits after it change documents only), the owner's project included:

- **Build:** `tools\build-windows.cmd` from nothing, exit 0.
- **Against the 2016 binary:** `py -3 tools/run-baseline.py --undo-check --fixtures --imports --extra samples/private` ends `PASS`: 54 comparisons match with no tolerance, the undo check is OK on all 8 projects, and the owner's project gives the same three exports and the same re-saved file, byte for byte.
- **Automated tests:** `tools\run-tests.cmd`, 8 tests, 0 failed.
- **The model alone:** `py -3 tools/run-baseline.py --core` ends `PASS` (39 match), its import comparison ends `PASS`, and `py -3 tools/check-core.py` finds 0 problems in 57 files.
- **The owner's project in the tests that take a folder:** the track-state test (1017 checks, 0 failed) and the project-as-text round trip pass on it.
- **Every edit undone and redone on its own** in the real windows (`FVD.exe --batch-undo-check ... --all-edits --each`): OK on the owner's project and on all eight baseline projects.
- **Typed values merge into one step** in the real boxes (`FVD.exe --batch-merge-check`): OK on all eight baseline projects and the owner's.
- **Running equals reopened** (`tests/commands --reopen`, with the owner's project): 68 checks, 0 failed. In 1105 command runs on the baseline projects and 173 on the owner's, the running track is the saved and reopened one and the one after undo and redo; nothing differs, not a byte.
- **Imported tracks** (`tests/commands --imported`): 39 checks, 0 failed.

`master` after the merge holds exactly the files verified here (nothing else went into `master` in between); the lead builds it once more and runs the comparison, and writes the result in pull request #46.

**Tried on screen by the lead session** (the real program, the same build, 2026-11-12): a number typed digit by digit is one undo step, and leaving the box or a second's pause ends the run; the Edit menu names the step; a changed start speed takes effect at once; Cancel in Track Properties and in the smoothing dialog changes nothing and adds no step, OK is one step that undo takes back; with a dialog open, Ctrl+Z does nothing; the ride sheet fills when a track is opened and follows the units chosen in Edit > Options; findings are listed on the ride sheet for a track with problems; on a 150,000-node ride an edit took 110 to 118 ms by the status bar while the verification above was running in the background (77 ms on an idle PC; the limit is 150 ms). `fvd.log` had no assert.

**Seen on the way, not defects of this stage:** the ride sheet does not follow a value changed inside Track Properties until OK; the tool tip of the rate boxes is one very long line.

**Not verified:**

- **By a person.** How editing feels, above all on a long ride, is the owner's to judge.
- **On screen:** the findings tint in the 3D view and on the plot, the colour picker, and the boxes going back to the model's values when a command is refused. The batch checks cover the model side of each; the lead's view of the middle of the screen was blocked.
- **An export opened in NoLimits 2.** The exported files are the 2016 program's byte for byte, so nothing new is expected (#13 holds the checks deferred from the baseline).
- **The cost of redrawing the 3D track per edit,** apart from the model's recompute, was not measured on its own; the status bar's figure is the recompute (issue #80).
- **Nine commands the batch undo check does not reach through the windows** are checked on the model only (`tests/commands`).

## Watch-outs

- **Results must stay bit-identical to the 2016 binary.** Moving a float operation changes the last digit, and the last digit reaches the exports. Run the baseline after any change to `core/`.
- **Every command must end in the reopened state** (decision record 0013). A new command is added to the registry and is then run by `tests/commands` on every baseline project: the running track, the saved and reopened one, and the one after undo and redo must be the same. A command that computes the track itself says so (`settlesItself`), or the track is computed twice.
- **A new model value must be saved, kept in the track's state (`core/trackstate.h`), or derived,** or undo will not bring it back.
- **Tracks with a Bezier section (NoLimits 1 import) are the named exception** to 0013 until the import work (I1): a reopen of such a track is not repeatable in the original program either (#63, item 16).
- **Many source files are stored with Windows line endings.** A shell tool that rewrites a file (`sed -i`) turns a small edit into a whole-file diff. Edit with a tool that keeps the file's endings and look at `git diff --stat`.
- **The batch scenarios find widgets by object name.** Renaming a widget in a `.ui` form breaks them and silently disconnects its slot.
- **A new file in `core/` goes into `core/core.pri`,** not `fvd.pro`, or the core-only programs stop linking.
- **`samples/private/` is ignored by git** and holds the owner's project and its references. The fork is public: nothing of the owner's goes into the repository without his word. Workers cannot see it; the lead runs the checks on it (`docs/testing.md`).
- **A long `fvdcore session` writes messages to standard error** when a train stalls; a caller must read or redirect them (`tools/fvdsession.py` does).
- **The GitHub workflows run on the subscription** (decision record 0005). The review runs when the `review` label is added; if no run starts, take the label off and add it again. It does not run on a pull request with merge conflicts.
- **Deleting a branch closes every open pull request based on it.** Retarget dependents first.
- **Run the build through `tools\build-windows.cmd`;** delete `build/` for a clean rebuild; after moving files, always.
- **`ui/qcustomplot.*` is a vendored library.** Never edit it.
- **A new compiler, glm or Windows runtime can move results by the last bit.** After `pacman -Syu` or a Windows update that touches the C runtime, rebuild and rerun the baseline; `docs/research/n2-matching-the-2016-maths.md` lists the maths functions that are exposed and not yet pinned.
