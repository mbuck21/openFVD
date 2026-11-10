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
| `tolerances.json` | The one file allowed to differ a little, how much, and why. |

`samples/baseline/README.md` says exactly how the project was made and which export settings were used.

## Check a build: one command

1. Build (`docs/building.md`): `tools\build-windows.cmd`. It leaves `build/dist/FVD.exe`.
2. From the repository root:

```
py -3 tools/run-baseline.py
```

For every project in `samples/baseline/` the script starts `FVD.exe --batch-export` (below) in a fresh temporary folder, compares each file the program wrote with its reference, prints one line per file (`MATCH` or `NO MATCH`, with the first differences), and ends with `PASS` or `FAIL` (exit code 0 or 1). A run that takes longer than 60 seconds is stopped and counts as failed. Nothing is written into the repository: the program's `fvd.log` lands in the temporary folder.

Options:

- `--exe <path>` tests another `FVD.exe` (default: `build/dist/FVD.exe` in this repository).
- `--keep <dir>` copies the files the program wrote (and its `fvd.log`, as `<project>.fvd.log`) into a folder, to look at them.
- `--extra <dir>` also tests every `*.fvd` in that folder. This is for the owner's own coasters, which stay out of the repository in `samples/private/` (ignored by git). Their references sit beside them as `<project>.ref.nl2elem`, `<project>.ref.tangent.nlelem`, `<project>.ref.spline.nlelem` and `<project>.ref.resaved.fvd`. A folder can have its own `tolerances.json`.
- `--undo-check` then runs the undo and redo check (below) on every project in `samples/baseline/` and requires `OK` from each: one line per project, `UNDO OK` or `UNDO FAIL` with the failing lines of its report.
- `--fixtures` then regenerates the scripted test projects (below) in a temporary folder and compares each with the committed `samples/baseline/gen-<name>.fvd`, byte for byte. Until those are committed it says so and does not fail.
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
- **Tangent:** 21 of 22 segments identical. In segment 18 the two control handles (`Kp1`, `Kp2`) differ by up to 0.975 mm; the end point `P1` is identical. That segment sits in the tight loop of the geometric section, where the exporter computes the handle length as `radius * tan(angle / 2)` with the angle near 180 degrees (`mnode::exportNode()` in `core/mnode.cpp`). The tangent is close to its pole there, so the last bit of a `float` moves the result by a millimetre on a handle about 8 m long. Hence the 1 mm tolerance for this file in `tolerances.json`.
- **Re-saved project:** 763 bytes (see "Check the project format").

A difference outside that list means the build or the change under test altered the geometry. If the change was meant to (a physics fix, an exporter fix), say so in the pull request, show the script's output, and replace nothing in `samples/baseline/`: add a new, dated set beside it instead, because the 2016 files are the fixed point. Never loosen a tolerance to make a run pass.

### Tolerances

`samples/baseline/tolerances.json` lists, by reference file name, the files that may differ a little. Every other file must match exactly. An entry looks like this:

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

It never shows a message box, writes no `.bak` file, and leaves the project it was given alone. It writes `fvd.log` in the folder it was started from, like a normal start. The code is in `ui/batchrunner.cpp`; a later `--batch-...` command goes there too.

All batch commands keep every window off the screen: a window that would be shown, such as a dialog a scenario opens by clicking its button, is marked "do not show on screen" just before, so it exists for Qt but nothing appears and nothing takes the keyboard. A batch command fails if a window reached the screen anyway.

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

Some edits are left out by default because the round trip fails on today's code; `KNOWN_BROKEN` in `ui/batchscenarios.cpp` lists them with the reason, and the report names each one. They are faults of the program, recorded so that the check passes today and catches any new change:

| Edit | Left out | Why the round trip fails today |
|---|---|---|
| 4, 5 anchor pitch and yaw change rate | always | Undo recomputes the anchor's normal and lateral force from the rate instead of restoring them; the saved forces differ in the last digits. |
| 9 anchor roll | when the anchor is rolled | Undo applies the old roll as a difference to the current one; with a rolled anchor the result differs in the last digits. |
| 29 transition length on the first force section | always | The track is not restored (seen on `gen-normal-shapes` and `gen-smooth-whole`). Length undo is still checked on the geometric section (edit 51). |
| 41 tension | always | Undo restores a remembered "old tension" that is never taken from the selected transition (`transitionWidget::changeSubfunc` sets the other four), so it restores whatever was left there; seen: minus infinity. |
| 42, 43, 44 append, prepend, remove a roll transition | on a smoothed track | The track is not restored. On the other projects these edits run. |
| 45, 46 append and prepend on normal force | always | A new normal-force transition starts as cubic with its shape argument never set; redo creates it again with another leftover value, so the saved file differs. |
| 49, 52 argument of a force or geometric section | when that section uses distance | Undo of switching from distance back to time does not restore the saved project. |
| 56 delete a section | always | Undo recreates the section's smoothing entry without the name the smoothing dialog gave it, so the saved file is shorter, and the section's own smoothing settings are lost. |

Options: `--all-edits` puts the left-out edits back (to check a fix); `--edits 3,17` makes only those edits and the first and last (left-out edits stay out unless `--all-edits` is given too); `--each` also undoes and redoes every edit on its own right after making it and writes the files that differ as `<project>.edit<n>.before.fvd` and so on (a diagnostic only, see below).

**Why the check compares only the end states.** The program recomputes the part of the track after an edit from the edited point onward, so after undoing one edit the recomputed nodes can differ from the ones stored before it in the last digits (seen: 0.001 mm on one tangent handle). Undoing everything ends with the first edit, which recomputes the whole track; that is the state compared. `--each` shows the per-edit differences for anyone chasing a failure.


## Check a build by hand

The fallback when the batch export itself is in doubt, and the way to produce references with the 2016 binary (`bin/win64/FVD.exe`, run from a copy outside the repository), which has no batch mode.

1. Start the program.
2. File, Load, and open `samples/baseline/four-sections.fvd`. Do not save over it.
3. File, Export As (Ctrl+Shift+E). Leave everything as it comes up: track 1, from section 1 to section 4, segment length 2.00 m, RelRoll threshold 85. Export once with each Exporter Type, saving the files somewhere outside the repository.
4. Compare each with its reference:

```
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.nl2elem my.nl2elem
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.spline.nlelem my-spline.nlelem
py -3 tools/compare_exports.py --tolerance-mm 1 samples/baseline/four-sections.2016.tangent.nlelem my-tangent.nlelem
```

Each prints what differs and ends with `MATCH` or `NO MATCH` (exit code 0 or 1). Positions are compared in millimetres and roll in radians (`--tolerance-rad`, default exact). A value that is not a finite number never matches. The expected result is the same as above.

## Check the project format

Open `samples/baseline/four-sections.fvd` and save it under another name (the batch export does this as `four-sections.resaved.fvd`). The 2016 binary and the 2026-11-10 build both write the same 763 bytes when they do this. (The file in the repository is 722 bytes: it was saved straight after the sections were created, before the program had filled in the smoothing list's names. Loading and saving adds them. Compare a re-saved file with another re-saved file, not with the original.)

```
py -3 tools/compare_exports.py resaved-by-reference.fvd resaved-by-candidate.fvd
```

A file the 2016 binary re-saved, put in `samples/baseline/` as `four-sections.2016.resaved.fvd`, would make `run-baseline.py` compare this too.

## What this does not cover

- The 3D view, shadows, the plots, the importers and the options dialog. Check those by hand when a change touches them, and write down what you did in the pull request.
- Undo and smoothing, the transition shapes and the other section options are covered by the scripted projects and the undo check above, but only once the generated projects and their 2016 exports are committed is their geometry compared with the 2016 binary.
- Export settings other than the dialog's defaults (a part of the track, another segment length, "without heartlining").
- Whether NoLimits accepts the files. The owner has NoLimits 2 installed; an export that matters gets opened there.
- Linux and macOS.

The batch export still runs the whole program, window classes included, because the model code is not yet separated from the widgets (`docs/roadmap.md`, Stage 1). It is a test driver, not a command-line version of FVD++.
