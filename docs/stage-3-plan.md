# Stage 3 build plan: the train on the finished track

Written 2026-11-13 by the planning session. **Nothing of Stage 3 is built.** This is the plan a fresh lead session builds from: the pieces, their order, what each must do and how it is proved. The reasons are in decision record 0014 (`docs/decisions/0014-stage-3-the-train-on-the-finished-track.md`, provisional until the owner answers its choices); the place of the stage among the others is in `docs/roadmap.md`.

Read first: `CLAUDE.md`, decision record 0014, then the research the stage stands on: R7 (`docs/research/r7-train-against-point.md`, with its prototype), R3 and R13 (force limits), R12 (NoLimits 2), R8 section 1 (the energy rule).

## 1. What the stage delivers, and what it must not touch

The owner's priorities of 2026-11-13 (his words are in `docs/feature-requests.md` under FR-4, FR-11 and FR-19):

1. A whole-train simulation with friction as in NoLimits 2.
2. The number of cars, and which car the design is for.
3. The ASTM force check for every seat, readable at a glance.
4. English units by default.

He asked that every part be optional and that his sketch of the screens not be taken literally.

**The rule of the stage: a project without a train is today's program.** The train is computed beside the design and writes nothing into it. Every piece ends with the baseline comparison against the 2016 binary unchanged, with no tolerance, the owner's private project included (the lead runs that part; workers cannot see `samples/private/`).

**Not in this stage** (Stage 4): the design itself following a car, real train dimensions, the comparison against NoLimits 2, the train drawn in 3D, the energy margin.

## 2. The owner's choices, and which piece waits for which

Decision record 0014 ends with eight choices, each with a recommendation. Build on the recommendation where the table says so; the answer changes a default or a label, not the structure.

| Choice | Recommendation | Gates |
|---|---|---|
| 1. Stage 3 shows the train; designing for a car is Stage 4 | yes | what happens after X1; nothing in Stage 3 |
| 2. The seat check as proposed (seat map of the whole ride, cursor mode, colour by test, ride strip, track coloured by the worst seat; 3D train in Stage 4) | yes | V2. Do not build V2 before the answer |
| 3. No train by default | yes | V1 (a default; build on the recommendation) |
| 4. The train's centre as the default reference | yes | D5 (a default; build on the recommendation) |
| 5. Buy ASTM F2291-26b and type the uncertain numbers into the limit file | yes | nothing; the limit file is data and can be corrected any time |
| 6. Train settings in an appendix the 2016 program ignores; rule 4 reworded | yes | merging D5. F4 can be built and merged before the answer: it saves nothing by itself |
| 7. A session in NoLimits 2 | when convenient | Stage 4 (N3). N3a prepares it |
| 8. The order after the train | the old Stage 3 remainder | nothing in Stage 3 |

## 3. What is already known about the code

Found while planning; each saves the builder a search.

- **Units** are one program option, `optionsMenu::measures` (`ui/optionsmenu.h`: `metricMPS` 0, `metricKPH` 1, `english` 2, which is feet and mph), chosen in Edit > Options and stored as the line `Measures <n>` in `options.cfg` beside the program. The default is set in the `optionsMenu` constructor when the file is missing or unreadable, and the file is then written. Nothing about units is in a project; the model is metric. 67 places read the unit factors (`getLengthFactor()`, `getSpeedFactor()` and the two strings).
- **Batch mode reads the same option.** `batchRunner::runMakeFixtures()` notes "the values were converted" when it is not metric, and the scripted scenarios convert typed values with the option's factors (`ui/batchscenarios.cpp`). A conversion to feet and back can move the last digit of a float, so with a non-metric option the baseline's generated projects may no longer match the 2016 references.
- **The project reader stops at `EOP`** (`readProject()` in `core/projectfile.cpp`: the loop reads three-letter tags and breaks on `EOP`). That code came from the original program unchanged, so the 2016 binary should ignore anything after `EOP`. This is read from the code, **not yet tried on the 2016 binary**.
- **The energy rule** is in R8 section 1.1; R7 reproduced the program's speed to 0.0004 m/s with it: the energy point is 0.9 of the way from the heartline toward the rail, friction is charged on the centre line's length, the kinetic term uses the centre-line speed, and the felt forces use the heartline speed. `docs/research/r7-prototype/ride.py` is a working reference in Python, and `results.txt` there holds numbers to test against.
- **A section with a held speed** is recognised by `measureSpeedIsHeld()` (`core/measure.h`). Bezier and NoLimits-CSV sections do not follow the energy rule (`core/secnlcsv.cpp` holds one speed along the whole section); the ride's "reproduces the design" test must name and exclude them narrowly.
- **Node positions are floats.** A second difference of positions over one millisecond is far too noisy to give a force (the along-track measure already works over a span of nodes, `MEASURE_ALONG_HALFSPAN`). Per-seat forces must come from the frame's turning with smoothing, or from the standard's 5 Hz filter, not from raw differences.
- **Measures are fixed functions without parameters** (`measureDef::fill` in `core/measure.h`). A train measure depends on the train and the watched car, so it needs either a provider beside the registry or a parameter; the plot and "colour by measure" read the registry (`buildMeasureMenu()`, `trackMesh::updateMeasureValues()`).
- **Every registered check shows on every project:** the ride sheet and `fvdcore check` run `checkRegistry::runAll()`. Registering the force limits as a check makes findings appear for existing projects (display only), and `tests/ridesummary` has known finding counts.
- **A cursor along the ride exists:** `glViewWidget::povPos` is a node index the 3D view rides with the arrow keys, and the plot draws it (`graphHandler` type `povPos`, `graphWidget::drawGraph(10)`).
- **The ride sheet's pattern for "a moment after the edit":** `trackHandler::trackGeometryChanged()` calls `MainWindow::rideSheetChanged()`, which restarts a 150 ms timer; the panel computes when it fires (`ui/ridesheet.*`). The train results follow the same pattern.
- **The train catalogue** (`data/traintypes.json`, `core/traintype.h`) has `seatsAcross`, `rowsPerCar`, `carsPerTrain`, `carLength` (mostly null) and `restraint`; Track Properties' "Train type" only fills the heartline box and the pick is not saved.
- **Undo:** a new model value must be saved, kept in `trackState`, or derived (`core/CLAUDE.md`); every edit is a named command, and `tests/commands` runs every registered command on every baseline project.
- **Whether the vendored QCustomPlot 1.2.1 has a colour-map plot** (`QCPColorMap`) was not checked; the ride strip may use it or be drawn with a plain `QWidget` paint. The library is never edited.

## 4. The pieces

Order: **wave 1** U1, P1, A3a, F4, N3a (independent; they share only one-line additions to `core/core.pri`, `resources.qrc` and the command table of `cli/`). **Wave 2** D5 (after F4 and P1) and A3b (after A3a). **Wave 3** A4 (after P1, A3a, D5). **Wave 4** V1 and V2 (after A4; V2 after the owner's choice 2). **X1** any time after P1. Then the closing.

Models as `docs/development-workflow.md` says: Opus for the physics, the limits and the file format; Sonnet otherwise.

### U1: English units by default (Sonnet)

- The fresh-install default becomes `english`. Nothing else about the options file changes; an existing file keeps its value.
- Batch mode runs in metric whatever `options.cfg` says: set the option in memory right after the hidden window is made (`createHiddenWindow()` in `ui/batchrunner.cpp`) and never write it back. Check every caller of `saveToOptionsFile()`.
- Look at what a first-time user sees (ride sheet, info panel, boxes, plot labels) and list anything that ignores the option; fix nothing that is not about the default.
- **Proof:** with no `options.cfg`, the full baseline passes and the file written afterwards says `Measures 2`; with a file saying `Measures 1`, the baseline passes and the file still says `Measures 1`; `tools\run-tests.cmd` passes.
- For the owner: his existing copies keep their setting; a clean build makes a new `build/dist` and so starts in feet and mph.

### P1: the ride (Opus)

New files `core/trainspec.*`, `core/trainride.*`, `cli/cmd_ride.cpp`, `tests/trainride/`. No existing computation is touched. Double precision.

- **`trainSpec`**, a plain value: catalogue id; cars; car pitch (centre to centre along the centre line); row offsets along the car; seat offsets across it (positive toward `vLat`, the rider's right); the offset from the heartline to where a rider's force is taken (0 by default); mass fields (car, rider, load, optional per-car factors; no box on screen, decision 0014 point 5); where a car's mass sits (default: the design's own energy point); the resistance (below); the reference car (-1 = the train's centre, else a car, 0 = front); the restraint kind. `trainSpec::point()` is the program's own point. `trainSpec::fromType()` fills from a catalogue entry and takes stand-ins for what it lacks (car pitch 2.5 m, rows 0.9 m apart, seats 0.55 m apart; all in one commented place), returning the list of stand-in fields so the screens can say "assumed".
- **Resistance, four terms summed:** a share of the train's weight (the meaning of `fFriction`), a share of the force pressing each car on the track (0 by default), a term times speed (0 by default), a term times speed squared (the meaning of `fResistance`). Default: the project's own two numbers. R12: NoLimits 2 uses the same two.
- **The motion:** rigid coupling, one speed, energy over all cars at their own heights (R7 section 2.2). Before the first node and after the last, the track continues straight.
- **Held-speed sections:** while every car is on held-speed track the train moves at that speed exactly; while it straddles held and free track it is held at the held speed only while free running would make it slower, and runs free once free running makes it faster (R7's chain rule). For one car this equals the design. A stand-in until lifts and brakes are section kinds (P3); document the ambiguous cases met.
- **A stall** ends the ride and is reported with its place; no not-a-number leaves the function.
- **What each car and each seat feels:** specific force in g, gravity included, as `gz` (the program's normal force), `gy` (sign of `forceLateral`), `gx` (the existing `forceAlong`); axes in R3 section 1, the seat formula in R3 section 7.
- **The result:** samples at 0.01 s (time, reference position, speed; per car and per seat the three components), and helpers that give one value per node: **what a car or seat feels as it passes the place of that node**. That mapping, by place and not by time, is what the plot and the 3D track use, so a crest's value stays at the crest. A per-car summary (lowest and highest of each component, the ride sheet's airtime bands).
- **`fvdcore ride`**: per-car summary, `--table` for the time series, `--json`.
- **Proof:** (1) `trainSpec::point()` reproduces the design's speed at every node of every baseline project within 0.002 m/s and its forces within a stated tolerance away from section joins; (2) R7's tables on `docs/research/r7-prototype/projects/` with R7's three trains, within 0.02 g, 0.02 m/s and 0.05 s; (3) the closed form of R7 section 6 to four decimals; (4) a seat `r` from the heartline on a straight rolling at a constant rate feels ω²r, and per-seat noise on a smooth track is under 0.01 g; (5) a ride leaves the track's saved bytes unchanged. Timing: well under half a second for a 100,000-node track with ten cars of 2 by 4 seats.

### A3a: the force-limit engine (Opus)

New files `core/forcelimits.*`, `data/limits/astm-f2291-public.json` (embedded through `resources.qrc` like the train catalogue), `cli/cmd_limits.cpp`, `tests/forcelimits/`. It registers no check and touches no window.

- **The data file** follows R3 section 6: every curve and rule number with its source and confidence; the edition described; the filter; the 0.2 s minimum; the slice step; the combination rule with a **radius factor of 1.0** and a note that the 2006 edition has 1.1 (R13); the reversal rule; the weightless-to-positive rule; the restraint areas as polygons; the children's multiplier as informative. **No limit number in the C++.** No clause numbers outside a `source` text with its edition (editions number clauses differently, R13). The loader takes a path so the owner can use a file with numbers from his own copy.
- **The engine** takes one rider's trace (`dt`, `gx`, `gy`, `gz`) and options (restraint kind, which selects the eyes-front line and the egg radius: base case, over-the-shoulder, prone; extended airtime line allowed or not). Steps, each its own function: the 5 Hz four-pole single-pass Butterworth filter (places reported with the filter's delay taken out); duration by slicing at 0.1 g levels (R3 section 3); the three two-axis checks; the reversal rule; the two airtime-into-positive rules; the restraint area from the 1 Hz-filtered trace, as information.
- **The share of the limit** ("utilisation"): per sample, how close that moment is to its nearest limit, 1.0 being at the limit, with the rule responsible; one array per rule family and the overall maximum. This is what every screen colours.
- **Findings:** rule, axis, stretch, level, limit, duration, share, the confidence of the numbers used, and a sentence. One finding per event, not one per slice level.
- **Proof:** synthetic traces with known answers: rest for 90 s; plateaus on both sides of each break of each curve; R3's "2 s valley peaking at 4.5 g" example; airtime cases with and without the extended line; the eyes-front line by restraint kind; a 0.15 s event giving no duration finding; an egg case, including a point between the 1.0 and 1.1 ellipses with both factors; a reversal; the push-pull case against the same pull-out without airtime; the weightless-to-2 g onset; the restraint area; the filter's -3 dB point; exactly-at-limit giving a share of 1.0; three broken data files refused with the place named. One 15,000-sample trace in well under 2 ms.

### F4: one way to add to the saved file (Opus)

- **The format.** After the project exactly as today and its `EOP`: the three bytes `FVX`, an integer appendix version (1), an integer length, that many bytes of one compact UTF-8 JSON document, and the three bytes `EOX`. The document is `{"needs": [], "project": {}, "tracks": [{}, {}]}`: one object per track in file order; **each feature owns one key** (the train is `"train"`). A later addition is a new key or a new field, never a new binary layout.
- **`needs`** names additions a reader must understand to compute the tracks correctly; an unknown name refuses the load with a plain message. Empty in Stage 3.
- **Unknown keys are kept** with their track or project, through undo and through deleting other tracks, and written back.
- **Nothing to add means no appendix:** a project without additions is written byte for byte as today. The baseline's re-saved projects depend on this.
- **A damaged appendix does not lose the project:** the tracks load, the additions are dropped, the load's message says so.
- The main version stays `v0.77`. The first addition that changes a computed track (Stage 4's design-for-a-car) bumps it.
- A track's additions are part of its state (`core/trackstate.*`), and the text form (`core/projecttext.*`, `docs/project-text-format.md`) carries them with an exact round trip. The JSON written must be deterministic.
- `fvdcore additions <project> --set key=<json> | --remove key | --show`, so a test file can be made by hand.
- **Proof:** no `FVX` in any re-saved baseline project; save, load, save gives the same bytes; each kind of damage handled; unknown keys survive a track deletion; undo and redo of an addition; the text round trips; the bytes up to `EOP` equal the same project without additions. **And on the 2016 binary itself, by the lead:** a project with an appendix opened in a copy of `bin/win64/FVD.exe` outside the repository opens and exports the same files as without it. Until that is seen, the claim that the 2016 program ignores the appendix is read from the code only.

### N3a: the kit for the owner's NoLimits 2 session (Sonnet)

R12 section 7 defines six experiments (E0 to E5) that settle what NoLimits 2 does not publish. The kit makes the session short: `samples/nl2-session/` with a project and its `.nl2elem` export per experiment (a level 800 m straight; a level run with a 1.5 g and a 4 g valley and one without; R7's camelback and lift crest), their predicted numbers, and a script that regenerates them; `tools/nl2log.py`, a recorder for NoLimits 2's telemetry (protocol on its help page, R12 section 5.2; standard library only; a self-test against a fake server, since it cannot be tried against NoLimits 2 without the owner's PC session); `tools/nl2fit.py`, which fits friction and drag from a recorded coast-down; and a one-page procedure with numbered steps, every step not confirmed in NoLimits 2's help marked "check on screen". Then the `owner` issue with those exact steps. Recordings go under `samples/private/`.

### D5: the train in the project (Opus)

- The track holds a `trainSpec` and whether a train is simulated at all (off by default; choice 3). It is saved under the key `"train"` of the track's additions (F4) and is part of the track's state.
- Named commands for every change (`train.setType`, `train.setCars`, `train.setCarPitch`, the rows, the seats, the reference, the restraint, the resistance, `train.clear`), with units, ranges and descriptions; `tests/commands` then runs them on every baseline project, including "running equals reopened".
- The catalogue's schema grows (a new `schemaVersion`): car pitch, row offsets, seat offsets, friction and drag per type, each a sourced value, null until Stage 4. The loader keeps reading the old file shape. `tests/traintypes` extended.
- **Proof:** a project with a train saved and reopened gives the same train and the same exports as without one; the file up to `EOP` is unchanged by the train; undo and redo of every train command; the baseline unchanged.

### A3b: the force limits at the design point (Sonnet)

A check `forceLimits` in the check registry: the design point's three forces (the measures `forceNormal` and `forceLateral` with smoothing, and `forceAlong`) resampled to a uniform trace with the ride summary's notion of time, run through the engine, its findings mapped back to node stretches. One line on the ride sheet ("Force limits (ASTM F2291, public numbers): within / n findings") and the sentence that this is a design aid from secondary sources. It works on any project, with no train. Decide with the lead how `tests/ridesummary`'s known finding counts change, and say in the pull request that existing projects now show these findings (display only).

### A4: every seat checked (Opus)

- A per-track cache of the ride and of the per-seat limit results, recomputed a moment after an edit or a change of the train (the ride sheet's timer pattern), never inside `updateTrack()`.
- Train measures, each one value per node by place: the watched car's three forces; the lowest and highest any car feels there (the band); the worst seat's share of a limit there, and which seat. Extend the measure mechanism for measures that depend on the train (section 3 above) without disturbing the existing twelve.
- Train findings: per seat, with car, row and seat in the sentence and the place on the track.
- A summary for the ride sheet and the seat map: per seat the worst share over the whole ride with its rule and place; per car the airtime seconds and the extremes.
- `fvdcore ride --check`.
- **Proof:** a one-car train with one seat on the heartline gives A3b's findings; R7's camelback with the 9-car train shows the end cars' stronger airtime in the measures; timing with ten cars of eight seats on the longest baseline project.

### V1: the train where the designer already looks (Sonnet)

- **Track Properties:** under "Train type", a "Simulate this train" tick, then cars, car length, rows per car, seats per row, the reference ("centre of the train", or a car), the restraint; stand-ins marked "assumed"; lengths in the chosen units. All through D5's commands, inside the dialog's one undo step. New widgets get new object names; no existing widget is renamed (the batch scenarios find widgets by name).
- **Ride sheet:** "Train: single point" with where to choose one, or the train in one line; with a train, forces and airtime as ranges over the cars, and the every-seat limits line with the label that checking every seat is the program's extension of the standard's test practice (R13).
- **Plot:** a "Train" group: the watched car beside the asked force, and the band.
- **3D:** the train measures in "colour by measure".
- **Proof:** the baseline and the undo check unchanged; a batch scenario that ticks a train and reads the ride sheet's text; tried on screen by the lead.

### V2: the Train panel (Sonnet; after the owner's choice 2)

A tab beside the ride sheet, refreshed by the same timer.

- **Seat map:** the train from above, front at the left or top, one box per seat. Default mode "whole ride": each seat coloured by its worst share of a limit, the number written in it (so it does not rest on colour alone). Mode "at the cursor": what each seat feels at that moment; the cursor is the 3D view's point-of-view position, and a slider in the panel moves it.
- **Colour by:** share of limit (all rules, or one family), highest vertical, lowest vertical, sideways, along-track, seconds of airtime.
- **Click a seat:** it becomes the watched seat (its curves on the plot) and its findings are listed in words; clicking a finding moves the cursor there.
- **Ride strip:** the ride left to right against the rows front to rear, coloured by the same quantity; a click sets the cursor.
- **Proof:** tried on screen by the lead on R7's camelback and on the owner's project; batch mode never shows the panel.

### X1: trial of the design following a car (Opus)

By script, no change to the program: R7's `redesign.py` repeated design (design with the point, ride the train, design again with that train's speed at the chosen car) on the camelback, a loop and a lift crest, for the centre, the front and the rear car. Report: does it settle, in how many rounds, how far the shapes move, what it costs per edit, and a recommendation for how Stage 4's P2 should compute. A research note, `docs/research/x1-design-for-a-car.md`.

### Closing

The whole verification from an empty `build` folder, as listed under "Verified" in `STATUS.md` for Stage 2, with the new tests added and the owner's project in every check that takes a folder; the 2016-binary check of F4; `STATUS.md` rewritten; decision record 0014 moved from provisional to accepted with the owner's answers written in; rule 4 in `CLAUDE.md` reworded if the owner agreed to choice 6; the fork's section of `README.md`; the final review of the integration pull request; the report with what the owner must do before merging.

## 5. How the work runs

As `CLAUDE.md` and `docs/development-workflow.md` say: an integration branch `claude/stage-3` from `master` with a draft pull request into `master`; one branch and pull request per piece into it; each code piece built, run and compared before its pull request says so; the `review` label on every code piece and on the integration pull request at the end; nothing into `master` except on the owner's word. Workers do not push or post.

## 6. What could go wrong

The risks are in `docs/roadmap.md` ("Risks the architect is watching"). The ones that bite in this stage: a stand-in train gives stand-in results, and the screens must say so; the ride's rule for lifts is a stand-in; the limit numbers rest mostly on one secondary source; the per-seat computation after every edit must stay fast or become "on request"; and F4's promise about the 2016 program is unproved until tried on the binary.
