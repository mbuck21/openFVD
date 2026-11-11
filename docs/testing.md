# Testing

There is no unit-test suite. FVD++ is a GUI program whose model code calls into its widgets, so a test means: run the real program on a known project, export, and compare the files with a known-good set. One command does that; the by-hand way below is the fallback and the way to make references with the 2016 binary.

## The baseline

`samples/baseline/` holds one small project and the three exports the **2016 release binary** (`bin/win64/FVD.exe`) produced from it. They are the reference: a build that changes these files has changed the geometry FVD++ produces.

| File | What |
|---|---|
| `four-sections.fvd` | The project: a straight, a curved, a force and a geometric section. |
| `four-sections.2016.nl2elem` | NoLimits 2 Exporter output. |
| `four-sections.2016.tangent.nlelem` | Tangent Exporter output. |
| `four-sections.2016.spline.nlelem` | Spline Exporter output. |
| `tolerances.json` | Files allowed to differ a little, how much, and why. Empty since 2026-11-10: every reference must match exactly. |

`samples/baseline/README.md` says exactly how the project was made and which export settings were used.

## Check a build: one command

1. Build (`docs/building.md`): `tools\build-windows.cmd`. It leaves `build/dist/FVD.exe`.
2. From the repository root:

```
py -3 tools/run-baseline.py
```

For every project in `samples/baseline/` the script starts `FVD.exe --batch-export` (below) in a fresh temporary folder, compares each file the program wrote with its reference, prints one line per file (`MATCH` or `NO MATCH`, with the first differences), and ends with `PASS` or `FAIL` (exit code 0 or 1). A run that takes longer than 60 seconds (180 for the undo check, which computes the whole track for every undo and redo and takes up to about 50 seconds on one project) is stopped and counts as failed. Nothing is written into the repository: the program's `fvd.log` lands in the temporary folder.

Options:

- `--exe <path>` tests another `FVD.exe` (default: `build/dist/FVD.exe` in this repository).
- `--keep <dir>` copies the files the program wrote (and its `fvd.log`, as `<project>.fvd.log`) into a folder, to look at them.
- `--extra <dir>` also tests every `*.fvd` in that folder. This is for the owner's own coasters, which stay out of the repository in `samples/private/` (ignored by git). Their references sit beside them as `<project>.ref.nl2elem`, `<project>.ref.tangent.nlelem`, `<project>.ref.spline.nlelem` and `<project>.ref.resaved.fvd`. A folder can have its own `tolerances.json`.
- `--undo-check` then runs the undo and redo check (below) on every project in `samples/baseline/` and requires `OK` from each: one line per project, `UNDO OK` or `UNDO FAIL` with the failing lines of its report.
- `--fixtures` then regenerates the scripted test projects (below) in a temporary folder and compares each with the committed `samples/baseline/gen-<name>.fvd`, byte for byte. Until those are committed it says so and does not fail.
- `--imports` then imports every input in `samples/import/` with the batch import (below) and compares the files and the report with the references there, byte for byte. Those references come from a rebuilt program, not the 2016 binary; `samples/import/README.md` says how they were made.
- `--core` runs every comparison above with the core-only program (next section) in place of `FVD.exe --batch-export`, against the same references; `--core-exe <path>` names another `fvdcore` (default: `build/dist/fvdcore.exe`, which `tools\build-windows.cmd` leaves; when that is missing, `build/tests/coreonly/release/fvdcore.exe`, where `tools\run-tests.cmd` leaves it). It cannot be combined with `--undo-check`, `--fixtures` or `--imports`, which need the windows, and says so.
- `--record` (only with `--extra`) writes the references that do not exist yet from the build being tested; it never overwrites one. Run it once with a build you trust, and every later build is compared with that one:

```
py -3 tools/run-baseline.py --extra samples/private --record
py -3 tools/run-baseline.py --extra samples/private
```

A project in `samples/baseline/` without a reference fails with `NO REFERENCE`; so does an `--extra` project without one when `--record` is not given. The re-saved project is compared only when a `<project>.2016.resaved.fvd` exists.

### Expected result

Measured on 2026-11-10 with the first MSYS2 build (GCC 16.2, Qt 5.15.19, glm 1.0.3), and the same with the batch export:

- **NoLimits 2:** MATCH with no tolerance. All 230 numbers are equal. The text is not byte-identical, because a current C runtime prints exponents with two digits (`e+00`) and the 2016 binary printed three (`e+000`); the tool compares numbers.
- **Spline:** MATCH with no tolerance; the file is byte-identical.
- **Tangent:** MATCH with no tolerance, all 22 segments identical. Until 2026-11-10 segment 18's two control handles (`Kp1`, `Kp2`) differed by up to 0.975 mm and `tolerances.json` allowed 1 mm for this file. The cause was not the tangent near its pole, as first thought, but `asinf` in `section::calcDirFromLast()`: the C runtime's `asinf` rounds some results differently from the 2016 binary's, and in the tight loop of the geometric section the exporter's handle length `radius * tan(angle / 2)` (`mnode::exportNode()`), with the angle near 180 degrees, turns that last bit into a millimetre. Since `core/fvdmath.cpp` computes `asin` as the 2016 binary did, the file matches exactly and the tolerance is gone (`docs/research/n2-matching-the-2016-maths.md`).
- **Re-saved project:** 763 bytes (see "Check the project format").

A difference outside that list means the build or the change under test altered the geometry. If the change was meant to (a physics fix, an exporter fix), say so in the pull request, show the script's output, and replace nothing in `samples/baseline/`: add a new, dated set beside it instead, because the 2016 files are the fixed point. Never loosen a tolerance to make a run pass.

The other seven projects (the five generated ones below and the two negative-tension projects `n1-tension-force-chain` and `n1-tension-loop`, `samples/baseline/README.md`) also match exactly, all four files each.

### Tolerances

`samples/baseline/tolerances.json` lists, by reference file name, the files that may differ a little. Every other file must match exactly. Since 2026-11-10 it lists none. An entry would look like this:

```
"four-sections.2016.tangent.nlelem": { "tolerance_mm": 1.0, "reason": "..." }
```

`tolerance_mm` is the largest position difference in millimetres, `tolerance_rad` the largest roll difference in radians (both default to 0). The `reason` is for people; write down why the difference is harmless.

## The batch export

```
FVD.exe --batch-export <project.fvd> <outdir>
```

starts the full program, with the same window classes as a normal start, but shows no window and takes no keyboard focus: it builds the main window hidden and sets up OpenGL on it without putting it on the screen. It then loads the project and, for every track that has at least one section, writes into `<outdir>`:

- `<project>.nl2elem`, `<project>.tangent.nlelem`, `<project>.spline.nlelem` for the first track, and `<project>.track2.nl2elem` and so on for further tracks. The settings are the export dialog's defaults: all sections, segment length 2.00 m, RelRoll threshold 85 degrees, heartlining on.
- `<project>.resaved.fvd`, the project saved again after the exports.
- `<project>.batch.txt`: one line per file written, any warning from loading (a missing ground texture, an old file version), and a last line `OK` or `ERROR: <what>`. The exit code is 0 or 1.
- `<project>.loadstate.txt`: what the windows show once the project is loaded: the Undo and Redo menu entries and, per track, its sections and nodes, the active section, the rows of the section list and which is selected, the undo list and the graph's horizontal range. It is not listed in the report and `run-baseline.py` does not compare it; it is for comparing two builds by hand (`--keep`) when a change touches how a load builds the windows.

It never shows a message box, writes no `.bak` file, and leaves the project it was given alone. It writes `fvd.log` in the folder it was started from, like a normal start. The code is in `ui/batchrunner.cpp`; a later `--batch-...` command goes there too.

## The batch import

```
FVD.exe --batch-import <nltrack|txt|csv> <input file> <outdir>
```

starts from the default project, like a normal start, and runs what the project panel's "Import NoLimits 1 Track", "Import from Pointlist" or "Import NoLimits 2 CSV" runs once its file dialog has given a file. It then writes the exports and the re-saved project as the batch export does (named after the input file without its extension; the imported track is track 2), and a report `<name>.batch.txt` with one line per file, the same lines as `loadstate.txt` above, and `OK` or `ERROR: <what>`. A file the importer cannot open fails with `ERROR`. A file it can open but not understand is read as from the menu, and can end the program (a point list with fewer than two points, a CSV line with fewer than ten columns). The inputs and references are in `samples/import/`.

All batch commands keep every window off the screen: a window that would be shown, such as a dialog a scenario opens by clicking its button, is marked "do not show on screen" just before, so it exists for Qt but nothing appears and nothing takes the keyboard. A batch command fails if a window reached the screen anyway.

## The ride sheet check

```
FVD.exe --batch-ride-sheet <project.fvd> <outdir>
```

loads the project, opens track 1 as a person would, makes the ride sheet panel (`ui/ridesheet.*`) refresh at once and writes every label and value of the panel, one line each (`<object name> | <label> | <value>`, for example `rideLengthValue | Length | 54.5 m`), to `<outdir>/<project>.ridesheet.txt`, with a report `<project>.batch.txt` ending `OK` or `ERROR: <what>`. With the unit setting at metres and m/s, each number equals the one `fvdcore summary <project>` prints for the same project (to the printed decimals); that is how the panel is checked against core without a screen. Not part of `run-baseline.py`.

## Scripted test projects

```
FVD.exe --batch-make-fixtures <outdir>
```

builds a fixed set of test projects without anyone clicking: for each one it starts a new project (File, New) and drives the same windows a person uses, by their names in the `.ui` forms: it adds sections from the track panel's Add menu, types values into the boxes, picks transitions on the plot, uses the transition editor's buttons, opens Track Properties and the smoothing dialog and clicks OK. No window appears on the screen. Each project is saved as `<outdir>/gen-<name>.fvd`. The report `<outdir>/make-fixtures.batch.txt` has one line per track (sections, node count, length, lowest and highest speed, normal and lateral force, read from the computed track), one `wrote` line per project, and a last line `OK` or `ERROR: <what>`. A project fails if any computed value is not a finite number, if the speed drops below 3 m/s, or if a force leaves -1.5 to +5 g. Two runs write byte-identical files. The scenarios are in `ui/batchscenarios.cpp`.

| Project | What it covers |
|---|---|
| `gen-normal-shapes.fvd` | Every shape the transition editor offers, on normal force with a time argument: linear; quadratic blend in, blend out and symmetric; cubic; quartic symmetric, "opposite direction first" and "overshoot" with their parameter; quintic simple and both double bumps with their parameter; sinusoidal; plateau. Centre and tension on two transitions. Locked roll and lateral functions. Straights with physics speed, a horizontal curve at a fixed 15 m/s, a geometric hump. |
| `gen-geometric-distance.fvd` | Distance argument on a geometric and on a force section; quaternion geometric, Euler force and Euler curve; pitch, yaw, lateral and roll functions with several transitions, added both after (append) and before (prepend); a hanging train (heartline -0.9 m), friction 0.02 and resistance 3; the anchor moved (x, y, z) and turned (pitch 4, yaw 30) with a start speed of 18 m/s; a curve with lead in and lead out; fixed-speed straight and curve. |
| `gen-curves-speed.fvd` | Curved sections in four directions (90, 30, -100, -90 degrees), with and without lead in and out, with fixed and physics speed, one in Euler; roll transitions on a straight and on a curve (with a centre value); the anchor rolled 8 degrees; friction 0.025 and resistance 2.5; smoothing switched on for section 2 only (0.3 s, 2 passes). |
| `gen-smooth-whole.fvd` | Fast roll transitions on force sections, a quaternion geometric section with locked pitch and roll, a force section with a distance argument, a curve with physics speed; smoothing switched on for the whole track (0.5 s, 2 passes). |
| `gen-two-tracks.fvd` | Two named tracks in one project; the second anchor moved and turned (yaw -45 degrees, speed 12 m/s). |

Not covered, because the windows cannot create them: Bézier and NoLimits-CSV sections (only the importers make them), and the transition shapes "to zero" and "freeform" (the transition editor does not offer them).

The lead opens each generated project in the 2016 binary by hand, exports it the same three ways as `four-sections.fvd`, and commits the project and its 2016 exports to `samples/baseline/` (`gen-<name>.fvd`, `gen-<name>.2016.nl2elem`, ...). From then on `run-baseline.py` compares them like `four-sections.fvd`, and `--fixtures` checks that the build still generates the committed projects byte for byte.

## Undo and redo check

```
FVD.exe --batch-undo-check <project.fvd> <outdir> [--all-edits] [--edits n,n,...] [--each]
```

loads the project, opens track 1 as a person would (select it, Edit), and writes its three exports and the re-saved project ("A"). It then makes a scripted list of edits through the windows, the same way the scripted projects are built: the anchor's values, Track Properties, the length, speed and options of the first straight, curved, force and geometric section, every box of the transition editor, append, prepend, remove and lock of transitions, adding and deleting sections. It writes the files again ("B", which must differ from A), undoes every step with Edit, Undo and writes "A2", redoes every step with Edit, Redo and writes "B2". It passes when A2 is byte-identical to A and B2 to B, for all exports of every track and the re-saved project. The first and the last edit change the anchor, which recomputes the whole track.

The report `<outdir>/<project>.undo.batch.txt` has one line per edit (what it changed, which undo action types the program recorded and which were expected; an edit without a target in this project is skipped with the reason), the A2 and B2 comparisons, the list of undo action types exercised and not exercised, and a last line `OK` or `ERROR: ...`. The A, B, A2 and B2 files stay in `<outdir>` as `<project>.undo-A.nl2elem` and so on.

No edit is left out. Undo and redo put back a snapshot of the whole track (`core/trackstate.h`, `docs/decisions/0011-undo-by-snapshot-commands-on-top.md`), so every kind of edit with undo comes back exactly; until Stage 2 thirteen edits were left out because the old undo re-applied each value by its own code and got eight kinds wrong (issue 31). `KNOWN_BROKEN` in `ui/batchscenarios.cpp` is kept, empty, until a later piece removes it; anything listed there is named in the report.

Options: `--all-edits` puts the left-out edits back (to check a fix); `--edits 3,17` makes only those edits and the first and last (left-out edits stay out unless `--all-edits` is given too); `--each` also undoes and redoes every edit on its own right after making it and writes the files that differ as `<project>.edit<n>.before.fvd` and so on (a diagnostic only, see below).

**Why the check compares only the end states.** An edit recomputes the part of the track after the edited point; undo and redo put the inputs back and recompute the whole track. The two can differ in the last digits (seen: 0.001 mm on one tangent handle), so undoing or redoing a single edit made with a partial recompute can show such a difference with `--each`. The first and the last scripted edit recompute the whole track, and a whole-track recompute gives what a fresh computation gives (also on smoothed tracks), so the end states A2 and B2 must match A and B exactly. `--each` shows the per-edit differences for anyone chasing a failure.


## The core-only program

`fvdcore` is a console program made from `core/` alone (the list of files is `core/core.pri`; no `ui/`, no `renderer/`, no QtWidgets, no OpenGL). It proves that the model does not need the windows, and it is the command-line program of the project: its sources are in `cli/` (`cli/README.md` lists the commands with an example each). `tools\build-windows.cmd` builds it from `cli/fvdcore.pro` and leaves `build/dist/fvdcore.exe`; `tests/coreonly/` builds the same sources as a test (`tools\run-tests.cmd`).

```
fvdcore export <project.fvd> <outdir>
fvdcore nodes <project.fvd> [--track N] [--every-m X | --every-s X | --all] [--frame world|anchor] [--out <file>]
fvdcore compare <a.fvd> <b.fvd> [--track N] [--tsv]
fvdcore summary <project.fvd> [--track N] [--sections] [--json]
fvdcore measures <project.fvd> [--track N] [--ids a,b,c] [--every-m X | --every-s X | --all] [--out <file>]   (or --list)
fvdcore check <project.fvd> [--track N] [--check ID] [--json] [--fail-on problem|warning|information]   (or --list)
fvdcore totext <project.fvd> <out.json|-> [--force]
fvdcore fromtext <in.json|-> <out.fvd> [--force]
fvdcore selftest <repository folder>
fvdcore <repository folder>            the same as selftest
fvdcore help
```

`export` does what `FVD.exe --batch-export` does, in the same order, and writes the same files with the same names (the exports, `<project>.resaved.fvd`, `<project>.batch.txt` with the same lines) and the same exit code, except `<project>.loadstate.txt`, which describes the windows. The two programs' files are identical byte for byte, also for a missing, empty, cut-off or wrong file. It writes no `fvd.log` and nothing else beside its output; `--log <file>` keeps Qt's debug output, which is otherwise dropped.

With a repository folder as its only argument (how `tools\run-tests.cmd` runs every test; also `fvdcore selftest <folder>`) it exports `samples/baseline/four-sections.fvd` into a temporary folder and compares the Spline export and the re-saved project with the 2016 binary's files, byte for byte. It then checks that `nodes --all` writes a header and one line per node, that `nodes` at its default spacing runs from the first to the last node, that `compare` of a project with itself reports zero everywhere, that `summary` prints the ride sheet (text and JSON, and refuses `--track 0`), that `totext` then `fromtext` gives back the bytes of a re-save and each refuses to replace an existing file without `--force`, that `measures --list` names every measure and `measures --all` has a column for each, that `check` exits 0, exits 3 with `--fail-on` when a finding of that severity exists and exits 1 on a missing project, and that an unknown command exits 1. `py -3 tools/run-baseline.py --core` does the full baseline comparison with it.

`py -3 tools/check-core.py` is the quick guard that goes with it: it fails, naming file and line, when a file in `core/` includes a header from `ui/` or `renderer/`, names `gloParent` or `glView`, or includes a QtWidgets or OpenGL header (`lenassert.h` too). `tools\run-tests.cmd` runs it first, then builds and runs the tests.

## Check a build by hand

The fallback when the batch export itself is in doubt, and the way to produce references with the 2016 binary (`bin/win64/FVD.exe`, run from a copy outside the repository), which has no batch mode.

1. Start the program.
2. File, Load, and open `samples/baseline/four-sections.fvd`. Do not save over it.
3. File, Export As (Ctrl+Shift+E). Leave everything as it comes up: track 1, from section 1 to section 4, segment length 2.00 m, RelRoll threshold 85. Export once with each Exporter Type, saving the files somewhere outside the repository.
4. Compare each with its reference:

```
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.nl2elem my.nl2elem
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.spline.nlelem my-spline.nlelem
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.tangent.nlelem my-tangent.nlelem
```

Each prints what differs and ends with `MATCH` or `NO MATCH` (exit code 0 or 1). Positions are compared in millimetres and roll in radians (`--tolerance-rad`, default exact). A value that is not a finite number never matches. The expected result is the same as above.

## Check the project format

Open `samples/baseline/four-sections.fvd` and save it under another name (the batch export does this as `four-sections.resaved.fvd`). The 2016 binary and the 2026-11-10 build both write the same 763 bytes when they do this. (The file in the repository is 722 bytes: it was saved straight after the sections were created, before the program had filled in the smoothing list's names. Loading and saving adds them. Compare a re-saved file with another re-saved file, not with the original.)

```
py -3 tools/compare_exports.py resaved-by-reference.fvd resaved-by-candidate.fvd
```

A file the 2016 binary re-saved, put in `samples/baseline/` as `four-sections.2016.resaved.fvd`, would make `run-baseline.py` compare this too.

## What this does not cover

- The 3D view, shadows, the plots and the options dialog; the importers only through the three small samples in `samples/import/`. Check those by hand when a change touches them, and write down what you did in the pull request.
- Undo and smoothing, the transition shapes and the other section options are covered by the scripted projects and the undo check above, but only once the generated projects and their 2016 exports are committed is their geometry compared with the 2016 binary.
- Export settings other than the dialog's defaults (a part of the track, another segment length, "without heartlining").
- Whether NoLimits accepts the files. The owner has NoLimits 2 installed; an export that matters gets opened there.
- Linux and macOS.

The batch export still runs the whole program, window classes included, because the model code is not yet separated from the widgets (`docs/roadmap.md`, Stage 1). It is a test driver, not a command-line version of FVD++.
