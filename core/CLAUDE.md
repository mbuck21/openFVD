# core/ (the model)

The track model and its physics: tracks, sections, nodes, transition functions, the project file, the NoLimits export files, the importers, roll smoothing and the train catalogue. It runs with no window and no OpenGL; the console program `fvdcore` (`cli/`, built into `build/dist/` by the main build) builds from this folder alone (`docs/testing.md`).

Read the root `CLAUDE.md` first, then `docs/codebase-map.md` for the data model and flows.

## What core may depend on

- **Allowed:** the C++ library, glm (through `lenassert.h`), QtCore (`QString`, `QList`, `QVector`, `QElapsedTimer`, the JSON classes) and the value type `QColor`.
- **Not allowed:** any header from `ui/` or `renderer/`, any QtWidgets header, the globals `gloParent` and `glView`, `options.cfg`.
- **Enforced by:** `tools/check-core.py` (fails on any of the above) and by the core-only build (`tests/coreonly/`, the test that builds `cli/` from `core/` alone; `docs/testing.md`). Run the check before every commit that touches this folder.
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
- **`trackState`** (`trackstate.*`): a track's whole state as a value: its saved bytes plus the live values they do not hold. `captureTrackState()` takes it; `restoreTrackState()` puts it back in place and recomputes the whole track. The base of undo (`docs/decisions/0011-undo-by-snapshot-commands-on-top.md`).
- **`trackHistory`** (`trackhistory.*`): the undo history of one track. Steps hold the state before and after, a label, the commands that made them and a selection. Scopes (`beginStep`/`endStep`, `historyScope`) make several changes one step or roll them back; `noteEdit` records a change already made. Undo and redo restore states. A restore rebuilds every section, function and transition, so after a rollback, undo or redo every target is found again from its numbers.
- **`commandRegistry`** (`command.*`, with the group files `commands_anchor.cpp`, `commands_section.cpp`, `commands_transition.cpp`, `commands_track.cpp`, `commands_smoothing.cpp`): every edit as a named command (`transition.setChange`), with typed parameters in model units, a description, a target given by numbers (`commandTarget`), and an apply function. `run()` validates and records one step; `describe()` is the list an agent reads. To add a command:
  1. In the group's file, write an apply function that makes exactly the model steps and the `updateTrack()` call of the window's slot, without moving float arithmetic.
  2. Fill a `commandSpec`: name `<object>.<verb><Property>`, description, target kind, undo label, each parameter with a unit (`"none"` if none) and a range if it has one, example arguments, and `merges` for a value typed digit by digit (`exampleTarget` when the default target does not fit).
  3. Append it to the list that file's function returns; nothing else registers it.
  4. Run `tools\run-tests.cmd`: `tests/commands` runs it on every baseline project with undo and redo and fails if a description, unit or example is missing. It also runs it at several places of every track and fails unless the track straight after it is the one saving and reopening gives, and the one undo then redo gives (decision record 0013, "running equals reopened"). `run()` takes care of that: after the apply function it puts the transitions through a load's sums (`track::settleTransitions()`) and computes the whole track from the anchor (`track::updateFromAnchor()`). While the apply function runs, `track::bDeferUpdate` is set, so its `updateTrack()` call computes nothing and each command computes the track once; an apply function must therefore not read nodes after its own `updateTrack()`. Set `settlesItself` only when the apply function ends in that state by itself (then nothing is deferred).

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
- **`updateTrack(index, node)` recomputes from a node onward**, not the whole track, and what it leaves can be centimetres (with roll smoothing, metres) away from the track the settings give: it keeps the smoothing ranges and nodes from before the edit. Every command therefore ends with the whole track computed from the anchor (`track::updateFromAnchor()`, decision record 0013), and **`updateFromAnchor()` on a running track must equal a fresh computation from the same settings** (a load). A new kind of section or recompute keeps that: nothing a section computes may depend on values left from its previous computation (overwrite a kept node as an appended one starts, take smoothing ranges after the nodes are computed), and the "running equals reopened" test in `tests/commands` is the judge.
- **A load is not the identity on every saved value** (issue 63, item 7): reading a function back links each transition's start and end again, which can move an end by a last digit; `track::settleTransitions()` makes the same sums on the running track. A load also takes the smoothing entries' names and ranges from the track.
- **A new field of the model must be saved, declared a live extra of the state (`trackState`), or derived from the others**, or undo loses it (decision record 0011).
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
