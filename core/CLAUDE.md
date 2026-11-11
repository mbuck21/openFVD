# core/ (the model)

The track model and its physics: tracks, sections, nodes, transition functions, the project file, the NoLimits export files, the importers, roll smoothing and the train catalogue. It runs with no window and no OpenGL; the console program in `tests/coreonly/` builds from this folder alone (`docs/testing.md`).

Read the root `CLAUDE.md` first, then `docs/codebase-map.md` for the data model and flows.

## What core may depend on

- **Allowed:** the C++ library, glm (through `lenassert.h`), QtCore (`QString`, `QList`, `QVector`, `QElapsedTimer`, the JSON classes) and the value type `QColor`.
- **Not allowed:** any header from `ui/` or `renderer/`, any QtWidgets header, the globals `gloParent` and `glView`, `options.cfg`.
- **Enforced by:** `tools/check-core.py` (fails on any of the above) and by the core-only build (`tests/coreonly/`, `docs/testing.md`). Run the check before every commit that touches this folder.
- **How core reaches the windows:** only through the interfaces below. The windows call core; core calls back through an interface the window implements.

## Interfaces that cross the boundary

| Interface | File | Called when | Implemented by |
|---|---|---|---|
| `trackListener` | `tracklistener.h` | geometry changed, status text, smoothing applied, section added during a load, load finished | `trackHandler` (`ui/trackhandler.*`); `track::mListener` is NULL for a track without one |
| `smoothHandlerListener` | `smoothhandler.h` | a smoothing entry's shown values changed, or it was deleted | `smoothUi` (`ui/smoothui.*`) |
| `projectFileClient` | `projectfile.h` | reading and writing a project: tracks cleared, ground texture, new track, smoothing updated, track loaded, load finished, track about to be saved | `projectWidget` (`ui/projectwidget.*`) |

A new call from core to a window means a new virtual function on one of these, implemented in `ui/`. Never an `#include` of a window class.

## Main classes

- **`track`** (`track.*`): one track. Owns the anchor node, the ordered `lSections`, the smoothing list and `display` (three `QColor`s and the wireframe flag). Created by `trackHandler` (and by `projectFileClient::projectNewTrack()`); its constructor takes no handler. Calls `mListener`.
- **`section`** (`section.*`) and its six subclasses: `secstraight`, `seccurved`, `secforced`, `secgeometric`, `secbezier`, `secnlcsv`. Each owns its `lNodes` and its transition functions and integrates itself in `updateSection()`. Created by `track::newSection()` or `track::appendSectionFromFile()`.
- **`mnode`** (`mnode.*`): one sample of the train, 1/`F_HZ` second apart. A plain value; sections copy it across the chain.
- **`func`, `subfunc`** (`function.*`, `subfunction.*`): a transition curve and its pieces. `subfunc::getValue()` is the whole maths of transitions.
- **`smoothHandler`** (`smoothhandler.*`): one roll-smoothing entry of a track. The algorithm itself is `track::applyRollSmooth()` and `track::applyRollSmoothFilter()`.
- **`projectfile.*`:** `readProject`, `writeProject`, `loadProjectFile`, `saveProjectFile`. Calls `track::loadTrack()` / `track::saveTrack()` per track.
- **`elementfile.*`:** `writeNL2Element`, `writeNL1TangentElement`, `writeNL1SplineElement`: the file around a track's export. Both the export dialog and the batch mode call them.
- **`exportfuncs.*`:** the binary read and write helpers (`writeBytes`, `readInt`, ...).
- **`noLimitsImporter`** (`nolimitsimporter.*`): takes a `track*`, builds `secbezier` sections with `track::appendSectionFromFile()`.
- **`fvdmath.*`:** `asinh2016` and `asin2016`, see the traps.
- **`trainCatalogue`** (`traintype.*`): reads `data/traintypes.json`. QtCore only; `tests/traintypes` builds it alone.

## Where a change lands

| Change | Open |
|---|---|
| New section type | `enum secType` in `section.h`; a `secXxx` class; `track::newSection()` and the tag `switch` in `track::loadTrack()`; the file list `core/core.pri`; then `ui/CLAUDE.md` for the panel |
| New transition shape | `enum eDegree` and `subfunc::getValue()`, `getMinValue()`, `getMaxValue()`, `isSymmetric()` in `subfunction.*` |
| Physics (forces, speed, energy) | `updateSection()` in the `sec*.cpp` files; `mnode::calcSmoothForces()`; constants in `lenassert.h` and `mnode.h` |
| New value saved in the project | `track::saveTrack()` / `loadTrack()` or the section's `saveSection()` / `loadSection()`; a version bump in `writeProject()` and an old-file branch in `readProject()` (root rule 4) |
| Export | `track::exportNL2Track()`, `exportTrack3()`, `exportTrack4()`, `mnode::exportNode()`; the file around it in `elementfile.cpp` |
| Roll smoothing | `track::applyRollSmooth()`, `applyRollSmoothFilter()`, `updateSmoothEntries()`; `smoothhandler.*` |
| New notice to the windows | a virtual function in `tracklistener.h` (or the other two interfaces), a call in `track.cpp`, an implementation in `ui/` |
| Train data | `data/traintypes.json`, `traintype.*`, `tests/traintypes` |
| New source file | the list in `core/core.pri`, so the core-only build sees it |

## Traps

- **Everything is `float`, and results must stay bit-identical to the 2016 binary.** The baseline is the gate (`docs/testing.md`). Moving a float operation, changing the order of a sum or switching a type changes the last bit, and the last bit reaches the exported files.
- **`fvdmath` exists for a reason.** The C runtime's `asinf` and `asinhf` round differently from the 2016 binary's. `asin2016` and `asinh2016` reproduce 2016 exactly. Do not replace them with the library calls, and do not move other maths calls there without proof (`docs/decisions/0008-float-maths-reproduces-the-2016-runtime.md`, `docs/research/n2-matching-the-2016-maths.md`; why a last bit survives: `docs/research/n1-difference-from-2016-on-an-owner-project.md`).
- **A force section starts from the force the previous section measured.** A change early in the chain moves everything after it (`docs/research/r5-fvd-workflow.md`, A.1; what `forceNormal` is: `docs/research/r3-force-limits.md`).
- **The `.fvd` file is written back to front.** `writeBytes()` writes each block reversed, so numbers are big-endian and a `vec3` or the colours appear reversed. Use the helpers in `exportfuncs.cpp`, never a raw `fwrite` of a struct in new code.
- **`QColor` is written as raw bytes** (`track::saveTrack()`, 16 bytes each, guarded by a `Q_STATIC_ASSERT_X`). Do not change `track::display` into anything else without a version bump (issue 18).
- **`updateTrack()` recomputes from a node onward**, not the whole track, so a recompute after an undo can differ from the stored nodes in the last digits (`docs/testing.md`, "Undo and redo check").
- **Notices are synchronous.** A listener runs inside the call that causes it (for example `updateTrack()`), before that call returns.

## Check a change

From the repository root:

```
py -3 tools/check-core.py
tools\run-tests.cmd
py -3 tools/run-baseline.py --core
tools\build-windows.cmd
py -3 tools/run-baseline.py --undo-check --fixtures --imports
```

The first three need no full build of the program. The last two are the gate for the whole program and must end `PASS`. Details of each: `docs/testing.md`.
