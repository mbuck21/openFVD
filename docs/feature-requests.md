# Feature requests

The owner's list of 2026-11-10 (FR-1 to FR-9) and what has been added since (FR-10 in issue #28, FR-11 to FR-18 in issue #30, FR-19 and the owner's priorities for the train on 2026-11-13), kept as given, with the architect's notes on what each one needs. The order of work is in `docs/roadmap.md`; what is built so far is in `STATUS.md`; this file is the record of what was asked for.

Each request has an id (`FR-n`) that the roadmap, issues and pull requests refer to. Add a new request at the end with the next number; do not renumber. Piece names in the notes below (A1, G1, P1 and so on) are the roadmap's; where an older note names a stage, the table here and the roadmap are right.

| Id | Request | Size | Roadmap |
|---|---|---|---|
| FR-1 | Standard heartline per train type | small, after research | done in Stage 1 |
| FR-2 | No macOS build | none (a non-goal) | deferred |
| FR-3 | Minimum curve radius check | medium | Stage 5 |
| FR-4 | Ride-force limits (ASTM), for every seat | large | Stage 3, at the design point and at every seat |
| FR-5 | Tooling for Claude to work the program without clicking | large, a foundation | Stages 1 and 2, the interface in Stage 5, the co-designer in Stage 7 |
| FR-6 | Throughput simulation with blocks, as in NoLimits 2 | very large | Stage 8 |
| FR-7 | Better force vector design: faster to build, no loss of detail | the main goal; ongoing | Stages 5 to 7 |
| FR-8 | Refactor and comment for agentic coding | medium, the architect's call | done in Stage 1; large files split as they are touched |
| FR-9 | More coaster and train types | small each, ongoing | any time |
| FR-10 | Parametric elements (for example a camelback hill) | large; research and experiments first | research done in Stage 1, trials in Stage 2, design in Stage 6, build in Stage 7 |
| FR-11 | Simulate the train as a train | large | experiment in Stage 2, the ride with a whole train in Stage 3, the design for a car and the numbers in Stage 4 |
| FR-12 | Close the circuit | medium, on the solver | trial in Stage 2, build in Stage 6 |
| FR-13 | Energy margin | small, on a new base | research in Stage 2, build in Stage 4 |
| FR-14 | Ride rules | medium | Stage 7 |
| FR-15 | Import and convert | very large | experiment deferred, analysis in Stage 5, the fit in Stage 8 |
| FR-16 | Launches, lifts, brakes, transfer track, switch track, swing launches | large; swing launches and switches very large | research in Stage 2; lifts, launches and brakes in Stage 6; the rest in Stage 8 |
| FR-17 | Comfort beyond the standard | medium | research in Stage 2, build in Stage 5 |
| FR-18 | Ride sheet | small | Stage 2 |
| FR-19 | English units by default | small | Stage 3 |

---

## FR-1: Standard heartline per train type

**Asked for.** A standard heartline height for each train type, so the designer picks the train instead of typing a number. The owner supplied the values below, pulled from NoLimits forums, and asked that they be verified as far as that is possible (definitive values will not exist for all of them) and that missing coaster and train types be added, especially the big manufacturers' main types (Intamin, B&M, RMC and so on).

**The owner's data, unverified.** Kept exactly as supplied. Negative values are trains that hang below the track.

```
NO LIMITS 2 - TRAIN HEARTLINES
Compatible with No Limits 2 Ver. 2.1.0.0
Information Compiled by: Coasterdave

Coaster                                     Feet      Meters
------------------------------------------------------------
4D Coaster                                  3.117     0.95
Classic Steel Looping Coaster               3.61      1.1
Classic Steel Looping Coaster (Modern)      3.61      1.1
Corkscrew Coaster                           2.133     0.65
Gerstlauer Bobsled Coaster                  3.61      1.1
Gerstlauer Euro-Fighter                     3.61      1.1
Gerstlauer Euro-Fighter 2                   3.61      1.1
Gerstlauer Spinning Coaster                 3.61      1.1
Gravity Group Timberliner                   3.28      1.0
Hyper Coaster                               2.953     0.9
Hyper Coaster (4 Seats Across)              3.61      1.1
Hyper Coaster (4 Seats Across with Scoops)  3.61      1.1
Hyper Coaster (4 Seats Staggered)           3.61      1.1
Inverted Coaster (2 Seats Across)           -2.953    -0.9
Inverted Coaster (4 Seats Across)           -3.609    -1.1
Inverted Face-to-Face Coaster               -2.953    -0.9
Inverted Impulse Coaster                    -3.806    -1.16
LIM Launched Coaster                        3.28      1.0
Mack Launched Coaster                       4.921     1.5
Mauer Söhne Spinning Coaster                3.773     1.15
Mauer Söhne X-Car Coaster                   4.25      1.295
Rocket Coaster                              3.61      1.1
Suspended Coaster                           -5.577    -1.7
Twisted Dive Coaster                        5.906     1.8
Twisted Floorless Coaster                   4.265     1.3
Twisted Flying Coaster                      -3.61     -1.1
Twisted Sitdown Coaster                     3.937     1.2
Twisted Stand-Up Coaster                    5.249     1.6
Twisted Wing Coaster                        1.64      0.5
Vekoma Flying Dutchman                      2.625     0.8
Vekoma Minetrain Coaster                    3.937     1.2
Vekoma Minetrain Coaster with Locomotive    3.937     1.2
Vekoma Motorbike Coaster                    3.61      1.1
Wooden Coaster (Classic 4-Seat)             3.28      1.0
Wooden Coaster (Classic 6-Seat)             3.28      1.0
Wooden Coaster (Trailered 2-Seat)           3.28      1.0
Wooden Coaster (Trailered 4-Seat)           3.28      1.0
Zamperla Twister Coaster                    3.61      1.1
```

**Where it stands in the code.** A track has one number, `track::fHeart`, typed into the "Heartline Height" box of the Track Properties dialog (`ui/trackproperties.cpp`). A new track starts at 0. The sign convention already matches the table: the mesh code treats a negative `fHeart` as a train below the track.

**What it needs.**

- Research first (roadmap R1): where each number is measured from (rail top, track centre, spine), whether NoLimits 2 has changed any since version 2.1, which the manufacturers publish, and which are a modeller's estimate. Each value gets a source and a confidence in the data file. A few stand out as worth a second look: Mack Launched at 1.5 m, the Wing Coaster at 0.5 m, and the Hyper Coaster at 0.9 m against 1.1 m for its four-across variants.
- A train-type data file (roadmap D2) that FR-3, FR-4 and FR-9 also use, so the heartline is one field of a train definition and not a list of its own.
- A picker in Track Properties that fills in the heartline (roadmap D3). In its first version it only fills the number, so the saved-file format does not change.

**Open question for the owner, later.** Whether the list should follow NoLimits 2's train names (so a design maps one to one onto an NL2 train) or real manufacturer models, or both with a cross-reference. The architect's default is both.

---

## FR-2: No macOS build

**Asked for.** Defer the macOS build; the owner does not need it.

**Decision.** macOS is a non-goal. The `osx/` folder and the `macx` block of `fvd.pro` stay in the tree untouched and unmaintained; they are removed if they get in the way of a change. New work is built and tested on Windows only and kept free of Windows-only code where that costs nothing, so Linux stays possible. Recorded in `docs/maybe-later.md`.

---

## FR-3: Minimum curve radius check

**Asked for.** Research the minimum curve radius for different track and train types, and a way to analyse a track to see whether it is violated anywhere.

**Where it stands in the code.** Every node already carries the curvature of the track at that point (`mnode::fFlexion()`, angle per metre), and the view can colour the track by it ("Track Flexion"). There is no notion of a limit.

**What it needs.**

- Research (roadmap R2): minimum radii by track system and train, separately for vertical curves, horizontal curves and the combined case, with sources. Real limits depend on the train's wheel assemblies and car length as well as the rail, and manufacturers publish little, so expect ranges and "typical" figures rather than one number per type.
- Limits stored on the train-type definition (D2).
- The analysis framework (roadmap A1): a check walks the track and returns findings (where, how much, against what limit), shown as marks on the 3D view and the plots and as a text report for automation.
- The check itself (roadmap A2): radius of the spine and of each rail, vertical and lateral components, against the limits.

---

## FR-4: Ride-force limits (ASTM), for every seat

**Asked for.** Research the ASTM standard for acceptable ride forces with different restraint types (the owner recalls a chart of maximum g against duration, depending on the restraint). Analyse a track to see where forces are exceeded. Then extend it to every seat: with four across, what are the forces at the outside seats; what are they in the rear car; does every seat meet the limits.

**Background for the research.** The standard is ASTM F2291, "Standard Practice for Design of Amusement Rides and Devices". It sets acceleration limits that depend on direction and on how long the acceleration lasts, has rules for accelerations in two directions at once, and defines restraint classes chosen from the accelerations the rider sees. The European counterpart is EN 13814 (ISO 17842). Both are copyrighted and sold, not free: their text and figures cannot be copied into this repository. The limits can be encoded as numbers with a citation. The research task (roadmap R3) establishes exactly what is needed and whether the owner should buy a copy; this note is from memory and is not a source.

**Where it stands in the code.** Each node has the force at the heartline (`forceNormal`, `forceLateral`), one sample per millisecond, so duration can be measured directly. There is no longitudinal force (along the track) stored, no limit, and no model of a train: the program follows one point.

**What it needs.**

- Research (R3), then limits as data, keyed by restraint class on the train type (D2).
- Part 1, at the heartline (roadmap A3): the three force components over time against the duration limits and the combined-direction rule, reported through the analysis framework (A1).
- Part 2, every seat (roadmap A4): a train placed on the track (cars, their length and coupling, seat positions across and along), and the acceleration at each seat from the motion of its car, which adds the effects of roll, pitch and yaw rates to the heartline value. The outside seats of a wide train and the ends of a long train are where this differs most. This needs seat geometry in the train definitions and is the first piece that models a whole train; FR-6 builds on the same model.

**The owner's words of 2026-11-13** (the third of the three priorities for the train stage): "implementing the ASTM safe forces check (not sure how the user will be shown this but maybe thinking to show another GUI where you can scroll through the ride and it is a top down picture of the train with seats color coded to see if all seats pass ASTM standards). Maybe then also have a visualize and color code the train with each test similar to how the track can be visualized to show speed, normal force etc." With it: "please don't just take my word for how this should work, please think how someone who is using this program would utilize these features and how they could be implemented most conveniently and intuitively into the tool without being a necessary part of the tool." Decision record 0014 has the design that came of it (the seat map, the ride strip, the track coloured by the worst seat) and R13 the second look at the numbers.

**A caution to keep.** This would be a design aid for a hobby tool. It is not a certification of a ride, and the program and its documents should say so wherever the check is shown.

---

## FR-5: Tooling for Claude to work the program without clicking

**Asked for.** A way for Claude to interact with the program as a person would, but without pressing GUI buttons; to be more aware of what it is building and has built. Think carefully about how to do it well and how the tooling keeps up as the code changes. Used first in development, later to help build coasters.

**Where it stands.** Today the only way in is the window. The baseline check on 2026-11-10 took about a hundred mouse and keyboard actions. The model cannot run without the widgets: `core/undoaction.cpp` refers to the UI 243 times and `core/track.cpp` 27 times; loading a file calls the track widget's "add section" slots.

**The architect's design, in three layers.**

1. **A core that runs without a window** (roadmap F1). The model, the physics, save and load, and the exporters stop calling into the widgets. This is also what FR-8 is about.
2. **One command layer** (roadmap F1, T1). Every edit a person can make ("add a force section", "set this transition to +2 g over 1.5 s") becomes a named command with typed parameters and units. The window's buttons call the commands; undo and redo record them; automation calls the same ones. Because there is a single list, the tooling cannot drift from the program: a new feature is a new command, and the description the agent reads is generated from the list. A test fails if a command has no description.
3. **Three ways to reach the commands** (roadmap F2, F3, T2):
   - a command-line program for batch work and tests (load, edit, recompute, export, run the checks, print a summary);
   - the project as readable text (a JSON description beside the binary `.fvd`), so a coaster can be read, compared and written as a document;
   - a live link into the running window, so the agent's edits appear in the session the owner is watching, plus "give me a picture of the view" and "describe what is on screen". Wrapped as a tool server so a Claude session gets tools such as `add_section`, `get_forces`, `check_track`, `snapshot`.

**Being aware of what was built** is a matter of good summaries: a compact description of a track (sections, lengths, speeds, peak forces, where the checks fail) that fits in a few hundred words, with the detail available on request.

**Constraint.** If the live link listens on a network port, the owner's rule for local servers applies (register it with `svc`, bind 127.0.0.1, a port from 7000 to 7099). A named pipe or standard input and output avoids that and is the default choice.

---

## FR-6: Throughput simulation with blocks, as in NoLimits 2

**Asked for.** Possibly further down the road: simulate the coaster to see the ride's throughput. That needs track pieces such as station, lift, brakes and trims, and block section logic built exactly as in NoLimits 2, so the same custom block script works in both programs. Probably a block section builder GUI as well.

**What it needs.**

- Research (roadmap R4): how NoLimits 2 defines blocks and its scripted block logic, what the script language and its interfaces are, and whether "the same script runs in both" is realistic in full or only for a defined subset. This decides the size of the whole request and is the main risk.
- Special sections (roadmap O1): station, lift, brake, trim and transport as section kinds with their own parameters (speeds, accelerations, lengths), exported to NoLimits 2 as such where the element format allows.
- A circuit simulation with several trains (roadmap O2): dispatch, travel times per block, waiting, and throughput in riders per hour. It reuses the train model from FR-4 part 2.
- Block logic and its editor (roadmap O3).

---

## FR-7: Better force vector design

**Asked for.** The biggest change and the main goal of the project: an overall improvement in how force vector design is done. Research and develop improvements that make creating a coaster faster without giving up detail. Think outside the box.

**How it will be approached.** As a study first (roadmap R5), then in waves. The study looks at how FVD++, newton2 and other tools are actually used, where the time goes when building a layout, and what the rest of this roadmap makes possible. Its output is a short list of directions with a sketch of each, for the owner to rank. Candidate directions the architect will examine, as proposals and not commitments:

- **Sections that aim at a goal.** Today the designer adjusts forces and durations by hand until the track ends where it should. A solver could adjust chosen parameters so a section ends at a given height, heading, position or speed: "level out at 2 m", "arrive back at the station".
- **Elements as parametric building blocks.** An airtime hill, a vertical loop, a zero-g roll, an overbanked turn, each described by a few meaningful numbers (target g, duration, entry speed) and expanded into ordinary editable sections.
- **Banking worked out for you.** Roll computed to hold the lateral force at zero or at a chosen value through a turn, instead of drawn by hand.
- **Limits visible while editing.** The radius and force checks from FR-3 and FR-4 shown live, so a problem is seen when it is made.
- **A faster editor.** Copy, paste, mirror and reorder sections; edit several at once; direct handles on the plots; recompute quick enough on long tracks to feel instant.
- **A coaster as text, and a co-designer.** With FR-5 in place, a layout can be described in words or a script and built by an agent, then refined by hand.
- **Whole-layout tools.** Clearance between track and track, a footprint to stay inside, the circuit closing on itself.

---

## FR-8: Refactor and comment for agentic coding

**Asked for.** Maybe refactor and comment the code to make it more context-efficient for agentic coding. The architect's call whether it is needed.

**The call: yes, but targeted, not a rewrite.**

- **Do:** separate the model from the widgets (roadmap F1). It is the same work FR-5 needs, it is what makes automated tests possible, and it is the single change that most reduces how much code a session must read to make a safe edit.
- **Do:** a short `CLAUDE.md` in `core/`, `renderer/` and `ui/` once F1 has settled their boundaries, and a header comment on each main class saying what it owns and who calls it.
- **Do, as files are touched:** split the very large files along lines that already exist in them (`renderer/trackmesh.cpp` is 3,700 lines, mostly one function per track style).
- **Do not:** reformat, rename for style, or modernise code that nobody is changing. It would make every later diff harder to review and risks changing results that the baseline does not cover.

---

## FR-9: More coaster types and train types

**Asked for.** Add more coaster types and train types.

**Where it stands in the code.** The only "type" today is the track style, a fixed list of eight rail cross-sections drawn in the view (`enum trackStyle`: generic, generic flat, Vekoma, B&M, triangle, box, small flat, double spine). It affects the picture only.

**What it needs.** The train-type data file from D2, so that adding a type is adding an entry (name, manufacturer, heartline, seating, restraint class, limits, the track style to draw it on) and not a code change. New rail cross-sections for the view are separate, smaller pieces in `renderer/trackmesh.cpp`. Which types matter most is the owner's to say; the research for FR-1 (R1) produces the candidate list, starting with the main models of Intamin, B&M, RMC, Mack, Vekoma, Gerstlauer and GCI.

---

## FR-10: Parametric elements

**Asked for** (2026-11-10, issue 28). A ride element such as a B&M-style camelback airtime hill added as one thing, defined by a few meaningful numbers (airtime strength and a little more), instead of built by hand from force sections every time. The owner's notes, as written:

- Parametrically defined elements?
- Premade parametric elements? Like a camelback airtime hill where you define airtime strength and some other things as well.
- A custom way to create parametric elements.
- Jerk limits, acceleration limits, curve radius limits for these parametric elements? An option to make sure an element complies with the ASTM acceleration limits in all seats.
- Should these parametric elements be fully editable after adding them? How do we anchor elements correctly?

**Nothing is to be built yet.** The owner's judgment, which the architect shares: a complicated feature with many caveats and edge cases. It needs research, experiments and testing first, and a design that comes out of them. Issue 28 lists the caveats and the experiments.

**Where it stands in the code.** There is no notion of an element: a track is a flat list of sections, each starting where the one before ends. FR-7 already named "elements as parametric building blocks" as a candidate direction; this request makes it a feature of its own.

**What it needs.**

- Research and thinking first (roadmap R6, in Stage 1): how elements are defined in practice, how other tools offer them, what an element is in terms of this program (sections, parameters, unknowns, goals), every caveat in issue 28 worked through, and a first numerical experiment on a stand-in model. Output: `docs/research/r6-parametric-elements.md`.
- The experiments on the real program (roadmap E1, Stage 2): they need the core running without a window and the command line.
- A written design with the owner's choices (roadmap E2, Stage 4), then the build (roadmap E3, Stage 5), which stands on the goal-seeking solver (G1), transitions that end at a value (G4), the measures and force limits (A1 to A3), and for the all-seats option the train model (A4, Stage 6). Holding an element's position is a pin (G2), placed by the designer.
- An addition to the saved file (roadmap F4; rule 4 in `CLAUDE.md`). R6 found a place for the element record that the 2016 program never reads, so files with elements could still open there as plain sections; the owner decides.

---

## FR-11: Simulate the train as a train

**Asked for** (2026-11-10, issue 30, as are FR-12 to FR-18). The physics should follow a whole train, not one point.

**Why.** The program moves a single point along the heartline and takes its speed from that point's height. A real train is 15 to 20 m of mass spread along the track: going over a crest, part of it is still climbing while part is already descending, so its speed differs from a point's. NoLimits 2 simulates the whole train, which is a likely reason the forces seen there do not quite match the design.

**Where it stands in the code.** Each node's speed comes from its own energy and height (`core/secforced.cpp` and the other section files). There is no train: no length, no cars. The train catalogue has no car lengths yet, and no manufacturer publishes them (R1).

**What it needs.**

- An experiment first (roadmap R7, Stage 2): how big the difference is, on a camelback and a loop, for a short and a long train. The size of the effect decides the priority.
- A computation that rides a finished track shape (roadmap P1, Stage 3), first with a point. It is shared with FR-13, FR-15 and FR-16.
- The train itself (roadmap P2, Stage 6), in two steps. First the ride over the finished shape with all the cars, which shows what each car feels and changes no design. Then a setting per project that makes the design use the train's speed. It changes what a project computes, so it is off for every existing project and the baseline keeps testing the point model.
- Car lengths and counts in the train data, entered by the owner where no source exists. FR-4's every-seat check (A4) uses the same train.

**The owner's words of 2026-11-13** (the first two of the three priorities for the train stage): "full train simulation where the friction and stuff is modeled similarly to NoLimits 2", and "allow the user to choose number of cars, maybe mass (if NoLimits 2 uses that for calcs). Also allow the user to choose which car they are doing the FVD for (I think normal convention would be to use the train's center of mass along its heartline but maybe have that as an option)." And for a later stage: "getting good values for train sizes, seat placement/offsets, restraint type, correct mass and friction compared to NoLimits 2." Decision record 0014 has the design; R12 found that NoLimits 2 has no mass and uses the same two resistance numbers as this program. The roadmap's Stage 3 is the ride with a whole train, Stage 4 the numbers and the design following a chosen car.

---

## FR-12: Close the circuit

**Asked for.** One command that builds the run from where the track ends back to the station.

**Why.** It is the owner's second-biggest time sink ("hitting a target") at the scale of the whole ride.

**What it needs.**

- It is the first slice of pinned points (R5, direction C2; roadmap G2, Stage 4): the station is a pin, and the piece that is solved to meet it is the closing run. So it is built on the goal-seeking solver (G1, Stage 3), not as a tool of its own.
- A trial first (roadmap E1, Stage 2): re-close one of the owner's projects by script, with a straight's length and a curve's angle as the unknowns. It shows whether a turn-and-brake-run template with a few unknowns is enough or a more general connecting piece is needed.
- An honest answer when no connection exists within the limits (too much speed, too little room): say so, show the closest result, leave the track as it was.
- Brakes for the last stretch (FR-16, roadmap P3).

---

## FR-13: Energy margin

**Asked for.** Show how much speed the ride has in reserve, and where it would stall or roll back.

**Why.** A design that just clears a hill at the nominal friction does not clear it on a cold day or with an empty train.

**The architect's note on how.** The first thought in issue 30 was to re-run the existing computation with a low and a high friction. That answers a different question: with other friction the design computation would reshape every force section to keep its forces, so the hills themselves would change. The margin has to be measured on the shape as designed. That needs the computation that rides a finished track (roadmap P1), which is why this request sits in Stage 3 and not first; the same computation serves FR-11, FR-15 and FR-16.

**What it needs.**

- Research (roadmap R8, Stage 2): sensible friction and resistance ranges for steel and wooden coasters, empty and full, with sources, and how the program's `fFriction` and `fResistance` relate to rolling and air resistance.
- P1, then the margin itself (roadmap A6, Stage 3): the speed band along the ride, the reserve at each crest, and the first point where the worst case stops, shown through the measures and findings (A1).

---

## FR-14: Ride rules

**Asked for.** Named values for the whole ride that sections and elements refer to.

**Why.** Today every limit and habit of a design (maximum g, airtime target, onset rate, roll rate) is typed into each transition by hand. Changing the character of a ride means editing every one.

**What it needs** (roadmap D4, Stage 5).

- A small table of named values per project. A transition or an element uses a rule instead of a number; changing the rule updates everything that uses it, as one undo step (the command list, T1). Saved sets of rules are styles the owner can reuse.
- Transitions that end at a value (G4) first: "end at the airtime target" is a rule a transition can point to; "change by" is not.
- Something that keeps a rule change from disturbing the rest of the ride: pins (G2) and elements (E3). Without them, changing a rule is the knock-on problem at full scale.
- The thresholds of the checks and the comfort measures (FR-17) come from the same table.
- A field in the saved file (F4).

**Find out first.** How a value in a transition refers to a rule without making the transition editor harder to use.

---

## FR-15: Import and convert

**Asked for.** Read an existing NoLimits 2 track and turn it into an editable force-vector project.

**Why.** To start from a ride that already exists, or to clean up a hand-built NoLimits track, instead of rebuilding it from nothing.

**Where it stands in the code.** The NoLimits 2 CSV import exists, but its section holds one speed throughout and computes no forces or energy (found in R5), and the importers have known defects (issue 41).

**What it needs.** Three steps of rising difficulty.

1. **Analyse** (roadmap I1, Stage 3): ride the imported shape with P1 and show its forces like a designed track's. Worth having alone.
2. **Segment** and 3. **fit** (roadmap I2, Stage 6): split the ride into stretches that behave like force or geometric sections, and find clean transitions that reproduce each within a tolerance, with the solver (G1). The result is an approximation; the tool shows how far it strays from the original.

Before step 2 is designed, an experiment (roadmap R11, Stage 2): how noisy a hand-built track's forces are, how well a fit can work, what tolerance is useful. It needs one or two NoLimits 2 tracks from the owner. I2 is built only if the experiment shows a useful fit.

---

## FR-16: Launches, lifts, brakes, transfer track, high-speed switch track, swing launches

**Asked for.** The special track a real ride has, as part of the design.

**Why.** These set the energy the rest of the ride lives on (a launch or lift puts it in, brakes and trims take it out), and they shape the layout.

**Where it stands in the code.** A section can hold a fixed speed (`bSpeed`, `fVel`), which is close to a lift. There is no acceleration given along the track, no along-track force, and a project is a list of separate tracks with no connections.

**What it needs.** Three groups, very different in size.

- **Lifts, launches, brakes, trims** (roadmap P3, Stage 4): sections where the speed is driven instead of carried by gravity. The first plan had these in the last stage with the block logic; for design they matter earlier, so the physics is pulled forward and the blocks stay where they were. They change what a project computes and add to the saved file, so they wait for F4.
- **Swing launches** (roadmap P4, Stage 7): the train passes over the same track several times, forwards and backwards. A force-vector design fixes the forces for one speed at each point, so the track is shaped for one chosen pass and the ride computation (P2) runs all the passes and checks each.
- **Transfer track and high-speed switches** (roadmap P5, Stage 7): tracks joined at a point, with the geometry of each position. Mostly a layout and export matter until the block simulation (FR-6) uses them.

Research first (roadmap R9, Stage 2): typical accelerations and speeds of LSM and hydraulic launches, lifts and brakes, with sources; how a swing launch is specified; what NoLimits 2 can take from an export (R4 found that the element file carries geometry only, so stations, lifts and brakes are set up again inside NoLimits 2).

---

## FR-17: Comfort beyond the standard

**Asked for.** Measures of how a ride feels that the acceleration limits do not capture.

**Why.** A ride can pass every limit and still be rough: abrupt transitions, snappy rolls, the head thrown sideways.

**What it needs** (roadmap A7, Stage 3).

- Measures computed along the track and drawn as colour and plots (A1): jerk (the rate of change of each force), roll rate and roll acceleration, and the sideways movement of the rider's head in a transition. With the train (A4, Stage 6), the same for the outer and end seats.
- Thresholds the designer sets, later from the ride rules (FR-14). These are the designer's own measures, not a standard: R3 found no general jerk limit in ASTM F2291.
- Research first (roadmap R10, Stage 2): which measures match what riders report as rough, and typical values on rides known as smooth and as rough, if any are published.

---

## FR-18: Ride sheet

**Asked for.** The ride's key numbers on one panel, updated while editing.

**Why.** To see at a glance what an edit did to the ride as a whole.

**What it needs** (roadmap A5, Stage 2).

- Length, height, drop, duration, top speed, total seconds of airtime, peak g in each direction, number of inversions, and the count of open findings from the checks. Mostly a matter of collecting what the nodes already hold.
- The same sheet as text from the command line. It is the "compact summary of a track" that FR-5 asks for, so it is built once and used by the panel, the command line and the automation interface.
- It is the first thing built on the measures and findings (A1), and the proof that they work.

**The owner's part.** Definitions to agree on: what counts as airtime, what counts as an inversion.

---

## FR-19: English units by default

**Asked for** (2026-11-13). "Make the program default to English units."

**Where it stands in the code.** Units are one program option (metres and m/s, metres and km/h, or feet and mph), chosen in Edit > Options and kept in `options.cfg` beside the program. Nothing about units is saved in a project; the model is metric throughout. Since piece U1 a fresh install starts in feet and mph, an existing `options.cfg` keeps its value, batch mode always computes in metric, and the length and speed boxes keep their metric limits in every unit.

**What it needs** (roadmap U1, Stage 3). The default of a fresh install becomes feet and mph; an existing `options.cfg` keeps its choice; the switch stays. Batch mode always computes in metric, so the comparison with the 2016 binary does not depend on the option.
