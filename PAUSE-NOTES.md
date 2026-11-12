# Pause notes for unit E1 (delete before the pull request)

Paused for a PC restart on 2026-11-11, about 21:00. Branch `claude/e1-solver-trials`, from `origin/claude/stage-2`.

## Where it stands

- **All trial scripts are written and every trial has been run to completion once** (the "first run"). The note `docs/research/e1-solver-trials.md` is fully written from those results, with summary, method, Trials A to E, timing, "what changes in the plan", program behaviour found, owner's questions, "Could not establish".
- **The self-check (re-run everything from a clean state) was half done** when the pause came: `results/` and `projects/` were deleted and re-run in the order timing, C, B, D, E, A. timing, C, B and D finished and **matched the first run in every column except the wall-time columns** (checked by diff). E was killed mid-run, A never started. `results/trial_a.txt` and `results/trial_e.txt` in this commit are the **first run's** files, copied back (the scripts have not changed since that run, only the note). `projects/e-*` are from the interrupted re-run's build step (identical builds; the slack solves re-save nothing else).
- `results/timing.txt` is from the re-run (the note's timing table already uses it).

## What to do next

1. Rebuild: `cmd.exe //c "tools\build-windows.cmd"` (background; a few minutes).
2. Finish the self-check: `py -3 docs/research/e1-trials/trial_e.py` (about 8 min) and `py -3 docs/research/e1-trials/trial_a.py` (about 15 to 20 min), one at a time, then diff the new `results/trial_e.txt` and `results/trial_a.txt` against this commit's versions: all columns except wall times (`solve s`, `scan s`, `ms/eval`, `wall ms`, and the "run time" line) should match.
3. Read the note once more as the owner; check every number against `results/*.txt`.
4. Delete this file, commit (subject in plain words, message ending with `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>`), push, and open the pull request with `gh pr create --base claude/stage-2 --title "<plain words>" --body-file <file>`. A ready body was drafted in the old worktree's `.scratch/pr-body.md` (not committed); its text is reproduced at the end of this file.

How to run each script: README in `docs/research/e1-trials/` (`py -3 docs/research/e1-trials/<script>.py` from the repository root; needs `build/dist/fvdcore.exe`).

## Headline numbers (first run)

- A1: 100/100 random reachable goals converged on `gen-normal-shapes.fvd` section 3 (median 10 evaluations, max 13, median 413 ms wall, 41 ms program per evaluation); 13/20 partly-unreachable goals converged, 7 stalled with closest result.
- A2: R6's six starts: 2/6 converge, to t2 0.438 s, t_air 4.092 s (same as R6's stand-in).
- A3: one solution or none in all 42 cells; all 12 cells with a solution converged from the scan (1 to 10 evaluations); fixed start failed in 34/42; scan 340 to 924 evaluations per cell; force drift under 0.0015 g.
- B: first drop 1 m deeper, E1 re-solves: end heading 4.75 -> 0.33 deg, end speed 0.485 -> 0.004 m/s, end still 1.5 m off (slide). Energy edits keep pose not speed.
- C: 30/32 solved; onset measured within 2 % of k dG/T; quintic -0.6 g at 2 g/s has no solution.
- D: zero-g roll 31/32; overbank 11/16, all four 20 m/s cases fail near vertical.
- E: connector slack re-closes to 0.3 to 1.8 cm and 0.002 deg in 13 to 19 evaluations; unbanked lateral force 1.4 g for a 2 m lift change; straight+curve slack only for along-track misses.
- Timing: about 1 ms per 1,000 nodes per evaluation up to 50,000 nodes, 1 to 2 ms at 153,000 (156 to 290 ms).

## Found outside the unit (for the report)

1. `tools/fvdsession.py` never reads the program's standard error; `lenAssert` lines ("Assertion 'diff==diff' in ../../core/secforced.cpp line 76 failed.") are written there after a stall, and a long session then hangs when the pipe fills. Reproduce one line: open `projects/a-camelback.fvd`, send `anchor.setSpeed` 25. Workaround in `e1solver.Model` (a drain thread).
2. `anchor.setPositionY` with the y the project text shows (46 in `projects/b-baseline.fvd`) raises the track by 1.1 m (the heartline height); 44.9 keeps it.
3. The `pitchRate` measure is the rate of the wrapped pitch (reverses at vertical): no measure gives an unwrapped angle; a full loop between two section ends is invisible without a guard.
4. Traps for G1: a force section's length is its shortest unlocked function's; `transition.setLength` refuses 0; forces of thousands of g are reported near a stall.

## Drafted pull request body

See the "What this does / Tests / New dependency / For the owner" text in the note's summary; the draft said: headline findings (as above), the index row for docs/README.md
`| docs/research/e1-solver-trials.md | E1: the solver, parametric elements and pins with a slack piece tried on the real program through fvdcore session; scripts and tables in docs/research/e1-trials/ |`,
"Not compiled: no C++ file changed", "New dependency: none (Python standard library)", "For the owner: the lead session merges this piece; read the Summary, then 'What changes in the plan' and the owner's questions; two choices are his later (exit height or speed; how much lateral force and bank change in a connector); none of this was tried on one of his projects", and it ends with the Claude Code line.
