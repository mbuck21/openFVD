# PAUSE NOTES: T1-U11 (every command ends in the reopened state)

Paused 2026-11-11 for a restart. Delete this file before the pull request.
Scratch files of the paused session (may be gone after the restart): `C:\Users\matth\AppData\Local\Temp\claude\41a1b27d6ab21bba\b27050bd-d115-495c-b1ce-aaba013c2543\scratchpad\u11\` (`keep\` has the logs, `cmdtest.sh` builds and runs only tests/commands, `fixture_evidence.py`, `fvd_fields.py`).

## Finished (built and tested, except where said)

- **Test** (tests/commands/main.cpp): `checkRunningEqualsReopened(projects, full)` (L against S: node count, exports, end position and speed exact; L against R: saved bytes and exports exact; saved bytes L against S bounded, now by 0), `checkDialogsReopened()` (smoothing dialog OK and Track Properties OK), `timeCommands(repo, longTrack)` (gen-normal-shapes, and a 150,124-node track in `--reopen` mode). `commands <repo> --reopen` runs only these with the full pattern; the default run uses section.add at all places and the others at the first, every third and the last place (`full == false`, added last, NOT yet run). Places where the model's lenAssert fires are left out and listed (only `anchor.setSpeed` halved on n1-tension-force-chain).
- **command.cpp `runSpec`**: after a successful apply, unless `spec.settlesItself`: `track::settleTransitions()` then `track::updateFromAnchor()`; then always `updateSmoothEntries()`. `settlesItself` (new field, command.h) is set for the anchor commands (`anchorSpec`), `track.setHeartline`, `smoothing.setEntry/addRegion/removeRegion` (their apply-true path `recomputeSmoothed()` now calls `updateFromAnchor()`; apply false leaves the track alone for the dialog).
- **track::settleTransitions()** (track.h/.cpp): replays the relink sums `func::loadFunction` makes (issue 63 item 7), so L's transition ends equal a reopen's. This fixed transition.add "before" on gen-smooth-whole (5 mm, L == R != S).
- **track.setHeartline**: startPos through `track::anchorStartPos()` (static_asserts added in commands_track.cpp that cos/sin of a float are the double ones, so bits are kept), then `settleAnchor()` + `updateFromAnchor()`. Fixed up to 3.2 m.
- **ui/batchrunner.cpp**: a single-edit undo or redo that does not restore is a failure; **tools/run-baseline.py** passes `--each`.
- **Fixtures re-recorded**: gen-geometric-distance, gen-normal-shapes, gen-two-tracks (evidence below), README paragraph added.
- Docs: decision record 0013 "Where enforced", core/CLAUDE.md, docs/testing.md (new section "Running equals reopened", per-edit gate).

## Half done

- **core/trackstate.cpp `restoreTrackState`** (last change, compiled by tests3 but its test run was killed): it now first runs `updateFromAnchor()`, keeps that if the first node comes out equal to the captured one, else puts the captured first node back, resets `smoothedUntil` and runs `updateTrack(0, 0)` as before. Reason: with the whole-track recompute, L's first node is "anchor turned by smoothing"; the old restore put that node back and `removeSmooth()` un-turned it inexactly, so R differed from L by 74 to 98 export bytes on gen-smooth-whole (setChange, setSpeedFixed; 1 case with undo-to-start between places, 7 once places accumulate). An earlier attempt (always `updateFromAnchor()`) broke tests/trackstate (stale first node on purpose, and imports whose first node is not the anchor); a second attempt (simulate the smoothing turn on the anchor and compare) did not match. **Next: run `tools\run-tests.cmd`; tests/trackstate (788 checks) and tests/commands must both PASS.**
- Then: rebuild (`tools\build-windows.cmd`), `py -3 tools/run-baseline.py --undo-check --fixtures --imports` (last run before the trackstate change: all baseline MATCH, UNDO OK on all 8 with `--each`, fixtures failed only on the three files since re-recorded, imports OK), `--core`, `--core --imports`, the 8 single-project `--batch-undo-check --all-edits --each` runs.
- The "before" table must be rerun with the final test on the original code: an extracted original tree is at scratch `before-tree\` (git archive of origin/claude/stage-2 with the new tests/commands/main.cpp copied in; `cmdtest-before.sh --reopen`). Its run was killed at the pause.
- Test run time: full pattern 384 to 403 s for ~1105 runs (commands 26 s, save and reopen 108 to 130 s, undo and redo 133 to 158 s, snapshots 29 to 45 s). The default (every third place) is untested; aim: a few minutes.
- Pull request not opened.

## Measurements so far

### Before (original code; earlier version of the test, with anchor second values; 1239 runs, 334 s)

Rows with a difference (all other command/project rows: 0 of N differ):

| command | project | cases | L!=S | L!=R | end mm | end m/s | export bytes | nodes / saved bytes |
|---|---|---|---|---|---|---|---|---|
| section.add | gen-curves-speed | 16 | 4 | 4 | 15.19 | 0.00053 | 30358 | 1 / 171 |
| section.add | gen-smooth-whole | 16 | 8 | 8 | 12340 | 3.455 | 25850 | 951 / 168 |
| transition.setChange | gen-smooth-whole | 7 | 1 | 2 | 0 | 0 | 3 | 0 / 10 |
| transition.add | gen-smooth-whole | 7 | 2 | 1 | 6.1 | 0.00025 | 4620 | 0 / 35 |
| track.setHeartline | gen-curves-speed | 2 | 2 | 0 | 169 | 0.0054 | 23142 | 120 / 31 |
| track.setHeartline | gen-normal-shapes | 2 | 2 | 0 | 1902 | 0.014 | 40899 | 8 / 9 |
| track.setHeartline | gen-smooth-whole | 2 | 2 | 0 | 819 | 0.169 | 25750 | 32 / 20 |
| track.setHeartline | gen-two-tracks (track 2) | 4 | 2 | 0 | 740 | 0.377 | 10305 | 86 / 0 |
| track.setHeartline | n1-tension-force-chain | 2 | 2 | 0 | 3239 | 0.022 | 72471 | 8 / 21 |

Totals: 25 of 1239 differ from the reopen, 15 from undo-redo; saved bytes L-S up to 175 (smoothing entry names after a rename, ranges, transition ends). Dialogs before: the smoothing dialog's OK on gen-smooth-whole 12 export bytes off (L == R); Track Properties' OK on gen-smooth-whole 343 mm, 0.069 m/s, 12 nodes. Section.add details: gen-smooth-whole straight after 1: 12.3 m, 3.45 m/s, 951 nodes; straight after 3: 6.2 m; curve after 1: 1.0 m; curve after 3: 2.2 m; force after 1: 1.2 m; gen-curves-speed force after 1: 15.2 mm.

Undo check `--each` before (old build): gen-smooth-whole 13 single edits not restoring (straight length/speed tick, curve radius/angle/direction/lead-in/lead-out/speed tick/orientation, straight roll change, add straight, add geometric, delete section), gen-geometric-distance 2 (transition append after normal 1, lock last), all others 0.

### After (before the restore change; 1105 runs, full pattern)

0 of 1105 differ from the reopen; 0 differ from undo-redo with undo-to-start between places (1 case, gen-smooth-whole setChange normal 1, fixed only by the restore change, with accumulation 7); saved bytes L-S: 0. Both dialogs exact. Undo check with `--each`: OK on all 8 projects, no single-edit difference.

### Cost (smallest of 5 runs)

| | setChange, last force section | setChange, first force section | section.add force in the middle |
|---|---|---|---|
| gen-normal-shapes (14,124 nodes), before | 10.5 ms | 12.5 ms | 8.8 ms |
| gen-normal-shapes, after | 23 to 30 ms | 25 to 36 ms | 22 to 35 ms |
| long track (187 sections, 150,124 nodes), before | 1.0 ms | 78 ms | 34 ms |
| long track, after | 77 to 97 ms | 158 to 194 ms | 112 to 139 ms |

The early typed value on the long track is above 150 ms: per the brief, no optimisation; report it and propose decision record 0013 point 4 (a run of typed changes to one value, the history's merge run, recomputes from the edited point while it lasts and settles when it ends; undo, redo, save, export and the solver always see the settled track). The windows also rebuild the whole mesh after each command (trackGeometryChanged from node 0), not measured.

### Fixture evidence (gen-geometric-distance, gen-normal-shapes, gen-two-tracks)

`fvdcore export` of old and new: re-saves and all exports byte-identical; all MATCH the `.2016.*` references; each new file equals its own re-save AND is byte for byte the committed `.2016.resaved.fvd`. Differing values (fvd_fields.py): every smoothing entry's name (empty -> track/section name; +55, +55, +67 bytes) and the whole-track/section entries' node ranges; in gen-geometric-distance also 11 transition-piece start values in 8 transitions. Header and inputs identical. The lead must open the three files in the 2016 binary before the merge.

## Outside the unit

- tests/trackstate deliberately checks that a restore brings back a stale first node (four-sections "anchor speed" edit made without updateFromAnchor); kept passing by the fallback.
- A restore is 3 to 4 times a whole recompute (90 ms against 25 ms on gen-normal-shapes); most of the test time.
- `transition.add`'s example target still avoids geometric sections on distance (comment in commands_transition.cpp `addTarget`); no longer needed.
