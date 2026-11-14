# X1: The design following a chosen car, tried by script

Piece X1 of Stage 3 (`docs/stage-3-plan.md`, section 4), for Stage 4's P2 (the design follows a chosen car of the train). Written 2026-11-13 from trials run on `claude/x1-design-for-a-car-trial` (at the tip of `claude/stage-3`), with the program built there by MSYS2 UCRT64. Nothing in the program was changed. The scripts, their outputs and the projects they made are in `docs/research/x1-trials/`; its README says how to rerun every table. Builds on R7 (`r7-train-against-point.md`, section 5, where `redesign.py` first tried the idea on the camelback), P1 (`fvdcore ride`, the train ridden by the program) and E1 (`e1-solver-trials.md`, the model for a trial of this kind).

## Summary

Force-vector design shapes the track so that one point, at the speed the program computes for it, feels the asked forces. A real train shares one speed over its whole length, so its cars feel something else (R7). "Designing for a car" means shaping the track with the speed the train has when the chosen car is at each place, so that this car feels what was asked. That speed depends on the shape, and the shape on the speed, so it has to be found by repetition: design with the point, ride the train, design again with the train's speed at the chosen car, and so on. This trial did that on R7's camelback, vertical loop and lift crest, for the centre, the front and the rear car of R7's 9-car, 19.8 m train, with every force measured by the program's own ride (`fvdcore ride`).

1. **For the train's centre it works everywhere.** The repetition settled (the shape moving less than 2 cm between rounds) on all three shapes and with both readings of the asked forces, in 5 to 6 rounds on the camelback, 8 on the loop and 11 to 19 on the lift crest with plain repetition, and in 6 to 10 rounds on every shape with the rule recommended below (blending the new speed half-and-half with the last). The centre car then feels the asked force to within 0.02 g everywhere (the measuring floor is 0.015 g), where on the point design it was off by 0.07 g (camelback), 0.31 g (loop) and 0.46 g (lift crest).
2. **The shape moves little for the centre, but the exit does not stay where it was.** Camelback crest 0.2 to 0.3 m along and 0.4 to 0.6 m up; loop top 1.5 m earlier; lift crest's drop 2.6 to 3.1 m further on and 0.8 m lower. The largest distance between the old and the new shape inside the element is 2 m (camelback, loop) to 4.6 m (lift crest). But with the same force program the exit no longer comes out level: 2 to 6 degrees of pitch, so everything after the element moves (the camelback's exit ends 3 to 4 m higher). Re-solving the element's durations each round to its goals (R7's version B; tried on the camelback) settles just as fast (4 to 6 rounds, plain repetition) and keeps the crest at 30 m and the exit level.
3. **For the front or the rear car it often does not work.** On the camelback it settles (8 to 13 rounds with the rule; plain repetition swings in one of the six cases) and moves the crest 4 to 7 m. On the loop, the front car has **no design** in either reading: every attempt asks for a loop taller than the train can climb and stalls, and solving shape and train together in one pass winds the track into a spiral at the top. The rear car's loop settles only with blending, and is then a different element: 5 to 6 m lower at the top, not closing (exit 39 to 59 degrees off), and the front car pulls 6.7 to 6.8 g in it. On the lift crest the front car's design stalls with the forces asked against time (the pull-out over-turns and the track climbs to 33 m above the top of the lift) and works against distance; the rear car's design works only against time and only with blending.
4. **Designing for one car never removes the spread between cars.** On the centre-car camelback the end cars still feel -0.54 to -0.56 g where -0.2 g is asked; designing for the front car makes the rear car feel -1.01 g and 4.99 g where -0.2 and 3.5 g are asked.
5. **One round costs one recompute plus one ride.** In the program that is about **20 to 25 ms on a 14,000-node track** (`timing.py`: a whole-track recompute 13 to 14 ms, the 9-car ride under about 10 ms), so with the recommended rule's 6 to 10 rounds for the centre car about **0.12 to 0.25 s per edit** (at most 13 rounds, about 0.33 s, in any case that settled), and ten times that on a 150,000-node ride: too slow to run after every change on a whole ride, fine as a button or after a pause.

**Recommendation for P2.** Build it as a repetition around P1's ride, aimed at the train's centre by default: after an edit (or on a button for a long ride), recompute the track with the speed P1's ride gives at the chosen car (`trainRideSpeedByNode()` already gives exactly that), ride again, and stop when the shape moves under 2 cm, with the new speed blended half-and-half with the last one used from the second redesign on, always, and at most 20 rounds (the most any case took was 13). Blending costs one to five rounds against plain repetition on the camelback, is even on the loop and saves rounds on the lift crest (10 against 19), and it is the only rule tried that settled every case that settles at all (14 of 18, against 10 of 18 plain), with no one watching; if the train stalls or the rounds run out, keep the point's design and say so in words. Keep each section's own argument (a time section asks its forces against the chosen car's time, a distance section against distance): both readings worked for the centre, and that way nothing new has to be explained. Offer the front and the rear car only behind a warning that some elements have no such design, and pair the whole thing with G1's element contracts, because the exit tilt it causes moves the rest of the ride. Do not build a "solve the train and the shape together" path: it is only possible in one pass for the front car, where it gave the same shape as the repetition to 1 mm, and the front car is the case that fails most.

**For the saved file:** the chosen car, the train it is computed with (cars, pitch, mass point) and the stopping rule all change the computed track, so they go in the main part of the file with a main version bump (rule 4, decision record 0014 point 15), not in F4's appendix; a project without them is the point design as today. And because the result comes from a repetition, the load must run the same repetition to get the same track: the rule (blending, tolerance, round limit) has to be fixed in the code and versioned with the file.

---

## 1. The question

R7 measured that the program's forces are right for one point only: on a 20 m train the end cars feel 0.2 to 0.5 g more than asked over a camelback crest, about 1 g more over a loop's top, and up to 2 g more over a lift crest. P2's second step would let the designer say "these are the forces the centre car (or the front, or the rear car) shall feel". The plan's questions for this trial: does the repetition that computes such a design settle, how fast, how far does it move the shapes, what do the chosen car and the others then feel, what does it cost per edit, what goes wrong, and how should P2 compute.

## 2. What "design again with the train's speed at the chosen car" means

The program's force step turns the track by (asked force minus gravity) divided by the speed, every millisecond, and advances it by the speed. "Design with the train's speed" replaces the point's speed in that step by the train's speed when the chosen car is at that place. Three choices remain, and the trial had to fix each in terms the program could implement.

**What the asked forces are a function of.**
- **Reading T (time):** the force program stays a function of time, and the time is now the chosen car's own clock (the design advances by the train's speed, so a design millisecond is the millisecond the chosen car takes there). This is what a time-argument force section would do if its speed were swapped, and it is R7's `redesign.py`. The element's duration is kept; its length changes.
- **Reading D (distance):** the force asked at each metre of track stays what the point design asked there. This is what a distance-argument force section would do. The element's length is kept; its duration changes.
- In both, the durations and changes of the force program are the point design's, so the element's goals (a crest height, a level exit, a loop that closes) are not kept. **Version B** (R7's) re-solves three durations of the camelback each round so its crest stays at 30 m, symmetric, with a level exit; that is what a designer with G1's element solver would get. It was tried on the camelback only.

**Where the speed comes from.**
- **From the last ride at each place (the repetition).** Round 0 is the point's design. Each round rides the last design with the train (`fvdcore ride`), takes the train's speed when the chosen car is at each place along the element, and designs again with it. P1's `trainRideSpeedByNode()` returns exactly this per node, so the program has the piece already. The repetition stops when the shape moves less than 2 cm between two rounds (at the same length along the track), or after 20 rounds. Every case was run two ways: **plain** (each design uses the last ride's speed as it is) and **the rule** (the first redesign uses the point design's ride as it is, every later one the mean of the new speed and the speed used last: blended half-and-half). The rule is the one recommended for P2 in section 6.
- **Solved together with the shape, in one pass.** Possible only for the **front car**: when it is at the head of the design, every other car stands on track already designed (or on the level track in front), so the train's energy, and its speed, can be computed in the same pass (`x1lib.TrainAhead`, the ride's rules in Python; on a fixed shape it reproduces `fvdcore ride`'s speed at the front car to 0.0003 m/s on all three point designs, `results/validate.txt`). For the centre or the rear car, the cars ahead stand on track not yet designed, so there is no single pass; it needs the repetition or a guess of the track ahead.

**Where the ride starts.** As `fvdcore ride` does: the train's centre at the track's start at the start speed, so while the front car is in the first 8.8 m the speed used is the start speed. On these shapes every element begins after a level run-in, so this matters only in the first metres.

**The measure of success** is what the chosen car feels on the final shape, measured by `fvdcore ride` on the written project, against what that design asked at each place: "felt minus asked", the largest difference over the element and where it is.

## 3. Method

### 3.1 The stand-in, and what it leaves out

The program has no input for "design with this speed", and the trial was not to change the program. So the design is made outside it and handed to it as a fixed shape:

- **The designer** is `x1lib.design()`, an extension of R7's `fvd2d.py`: the program's force-section step (`secforced::updateSection()`, time argument, 1 ms steps) in the vertical plane, in double precision, with the speed taken from the point's energy (the program's rule), from a speed table (the repetition), or from `TrainAhead` (one pass, front car).
- **The shape goes to the program** as a project with one NoLimits CSV section: the heartline every 0.1 m with its direction and lateral vectors, written as project text and turned into a `.fvd` by `fvdcore fromtext`. The lift crest keeps R7's held lift section in front of it. The program then rides that shape with `fvdcore ride --cars 9 --pitch 2.2 --json --table`; nothing a car "feels" in this note comes from the script's own arithmetic.
- **It was checked** (`validate.py`, `results/validate.txt`): the point design written this way rides like the program's own force-section project of the same element, within 0.0011 m/s at every sample (0.053 m/s on the lift crest, a time shift from the stand-in's first sample, see the list below) and 0.005 g in every car's lowest and highest force (points every 0.1 m; at 0.25 m spacing, up to 0.015 g). The program's point on the stand-in feels the asked force to within 0.015 g (camelback, at the start of the pull-out), 0.008 g (loop) and 0.004 g (lift crest); on the program's own projects 0.011, 0.007 and 0.004 g. That is the **measuring floor**: differences under about 0.02 g in the tables below are not differences.

| Shape | CSV points | Cars | Largest speed difference (m/s) | Largest force difference (g) |
|---|---|---|---|---|
| camelback | 0.1 m | 1 / 9 | 0.0001 / 0.0001 | 0.0001 / 0.0008 |
| loop | 0.1 m | 1 / 9 | 0.0009 / 0.0011 | 0.0001 / 0.0004 |
| lift crest | 0.1 m | 1 / 9 | 0.060 / 0.053 (time shift) | 0.0011 / 0.0041 |

What the stand-in leaves out:
- **Only the vertical plane.** No roll, no lateral force, no turning element; R7 found a level turn unaffected, but a turn that climbs would be affected like a hill.
- **One force section per element**, as R7 built them. The program's chaining of forces from section to section, other section kinds (geometric, curved, Bezier) and the roll smoothing were not exercised with a train speed.
- **The program's own force step was not run with a train's speed.** The stand-in copies that step; on the point design it reproduces it to 0.001 m/s and 0.01 g, so the copy is close, but the program might differ where the speed jumps (the lift's end).
- **The speed is handed over as a table** every 5 cm along the element, linearly interpolated.
- **The lift crest's project starts at an anchor speed of 30 m/s** instead of 3.5 m/s, only because a CSV section spaces its nodes by the anchor speed (3.5 mm nodes made each ride take 10 s); the lift holds 3.5 m/s from the first node, so only the first sample of the ride changes (the ride notes a "brake" at 0 m). That first sample at 30 m/s puts the train about 0.26 m ahead for the rest of the ride, which shows as up to 0.06 m/s of speed difference at the same time in the pull-out; by place the two agree, and the forces agree with the program's own project to 0.005 g.
- **One train**, R7's "long" one (9 cars 2.2 m apart, 19.8 m: R7's B&M mini-hyper stand-in), with `fvdcore ride`'s model (rigid train, mass at the program's energy point, the project's friction and drag, R7's chain rule). Car lengths are still assumptions (R1). A 10 m train would show about a quarter of these effects and a 30 m train about twice (R7 section 4.4); not run. The train's forces are `fvdcore ride` as it was on the trial's branch point (the tip of `claude/stage-3` when the branch was made).
- **Only the vertical force.** The forces asked, designed and measured are the vertical (normal) force alone; the along-track force (`gx`) was neither asked nor measured, and the speed the design uses comes from the ride's energy, not from its along-track force. So the known gap between the two along-track forms (issue 96) does not touch these results.

### 3.2 The shapes

R7's three elements (`docs/research/r7-prototype/projects/shapes.json`), heartline 1.1 m, friction 0.03, resistance 2e-5:

| Shape | What | Point design |
|---|---|---|
| camelback | 1.5 s level at 30 m/s, pull-up to 3.5 g, -0.2 g held 3.1 s over a 30 m crest, pull-out, 1.5 s level | crest speed 15.47 m/s, heartline radius 22.5 m |
| loop | 1 s level at 25 m/s, 4.2 g entry, 0.5 g at the top, 4.2 g exit, closes at 360 degrees | 20.0 m tall, 12.39 m/s at the top, radius 8.1 m |
| lift crest | 14 m of level lift held at 3.5 m/s, then a crest at 0.6 g, a 45 m drop, a 3.5 g pull-out, 2 s level | tightest radius 6.3 m; the drop passes -30 degrees at 5.42 m/s |

R7's lift crest is used rather than the NoLimits 2 kit's (`samples/nl2-session/e4-liftcrest-0.60.fvd`, the same drop behind a longer lift): `fvdcore ride` extends a held section backwards past the start, so the train has its run-in either way, and R7's file is the one the earlier numbers refer to.

## 4. Results

All tables are from `py -3 docs/research/x1-trials/iterate.py` (`results/iterate.txt`; every round of every case in `results/iterate-rounds.txt`). "Felt minus asked" is from `fvdcore ride` on the final project.

### 4.1 Does it settle, and in how many rounds

| Shape | Chosen car | Reading | Plain: result, rounds | Rule: result, rounds | Chosen car's felt minus asked: point design / final (rule) | Speed at the crest (m/s): the point's on the point design / the design speed in the last round (rule) |
|---|---|---|---|---|---|---|
| camelback | centre | T | settled, 5 | settled, 6 | -0.07 / +0.02 g | 15.47 / 15.69 |
| camelback | centre | D | settled, 6 | settled, 9 | -0.07 / +0.01 g | 15.47 / 15.55 |
| camelback | front | T | settled, 5 | settled, 8 | -0.60 / +0.01 g | 15.47 / 15.55 |
| camelback | front | D | settled, 5 | settled, 10 | -0.60 / +0.01 g | 15.47 / 16.46 |
| camelback | rear | T | settled, 9 | settled, 13 | -0.52 / +0.01 g | 15.47 / 16.57 |
| camelback | rear | D | swings, not settled after 20 | settled, 12 | -0.52 / +0.01 g | 15.47 / 14.46 |
| loop | centre | T | settled, 8 | settled, 9 | +0.31 / +0.01 g | 12.39 / 13.19 |
| loop | centre | D | settled, 8 | settled, 7 | +0.31 / +0.01 g | 12.39 / 13.36 |
| loop | front | T | **train stalls**, 2 | **train stalls**, 2 | +1.58 / - | 12.39 / - |
| loop | front | D | **train stalls**, 1 | **train stalls**, 1 | +1.58 / - | 12.39 / - |
| loop | rear | T | train stalls, 5 | settled, 13 | +1.70 / +0.01 g | 12.39 / 17.34 |
| loop | rear | D | train stalls, 4 | settled, 12 | +1.70 / +0.01 g | 12.39 / 18.17 |
| lift crest | centre | T | settled, 19 | settled, 10 | -0.46 / +0.00 g | 5.42 / 7.39 |
| lift crest | centre | D | settled, 11 | settled, 9 | -0.46 / +0.01 g | 5.42 / 7.36 |
| lift crest | front | T | **train stalls**, 1 | **train stalls**, 1 | -0.38 / - | 5.42 / - |
| lift crest | front | D | settled, 4 | settled, 10 | -0.38 / +0.01 g | 5.42 / 3.56 |
| lift crest | rear | T | train stalls, 2 | settled, 13 | -2.18 / +0.00 g | 5.42 / 13.84 |
| lift crest | rear | D | **train stalls**, 1 | **train stalls**, 1 | -2.18 / - | 5.42 / - |

"Rounds" counts the redesigns after the point design. Where both settled, the plain and the rule's final shapes agree to a few centimetres (`results/iterate.txt`, "How far the shape moves").

(The lift crest's "crest" here is where the drop passes -30 degrees, R7's mark.)

What it shows:
- **The centre settles in all six cases, plain or with the rule.** Plain, the shape halves its change about every round on the camelback and the loop, and the lift crest takes longest (19 rounds against time) because each round overshoots: the speed at the -30 degree mark swings 10.6, 5.9, 8.7, 6.7, 8.0 ... m/s before it settles at 7.4 (`iterate-rounds.txt`). The rule takes 6 to 10 rounds on every shape: one to three more than plain on the camelback, about the same on the loop, and 2 to 9 fewer on the lift crest.
- **The end cars swing.** Where the train's speed at the chosen car depends strongly on the shape further along (the rear car of the camelback against distance: crest speed 17.6, 11.9, 17.7, 12.7 ... m/s), plain repetition overshoots and either swings for 20 rounds or makes, in round 1 or 2, a shape the train cannot ride. The rule settled four of the eight cases that failed plain (14 of 18 settle with it, 10 of 18 plain), at the price of three to six more rounds in the end-car cases plain repetition already settled; the other four stall in round 1 or 2 either way, because the first redesign (from the point's speed) is the same either way and already asks too much.
- **The final felt-minus-asked is at the measuring floor in every case that settled**: what the design asks is what the chosen car feels, measured by the program's ride.

### 4.2 How far the shapes move

Last round against the point design. "Crest moved": along the track's direction / up. "Largest gap": the largest distance from the new shape to the old one, before the element's final level run-out (where it is, in metres into the element). Exit: pitch change and height change at the end of the element.

| Shape | Chosen car | Reading | Crest moved (m) | Largest gap (m) | Exit pitch (deg) | Exit height (m) |
|---|---|---|---|---|---|---|
| camelback | centre | T | +0.28 / +0.36 | 1.92 at 179 m | +2.0 | +3.26 |
| camelback | centre | D | +0.19 / +0.59 | 2.12 at 179 m | +3.2 | +4.31 |
| camelback | front | T | +4.84 / +2.13 | 9.25 at 173 m | +10.1 | +14.95 |
| camelback | front | D | +4.91 / +0.48 | 9.88 at 179 m | +21.7 | +23.16 |
| camelback | rear | T | -3.72 / 0.00 | 4.81 at 176 m | +7.5 | +9.22 |
| camelback | rear | D (rule) | -6.55 / +3.74 | 5.83 at 105 m | +1.3 | +3.58 |
| loop | centre | T | -1.51 / +0.35 | 2.00 at 81 m | +2.6 | +2.59 |
| loop | centre | D | -1.51 / +0.11 | 2.05 at 103 m | +5.7 | +4.21 |
| loop | rear | T (rule) | -4.16 / -5.16 | 32.6 at 120 m | -39.4 | -54.3 |
| loop | rear | D (rule) | -1.32 / -6.18 | 26.6 at 103 m | -59.4 | -40.0 |
| lift crest | centre | T | +3.05 / -0.85 | 4.30 at 88 m | +3.5 | +7.55 |
| lift crest | centre | D | +2.60 / -0.81 | 4.63 at 89 m | +5.7 | +10.27 |
| lift crest | front | D | -0.67 / +0.31 | 5.17 at 59 m | -1.4 | -5.66 |
| lift crest | rear | T (rule) | +13.22 / -4.24 | 11.79 at 56 m | +6.5 | +9.56 |

What it shows:
- **For the centre, the element itself moves by decimetres to a few metres**, as R7 found for the camelback (R7 section 5 version A: crest +0.3 m along, +0.36 m up, exit +2.0 degrees, +3.3 m; reproduced here by the program's ride to 0.05 m and 0.1 degree). The biggest move is where the element ends, because the same force program, at a slightly different speed, no longer turns the track back to level.
- **The exit tilt is the real cost.** 2 to 6 degrees for the centre, 7 to 22 degrees for the end cars on the camelback. Everything after the element follows it: on a ride, designing one element for the centre car would move the rest of the ride by metres. Version B removes it (next section); without an element contract or a re-solve, P2 would spoil every following element.
- **Against time or against distance** matters little for the centre (the crest moves the same; T tilts the exit a little less, 2.0 against 3.2 degrees on the camelback, because the element may change length). For the end cars the two differ by metres.
- **The loop designed for its rear car is a different element**: 5 to 6 m lower at the top, not closing by 39 to 59 degrees. The rear car's speed in the second half of the loop is 5 to 6 m/s above the point's (17.3 to 18.2 against 12.4 m/s at the top), so the asked 0.5 g at the top needs a far larger radius, which the force program, with its durations kept, cannot fit into a closed loop.
- **The lift crest designed for its rear car is a much gentler crest**: the -30 degree mark comes 13 m later and 4 m lower, because the rear car crosses it at 13.8 m/s instead of the point's 5.4. Every car then feels at least 0.6 g (next table): the back-row whip is designed out, and with it the back-row airtime riders like.

### 4.3 What each car feels

From `fvdcore ride` on the final shapes. Lowest and highest vertical force (g), seconds below -0.5 g (ejector), and the car's largest felt minus asked (g) over the element. On the point design "asked" is the point's force program; on a design for a car it is what that design asks.

| Shape | Design | Front car: lowest / highest / ejector s / felt - asked | Centre car | Rear car |
|---|---|---|---|---|
| camelback | point | -0.60 / 3.81 / 0.97 / -0.60 | -0.27 / 3.47 / 0 / -0.07 | -0.58 / 3.87 / 0.85 / -0.52 |
| camelback | for the centre, T | -0.56 / 3.85 / 0.75 / -0.57 | **-0.20 / 3.50 / 0 / +0.01** | -0.54 / 3.91 / 0.60 / -0.50 |
| camelback | for the centre, D | -0.56 / 3.86 / 0.79 / -0.58 | **-0.20 / 3.50 / 0 / +0.02** | -0.55 / 3.91 / 0.65 / -0.50 |
| camelback | for the front, T | **-0.20 / 3.50 / 0 / +0.02** | -0.54 / 4.42 / 0.50 / +0.93 | -1.01 / 4.99 / 0.99 / +1.51 |
| camelback | for the rear, T | -0.93 / 4.59 / 1.18 / +1.12 | -0.50 / 4.13 / 0.15 / +0.64 | **-0.20 / 3.50 / 0 / +0.01** |
| loop | point | 0.99 / 4.84 / 0 / +1.58 | 0.76 / 4.11 / 0 / +0.31 | 0.85 / 4.99 / 0 / +1.70 |
| loop | for the centre, T | 0.72 / 4.96 / 0 / +1.40 | **0.50 / 4.20 / 0 / +0.01** | 0.54 / 5.13 / 0 / +1.47 |
| loop | for the rear, T (rule) | 0.05 / 6.73 / 0 / +2.86 | 0.18 / 5.53 / 0 / +1.41 | **0.50 / 4.20 / 0 / +0.01** |
| lift crest | point | 0.61 / 3.23 / 0 / -0.38 | 0.17 / 3.46 / 0 / -0.46 | -1.55 / 3.64 / 0.26 / -2.18 |
| lift crest | for the centre, T | 0.62 / 3.27 / 0 / -0.36 | **0.60 / 3.50 / 0 / +0.00** | -0.18 / 3.68 / 0 / -0.79 |
| lift crest | for the front, D | **0.60 / 3.50 / 0 / +0.00** | -0.20 / 3.86 / 0 / -0.80 | -2.64 / 4.08 / 0.40 / -3.25 |
| lift crest | for the rear, T (rule) | 0.65 / 3.26 / 0 / -0.45 | 0.62 / 3.44 / 0 / +0.25 | **0.60 / 3.50 / 0 / +0.00** |

(The front car's felt minus asked on the lift crest is mostly in the pull-out, where it arrives before the train behind has dropped, and is slower than asked: 3.23 to 3.27 g against 3.5 g. All rows of all cases are in `results/iterate.txt`.)

What it shows:
- **The chosen car gets exactly what is asked; the others do not.** Designing for the centre leaves the end cars of the camelback at -0.54 to -0.56 g (-0.2 g asked) and the loop's ends at 4.96 to 5.13 g (4.2 g asked). The spread between the cars is the train's length against the element's radius (R7 section 4.4), and no choice of design speed removes it; it only chooses who gets the asked force.
- **Designing for an end car makes the other end worse**: the camelback for its front car gives the rear car -1.01 g and 4.99 g; the lift crest for its front car gives the rear car -2.64 g, against -1.55 g on the point design.
- **The lift crest for the centre** cuts the rear car's whip from -1.55 to -0.18 g because the crest gets wider for the centre car's higher speed.

### 4.4 Other ways to compute

| Shape | Method | Chosen car | Result | Felt minus asked | Note |
|---|---|---|---|---|---|
| camelback | shape and train solved together, one pass | front (T) | ridden | +0.02 g | the same shape as the repetition to 0.001 m |
| loop | the same | front (T) | ridden, but not a loop | -4.87 g | the front car reaches the top at 4.1 m/s; the 0.5 g asked there means a heartline radius of about 1 m, and the force program winds the track round 2,874 degrees in all |
| lift crest | the same | front (T) | train stalls | (+0.01 g until the stall) | the pull-out turns the track up into a climb the train cannot make |
| camelback | durations re-solved each round (R7 B) | centre (T) | settled, 4 rounds | +0.01 g | durations 0.472 / 3.176 / 0.395 s (point: 0.489 / 3.131 / 0.400); crest 0.41 m further on, at 30.00 m; exit level |
| camelback | the same | front (T) | settled, 5 rounds | +0.02 g | 0.399 / 3.201 / 0.329 s; crest 5.07 m further on, at 30.00 m; exit level |
| camelback | the same | rear (T) | settled, 6 rounds | +0.01 g | 0.490 / 3.376 / 0.443 s; crest 3.73 m earlier, at 30.00 m; exit level |

- **Solving shape and train together** is possible in one pass only for the front car, and where a design exists it gives the repetition's shape to a millimetre. Where none exists (the loop, the lift crest against time) it fails in the same way. It saves the rounds for the one car that fails most often; not worth a separate path.
- **Version B is the useful form**: the element keeps its goals (crest 30 m, level exit), so nothing after it moves, and it settled in 4 to 6 rounds for all three cars. The durations it finds for the centre car are R7's to the millisecond (R7: 0.472 / 3.176 / 0.395 s). It needs a solver around each round (here R7's Newton, about 10 to 20 designs per round): that is G1's element contract, so P2 and G1 belong together.

### 4.5 What goes wrong

1. **A design that asks the train to climb higher than it can (loop, front car).** At the loop's entry the front car is faster than the point (the train behind is still level), so the asked 4.2 g gives a larger radius and the loop climbs higher (29 m instead of 20 m after round 1). By the top the train has lost far more speed, and the next round's design stalls. Neither blending nor the one-pass solve finds a loop: with the point's force program there is no loop for the front car of a 19.8 m train at 25 m/s.
2. **A force program against time over-turns when the chosen car is slower (lift crest, front car).** The front car crosses the crest at the chain speed (the train behind is still held by the chain) and reaches the pull-out slower than the point; the 3.5 g pull-out, held for the point's duration, turns the track past level into a climb to 33 m above the top of the lift, and the train stalls 72.2 m along (`results/iterate-rounds.txt`; or `fvdcore ride work/liftcrest-front-T-plain-r1.fvd --cars 9 --pitch 2.2` on the file `iterate.py` writes). Against distance the same case settles in 4 rounds.
3. **A design that asks the rear of the train to do something the front cannot (lift crest, rear car, against distance).** In round 1 the rear car's speed at each place is taken from the point design, where it whips over the crest at 9 to 12 m/s (R7 section 4.3); designing the first metres of the drop for that speed makes a crest so wide that the drop does not reach -30 degrees until 134 m further on, and the train stalls on it. Against time, with the rule, the same car settles to a gentler crest. The trial did not find a way to tell in advance which reading works for which car.
4. **The swing (rear car, camelback, against distance).** The rear car's speed at the crest depends on where the front of the train is, which the last round's shape decides; plain repetition swings with a ratio of about 0.8 per round and had not settled after 20 rounds. With the rule it settled in 12.
5. **The exit tilts** (section 4.2) in every case without version B.
6. **A loop that tightens or opens?** The plan expected a loop where the chosen car's speed is lower than the point's to tighten. For the centre car the speed at the top is higher (13.2 against 12.4 m/s), so its loop's top moved 1.5 m earlier and 0.1 to 0.35 m higher, and its radius there grew; the speed is lower than the point's only near the entry and exit (R7: -0.36 m/s), where the shape barely moved. No case tightened enough to matter.

## 5. What it costs

One round, inside the program, would be one recompute of the track with the new speed table plus one ride of the train. `py -3 docs/research/x1-trials/timing.py` (`results/timing.txt`; run alone):

| Project | Nodes | Whole-track recompute (ms) | `fvdcore ride`, 9 cars (ms) | One round (ms) |
|---|---|---|---|---|
| `samples/baseline/gen-normal-shapes.fvd` | 14,125 | 13.6 | 7 | 21 |
| R7's camelback | 9,087 | 3.6 | 4 | 7 |

The recompute is the program's own time in `fvdcore session`. The ride's time is the wall time of `fvdcore ride` less that of `fvdcore nodes` on the same project, best of 15: a difference of a few milliseconds against about 120 ms of starting the program, so it is good to about 10 ms. R7's layout was left out: `fvdcore nodes` alone takes about 2 s on it, which hid the ride.

So, on a track of normal length (14,125 nodes): one round about 20 to 25 ms; with the recommended rule the centre car's 6 to 10 rounds take about 0.12 to 0.25 s, and the slowest case that settled (13 rounds) about 0.33 s. Plain repetition would take 0.1 to 0.5 s (5 to 19 rounds). On a 150,000-node ride, about ten times that. For comparison, E1 found a solve over 0.2 s should be a button. Version B adds a solve per round (E1: 5 to 10 evaluations per re-solve from the previous solution), so about ten times more again.

Two ways to cut it, not measured: recompute and ride only from the edited section on (the speed before an edit does not change, except that the train ahead of a car reaches back by half its length), and start each edit's repetition from the last settled speed table, which should leave 2 to 4 rounds for a small edit instead of 6 to 10.

(The trial's own wall time per round, about 0.2 s here, is mostly starting `fvdcore` twice and does not stand for the program's cost; `results/iterate-times.txt`.)

## 6. Recommendation for P2

| Way to compute | Cost per edit (14,000 nodes) | Risk |
|---|---|---|
| **Repetition around P1's ride, blended always, centre car by default** (recommended) | 6 to 10 rounds (centre) x 20 to 25 ms: 0.12 to 0.25 s; at most 13 rounds seen; fewer when started from the last speed table | does not settle or stalls for some end-car designs: needs a fallback and words for it |
| Repetition with version B (each element re-solved to its goals every round) | about ten times the above | needs G1's element contracts first; the only form that keeps the rest of the ride in place |
| Shape and train solved together in one pass | one pass (about one recompute plus a ride's worth of arithmetic) | possible only for the front car, the car that fails most; a separate code path for a rare case |
| Live, after every keystroke | as the first row, every time | too slow on a whole ride (section 5) |

1. **Compute by repetition** around P1's ride: after an edit has settled (or on a button for a long ride), recompute with the speed `trainRideSpeedByNode()` gives at the chosen car, ride, repeat. Blend the new speed half-and-half with the last one used from the second redesign on, always. Measured: it settled 14 of 18 cases against plain repetition's 10, it never needed more than 13 rounds, and for the centre car it took 6 to 10 rounds against plain's 5 to 19 (one to three more on the camelback, about the same on the loop, up to nine fewer on the lift crest). "Plain first, blend once a round overshoots" might save a few rounds on the camelback but was not tried, and it cannot catch the lift crest's rear car, whose plain repetition stalls in its second redesign before any overshoot is seen. Stop at 2 cm, at most 20 rounds.
2. **Fallback in words**: if the train stalls, or the rounds run out, keep the point design and say "no design for the front car on this element: the train cannot follow it" rather than leaving a half-way shape.
3. **Aim at the centre by default** (as R7 recommended): it always worked here, moves the element least, and is closest to what existing projects mean. Offer the front and rear car with a warning.
4. **Keep each section's own argument** (time sections against time, distance sections against distance). For the centre both worked equally well; no new concept for the designer.
5. **Build it with or after G1's element contracts.** Without a re-solve every element designed for a car tilts its exit and moves the rest of the ride. Version B is cheap in rounds (4 to 6) and solves that.
6. **Show the other cars** (A4's seat map, P1's ride): designing for one car never fixes the others, and the designer must see what the ends then feel.

## 7. What it means for the saved file

Designing for a car is the first saved value that changes the computed track (decision record 0014 point 15, rule 4 in `CLAUDE.md`), so it bumps the main file version and the 2016 program will refuse such a project. Everything the computation reads changes the track, so all of it goes in the main part, not F4's appendix: the choice (point, centre, front, rear, or a car number), the train it is computed with (cars, pitch, mass point, and the resistance it uses), and the rule of the repetition. A project without them loads as the point design, as today, so the owner's existing projects keep their shapes.

The repetition itself is the harder part: the saved track is whatever the repetition settles on. To reopen a project with the same shape, the load must run the same repetition with the same rule (blending, tolerance, round limit, start from the point design), so that rule belongs to the file version; changing it later changes old projects by up to the tolerance (2 cm here) or, where it settles differently, by more. The alternative, saving the settled speed table per node, makes the load exact and fast but adds a large block to the file (about 56 KB for 14,000 nodes) that goes stale the moment anything is edited. Recommendation: save the inputs and a fixed, versioned rule; recompute on load; say on load when it did not settle.

## 8. Program behaviour found

1. **The NoLimits CSV section misplaces its nodes on unevenly spaced points** (`secnlcsv::getNodeAtDistance()`). Its interpolation search for the point before a distance can stop one point too far when it closes from the right, and then returns that point (the interpolation factor is clamped at 0), so nodes jump ahead by up to one point spacing and back. The track's length comes out too long and its forces are noise. Shown on a plain straight by `validate.py`: a level straight of 2,001 points from 0 to 200.02 m, spacing shrinking from 0.12 to 0.08 m (`work/uneven-straight.csv`, written by the script), then `fvdcore import csv work/uneven-straight.csv work/uneven-straight.fvd` and `fvdcore nodes work/uneven-straight.fvd --every-m 20 --frame anchor`: the last node has `dist_m` 226.54 m at `heart_x` 200.02 m, 13.3 % too long. On the camelback written with unevenly spaced points the ride took 9.97 s instead of 9.09 s. NoLimits 2's own exports are evenly spaced, which is probably why it was not noticed; a CSV made by another tool, or by a later NoLimits version, would be wrong in the program. The trial works round it by writing evenly spaced points (`x1lib.csv_points`). Not fixed here.
2. **A CSV section's node count follows the anchor speed** (the node spacing is the anchor speed / 1000 m): behind a slow lift that is 3.5 mm nodes, 45,000 nodes for a 160 m drop, and `fvdcore fromtext` and `fvdcore ride` each took about 10 s on it. Not a defect, but a trap for anyone importing a long NoLimits track behind a slow start.
3. Nothing wrong was found in `fvdcore ride`: on a fixed shape its speed at the front car agreed with an independent Python copy of its rules (`TrainAhead`) to 0.0003 m/s on all three point designs (`validate.py`, `results/validate.txt`), and on the stand-in projects with its ride of the program's own projects to 0.001 m/s.

## 9. What was not tried, and could not be established

- **Other trains.** Only R7's 19.8 m train. The effects scale with (train length / radius) squared (R7 section 4.4), so a 10 m train would settle more easily and a 30 m train fail more often; not run.
- **Version B on the loop and the lift crest**, and with distance reading. Only the camelback was re-solved. Whether the loop for its front car exists once its durations are free (a lower, rounder loop) is open; with the point's durations it does not.
- **The program's own force step with a train speed**, other section kinds, roll and lateral force, turning elements, several sections per element (section 3.1).
- **Starting each edit from the last settled speed**, and a recompute from the edited section on: the costs in section 5 are for a full repetition from the point design.
- **A damping chosen per case**, or a better repetition (Newton on the speed table, or a relaxation that adapts): only "plain" and "blend half-and-half always" were tried, not "plain first, blend on overshoot".
- **Whether a designer wants this**: the owner has not tried a design for a car; whether he prefers the centre car's forces exact or the point's design as today is his choice (R7 recommended the centre).
- **NoLimits 2**: whether it rides a train like `fvdcore ride` is the owner's session (R12, `samples/nl2-session/`); every "what a car feels" here is the program's model.

## How to rerun

```
tools\build-windows.cmd
py -3 docs/research/x1-trials/validate.py     # under a minute
py -3 docs/research/x1-trials/iterate.py      # about 2 to 3 minutes
py -3 docs/research/x1-trials/timing.py       # under a minute; run alone
```

Each writes `docs/research/x1-trials/results/<name>.txt`. Two runs of `validate.py` and `iterate.py` gave identical result files and identical projects; `timing.txt` and `iterate-times.txt` are wall times and vary.
