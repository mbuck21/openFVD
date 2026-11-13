# 0014: Stage 3 is the train: ridden over the finished track, checked at every seat, optional throughout

- Status: proposed (the structure is the architect's and is being built; the product choices in "Asked of the owner" wait for his word)
- Date: 2026-11-13
- Needs owner approval: yes for the eight choices listed at the end; no for how it is built.

## Context

- The owner set the next stage's priorities on 2026-11-13: (1) a full train simulation, with friction modelled as NoLimits 2 does; (2) the number of cars, perhaps the mass, and which car the force-vector design is for; (3) the ASTM force check for every seat, shown at a glance; and English units by default. He asked that his sketch of the screens not be taken literally, that every part be optional, and to be told whether it is too much for one stage. He named a follow-on stage himself: good values for train sizes, seat positions, restraints, mass and friction against NoLimits 2.
- R7 measured why this matters: the program's forces are right for the middle of a train only; the end cars of a 20 m train differ by 0.2 to 0.5 g over a hyper-style crest and by more than 1 g over a slow lift crest.
- The roadmap had the pieces spread over Stages 3 and 6: riding the finished track with a point (P1), the force check at the heartline (A3), the train (P2), forces at every seat (A4); and the saved-file mechanism (F4) in Stage 4.
- What the code offers already: measures and findings (record 0012) with colour on the 3D track and curves on the plot, the ride sheet, named commands with undo, the train catalogue (64 types; car lengths and seat positions mostly missing, R1), a point-of-view position that already moves along the track in the 3D view and is marked on the plot, and a project reader that stops at the end marker `EOP`.

## How a designer uses it (what the design is judged against)

A designer shapes a hill, exports, rides it in NoLimits 2 in the back row, finds it stronger than asked, comes back and adjusts. The stage brings that loop into the program, in four steps that each work without the next:

1. **Nothing changes until a train is chosen.** An old or a new project opens as today. The ride sheet says "Train: single point" and where to choose one.
2. **Choosing a train is one pick.** Track Properties already has "Train type" (it fills the heartline). It gains a "Simulate this train" tick and, under it, cars, car length, rows per car and seats per row. Every box is filled from the catalogue; a value the catalogue does not have is a stand-in and is marked "assumed" on screen. The pick is saved with the project. There is no mass box: NoLimits 2 has no mass either (point 5).
3. **The answers appear where the designer already looks,** a moment after each edit:
   - *Ride sheet:* forces and airtime as a range over the cars ("lowest vertical force -0.2 g at the centre, -0.6 g in the rear car"), and one line for the force limits ("all 36 seats within the limits" or "4 seats over: worst rear car, left outer seat").
   - *Plot:* a "Train" group: what the watched car feels as it passes each place, beside the force that was asked for, and a band from the least to the most any car feels there.
   - *3D view:* the existing "colour by measure" gains train measures, above all "worst seat, share of limit": the track itself shows where on the ride any seat is near or over a limit.
4. **One panel answers "which seat":** the Train panel, a tab beside the ride sheet (below).

## Decision

### The physics

1. **The train is a second computation over the finished track** (`core/trainride.*`), never part of the design computation. It reads the nodes and writes none. No project computes differently, and the comparison with the 2016 binary is untouched. It works in double precision.
2. **A point is a train of one car.** "Ride the finished track" (P1) and "the train" (P2, first step) are one piece. Its acceptance test is the roadmap's: with one car at the program's own energy point and the project's own friction it reproduces the design's speed at every node (to 0.002 m/s) and its forces.
3. **The cars are rigidly coupled and share one speed,** which comes from the energy of all cars, each at its own height (R7's model, validated there against a closed form). Each car feels the force of the shared speed on the shape where that car is.
4. **Friction and drag are four terms summed** (`rideResistance`): a share of the train's weight (the program's `fFriction`), a share of the force pressing the cars on the track (so a 4 g valley costs more than a crest), a term that grows with speed, and a term that grows with speed squared (the program's `fResistance`). The default is the project's own two numbers, so the ride agrees with the design. **R12 found that NoLimits 2 uses the same two numbers** (`docs/research/r12-nolimits2-train-physics.md`): a constant friction and an air resistance, chosen by its "Physics Model" setting; the author of FVD++ said version 0.75 moved to "NL2 style". So the default is already NoLimits 2's kind of model, and the other two terms stay at zero unless the owner's recordings show NoLimits 2 needs them. One user's coast-down fits (low confidence) give friction about 0.026 for steel and 0.017 for wood, drag 2e-5 for the "NL 2.0" model and 1e-4 for "NL 2.2"; they are offered as presets, marked as one tester's fit. What R12 could not find (the coupling of the cars, where a car's mass sits, whether friction depends on load, the lift crest, any car length) becomes six short experiments for the owner in NoLimits 2 (an `owner` issue). The numbers that make the program match NoLimits 2 are the follow-on stage's work; this stage builds the shape they go into.
5. **Mass is not a setting.** R12 found no mass anywhere in NoLimits 2: not in its scripting interface, its help or its park file. In a model where friction and drag are per kilogram, the mass of the train cancels. The train's description carries mass fields (car, rider, how full) so that a train with a heavier lead car, or a drag given as an area, can be described later; they have no box on screen in this stage.
6. **A section with a held speed** (a lift, a station, a brake run: speed not carried) holds the train exactly while every car is on held-speed track. While the train straddles held and free track, the held part can push but not hold back: the train runs ahead once gravity makes it faster. That gives the rear-car whip over a lift crest that R7 found, and equals the design for one car. It is a stand-in until lifts and brakes are section kinds of their own (P3).
7. **Every seat:** each row rides the track at its own place; a seat's force is its car's force plus the terms from the turning of the frame at the seat's offset (R3, section 7). Forces are reported in the standard's rider axes: pressed into the seat, sideways, pressed into the seat back.

### The force limits

8. **The engine is separate from where the forces come from** (`core/forcelimits.*`): it takes what one rider feels against time and applies the limits. The same engine checks the design point of any project (no train needed) and every seat of a train.
9. **Every limit number is data** (`data/limits/astm-f2291-public.json`), with its source and confidence beside it, as R3 proposed; none is in the code. The owner can replace the file's numbers from his own copy of the standard without a rebuild. R13 (`docs/research/r13-force-limits-second-sources.md`) looked for second sources for the twelve items R3 marked uncertain: two are now corroborated, three partly, six still rest on one source, and one is contradicted between editions (the 2006 edition multiplies the radii of the combined-direction rule by 1.1; Rohde's account of the 2023 edition has no factor). The file takes the stricter reading, no factor, and says so beside the number.
    - **Checking every seat is the program's own extension.** R13 found that the standard's test practice puts one accelerometer per car, usually in the front, middle and rear car, and that no public text says the limits hold at every seat. The every-seat check applies the same limits at more places than a test would measure, and the screens say so.
    - **Where a rider's force is taken:** at the heartline by default (where a design is made, and close to the European measuring height); the train's description has the offset as a number.
10. **The result is a share of the limit, not only pass or fail.** For every moment and every seat the engine says how close that seat is to its nearest limit (1.0 is at the limit) and which rule that is. That number is what the screens colour, so a seat at 95 percent looks different from one at 40.
11. **The program says what it is:** wherever a limit is shown, that the numbers are transcribed from public secondary sources, with their confidence, and that this is a design aid and not a certification.

### The screens

12. **The owner's idea, judged.** A top-down picture of the train with coloured seats is the right picture for "which seats": where a seat is along the train and across the car are the two things that decide what it feels. Scrubbing through the ride to find a problem is the weak part: the limits are about how long a force lasts, so a single moment neither passes nor fails, and a problem that lasts half a second is easy to scrub past. So:
    - **The seat map shows the whole ride by default:** each seat coloured by its worst share of a limit anywhere on the ride, with the number written in it. One look, no scrubbing. A second mode, "at the cursor", colours by what each seat feels at that moment and follows the point-of-view position the 3D view and the plot already share; a slider in the panel moves it.
    - **"Colour by" chooses the test,** as the 3D track is coloured by a measure: share of limit (all rules, or one rule), highest and lowest vertical force, sideways force, along-track force, seconds of airtime.
    - **Clicking a seat** makes it the watched seat: its curves go on the plot and its findings are listed in words ("rear car, row 2, left: below -1.5 g for 0.6 s at 412 ft, section 7"). Clicking a finding moves the cursor there.
    - **Added by the architect: the ride strip.** A band under the seat map with the ride from left to right and the rows of the train from front to rear, coloured by the same quantity. It shows in one still picture where on the ride and how far back in the train a problem sits, which the seat map alone cannot.
    - **The train drawn in the 3D view** (cars and seats at the cursor, coloured) is left to the follow-on stage, when car and seat dimensions are real: drawn from stand-in dimensions it would look more certain than it is. The 3D track coloured by a train measure (point 3 above) is in this stage.
13. **The watched car.** The train's description has a reference: the train's centre by default (the owner's own guess at the convention, and what R7 recommends), or any car. In this stage it decides which car is at the start at time zero and which car's forces are drawn beside the forces asked for. Making the *design itself* follow that car is the follow-on stage (point 17).

### The saved file

14. **One mechanism for additions (F4), built now:** an appendix after the project's end marker. The reader of the 2016 program, and of this fork until now, stops at `EOP`, so everything after it is ignored there. The appendix starts with its own tag and version number and holds tagged blocks, each a length and a text payload (JSON, UTF-8), so a later addition is a new block or a new key, never a new binary layout. A block the program does not know is kept and written back. The text form of a project (F3) carries the same blocks.
15. **What that means for rule 4** ("the `.fvd` format changes only with a version bump and a load path for old files"): the main version stays `v0.77` while additions do not change the computed track, so such a project still opens in the 2016 program (which drops the appendix if it re-saves). The appendix has its own version number. The first addition that changes the computed track (designing for a car, a launch) bumps the main version, so the 2016 program refuses the file instead of computing a different ride. This is the recommendation already made in record 0010; it needs the owner's word because it amends a rule that does not bend.
16. **The train is part of the track's state:** every change of it is a named command and an undo step, like any other edit.

### Scope

17. **One stage holds "see the train"; "design for a car" and the real numbers are the next.** Stage 3 builds everything above: nothing in it changes a design, so nothing in it can spoil an existing project. Stage 4 then holds what does change a design or needs NoLimits 2 beside it: the design following a chosen car (P2's second step), the numbers (car lengths, seat positions, restraints, mass, friction matched against NoLimits 2), the train drawn in 3D, and the energy margin (A6), which needs trustworthy friction. Reason for the order: designing for a car bakes the train's length into the shape of the track; with stand-in lengths that would bake a guess into the owner's coasters. Stage 3 ends with a trial of design-for-a-car by script, so Stage 4 starts from measurements, as G1 did from E1.
18. **English units** become the default of a fresh install (`options.cfg` absent). Units are a program option, not saved in a project; an existing `options.cfg` keeps its choice. Batch mode always computes in metric, so the comparison with the 2016 binary cannot depend on the option.
19. **The rest of the old Stage 3** (minimum radius A2, comfort A7, imported-track analysis I1, the knock-on view W1, the solver G1, the automation interface T2) moves behind the two train stages unchanged; the order after the train is the owner's.

## Asked of the owner

Each with the architect's recommendation; what is built before the answers does not depend on them.

1. **The split** (point 17): design-for-a-car in Stage 4 with the real numbers. Recommended. The alternative is to build it at the end of Stage 3 on stand-in train lengths.
2. **The seat check as proposed** (point 12): seat map showing the whole ride by default, "at the cursor" as a second mode, colour by test, the ride strip, the track coloured by the worst seat; the 3D train later. Recommended.
3. **No train by default** (point 1 of the walk-through): a project has no train until one is ticked. Recommended. The alternative is a generic train on every new project.
4. **The train's centre as the default reference** (point 13). Recommended.
5. **Buying ASTM F2291** (the current edition 26b, US$121). Recommended now that the check is being built: R13 could corroborate only two of the twelve uncertain items from public sources, and found one contradiction. The owner reads the copy himself and types the numbers into the limit file (the seller's terms prohibit giving the text to an AI). Until then the check runs on the public numbers and marks their confidence.
6. **The saved file** (points 14 and 15): train settings in an appendix the 2016 program ignores, the main version unchanged until a feature changes the computed track; rule 4 reworded to say so. Recommended.
7. **A short session in NoLimits 2** to settle what R12 could not: an `owner` issue with exact steps, needed before Stage 4's numbers, not before Stage 3.
8. **The order after the train:** the rest of the old Stage 3 next, as it was. Recommended unless the solver matters more than the train's numbers.

## Where enforced

- `docs/roadmap.md` (reworked: Stages 3 and 4 are the train; the later stages renumbered).
- `core/trainspec.*`, `core/trainride.*`, `core/forcelimits.*`, `data/limits/`; `tests/trainride/`, `tests/forcelimits/`.
- The baseline comparison (`docs/testing.md`) on every piece: unchanged, with no tolerance.
- `CLAUDE.md` rule 4, reworded once the owner agrees to choice 6.
