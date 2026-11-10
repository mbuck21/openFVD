# Testing

There is no automated test suite. FVD++ is a GUI program whose model code calls into its widgets, so a test today means: build it, open a project, export, and compare the files with a known-good set. This page says how, with the tools the repository provides.

## The baseline

`samples/baseline/` holds one small project and the three exports the **2016 release binary** (`bin/win64/FVD.exe`) produced from it. They are the reference: a build that changes these files has changed the geometry FVD++ produces.

| File | What |
|---|---|
| `four-sections.fvd` | The project: a straight, a curved, a force and a geometric section. |
| `four-sections.2016.nl2elem` | NoLimits 2 Exporter output. |
| `four-sections.2016.tangent.nlelem` | Tangent Exporter output. |
| `four-sections.2016.spline.nlelem` | Spline Exporter output. |

`samples/baseline/README.md` says exactly how the project was made and which export settings were used.

## Check a build against the baseline

1. Build (`docs/building.md`) and start `build/dist/FVD.exe`.
2. File, Load, and open `samples/baseline/four-sections.fvd`. Do not save over it.
3. File, Export As (Ctrl+Shift+E). Leave everything as it comes up: track 1, from section 1 to section 4, segment length 2.00 m, RelRoll threshold 85. Export once with each Exporter Type, saving the files somewhere outside the repository.
4. Compare each with its reference:

```
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.nl2elem my.nl2elem
py -3 tools/compare_exports.py samples/baseline/four-sections.2016.spline.nlelem my-spline.nlelem
py -3 tools/compare_exports.py --tolerance-mm 1 samples/baseline/four-sections.2016.tangent.nlelem my-tangent.nlelem
```

Each prints what differs and ends with `MATCH` or `NO MATCH` (exit code 0 or 1).

### Expected result

Measured on 2026-11-10 with the first MSYS2 build (GCC 16.2, Qt 5.15.19, glm 1.0.3):

- **NoLimits 2:** MATCH with no tolerance. All 230 numbers are equal. The text is not byte-identical, because a current C runtime prints exponents with two digits (`e+00`) and the 2016 binary printed three (`e+000`); the tool compares numbers.
- **Spline:** MATCH with no tolerance; the file is byte-identical.
- **Tangent:** 21 of 22 segments identical. In segment 18 the two control handles (`Kp1`, `Kp2`) differ by up to 0.975 mm; the end point `P1` is identical. That segment sits in the tight loop of the geometric section, where the exporter computes the handle length as `radius * tan(angle / 2)` with the angle near 180 degrees (`mnode::exportNode()` in `core/mnode.cpp`). The tangent is close to its pole there, so the last bit of a `float` moves the result by a millimetre on a handle about 8 m long. Hence `--tolerance-mm 1` for this file.

A difference outside that list means the build or the change under test altered the geometry. If the change was meant to (a physics fix, an exporter fix), say so in the pull request, show the tool's output, and replace nothing in `samples/baseline/`: add a new, dated set beside it instead, because the 2016 files are the fixed point.

## Check the project format

Open `samples/baseline/four-sections.fvd` and save it under another name. The 2016 binary and the 2026-11-10 build both write the same 763 bytes when they do this. (The file in the repository is 722 bytes: it was saved straight after the sections were created, before the program had filled in the smoothing list's names. Loading and saving adds them. Compare a re-saved file with another re-saved file, not with the original.)

```
py -3 tools/compare_exports.py resaved-by-reference.fvd resaved-by-candidate.fvd
```

## What this does not cover

- The 3D view, shadows, the plots, undo, the importers, smoothing, the options dialog, and every transition shape other than the default cubic. Check those by hand when a change touches them, and write down what you did in the pull request.
- Whether NoLimits accepts the files. The owner has NoLimits 2 installed; an export that matters gets opened there.
- Linux and macOS.

An automated version (a command-line driver that loads a project and exports without the GUI) is in `docs/maybe-later.md`; it needs the model code separated from the widgets first.
