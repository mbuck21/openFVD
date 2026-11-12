# E1: The solver and parametric elements, tried on the real program

Unit E1 of Stage 2 (`docs/roadmap.md`), for FR-7, FR-10 and FR-12. Written 2026-11-11 from trials run on `claude/stage-2` through `fvdcore session`. Nothing in the program was changed. The scripts, the projects they use and their outputs are in `docs/research/e1-trials/`; its README says how to reproduce every table. Builds on `r5-fvd-workflow.md` (C1 goal-seeking, C2 pins with a slack piece) and `r6-parametric-elements.md` (Part 2, Part 3, section 4.2, whose list of experiments was the protocol here).

## Summary

- **The plan for G1 holds.** A plain damped Newton solver with a finite-difference Jacobian, working only through the program's named commands, aims sections and elements reliably once it starts from a valid shape: **100 of 100** random reachable goals on a force section of a baseline project converged (median 10 evaluations, at most 13); the camelback converged in **every one of the 12 map cells that have a solution**, in 1 to 10 evaluations from the scan's start; the zero-g roll in **31 of 32** cases. The real program reproduced R6's 2D stand-in almost to the digit (the same camelback in 2 of 6 starting guesses, t₂ 0.438 s and t_air 4.092 s both times; Experiment C's lengths to 0.1 m).
- **Finding a valid start is the expensive part, and it is costly on the real program.** R6's fixed start failed in 34 of 42 cells; here R6's "scan for a valid start" took **338 to 924 evaluations per element** (8 to 31 s from Python, 4 to 16 s of it the program's own time, 10 to 18 ms per evaluation on a 6,000 to 8,000-node element). R6's line "if the scan takes more than about a second, it needs a cheaper first guess" **fired**.
- **A solve on a whole ride is not "live".** One evaluation costs about **1 to 2 ms per 1,000 nodes** (measured from 8,000 to 153,000 nodes: 8 ms to 156 to 290 ms), half in the command and its undo snapshot, half in the whole-track recompute. Aiming a section of the 14,000-node baseline project took a median 0.4 to 0.5 s (10 evaluations at 41 to 50 ms of the program's own time each: two commands and a recompute); on a 150,000-node ride the same solve would take **about 3 to 6 s**. R6's "a solve over 0.2 s means a button" line **fired** for anything but a short ride. Recommendation: **Solve is a button**, as R5 and R6 already proposed.
- **The exit contract works for reshaping edits and not for energy edits**, as R6 predicted: with the first drop 1 m deeper, re-solving the first camelback cut the change at the end of the track from 4.75° to 0.33° in heading and from 0.49 m/s to 0.004 m/s in speed (13 to 120 times), but the rest of the ride still slides 1.5 m along the track. With the lift 1 m higher, the elements keep their pose but everything after them is 0.2 to 0.3 m/s faster, and the turn after them comes out 1.2° different.
- **Pins with a slack piece work (FR-12's first slice).** A single connector force section (its duration and two bumps each of vertical and lateral force) re-closed two layouts after every early edit to **0.3 to 1.8 cm and under 0.002°** in heading and pitch, in 13 to 19 evaluations. A straight plus a curve re-closes x, z and heading only, and only when the miss is mostly along the track. But the connector with lateral force and no bank is ugly: a 2 m lift change already needs 1.4 to 1.8 g of unbanked lateral force. **The connector must bank (a sixth unknown, roll) or be solved as a small optimisation with a smoothness preference**, which R5 named as the fallback.
- **Limits as inputs work** (Trial C): onset limits from 2 to 10 g/s set every transition length; 30 of 32 cases solved; the program's own onset measure agreed with k·ΔG/T to within 1 % (2 % for a 0.1 s window). The quintic with −0.6 g airtime at 2 g/s has no solution: the transitions eat the airtime first.
- **Turning elements need more than a simple solve near vertical.** The overbanked turn converged in 11 of 16 cases (13 to 26 evaluations), but failed in every case at 20 m/s, where a section ends with the track pitched 39° to 89° down. R6's line "fails often near the crest" **fired** for low speeds: offer the overbank only with a start search, and expect multiple shooting there.
- **Three things in the program and its tools to know about** (section "Program behaviour found"): the Python helper hangs a long session because it never reads the program's error output; `anchor.setPositionY` takes the heartline point while the project text shows the stored start point, 1.1 m lower here, and no document says so; the `pitchRate` measure reverses at the vertical, so a full loop between two section ends is invisible to a solver unless it guards for it.
- **Owner's questions:** the trials support "a button, and a result to accept", "camelback, then zero-g roll, then overbank", and "keep exit height by default"; they cannot answer which exit (height or speed) the owner prefers for his edits, and none of this was tried on one of his own projects.

---

## Method

**How the program is driven.** Each script opens a project with `fvdcore session` (through `tools/fvdsession.py`) and sends named commands. An **evaluation** is: set the unknowns (only the commands whose value changed), send `recompute` (a whole-track recompute, so the numbers do not depend on issue 63, item 14), then read. Goals are read from `sectionEnd` (end position, heading, pitch, bank, speed, and `non_finite`) and, for an element, from `measures` (`height` for the crest by golden-section search, `pitchRate` for the steepest point and the loop guard). Evaluations are the machine-independent unit of every timing here; wall times are from the machine that ran the trials, from Python, with the pipe; where a wall time is given as a range it covers the two complete runs of the scripts (the second run reproduced every other column of Trials A and E exactly).

**The solver** (`e1-trials/e1solver.py`, one small module all trials use): damped Newton on the goals divided by their tolerances; a forward-difference Jacobian (backward where a bound or an invalid run is hit); the Newton step solved exactly (least squares if there are more goals than unknowns); the step cut to each unknown's bounds and to a largest step; backtracking by halving until the scaled miss falls (down to 1/64 of the step). An evaluation that is not a number (`non_finite`, a stall) or breaks a shape guard returns "invalid" and counts as worse, never as an error. A result is "converged" (all goals within tolerance), "start invalid", "stalled" (no step lowers the miss: the closest result is kept), "no valid step" (no Jacobian column could be built) or "budget". The condition number of the Jacobian at the start is reported.

**Goals in unwrapped angles.** Section ends report pitch wrapped to ±90° and heading wrapped to ±180°. Pitch is unwrapped from the heading (a heading flipped by 180° means the track has gone over the top or under the bottom) and from the previous end; heading and bank by continuity from section end to section end, or against the commanded value where that is the only reference (the zero-g roll's 360°). That is not enough: in the camelback trials the 3.5 g pull-out sometimes turned a **full loop between two section ends**, which reads as a normal exit. The `pitchRate` measure cannot be summed to unwrap it, because it is the rate of the wrapped pitch and reverses at the vertical (section "Program behaviour found"). The guard that worked: in a valid drop the pitch rate reverses once; a loop adds two reversals (sampled every 50 nodes). Until it was added, the camelback map showed hills that were loops.

**Test projects** are written as project text by `fvdtext.py` and turned into `.fvd` by `fvdcore fromtext`; physics as in R6 (heartline 1.1 m, friction 0.03, resistance 2e-5), time-argument force sections. A force section's length is that of its shortest unlocked function, so the flat roll and lateral functions are locked; otherwise changing a force transition's length does not change the section. The layouts for Trials B and E start at the top of the lift (the anchor, 46 m, 5 m/s): in FVD a lift is sections held at the lift speed and the train leaves its top at that speed whatever its height, so "lift 1 m higher" with the first drop's valley kept on the ground is the anchor 1 m higher with the first drop re-solved (`layouts.py` says why this is the same track without the lift's 20,000 slow nodes).

**Tolerances.** 0.05° and 0.05 m for elements (R6), 0.01 m and 0.05° for the section aim, 3 cm and 0.05° for the pins. A section's end moves in whole 1 ms nodes, 2.5 cm at 25 m/s, so a position tolerance under about 2 cm is met only when another unknown can make up the last node; with 1 cm the connector stalled at 1.04 cm.

## Trial A: aim one section, and the camelback map

**A1, R5 C1's trial.** `gen-normal-shapes.fvd`, section 3 (a 4.6 s force section; the track has 14,124 nodes). Free: the change of normal-force transition 5 and the size of the lateral-force bump (transition 1). Goals: end height and end heading.

| Goal | Result | Evaluations | Wall time | Solved values |
|---|---|---|---|---|
| end 1.0 m lower, heading +10° | converged, miss 0.00 cm and 0.000° | 7 (2 Newton steps) | 235 to 394 ms (227 to 379 ms in the program) | −0.287 g, lateral bump 0.119 g |
| 100 random reachable goals | **100 converged** | median 10, max 13 | median 413 to 492 ms, max 631 to 976 ms | program time 41 to 50 ms per evaluation |
| 20 goals in a box, partly out of reach (height −30 to +15 m, heading ±60°) | 13 converged (4 to 16 evaluations), 7 stalled (22 to 58) with the closest result kept, for example 0.71 m and 1.91° short | | | |

The Jacobian's condition number at the start was 1.5: height and heading are independent here. A stalled solve keeps a closest result, which is what R5's "cannot reach; closest is ..." needs; the evaluations it costs (22 to 58) are what a "Solve" button spends before saying so.

**A2, R6's six starting guesses** (entry 30 m/s, airtime 0.0 g, crest 38 m above the entry; unknowns t₂ = t₄ and t_air; goals exit level and crest height):

| start t₂, t_air (s) | result | evaluations | t₂ (s) | t_air (s) | exit pitch (°) | crest (m) | exit height (m) | exit speed (m/s) |
|---|---|---|---|---|---|---|---|---|
| 0.5, 2.0 | start invalid | 1 | | | | | | |
| 0.2, 4.0 | converged | 19 | 0.438 | 4.092 | 0.03 | 38.00 | −1.08 | 28.68 |
| 1.0, 1.0 | start invalid | 1 | | | | | | |
| 0.1, 8.0 | converged | 16 | 0.437 | 4.094 | −0.03 | 38.00 | −1.16 | 28.71 |
| 2.0, 3.0 | start invalid | 1 | | | | | | |
| 0.6, 0.5 | start invalid | 1 | | | | | | |

The same two starts converge as in R6's stand-in, to the same hill (R6: 0.438 s, 4.092 s, −1.10 m, 28.69 m/s). Evaluations here count the Jacobian's runs; R6's "runs" were 7 and 11.

**A3, the camelback map** (crest 25 m above the entry, exit level). Each cell: solutions the scan found / reachable crest range (m) / Newton from R6's fixed start / Newton from the scan's point nearest 25 m (evaluations, or "fail"). The scan: t₂ from 0.05 to 1.95 s in 0.1 s steps and 2.2 to 3.95 s in 0.25 s steps; at each, t_air solved for a level exit from the previous point's value, or from a grid of t_air (0.25 to 8 s) with Newton from each sign change when that fails.

| entry (m/s) | +0.2 g | 0.0 g | −0.2 g | −0.4 g | −0.6 g | −0.8 g | −1.0 g |
|---|---|---|---|---|---|---|---|
| 15 | 0 / 4–10 / fail / fail | 0 / 4–10 / fail / fail | 0 / 5–8 / fail / fail | no valid hill | no valid hill | no valid hill | no valid hill |
| 20 | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill |
| 25 | 0 / 29–30 / fail / fail | 0 / 27–30 / fail / fail | 0 / 25–29 / fail / fail | 1 / 24–28 / fail / 4 | 1 / 22–27 / fail / 7 | 1 / 21–26 / fail / 4 | 1 / 21–26 / fail / 8 |
| 30 | 0 / 34–43 / fail / fail | 0 / 30–43 / fail / fail | 0 / 28–42 / fail / fail | 0 / 26–40 / fail / fail | 1 / 24–38 / 10 / 7 | 1 / 23–38 / 10 / 4 | 1 / 22–37 / 7 / 7 |
| 35 | 0 / 36–59 / fail / fail | 0 / 32–59 / fail / fail | 0 / 29–57 / fail / fail | 0 / 27–54 / fail / fail | 1 / 25–55 / 14 / 4 | 1 / 24–53 / 10 / 10 | 1 / 22–52 / 7 / 4 |
| 40 | 0 / 38–77 / fail / fail | 0 / 33–76 / fail / fail | 0 / 30–74 / fail / fail | 0 / 27–73 / fail / fail | 0 / 26–70 / fail / fail | 1 / 24–68 / 7 / 4 | 1 / 23–65 / 7 / 1 |

What it shows:
- **One solution or none, never two**, as in R6. A solution exists exactly where 25 m lies inside the scan's reachable range, and in all 12 such cells Newton from the scan converged in 1 to 10 evaluations (17 to 262 ms). Where 25 m is outside the range it stalls, correctly.
- **The fixed start failed in 34 of 42 cells**, including every 25 m/s cell with a solution, exactly R6's count. R6's six guesses converged in at most 2 of 6 in any cell.
- **"No solution" has the same two sides as in R6:** too slow (15 to 20 m/s; at 15 m/s the real program finds small 4 to 10 m hills that R6's stand-in did not), or 25 m is below the smallest hill this pull-up makes (30 to 40 m/s with weak airtime).
- **The scan is costly:** 338 to 924 evaluations per cell, 8 to 31 s from Python; 4 to 16 s of it in the program itself (10 to 18 ms per evaluation, the `ms/eval` column of `results/trial_a.txt`, which counts the commands and the recompute, not the reads). Most of it is spent on t₂ values with no level exit at all.
- **Force drift is negligible:** the exit force differs from 1 g by at most 0.0008 g; the force at the start of the crest section from the airtime asked for by at most 0.0015 g (R6 section 2.1, item 2).

## Trial B: the exit contract

Layout (`projects/b-baseline.fvd`): top of the lift (46 m, 5 m/s), the first drop solved to a level valley at height 0 (29.28 m/s), camelback E1 (0.0 g, crest at 36 m), camelback E2 (−0.6 g, crest at 26 m), a banked turn, a 30 m run-out. Each element's contract: exit level at height 0 with its crest at the stated height, unknowns t₂, t_air, t₄ (finding the baseline took 726 and 731 evaluations, most of it the scan). Each change re-solves the first drop so its valley stays on the ground, except "1 m deeper"; re-solves start from the baseline solution. Differences from the baseline at E1's exit, E2's exit and the end of the track (dx along the first heading, dy height, d pitch and d heading in degrees, dv in m/s; "distance" is between the two end points):

| change | case | E1 dx | E1 dy | E1 dv | E2 dy | E2 dv | end distance | end dy | end dheading | end dv | evaluations |
|---|---|---|---|---|---|---|---|---|---|---|---|
| first drop 1 m deeper | nothing re-solves | 4.08 | 0.01 | −0.047 | 1.35 | −0.528 | 6.69 | 1.13 | 4.75 | −0.485 | 0 |
| | E1 re-solves | 1.51 | 0.02 | −0.041 | −0.03 | −0.018 | 1.52 | −0.09 | −0.33 | 0.004 | 9 |
| | E1 and E2 re-solve | 1.51 | 0.02 | −0.041 | 0.16 | −0.090 | 0.66 | 0.15 | 0.71 | −0.092 | 18 |
| | both keep exit speed | 1.60 | −0.02 | −0.028 | 0.06 | −0.055 | 0.72 | 0.03 | 0.38 | −0.047 | 752 |
| lift +1 m | nothing re-solves | 4.08 | 1.01 | −0.047 | 2.35 | −0.528 | 6.93 | 2.13 | 4.75 | −0.485 | 0 |
| | E1 re-solves | 5.36 | 0.03 | 0.287 | 0.27 | 0.189 | 8.61 | 0.37 | −0.66 | 0.172 | 5 |
| | E1 and E2 re-solve | 5.36 | 0.03 | 0.287 | 0.23 | 0.207 | 9.36 | 0.28 | −1.23 | 0.207 | 27 |
| | both keep exit speed | 4.35 | 0.93 | −0.014 | 1.09 | −0.065 | 4.19 | 1.05 | 0.41 | −0.053 | 38 |
| lift −1 m | nothing re-solves | −4.05 | −1.05 | 0.038 | −2.43 | 0.451 | 5.41 | −2.37 | −3.84 | 0.460 | 0 |
| | E1 and E2 re-solve | −5.71 | 0.02 | −0.287 | 0.23 | −0.306 | 10.85 | 0.07 | 1.69 | −0.271 | 14 |
| | both keep exit speed | −4.79 | −0.82 | −0.003 | −0.73 | 0.003 | 5.22 | −0.77 | −0.41 | 0.023 | 22 |
| start speed 8.79 m/s (+3 % at the valley) | nothing re-solves | 18.93 | 2.44 | −0.074 | 5.41 | −1.151 | 11.41 | 4.77 | 10.71 | −0.979 | 0 |
| | E1 re-solves | 22.00 | 0.06 | 0.733 | 0.80 | 0.445 | 28.61 | 1.20 | −0.33 | 0.336 | 9 |
| | E1 and E2 re-solve | 22.00 | 0.06 | 0.733 | 0.17 | 0.672 | 33.70 | 0.48 | −3.76 | 0.619 | 14 |

All rows, the "start speed +3 %" change (5.00 to 5.15 m/s at the top of the lift: +0.02 m/s at the valley, too small to matter) and the `fvdcore compare` table of every section end are in `results/trial_b.txt`.

What it shows:
- **Reshaping (first drop 1 m deeper): the contract works.** Re-solving E1 alone brings the end of the track back from 4.75° to 0.33° in heading, from 1.13 m to 0.09 m in height and from 0.49 m/s to 0.004 m/s in speed: 13 to 120 times less. The end still lies 1.5 m away, almost all of it along the track (E1's length changed). That slide is what R6 expected and what only a pin removes.
- **Energy edits (lift ±1 m, a faster start): the contract keeps the pose, not the speed.** E1 exits level at its height but 0.29 m/s faster per metre of lift (0.73 m/s for +3 % at the valley); the unsolved turn after the elements then comes out 0.3° to 3.8° different and the end lands 7 to 34 m away. "Keep exit speed" instead keeps the end within 4 to 5 m and 0.4° but 0.8 to 1.1 m higher or lower. R6's "an element can promise its exit height or its exit speed, not both" holds on the real model.
- **In FVD, "lift 1 m higher" and "first drop 1 m deeper" are the same shape change** (the rows "nothing re-solves" agree to the last digit except the 1 m of height): what differs is only where the elements' absolute goals sit. The contract decides which of the two is an energy change.
- **Continuation works:** re-solves from the previous solution took 5 to 38 evaluations. Two re-solves of "keep exit speed" lost the solution and fell back to the scan (752 and 821 evaluations; one did not converge): the speed goal is harder to hold from the height solution's start.
- R6's line "re-solving does not cut the downstream change by at least ten times for reshaping edits" **did not fire** for pose and speed; it did for position along the track (4 times), as R6 expected.

## Trial C: limits as inputs

Every transition as short as an onset limit J allows (k·ΔG/J, k = 1.5 for a cubic, 1.875 for a quintic); unknowns the valley force G_p and t_air; goals exit level, crest 25 m above the entry; start from a scan of G_p (1.5 to 5.0 g). Selected rows (all 32 in `results/trial_c.txt`):

| shape | airtime (g) | entry (m/s) | J (g/s) | result | evals | G_p (g) | t_air (s) | t₂ (s) | length (m) | span (m) | program's onset (g/s) | 0.1 s onset (g/s) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cubic | 0.0 | 30 | 2 | converged | 86 | 2.12 | 1.67 | 1.59 | 180.9 | 169.9 | 2.03 | 2.00 |
| cubic | 0.0 | 30 | 10 | converged | 130 | 3.44 | 3.27 | 0.52 | 141.0 | 127.4 | 10.03 | 9.88 |
| cubic | −0.6 | 30 | 2 | converged | 92 | 2.12 | 0.31 | 2.04 | 171.7 | 160.1 | 2.03 | 2.00 |
| cubic | −0.6 | 30 | 10 | converged | 87 | 3.66 | 2.22 | 0.64 | 125.3 | 109.2 | 10.03 | 9.92 |
| cubic | −0.6 | 40 | 2 | converged | 93 | 2.08 | 0.34 | 2.01 | 245.9 | 238.3 | 2.02 | 2.00 |
| quintic | 0.0 | 30 | 2 | converged | 89 | 1.98 | 1.29 | 1.85 | 188.8 | 178.1 | 2.02 | 2.00 |
| quintic | 0.0 | 30 | 10 | converged | 79 | 3.23 | 3.12 | 0.60 | 144.6 | 131.2 | 10.03 | 9.83 |
| quintic | −0.6 | 30 | 2 | **no solution** (stalled) | 106 | | | | | | | |
| quintic | −0.6 | 30 | 3 | converged | 105 | 2.24 | 0.61 | 1.77 | 163.0 | 150.5 | 3.03 | 3.00 |
| quintic | −0.6 | 40 | 2 | **no solution** (stalled) | 98 | | | | | | | |

What it shows:
- **The cubic rows reproduce R6's stand-in** (2.12 g, 1.67 s, 180.9 m, 169.9 m at 0.0 g, 30 m/s, 2 g/s; 3.44 g and 141.0 m at 10 g/s): the stand-in's physics was right.
- **Going from 10 g/s to 2 g/s makes the element 26 to 37 % longer** with gentler valleys (3.2 to 3.7 g down to 1.95 to 2.1 g).
- **The limit eats the airtime first, then the solution.** With −0.6 g at 2 g/s the cubic keeps 0.31 to 0.34 s of sustained airtime; the quintic, whose transitions are 25 % longer for the same onset, has none left and the element has no solution.
- **The onset the program measures agrees with k·ΔG/T:** its `jerkNormal` (a 20 ms difference) is 0 to 1 % above J, the change over a 0.1 s window 0 to 2 % below. R6's line "differs by more than 10 %" **did not fire**: transition lengths can be computed from the shape, not solved against the checker.

## Trial D: two harder elements

**D1, the zero-g roll** (camelback with 2.5 g valleys, 1 s ramps, 0.4 s holds; on its crest a roll rate 0 → r → 0 over t_air; unknowns t₂ = t₄, t_air, r; goals exit level, crest height at the middle of the reachable range for that speed, roll 360°). 32 cases: airtime 0.0 and −0.2 g, entry 20, 25, 30, 35 m/s, entry bank 0° and 10°, quaternion and euler orientation. A few rows (all in `results/trial_d.txt`):

| airtime | orientation | entry (m/s) | entry bank | result | all evals / Newton | t_air (s) | r (°/s) | heading drift (°) | exit bank (°) |
|---|---|---|---|---|---|---|---|---|---|
| 0.0 | quaternion | 25 | 0 | converged | 511 / 9 | 2.330 | 308.6 | −0.01 | 0.00 |
| 0.0 | quaternion | 25 | 10 | converged | 466 / 9 | 2.249 | 310.0 | 35.91 | 4.49 |
| 0.0 | euler | 20 | 0 | **stalled** (exit pitch −0.30°) | 471 / 11 | 2.041 | 352.7 | −0.13 | 0.00 |
| 0.0 | euler | 25 | 10 | converged | 569 / 19 | 2.256 | 319.1 | 37.81 | 9.99 |
| −0.2 | quaternion | 30 | 0 | converged | 219 / 9 | 2.333 | 309.5 | 0.04 | 0.00 |
| −0.2 | quaternion | 30 | 10 | converged | 273 / 9 | 2.262 | 314.3 | 26.25 | 6.13 |

- **31 of 32 converged**, the final Newton in 9 to 19 evaluations, after a scan of 170 to 690. The roll goal is independent of the pitch goals (the roll does not change the path at 0 g, as R6 said), so adding it costs almost nothing.
- **R6's worry about −0.2 g did not show at the exit:** the track yaws during the roll, but the symmetric roll brings the heading back to within 0.12°.
- **An entry with 10° of bank is the real problem:** the bank persists through both 2.5 g valleys and turns the whole element by 24° to 38°, and the exit keeps 4° to 7° (quaternion) or 10° (euler) of bank. R6's recommendation stands: refuse a banked entry in version 1, or make exit heading and bank goals.

**D2, the overbanked turn** (pull-up at 2.5 g with a roll-in, a hold at 1.0 g, a roll-out with a pull-out; unknowns roll-in rate, hold, roll-out rate; goals heading change 180°, exit pitch 0, exit bank 0), from one fixed start, then from the previous speed's solution:

| orientation | entry bank | 20 m/s | 25 m/s | 30 m/s | 35 m/s |
|---|---|---|---|---|---|
| quaternion | 0° | budget (78); pitch −53°/−57° at the section ends | converged (17); bank 116° | converged (21); bank 71° | converged (21); bank 48° |
| quaternion | 10° | stalled (27) | converged (21); bank 112° | converged (21) | converged (26) |
| euler | 0° | stalled (42); pitch −89° at the hold's end | stalled (24) | converged (13); bank 77° | converged (17) |
| euler | 10° | stalled (23) | converged (23); bank 103° | converged (17) | converged (21) |

- **11 of 16 converged** with three unknowns; when it converges, heading and bank errors are under 0.05°. Three unknowns were enough, so R6's "more than about five unknowns" did not fire.
- **It fails at low speed, where the track goes near vertical** inside the element (pitch 39° to 89° down at section ends), and in one euler case at 25 m/s. Whether a solution exists there was not established (no scan was built for this element). R6's "fails often near the crest" **fired** for the low-speed end: before an overbank is offered it needs a start search, and probably multiple shooting (aim at the crest first, then the exit).
- **The same element is only "overbanked" at 25 m/s:** at 30 to 35 m/s the solution banks 48° to 77°. The element's definition must state its bank (a goal), not leave it to the solver.
- **Orientation matters, as R6 said:** with quaternion orientation the world bank at the end of the roll-in is 116° for a commanded 57° (the roll is about the pitched-up track), with euler it equals the command. The solved numbers differ by up to a factor of two; an element must fix its orientation.

## Trial E: re-close after an early edit (pins and slack, FR-12)

Two layouts from the top of the lift: **L1**, out and back (first drop, camelback, a 180° banked turnaround, a 60 m run back, then the slack); **L2**, a twister (first drop, a 90° turn, camelback, a second 90° turn, a run back, then the slack). The end of the track is the pin (position, heading, pitch, bank); it stands in for the station. Both come back towards the start without closing a circuit (L1 ends 92 to 106 m from its start, L2 280 to 295 m). Slack (a): a straight's length and a flat curve's angle and radius (goals x, z, heading). Slack (b): one connector force section, its duration and two quartic bumps each of vertical and lateral force (goals x, y, z, heading, pitch; bank not a goal). Policies: "slack only", or "contract + slack" (the camelback and the turns re-solve their exits first, then the slack).

| layout | slack | edit | policy | miss before (cm / °heading) | result | miss after (cm; ° heading / pitch / bank) | slack forces after (min / max vertical / lateral g) | evaluations (contract + slack) |
|---|---|---|---|---|---|---|---|---|
| L1 | (b) | first drop 1 m deeper | slack only | 1213 / 3.19 | converged | 0.6; 0.000 / 0.000 / −0.04 | 0.87 / 1.17 / 0.81 | 0 + 19 |
| L1 | (b) | first drop 1 m deeper | contract + slack | 1213 / 3.19 | converged | 0.5; 0.000 / 0.000 / −0.87 | 0.93 / 1.07 / 0.14 | 16 + 13 |
| L1 | (b) | lift 1 m higher | slack only | 1205 / 3.19 | converged | 1.8; 0.000 / 0.000 / 0.00 | 0.95 / 1.09 / 0.83 | 0 + 19 |
| L1 | (b) | airtime 0.2 g stronger | slack only | 38406 / −81.6 | stalled | 34695 | | 0 + 49 |
| L1 | (b) | airtime 0.2 g stronger | contract + slack | 38406 / −81.6 | converged | 0.6; −0.002 / −0.001 / −1.28 | 0.88 / 1.12 / 0.34 | 20 + 13 |
| L2 | (b) | first drop 1 m deeper | slack only | 1082 / 1.46 | converged | 0.3; 0.000 / 0.000 / −0.18 | 0.96 / 1.04 / 0.58 | 0 + 19 |
| L2 | (b) | lift 1 m higher | contract + slack | 1082 / 1.46 | converged | 0.5; 0.000 / 0.000 / 0.00 | 1.00 / 1.00 / 0.52 | 17 + 13 |
| L1 | (a) | first drop 1 m deeper | slack only | 1232 / 3.18 | stalled (straight at its lower bound) | 676 | | 0 + 19 |
| L1 | (a) | first drop 1 m deeper | contract + slack | 1232 / 3.18 | converged | 111 (all height); −0.006 / −0.51 / −0.70 | 1.00 / 1.00 / 0.52 | 16 + 9 |
| L2 | (a) | first drop 1 m deeper | contract + slack | 1103 / 1.46 | converged | 7.9; 0.004 / 0.002 / 0.05 | 1.00 / 1.00 / 0.42 | 21 + 13 |
| L2 | (a) | lift 1 m higher | contract + slack | 1102 / 1.46 | stalled (straight at its lower bound) | 89 | | 17 + 31 |

All 24 rows are in `results/trial_e.txt`. Wall time per converged re-close: 0.4 to 0.5 s for the slack alone, 1.0 to 2.6 s with the contract (two runs; wall times differed by up to 1.5 times between them), on layouts of 24,000 to 27,500 nodes.

How large a lift change the slack absorbs (L1; "absorbed" means converged with the slack's vertical force within −1.0 to 4.5 g and its lateral force at most 1.0 g; each size starts from the previous size's solution):

| slack | policy | absorbed | first size that fails, and why |
|---|---|---|---|
| (a) straight + curve | contract + slack | −1 to +4 m | −2 m and +8 m: no convergence |
| (b) connector | slack only | −1 to +1 m | ±2 m: lateral force 1.4 to 1.8 g; −4 m: no convergence |
| (b) connector | contract + slack | −2 to +8 m | −4 m and +16 m: lateral force 1.1 to 1.7 g; −8 m: no convergence |

What it shows:
- **R5 C2's cheap trial passes for the connector:** every edit that leaves the layout recognisable re-closes to 0.3 to 1.8 cm, with heading and pitch exact to 0.002°, in 13 to 19 evaluations. The rest of the ride after the pin is untouched by construction.
- **Bank is the connector's missing goal:** with no roll unknown, up to 1.3° of bank miss remains. A full pose needs six unknowns (R5 C2 said so).
- **The connector's forces are the limit, and they are ugly:** it steers with unbanked lateral force, 0.6 to 1.0 g for a 1 m edit when it works alone (0.1 to 0.5 g after the contract), 1.4 to 1.8 g for 2 m. Real track would bank instead. Either the connector carries a roll function, or the solve becomes a small optimisation that keeps the forces low and smooth (R5's fallback; R5's "solved values that are technically right but ugly" happened).
- **The contract and the pin compose, as R6 2.4 said:** re-solving the elements first leaves the slack only a slide along the track, and the slack then absorbs edits two to eight times larger.
- **Slack (a) is weaker than R5 hoped:** it cannot change height, pitch or bank (the 1.1 m height miss in L1 is the turnaround's, whose exit height is not held), and a straight plus 30° of curve cannot move the end sideways, so it fails whenever the miss is not along the track.
- **A large edit without the contract is not absorbable at all:** making the camelback's airtime 0.2 g stronger without re-solving it moved the end of the track by 150 to 385 m.

## Timing: what one evaluation costs on a long ride

`timing.py`: a camelback followed by level sections, up to 152,768 nodes; one evaluation = one command, a whole-track recompute, one `sectionEnd`. Mean of 20; run alone.

| nodes | edit in | command (ms) | recompute (ms) | evaluation, wall (ms) | one `sectionEnd` (ms) | one `measures` (ms) |
|---|---|---|---|---|---|---|
| 7,768 | first section | 3.5 | 3.8 | 7.9 | 0.39 | 0.20 |
| 17,768 | first section | 7.9 | 8.3 | 17.3 | 0.67 | 0.43 |
| 52,768 | first section | 23.5 | 24.0 | 50.3 | 1.99 | 1.20 |
| 152,768 | first section | 141.4 | 136.2 | 290.7 | 11.9 | 6.3 |
| 152,768 | last section | 3.3 | 94.3 | 108.3 | 8.3 | 4.5 |

(`results/timing.txt`, the run made by the README's command. Of three runs, two gave about 290 ms for an edit at the start of the 152,768-node ride and one 156 ms, with 71 ms each in the command and the recompute; the smaller rides agreed within 10 %.) **An evaluation costs about 1 ms per 1,000 nodes up to 50,000 nodes, and 1 to 2 ms per 1,000 nodes at 150,000**, half in the command (its recompute from the edited point and the undo step's snapshots of the whole track) and half in the whole-track recompute that issue 63 makes necessary today. Each `sectionEnd` or `measures` request also reads the whole track: about 0.02 to 0.08 ms per 1,000 nodes.

So on a 150,000-node ride: a 10-evaluation solve with one command per evaluation takes 1.6 to 3 s plus its reads (A1's two commands per evaluation: 3 to 6 s); a camelback re-solve from the previous solution (5 to 10 evaluations) 0.8 to 3 s; a scan for a start (338 to 924 evaluations) 1 to 4.5 minutes; a pin re-close with contract (25 to 35 evaluations) 4 to 10 s. On a 14,000-node ride the same are 0.15 s, 0.1 s, 5 to 13 s and 0.5 s. These are the program's own costs through today's command list; a solver inside the program can be cheaper (next section).

## What changes in the plan

**R6 section 4.2, line by line.**

| Experiment | "Changes the design if" | Fired? |
|---|---|---|
| E1 | the scan-started solve fails in cells that have a solution | **No.** 12 of 12. |
| E1 | the scan takes more than about a second | **Yes.** 338 to 924 evaluations: 4 to 16 s of program time even for an element alone. A cheaper first guess is needed (R6's closed form, or a scan with far fewer points). |
| E1 | a solve takes more than 0.2 s | **Yes**, on a whole ride: 0.4 to 0.5 s median for a section aim on a 14,000-node project (nearly all of it the program's own time), 3 to 6 s for the same two-unknown solve on a 150,000-node one. Re-solve is a button. |
| E1 | two solutions appear in normal use | **No.** Never more than one. |
| E2 | re-solving does not cut the downstream change by ten times for reshaping edits | **No** for pose and speed (13 to 120 times); position along the track only 4 times, as R6 expected. |
| E2 | the owner prefers "keeps exit speed" | Owner's question; the table above is what he would compare. |
| E3 | the measured onset differs from k·ΔG/T by more than 10 % | **No.** Within 2 %. |
| E4 | the overbank needs more than about five unknowns, or fails often near the crest | Five unknowns: **no** (three). Near vertical: **yes**: all four variants at 20 m/s failed, each with a section end pitched 39° to 89° down. |
| E5, E6 | the file record; per-seat airtime | Not part of this unit (E5 is the saved-file check, E6 needs the train model). |

**Live re-solve or a button.** A button. Only short rides solve in under 0.2 s; a 150,000-node ride takes seconds per element and minutes when a start must be found. This is also R5's and R6's recommendation and the roadmap's recommendation to the owner.

**G1 and G2 stand.** The engine works as planned on the real model. The solver's rules, from these trials:
1. Damped Newton, forward-difference Jacobian, backtracking, bounded unknowns, "not a number or a broken guard is worse": as planned, unchanged.
2. **Goals in unwrapped angles need the model's help.** Section ends cannot see a full loop between them, and the `pitchRate` measure cannot unwrap it. The solver inside the program should integrate the turned angle from the nodes' directions (cheap in C++) or carry an unwrapped pitch and heading per node.
3. **Tolerances follow the node step:** 3 cm (about one node at 25 m/s) and 0.05°; tighter only if another unknown can absorb the last node.
4. **Continuation first, scan second, and make the scan cheap:** from the previous solution, re-solves took 5 to 38 evaluations; the scan 338 to 924. Use R6's closed form for the first guess, or scan only the unknown that matters with a coarse grid.
5. **Do not pay for undo and the whole track on every trial evaluation.** Half of each evaluation is the command's undo snapshots, the other half the whole-track recompute. A solver inside the program can run its trial evaluations inside one open step (rolled back at the end, one undo step for the result) and recompute only from the edited section to the last section a goal reads, with a single whole-track recompute when it accepts (decision record 0013 still holds for what the designer sees). That could cut a solve on a long ride by several times; it was not measured here, because the command list has no such operation.
6. **Report the closest result** when a solve stalls (A1c: 7 of 20 out-of-reach goals, each with its closest miss), and the reachable range when a scan was run.
7. **Elements state their orientation and refuse a banked entry** (D1: 10° of bank turns a zero-g roll by 24° to 38°), and an overbank states its bank as a goal (D2).

**Where a simple solve is not enough.**
- **The overbank near vertical** needs a start search and probably multiple shooting (aim the crest, then the exit) before it is offered.
- **The connector of G2** needs a roll unknown for the bank goal and, because it otherwise chooses unbanked lateral force, a smoothness or least-force preference: a small optimisation, not a plain solve. A straight-plus-curve slack is only worth offering for misses along the track.
- **Energy edits** (lift, launch) are not absorbed by element contracts; only a pin restores position after them, and the slack must then take the speed change too.

**What did not change.** Elements as data-driven groups of ordinary sections (R6 4.1), the camelback first with three goals and three unknowns, and the build order G1 → G2 → E3.

## Program behaviour found

Recorded here and in the report; none blocked a trial once it was worked around inside the trial scripts.

1. **`tools/fvdsession.py` hangs a long session.** The program writes `lenAssert` messages to its standard error, for example `Assertion 'diff==diff' in ../../core/secforced.cpp line 76 failed.` on some runs where the train stalls before a force section. `fvdsession.py` opens that pipe but never reads it; after some thousands of such runs the pipe is full, the program blocks writing to it, and both sides wait for ever (it happened twice in Trial A's map, at the 15 m/s cells). Reproduce one line: open `projects/a-camelback.fvd` (entry 30 m/s) and send `{"op":"run","name":"anchor.setSpeed","args":{"speed":25}}`; the train then stalls in section 2 (`sectionEnd` 3 says `non_finite`) and the line above appears on standard error. The trials drain the pipe with a thread (`e1solver.Model`). `cli/README.md` does not say that the session writes to standard error.
2. **`anchor.setPositionY` and the project text mean two different points (a gap in the documents, not a defect).** On `projects/b-baseline.fvd`, whose text has the anchor at `"position": [0, 46, 0]`, `{"op":"run","name":"anchor.setPositionY","args":{"y":46}}` raises the whole track by 1.1 m (the heartline height); `y` = 44.9 leaves it in place. The command's description says it sets "the height of the heartline point at the start, as the anchor's Y box shows it". Both are right about themselves: the project text writes the track's stored start point (`startPos`, `core/projecttext.cpp`), and the anchor's Y box and the command take the heartline point, which on a level, unbanked start lies the heartline height above it (`track::anchorStartPos`, `core/commands_anchor.cpp`). What is missing is a line in `docs/project-text-format.md` saying that `position` is not the heartline point the box shows; a solver that reads a height from the text and sends it to the command must add the heartline height. The trials move the start by text height − 1.1 + change and check the move on node 0 (`layouts.raise_start`).
3. **The `pitchRate` measure is the rate of the wrapped pitch.** It reverses at the vertical while the track keeps turning the same way, so it cannot be summed into a turned angle. Not wrong for a plot, but nothing in the model gives a solver an unwrapped pitch or heading (rule 2 above). Shown by the shell commands `fvdcore nodes samples/baseline/n1-tension-loop.fvd --every-s 0.25` (the `pitch` column) and `fvdcore measures samples/baseline/n1-tension-loop.fvd --ids pitchRate --every-s 0.25`: from 5.00 s to 5.85 s the pitch reads −69.2°, +77.7°, −86.2°, +85.3°, +87.2° while `pitchRate` reads −3634, +3634, −3634, −3634, +3634 °/s, so the sign of the rate is not the direction the track is turning at those samples. (In the trials the same effect is what the loop guard counts.)
4. Not defects, but traps for G1: a force section's length is that of its shortest unlocked function, so changing one function's transition lengths does not lengthen the section unless the others are locked or changed too; `transition.setLength` refuses 0 (its minimum is 0.001 s), so a bound of 0 on a duration must be a small positive number; near a stall the program reports forces of thousands of g (the camelback at 15 m/s with +0.2 g airtime, t₂ 1.15 s and t_air 0.5 s reports 25,010 g in its `summary` findings), which a goal on a force must treat as invalid.

## The owner's questions, with what the trials now say

From R6 section 4.4:
1. **Which numbers define the camelback.** The recommended set (airtime force, valley force, crest height, onset limit given; climb, drop and crest durations solved for exit level at the entry height) works on the real model: three goals, three unknowns, re-solves in 5 to 38 evaluations from the previous solution. With an onset limit as an input, the valley force becomes the unknown and the airtime shrinks first (Trial C).
2. **Do elements hold position?** They cannot: even a reshaping edit slides everything after a re-solved element 1.5 m along the track, an energy edit 4 to 34 m. Holding position is the pin's job. **Exit height or exit speed:** for reshaping edits it hardly matters (the end is within 0.7 m and 0.7° either way); for a lift change "keep height" lands the end 9 to 11 m away at the right height, "keep speed" 4 to 5 m away but 0.8 to 1.1 m high or low. The trials cannot say which the owner prefers.
3. **The file format.** Not tested here (E5).
4. **Which elements first.** The trials support camelback, then zero-g roll (31 of 32 solved, the roll goal costs almost nothing), then the overbank (11 of 16, fails near vertical; needs more work).
5. **Live or a button.** A button (timing above).

From R5:
1. **Ranking.** Pins with a slack piece are viable: the connector re-closes to under 2 cm and 0.002° in about 15 evaluations. Goal-seeking first, as the engine, stands.
2. **Button or live; which values the program may change.** A button. The slack that worked is a connector the program shapes freely, and its forces are where the cost shows: the owner should say how much lateral force and bank change he would accept in a connector.
3. **When the pin cannot be met within limits.** The solver always ends with a closest result (a stall keeps it), so "give the closest result and mark the problem" costs nothing extra; "leave the old track" is one undo.
4. **Keep shape or keep forces.** Not tried.
5. **Old projects.** Not affected: nothing here changes what a saved file computes.
6. **A test case of the owner's.** Still needed: every layout here is synthetic.

## Could not establish

- **The owner's own layouts.** R5's re-close trial asked for one of his projects; none was available to this unit, so two synthetic layouts were used, and neither closes a real circuit (they end 92 to 106 m and 280 to 295 m from their start).
- **Whether the overbank has a solution at 20 m/s.** It failed from a fixed start and from continuation; no scan was built for it.
- **How fast a solver inside the program can be.** The per-evaluation cost was measured through today's commands (undo snapshots and a whole-track recompute each time); an in-program solver that skips both was not built.
- **Inversions in general.** Only the zero-g roll (a roll inversion at the crest) and the overbank were tried; loops, dive loops and goals placed inside an inversion were not.
- **Speed hills at 15 m/s.** The real program found small valid 4 to 10 m camelbacks at 15 m/s that the 2D stand-in did not; whether they are rideable was not checked.
- **E5 (the element record in the saved file) and E6 (per-seat airtime)** are outside this unit.
