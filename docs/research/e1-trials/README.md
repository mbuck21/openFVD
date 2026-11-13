# E1 trials: the solver and parametric elements on the real program

The scripts behind `docs/research/e1-solver-trials.md`. They drive the real model through `fvdcore session` (`cli/README.md`, `tools/fvdsession.py`); nothing in the program is changed. Python 3, standard library only.

## Before you run

1. Build the program: `tools\build-windows.cmd` (it leaves `build\dist\fvdcore.exe`, which every script uses).
2. Run each script from the repository root with `py -3`. Each prints its rows as it goes and writes `results/<name>.txt`; the tables in the note are copied from those files.

| Script | What | Run time here (one at a time) |
|---|---|---|
| `trial_a.py` | A1 aim a force section of `gen-normal-shapes.fvd` at an end height and heading, 100 random reachable goals and 20 goals partly out of reach; A2 R6's six starting guesses; A3 the camelback map (6 speeds x 7 airtimes) | about 15 to 20 min |
| `trial_b.py` | the exit contract: two camelbacks, four changes plus R6's +3 % valley speed, four cases each | about 4 min |
| `trial_c.py` | limits as inputs: transition lengths from an onset limit, cubic and quintic, 32 cases | about 2 min |
| `trial_d.py` | the zero-g roll and the overbanked turn at 20 to 35 m/s, entry bank 0 and 10 degrees, both orientations | about 8 min |
| `trial_e.py` | re-closing a layout at a pinned end after an early edit, two layouts, two kinds of slack, and how large a lift change each slack absorbs | about 8 min |
| `timing.py` | the cost of one evaluation against the length of the ride (8,000 to 153,000 nodes) | under a minute |

`trial_a.py --quick` runs a small part of A in about a minute (`results/trial_a_quick.txt`).

Timings depend on the machine; the number of **evaluations** does not, and every timing claim in the note is backed by it. Run `timing.py` alone (not beside another trial) for the per-evaluation costs.

## The files

| File | What |
|---|---|
| `e1solver.py` | the solver (damped Newton, forward-difference Jacobian, backtracking, bounds, "invalid counts as worse"), `Model` (a session that sends only the commands whose value changed and keeps the program's standard error drained), `Problem` and `Unknown`, and the readers: crest height by golden-section search on the `height` measure, the steepest point by bisection on `pitchRate`, the loop guard |
| `elements.py` | the camelback of R6 section 2.2 (three force sections), its scan for a valid start, the zero-g roll and the overbanked turn |
| `layouts.py` | the first drop (solved to a level valley), the banked turn, the two kinds of slack piece, the pin, and why the layouts start at the top of the lift |
| `fvdtext.py` | writes test projects as project text (`docs/project-text-format.md`) and lets `fvdcore fromtext` make the `.fvd` |
| `common.py` | the result writer and folders |
| `projects/` | the projects the trials start from or produce (`.fvd` and, for the generated ones, the `.json` text they were made from). Each script rewrites the ones it uses. |
| `results/` | the outputs, one text file per script |
| `work/` | throw-away files of a run (ignored by git) |

## What one evaluation is

Set the unknowns with session commands (only those whose value changed), send `recompute` (a whole-track recompute; issue 63, item 14), then read: `sectionEnd` of the sections the goals need, plus for a camelback about 20 `measures` requests for the crest height, about 25 for the steepest points and about 40 for the loop guard. The program's own share (`stats`: `run_ms` + `recompute_ms`) is printed beside the wall time where it matters.
