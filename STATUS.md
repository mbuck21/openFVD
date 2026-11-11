# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-10, written by the lead session at the end of Stage 1. Stage 1 was merged into `master` that evening at the owner's word (pull request #17, merge commit 365892c).

## Focus

- **Stage 1 of the roadmap is finished and in `master`.** It delivers: a safety net that runs in one command, the model separated from the windows, the research behind the planned features, a train catalogue with a picker for the heartline, and a rebuilt program that matches the 2016 release exactly on every project tested, the owner's own included.
- **Stage 2 is next and not started:** the command-line front end on the window-less core (F2), the project as text (F3), the analysis framework (A1), the command list that replaces undo (T1), and the experiments for parametric elements on the real core (E1). `docs/roadmap.md` has the order.

## Milestone now

**Milestones 1 to 3 are reached.** 1: a reproducible build. 2: a clean baseline against the 2016 binary. 3: Stage 1, in eighteen pieces plus four that came up on the way:

- **The safety net.** `py -3 tools/run-baseline.py --undo-check --fixtures --imports` exports eight projects through the program's batch mode (no window) and compares them with the 2016 binary's files with no tolerance; regenerates five of the projects by driving the windows and compares them byte for byte; runs an undo and redo round trip on all eight; and checks the importers. `tools\run-tests.cmd` runs a guard script and three automated tests. `docs/testing.md`.
- **The model without the windows.** Nothing in `core/` includes a window header or names the main window or the 3D view (decision records 0007 and 0009). A console program built from `core/` alone, `fvdcore`, loads, computes, exports and saves, and its files are identical to the windowed program's. `tools/check-core.py` fails if `core/` is tied to the windows again.
- **The 2016 maths.** The rebuilt program differed from the 2016 binary on the owner's project by up to 74 mm. The cause was one C maths function taken from Windows' runtime where the 2016 binary carried its own. `core/fvdmath.*` now computes it exactly as the 2016 binary did, at the owner's word (decision record 0008). The baseline needs no tolerance any more.
- **Research,** in `docs/research/`: heartlines and trains (R1), minimum radii (R2), ride-force limits (R3), NoLimits 2 blocks (R4), the workflow study (R5), parametric elements (R6, for issue #28), and the two notes on the 2016 difference (N1, N2).
- **Train types as data.** `data/traintypes.json` holds 64 train types with a source and a confidence for each heartline; Track Properties has a "Train type" list that fills the heartline box.
- **Notes for sessions.** A `CLAUDE.md` in `core/`, `ui/` and `renderer/`, a header comment on each main class, the map rewritten.

Open issues:
- #28 (`claude`): parametric elements. Research done (R6); experiments on the real core and the design are Stage 2 and later.
- #31 (`claude`, `bug`): undo does not restore the design for eight kinds of edit. Fixed with the command list (T1).
- #41 (`claude`, `bug`): defects in the importers, the export and smoothing dialogs.
- #18 (`claude`): three latent defects in the core.
- #13 (`owner`): three checks deferred from the baseline (an export opened in NoLimits 2, the `.3ds` decision, a NoLimits 2 CSV import by hand).
- #3 (`claude`): Qt 5.15 or Qt 6. Not urgent.

## Waiting on the owner

- **The word to start Stage 2,** and which of its pieces first if he has a preference (`docs/roadmap.md`, Stage 2).
- **Try the "Train type" list** in Track Properties (File, Load a project; select the track; Properties), at his convenience.
- **Read the questions at the end of `docs/research/r5-fvd-workflow.md` and `docs/research/r6-parametric-elements.md`.** They decide what the main-goal work of Stages 3 and 4 is: the ranking of the directions (pinned points with a slack piece, the goal-seeking solver, and the others), whether re-solving happens on a button or live, which numbers define the first parametric element, and whether a file with elements must still open in the 2016 program.
- **A larger finished layout,** if he finds one, as a test track. `TheDays.fvd` is in use, kept out of the repository in `samples/private/`.
- **Later, when the force check is built (Stage 3):** whether to buy ASTM F2291 (about US$121) and transcribe twelve uncertain numbers himself; the standard's seller prohibits giving it to an AI (`docs/research/r3-force-limits.md`).
- #13, when convenient.

## Verified

- 2026-11-10, on the last commit of `claude/stage-1` (fdff428; `master` after the merge is file for file the same), built from an empty `build` folder with MSYS2 UCRT64 (GCC 16.2, Qt 5.15.19, glm 1.0.3):
  - `run-baseline.py --undo-check --fixtures --imports --extra samples/private`: 54 comparisons MATCH with no tolerance, UNDO OK on eight projects, PASS. The owner's project matches the 2016 binary's three exports and re-saved project exactly.
  - `run-baseline.py --core --extra samples/private`: 39 comparisons MATCH, PASS. `fvdcore` loads only Qt5Core and Qt5Gui.
  - `tools\run-tests.cmd`: `check-core` and three tests, PASS.
- 2026-11-10, the 2016 binary driven on screen: the five generated projects, the two negative-tension projects, the re-save of `four-sections` and the owner's project exported and re-saved; these are the references in `samples/baseline/` and `samples/private/`.
- 2026-11-10, the finished build driven on screen: edit, undo and redo of a section length; the smoothing dialog (tick, new region, name, save, reopen, remove); wireframe on and off; the train list (pick, OK, undo, cancel and reopen); save and export through the dialogs. The dialog's export equals the batch export of the saved project.
- Claude Code Review ran on every piece that changed code and on the integration pull request; every comment was answered or fixed.
- Not verified: an export opened in NoLimits 2; Import Track from Project and loading v0.30 files (no test covers them; they run through code Stage 1 changed); the point-list importer's output (it is not repeatable in the original, issue #41); the `.3ds` export; Linux and macOS.

## Watch-outs

- **Results must stay bit-identical to the 2016 binary.** Moving a float operation changes the last digit, and the last digit reaches the exports: a force section starts from the force the previous section measured. Run the baseline after any change to `core/`.
- **Undo is known to be wrong for eight kinds of edit** (#31). The undo check leaves those edits out; `ui/batchscenarios.cpp` lists them. Do not "fix" the list; fix the edit and take it off.
- **Many source files are stored with Windows line endings.** A shell tool that rewrites a file (`sed -i`) turns a small edit into a whole-file diff. Edit with a tool that keeps the file's endings and look at `git diff --stat`.
- **The batch scenarios find widgets by object name.** Renaming a widget in a `.ui` form breaks them and silently disconnects its slot.
- **A new file in `core/` goes into `core/core.pri`,** not `fvd.pro`, or the core-only program stops linking.
- **`samples/private/` is ignored by git** and holds the owner's project and its references. The fork is public: nothing of the owner's goes into the repository without his word.
- **The GitHub workflows run on the subscription** (decision record 0005). The review runs when the `review` label is added; if no run starts, take the label off and add it again. It does not run on a pull request with merge conflicts.
- **Deleting a branch closes every open pull request based on it.** Retarget dependents first.
- **Run the build through `tools\build-windows.cmd`;** MSYS2 names the Qt tools `qmake-qt5` and `windeployqt-qt5`. Delete `build/` for a clean rebuild; after moving files, always.
- **`ui/qcustomplot.*` is a vendored library.** Never edit it.
- **A new compiler, glm or Windows runtime can move results by the last bit.** After `pacman -Syu` or a Windows update that touches the C runtime, rebuild and rerun the baseline; `docs/research/n2-matching-the-2016-maths.md` lists the maths functions that are exposed and not yet pinned.
