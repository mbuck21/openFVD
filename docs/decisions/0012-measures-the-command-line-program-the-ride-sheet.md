# 0012: Measures as a registry in core; the command-line program in `cli/`; the ride sheet

- Status: accepted
- Date: 2026-11-11
- Needs owner approval: no for the structure. The ride sheet's definitions are the owner's, given on 2026-11-11.

## Context

- Record 0010 widened the analysis framework to "measures and findings" and made the ride sheet (FR-18) its first user.
- The plot has fourteen curve types addressed by index, with index arithmetic in several places (`ui/graphhandler.h`, `ui/graphwidget.cpp`); the 3D view colours the track by one of five fixed values through vertex attributes and hand-written ramps in `shaders/track.vert`. Adding a curve or a colour per measure would repeat that for every new measure.
- Simple ride statistics exist only in window code (`batchScenarios::trackStats()` in `ui/`). The along-track force is stored nowhere.
- The core-only program `fvdcore` lives under `tests/coreonly/`, is built only by the test runner and has no deployed copy.
- Several Stage 2 pieces each add commands to that program.

## Decision

1. **A measure is an entry in a registry in core** (`core/measure.*`): a name, a unit, a description and one function that fills a value per node. Measures are computed beside the nodes and never written into them, so no project computes differently and the comparison with the 2016 binary is untouched.
2. **A finding** (`core/finding.*`) is a stretch of track where a measure breaks a limit: from node, to node, the worst value, the limit, a severity and a sentence. A check is a function from a track to findings. Stage 2 ships one plain check (speed below 3 m/s; vertical force outside -1.5 to +5 g) to prove the path; the real checks are Stage 3.
3. **The windows and the command line read the registry; they hold no measure maths.** The plot gets a "Measures" group filled from the registry. The 3D view gets one "colour by measure" mode with a chooser, not a mode per measure. Findings show as shaded spans on the plot and a tint on the track.
4. **The command-line program moves to `cli/`,** is built and deployed by `tools/build-windows.*` beside `FVD.exe`, and keeps the name `fvdcore`. Each command is a file of its own (`cli/cmd_<name>.cpp`) listed in one table, so a new command is a new file and one line. `tests/coreonly/` stays as the proof that the program builds from `core/` alone.
5. **The command line prints metric units** (m, m/s, g, degrees, s). Display units (feet, mph) are the windows' business.
6. **The ride sheet** is a panel in the main window beside the read-out row, visible while editing, hidden from the menu if unwanted. Its numbers come from one core function (`core/ridesummary.*`) that `fvdcore summary` also prints. It refreshes a moment after a change, not on every frame of a drag.
7. **Definitions on the ride sheet, the owner's:**
   - *Floater airtime:* seconds with the vertical force at the heartline from +0.1 g down to -0.5 g. The upper bound is +0.1 g and not 0, so a hill designed for exactly 0 g counts.
   - *Ejector airtime:* seconds below -0.5 g.
   - *Total airtime:* both together.
   - *Inversion:* counted each time the train's up direction passes more than 135 degrees from upright.
   - The three thresholds are named constants in one place and become ride rules in Stage 5 (FR-14).
8. **The import experiment R11 is deferred** at the owner's word until NoLimits 2 tracks are at hand. It is needed before I2 (Stage 6), not before I1.

## Where enforced

- `core/measure.*`, `core/finding.*`, `core/ridesummary.*`; `tests/measures/`, `tests/ridesummary/`.
- `cli/`; `tools/build-windows.sh`; `tools/run-baseline.py --core`.
- `tools/check-core.py` already fails if `core/` reaches into `ui/` or `renderer/`.
- `docs/roadmap.md`, Stage 2.
