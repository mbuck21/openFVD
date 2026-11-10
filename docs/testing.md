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

- The 3D view, shadows, the plots, undo, the importers, smoothing, the options dialog, and every transition shape other than the default cubic. Check those by hand when a change touches them, and write down what you did in the pull request.
- Export settings other than the dialog's defaults (a part of the track, another segment length, "without heartlining").
- Whether NoLimits accepts the files. The owner has NoLimits 2 installed; an export that matters gets opened there.
- Linux and macOS.

The batch export still runs the whole program, window classes included, because the model code is not yet separated from the widgets (`docs/roadmap.md`, Stage 1). It is a test driver, not a command-line version of FVD++.
