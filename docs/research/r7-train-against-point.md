# R7: The train against the point

Experiment for FR-11 (simulate the train as a train), roadmap piece R7, Stage 2. Written 2026-11-11 from the code on `claude/stage-2`, a stand-alone Python prototype in `docs/research/r7-prototype/` and web sources (accessed 2026-11-11). Nothing in the program was changed. Builds on `r1-heartlines-and-trains.md` (car counts), `r8-friction-and-margins.md` (what friction and drag mean in the program) and `r6-parametric-elements.md` (the camelback and how an element is tuned).

## Summary

The program moves one point along the heartline and takes its speed from that point's height. A train is 10 to 30 m of mass: over a crest part of it still climbs while part already falls, so its speed, and every force that depends on the speed, differs from the point's. This experiment rode the program's own track shapes with one point and with trains of 10, 20 and 30 m, and compared.

**Headline numbers** (point = what FVD++ shows today; train = what a rider feels, and what NoLimits 2 would show for the exported track, assuming it simulates the whole train as described in FR-11):

| Element | Point | 10 m train (4 cars) front / middle / rear | 20 m train (9 cars) front / middle / rear | 30 m train (10 cars) front / middle / rear |
|---|---|---|---|---|
| Camelback, 30 m tall, entry 30 m/s: speed over the crest | 15.47 m/s | +0.12 m/s | +0.46 m/s | +1.00 m/s |
| Same: force at the crest (asked -0.2 g) | -0.20 g | -0.26 / -0.22 / -0.24 g | -0.45 / -0.27 / -0.40 g | -0.72 / -0.36 / -0.65 g |
| Same: seconds below 0 g (ejector, below -0.5 g) | 3.24 s (0) | 3.21 / 3.22 / 3.21 s (0) | 2.64 / 3.19 / 2.37 s (0.96 / 0 / 0.86) | 2.33 / 3.13 / 2.21 s (1.49 / 0 / 1.34) |
| Same: highest force in the valleys (asked 3.5 g) | 3.51 g | 3.66 / 3.56 / 3.70 g | 3.82 / 3.47 / 3.87 g | 3.90 / 3.49 / 3.94 g |
| Vertical loop, 20 m tall, entry 25 m/s: speed over the top | 12.39 m/s | +0.32 m/s | +1.20 m/s | +2.47 m/s |
| Same: force at the top (asked 0.5 g) | 0.50 g | 0.73 / 0.59 / 0.69 g | 1.44 / 0.80 / 1.36 g | 2.22 / 1.17 / 1.99 g |
| Same: highest force at entry and exit (asked 4.2 g) | 4.21 g | 4.53 / 4.33 / 4.65 g | 4.85 / 4.11 / 5.00 g | 5.02 / 4.08 / 5.12 g |
| Lift crest at 3.5 m/s, designed at 0.6 g: lowest force over the top | 0.60 g | 0.61 / 0.28 / -0.18 g | 0.61 / 0.17 / -1.55 g | 0.62 / -0.36 / -2.94 g |

(Middle for an even number of cars: the car just behind the centre, or the mean of the two centre cars where the force at one place is given; `results.txt` says which.)

1. **The speed changes little; the forces at the two ends of the train change a lot.** The train's speed over a crest is at most 0.5 m/s above the point's for a 20 m train on a hyper-style camelback, 1.2 m/s on a 20 m tall loop. But the front and the rear car pass the crest while the rest of the train is lower, so they go over it faster, and the force goes with the speed squared over the crest radius. A camelback asked for -0.2 g gives -0.45 g in the front car and -0.40 g in the rear car of a 20 m train, and nearly a second of ejector airtime that the program does not show. A loop asked for 0.5 g at the top gives 1.4 g at both ends. In the valleys the ends see 0.3 to 0.4 g more than asked.
2. **The middle car is close to the point.** It differs by 0.07 g on the camelback crest and 0.3 g on the loop top for a 20 m train, and its airtime seconds are within 0.05 s (0.11 s for a 30 m train). A design held at the middle car is, to within a few hundredths of a g on a camelback, the design the program makes today.
3. **The size of the effect grows with the square of train length over crest radius.** At the crest of the camelback (heartline radius 22.5 m) the front car's extra force is about 0.3 × (train length / radius)² g, the middle car's about one twelfth of (train length / radius)², which is the closed-form value for a train on a circular crest. Below a train length of about a quarter of the crest radius (about 5 m here) the effect is about 0.01 g. Small loops and slow lift crests have small radii and show the biggest effects; long, fast airtime hills the smallest.
4. **The worst case is the slow crest at the top of a lift.** The point at chain speed (3.5 m/s) shapes a tight crest (heartline radius 5 to 8 m); the front car crosses it at the chain speed, but the rear car of a 20 m train goes over it at 9 to 12 m/s, pulled by the cars already on the drop, and feels -1.5 g (crest designed at 0.6 g) to -3.9 g (designed at 0.3 g). That is the "back-row whip" coaster riders know; the program cannot show it at all.
5. **Designing with the train's speed changes the shape only a little** (question b, section 5). A camelback re-solved so that the middle car of a 20 m train feels exactly -0.2 g over a 30 m crest has its crest 0.4 m further along and its airtime stretch 1.6 m longer (52.4 against 50.7 m); for a 30 m train 0.9 m and 3.5 m. It gives the middle car the asked -0.20 g instead of -0.27 g, and leaves the front and rear cars at -0.54 and -0.53 g. No design speed removes the spread between the cars; it only chooses which car gets the asked force.

**Recommendation.**
- **The effect is large enough to matter, so P2 should move up**, as the roadmap's own condition says, to right after Stage 3. The program's forces are right for the middle of the train only; at the two ends of a full-length train they are off by 0.2 to 0.5 g over hyper-style crests, about 1 g over a small loop's top, and by 2 g or more over a lift crest. The airtime seconds on the ride sheet are wrong for the end cars in both directions: shorter and much stronger.
- **Of P2's two steps, the first matters far more**: ride the finished track with the whole train and report the force at every car (and with it A4, forces at every seat). It shows what riders and NoLimits 2 will see and changes no design. The second step, designing with the train's speed, moves a camelback by decimetres to a few metres; useful, but small next to the first.
- **The design should aim at the middle car** (the train's centre). It is closest to what the program does now, so existing projects keep their meaning, the change in shape is the smallest, and the ends then get the stronger airtime riders expect from the front and back rows. Aiming at the front car instead moves a 20 m train's camelback crest 4.9 m and gives the rear car -0.88 g where -0.2 g was asked.

Confidence: high for the numbers as numbers of this model on these shapes (the point model reproduces the program's speed to 0.0004 m/s; the train model reproduces the closed form exactly). Medium for how far they carry to real rides: car lengths are assumptions, cars are point masses rigidly coupled, and NoLimits 2's own train physics was not checked (section 9).

---

## 1. The question and the approach

FR-11: "the physics should follow a whole train, not one point". The program's speed rule (R8 section 1, checked again here in `core/secforced.cpp`): per kilogram, energy `E` loses `fResistance · v³ / 1000` each millisecond, and `v = sqrt(2 (E − g (h + fFriction · s)))`, with `h` the height of one point and `s` the length run. A train of N cars has N heights. Two questions, kept apart below:

- **(a) What a rider in each car feels on the track as designed.** The shape is fixed (it is what is exported); ride it with a train instead of a point. This is what NoLimits 2 would show for the exported track.
- **(b) What changes if the design itself used the train's speed.** In force-vector design the shape follows from the forces asked for and the speed; with the train's speed the same forces give a different shape.

The approach: make test tracks with the real program, dump every node with `fvdcore nodes`, and ride those fixed shapes in Python with the program's own point rule (validated against the dump) and with a train.

## 2. Method

### 2.1 The test tracks

All were computed by the real program: written as `.fvd` projects by a small Python writer of the documented format (`fvdwrite.py`, after `track::saveTrack()` and the save code of `secforced`, `func` and `subfunc`) and dumped with `fvdcore nodes <project> --every-m 0.1 --frame anchor`. The force programs were tuned beforehand with `fvd2d.py`, a two-dimensional copy of the program's force-section step that reproduces the program's result for the same force program to 0.1 mm in height on the camelback and 2 mm on the loop (`results.txt`). The projects are in `r7-prototype/projects/` and open in FVD++. Settings throughout: heartline 1.1 m, friction 0.03, resistance 2e-5 (the program's defaults).

| Track | What | Main numbers (point) |
|---|---|---|
| `camelback` | 1.5 s level approach at 30 m/s; pull-up 1 → 3.5 g; fall to -0.2 g; -0.2 g held 3.13 s; mirror; 1.5 s level run-out. Tuned (three durations) for a crest 30 m above the entry, a symmetric crest and a level exit. | Crest speed 15.47 m/s, heartline crest radius 22.5 m, 3.24 s below 0 g over 59 m of track, 219 m long. Within R6's typical numbers (section 1.1 there). |
| `loop` | 1.0 s approach at 25 m/s; 1 → 4.2 g; fall to 0.5 g at the top; mirror; tuned to close at 360°. | 20.0 m tall, 12.39 m/s and 0.5 g at the top, heartline radius 8.1 m at the top, 4.2 g at entry and exit. Pendrill's measured Invertigo loop (about 4.5 and 2.2 g, R6 section 1.1) is of this kind. |
| `layout` | A 3.5 m/s fixed-speed section (the lift, as a chain), a crest at 0.6 g, a 45 m drop to a 3.5 g valley, a 28 m floater camelback (-0.2 g), a 12 m ejector hill (-0.6 g), a level banked turn (about 70° of bank, 2.6 g, 250° of heading) and a run-out. The vertical part tuned in stages, the turn by hand. | 518 m, top speed 28 m/s, 4.91 s below 0 g. |
| `liftcrest-0.30`, `-0.60`, `-0.75` | The lift, crest and drop of the layout alone, with the crest designed at 0.3, 0.6 and 0.75 g for the point; each tuned for a 45 m drop and a level exit. | Tightest heartline radius over the crest 4.8, 6.3 and 8.2 m. |

### 2.2 The two models (`ride.py`)

**Point model on a fixed shape**, the program's rule: rail length `s` from the dump (`dist_m`, which is the centre line, the length `fTotalLength` the code charges friction on), height `h` of the point 0.9 of the way from the heartline to the rail (`vPosHeart(0.9 · fHeart)` in the code; computed from `heart_y` and `pos_y`), drag `fResistance · v² · ds` per step `ds` (the program's `v³ / 1000` per millisecond), and the speed from the energy as above. On a fixed-speed section the speed is held and the energy reset, as the code does.

**Felt forces** at a place, from a speed: the heartline's turning per metre toward the rider's head (`kN`) and sideways (`kL`), taken from the dumped direction vectors of the neighbouring points; normal force `−norm_y + kN · vh² / g`, lateral force `−lat_y − kL · vh · v / g`, with `vh` the heartline speed (rail speed times heartline length per metre of rail) and `v` the rail speed. This is the decomposition `secforced::updateSection()` uses, including its use of the rail speed for the lateral part.

**Train model:** N cars, each a point mass at its own place, car centres L apart along the centre line, the train's centre midway between the first and the last car. Rigidly coupled along the track, so all share one rail speed. Energy per kilogram of train: `v²/2 + g (mean of the cars' heights + fFriction · s)`, the same drag per kilogram. Each car's felt force is computed with the shared speed and the shape where that car is. The along-track force at a car is the train's acceleration over g plus the slope there: it is what the coupling makes a rider feel (positive: pressed into the seat back).

Modelling choices, all stated:
- All cars have the same mass; the train has no locomotive or empty cars.
- Each car's mass sits at the program's energy point (0.9 of the way from the heartline toward the rail; with a 1.1 m heartline that is 0.11 m above the rail centre), and the kinetic energy uses the rail speed, both as the program does for its point. A real car's centre of mass is higher (the riders); for the train-against-point comparison the choice cancels to first order because both models use the same point.
- No wheel, bogie or coupling compliance, no slack between cars, no pitching of a car body over its own length, no wheel-load-dependent friction (the program charges friction per metre whatever the load, R8 section 1.3; the train model does the same).
- On a chain (the layout's lift section) the train is held at the chain speed while its rear car is on the chain and gravity alone would make it slower; once gravity would make it faster it runs free, as a train runs ahead of the chain dogs over a lift crest. The point model holds the chain speed to the end of the section, as the program does.
- Off either end of the dumped track the track is continued in a straight line along its end direction, so the cars of a train starting or ending there have somewhere to be. Every test track starts and ends with a straight that is longer than half the longest train, except the layout's lift (a level 14 m crest section; the cars behind it stand on a level extension, which in a real ride would be the inclined lift).
- Both models start with the point (or the train's centre) at the start of the track at the project's start speed.

### 2.3 The trains

No manufacturer publishes car lengths (R1: "No car length was found for any train"). Counts come from R1 and `data/traintypes.json`; the lengths are **my assumptions**, chosen as car pitch (front of one car to the front of the next):

| Name | Cars × length | Train | Stands for |
|---|---|---|---|
| short | 4 × 2.5 m | 10 m | Intamin Blitz on Taron (4 cars of 2 rows, R1), Mack launch trains (4 to 5 cars) |
| long | 9 × 2.2 m | 19.8 m | B&M mini-hyper (9 cars of one row of four, R1); B&M hyper trains of 8 cars of 2 rows (R1) are of the same length if a car is about 2.5 m |
| extreme | 10 × 3.0 m | 30 m | a long wooden or mine-train consist; an upper bound |

The scaling section uses 2.5 m cars from 5 m to 40 m.

## 3. Validation

Before any train was ridden, the point model was run on each dumped shape with the project's own start speed and settings and compared with the program's own numbers in the same dump. From `results.txt`:

| Shape | Points | Worst speed difference | Worst normal-force difference | Worst lateral-force difference |
|---|---|---|---|---|
| camelback | every 0.1 m | 0.0003 m/s | 0.037 g | 0.000 g |
| camelback | every node (1 ms) | 0.0003 m/s | 0.012 g | 0.000 g |
| loop | every 0.1 m | 0.0004 m/s | 0.024 g | 0.000 g |
| loop | every node | 0.0004 m/s | 0.006 g | 0.000 g |
| layout | every 0.1 m | 0.0003 m/s | 0.043 g | 0.018 g |
| layout | every node | 0.0001 m/s | 0.023 g | 0.008 g |

- **Speed: reproduced to 0.0004 m/s** everywhere, which is the float rounding of the program. This confirms the energy rule, the energy point and the length friction is charged on.
- **Forces: within 0.04 g** at 0.1 m spacing, 0.02 g at full resolution. The largest differences are at the start of force transitions, where the program's force at a node uses the turn from the previous node only and the prototype a centred difference over its neighbours; the difference shrinks with the spacing as expected.
- **Nodes at exactly vertical were left out** (16 in the loop's every-node dump, within 0.5° of vertical). Only two of them actually differ, one at each vertical crossing of the loop, and there it is the program's printed force that is wrong: at the node where the pitch passes 90° it prints -2.73 g normal and 0.86 g lateral where its neighbours read 3.21 and 0.00, and on the way down 2.50 g and 11.04 g lateral where they read 3.48 and 0.00. The heading jumps by 180° there, so the yaw change between the two nodes is meaningless. This is outside R7; listed for the lead at the end.

## 4. Question (a): what each car feels on the track as designed

All numbers are from `results.txt`. "Speed difference" is the train's speed when its centre is at the place minus the point's speed there. A car's force "at the crest" is the force it feels when it passes the crest.

### 4.1 Camelback

| Train | Speed difference at the pull-up / crest / pull-out (m/s) | Largest anywhere |
|---|---|---|
| short (10 m) | -0.04 / +0.12 / -0.05 | +0.12 at the crest |
| long (19.8 m) | -0.17 / +0.46 / -0.19 | +0.46 at the crest |
| extreme (30 m) | -0.36 / +1.00 / -0.39 | +1.00 at the crest |

The train is faster than the point when centred on the crest (its ends hang down the two sides) and slower when centred in a pull-up (its ends are higher). At the exit, all are back to the same speed.

| Train | Car | Force at the pull-up (point 3.50 g) | At the crest (point -0.20 g) | At the pull-out (point 3.51 g) | Lowest | Highest | Below 0 g | Floater (+0.1 to -0.5 g) | Ejector (below -0.5 g) |
|---|---|---|---|---|---|---|---|---|---|
| point | | 3.50 | -0.20 | 3.51 | -0.20 | 3.51 | 3.24 s | 3.26 s | 0 |
| short | front | 3.65 | -0.26 | 3.25 | -0.35 | 3.66 | 3.21 s | 3.23 s | 0 |
| short | middle | 3.49 | -0.22 | 3.49 | -0.25 | 3.56 | 3.22 s | 3.25 s | 0 |
| short | rear | 3.30 | -0.24 | 3.69 | -0.35 | 3.70 | 3.21 s | 3.24 s | 0 |
| long | front | 3.79 | -0.45 | 2.92 | -0.60 | 3.82 | 2.64 s | 2.18 s | 0.96 s |
| long | middle | 3.47 | -0.27 | 3.46 | -0.27 | 3.47 | 3.19 s | 3.22 s | 0 |
| long | rear | 3.01 | -0.40 | 3.85 | -0.58 | 3.87 | 2.37 s | 2.31 s | 0.86 s |
| extreme | front | 3.86 | -0.72 | 2.66 | -0.88 | 3.90 | 2.33 s | 1.53 s | 1.49 s |
| extreme | middle | 3.43 | -0.36 | 3.41 | -0.37 | 3.49 | 3.13 s | 3.16 s | 0 |
| extreme | rear | 2.75 | -0.65 | 3.91 | -0.84 | 3.94 | 2.21 s | 1.70 s | 1.34 s |

What it shows:
- **The front car** enters the pull-up while the train behind it is still on the level, so it is fast there (3.8 g) and slow in the pull-out (2.9 g). Over the crest it is fast (the train behind is below it) and gets its strongest airtime early in the crest; the **rear car** the mirror image, strongest at the end of the crest and in the pull-out. Pendrill describes the same pattern for the Helix's airtime hill (section 6).
- **Airtime seconds go down while airtime strength goes up** at the ends: a 20 m train's front car is below 0 g for 2.64 s instead of 3.24 s, but almost a second of that is ejector airtime the point never reaches.
- **Along the track**, the coupling pushes and pulls: over the crest the front car of a 20 m train feels -0.38 g (thrown forward into the restraint as the train holds it back) and the rear car +0.32 g (pushed into the seat back); in the pull-out the rear car feels -0.35 g. The point feels only friction and drag, -0.03 g. (`results.txt`, "Along-track force".)

Plots: `r7-prototype/plots/camelback-speed.svg`, `camelback-normal-long.svg`.

### 4.2 Vertical loop

| Train | Speed difference at entry / top / exit (m/s) |
|---|---|
| short | -0.09 / +0.32 / -0.11 |
| long | -0.36 / +1.20 / -0.42 |
| extreme | -0.77 / +2.47 / -0.80 |

| Train | Car | At entry (point 4.20 g) | At the top (point 0.50 g) | At exit (point 4.21 g) | Lowest | Highest |
|---|---|---|---|---|---|---|
| short | front | 4.43 | 0.73 | 3.60 | 0.53 | 4.53 |
| short | middle | 4.17 | 0.59 | 4.15 | 0.51 | 4.33 |
| short | rear | 3.83 | 0.69 | 4.59 | 0.48 | 4.65 |
| long | front | 4.56 | 1.44 | 2.93 | 0.99 | 4.85 |
| long | middle | 4.09 | 0.80 | 4.06 | 0.76 | 4.11 |
| long | rear | 3.24 | 1.36 | 4.80 | 0.85 | 5.00 |
| extreme | front | 4.61 | 2.22 | 2.77 | 1.00 | 5.02 |
| extreme | middle | 3.97 | 1.17 | 3.92 | 1.00 | 4.08 |
| extreme | rear | 2.80 | 1.99 | 4.84 | 1.00 | 5.12 |

The loop's top radius (8.1 m on the heartline) is small next to a 20 m train, so the effect is about three times the camelback's: a top asked at 0.5 g becomes 1.4 g at the ends of a 20 m train, the entry 4.85 g at the front and the exit 5.0 g at the rear. For a 30 m train the top half of the loop (27.8 m) is shorter than the train. None of the cars has airtime; the point design's 0.5 g top is the lowest any car feels. Along the track the front car of a 20 m train is thrown forward with -0.68 g over the top.

### 4.3 Lift crest, first drop and the layout

The lift crest is where the point model is furthest from the train, because the point crosses it at chain speed and the train does not.

| Crest designed at (for the point) | Tightest heartline radius | Train | Train speed when its centre passes -30° pitch (point) | Lowest force: front / middle / rear | Ejector seconds, rear car |
|---|---|---|---|---|---|
| 0.30 g | 4.8 m | short / long / extreme | 5.61 / 6.91 / 8.10 m/s (4.68) | 0.33 / -0.33 / -1.23; 0.36 / -0.55 / -3.89; 0.38 / -1.58 / -6.53 g | 0.31 / 0.42 / 0.45 s |
| 0.60 g | 6.3 m | short / long / extreme | 5.91 / 6.83 / 7.78 m/s (5.49) | 0.61 / 0.28 / -0.18; 0.61 / 0.17 / -1.55; 0.62 / -0.36 / -2.94 g | 0 / 0.26 / 0.32 s |
| 0.75 g | 8.2 m | short / long / extreme | 7.39 / 7.71 / 8.29 m/s (7.38) | 0.75 / 0.58 / 0.34; 0.75 / 0.52 / -0.41; 0.76 / 0.24 / -1.18 g | 0 / 0 / 0.19 s |

- The front car behaves like the point: it crosses the tightest part of the crest at the chain speed, 3.5 m/s, because the train behind it is still held by the chain. The rear car is pulled over the crest by the cars already falling, at 9.1 to 11.6 m/s (20 m train) or 11.4 to 14.5 m/s (30 m train), on a curve shaped for the point's 3.6 to 3.9 m/s there (`results.txt`, column "speed when this car passes the tightest point").
- A crest designed at 0.3 g for the point is not a sensible real design (4.8 m radius); it is here to show how fast the rear-car force grows when the crest is tightened. A gentler crest (0.75 g) still gives the rear car of a 20 m train -0.41 g and of a 30 m train -1.18 g, which is the familiar strong back-row airtime over a first drop.

**The layout** (crest at 0.6 g), per element, seconds below 0 g:

| Element | Point | short front / middle / rear | long front / middle / rear | extreme front / middle / rear |
|---|---|---|---|---|
| first drop | 0.00 | 0.00 / 0.00 / 0.20 | 0.00 / 0.00 / 0.41 | 0.00 / 0.26 / 0.45 |
| floater camelback (28 m, -0.2 g, crest 15.5 m/s) | 3.21 | 3.18 / 3.20 / 3.19 | 2.40 / 3.18 / 2.27 | 2.26 / 3.14 / 2.17 |
| ejector hill (12 m, -0.6 g, crest 21.4 m/s) | 1.70 | 1.69 / 1.70 / 1.70 | 1.65 / 1.69 / 1.67 | 1.62 / 1.68 / 1.64 |

Whole layout, 20 m train: below 0 g 4.06 / 4.87 / 4.35 s (front / middle / rear) against 4.91 s for the point; ejector 2.21 / 1.59 / 2.29 s against 1.60 s; lowest force -0.84 / -0.62 / -1.55 g against -0.60 g. The ejector hill changes little (its crest is fast and wide: -0.72 g at the front of a 20 m train against -0.60 g asked). **The banked turn does not change at all** (within 0.03 g for every car): it is level, so the train's height, and its speed, do not depend on where the cars are.

### 4.4 How the difference scales

Train lengths from 5 to 40 m in 2.5 m cars, at the top of each element. "Circle" is the closed form for a train on a circular crest of the same radius (section 6).

Camelback (heartline radius 22.5 m at the crest; the point is under +0.1 g over 59.4 m of track):

| Train length | / airtime stretch | / crest radius | Speed difference at the top | Front minus point | Middle minus point | Rear minus point | Middle, circle |
|---|---|---|---|---|---|---|---|
| 5 m | 0.08 | 0.22 | +0.02 m/s | -0.011 g | -0.007 g | -0.004 g | -0.003 g |
| 10 m | 0.17 | 0.44 | +0.12 | -0.059 | -0.021 | -0.039 | -0.015 |
| 15 m | 0.25 | 0.67 | +0.26 | -0.141 | -0.045 | -0.108 | -0.036 |
| 20 m | 0.34 | 0.89 | +0.47 | -0.250 | -0.077 | -0.204 | -0.064 |
| 25 m | 0.42 | 1.11 | +0.71 | -0.380 | -0.116 | -0.323 | -0.100 |
| 30 m | 0.51 | 1.33 | +1.00 | -0.528 | -0.163 | -0.460 | -0.144 |
| 40 m | 0.67 | 1.78 | +1.67 | -0.861 | -0.275 | -0.763 | -0.252 |

Loop (heartline radius 8.1 m at the top; the upside-down half is 27.8 m of track):

| Train length | / top half | / top radius | Speed difference at the top | Front minus point | Middle minus point | Rear minus point | Middle, circle |
|---|---|---|---|---|---|---|---|
| 5 m | 0.18 | 0.62 | +0.07 m/s | +0.039 g | +0.031 g | +0.024 g | +0.024 g |
| 10 m | 0.36 | 1.23 | +0.32 | +0.229 | +0.092 | +0.188 | +0.117 |
| 15 m | 0.54 | 1.85 | +0.71 | +0.544 | +0.190 | +0.489 | +0.267 |
| 20 m | 0.72 | 2.47 | +1.22 | +0.939 | +0.322 | +0.860 | +0.466 |
| 25 m | 0.90 | 3.09 | +1.82 | +1.357 | +0.484 | +1.219 | +0.701 |
| 30 m | 1.08 | 3.70 | +2.48 | +1.752 | +0.669 | +1.520 | +0.960 |
| 40 m | 1.44 | 4.94 | +3.80 | +2.385 | +1.066 | +1.915 | +1.496 |

Reading the tables:
- The **middle car** follows the circle formula: its force changes by about (train length / radius)² / 12 g. On the camelback the formula is within 0.025 g at every length; on the loop the real middle car sees less than the circle predicts, because the loop's radius grows away from the top, so the cars on its sides are less low than on a circle.
- The **end cars** change about three times as much as the middle: on this camelback about 0.3 × (length / radius)² g at every length tried, on this loop about 0.15 × (length / radius)² g up to a train of about three top radii (beyond that it grows more slowly). Both are fits to these two shapes, not laws.
- A useful single number is therefore **train length over crest radius**. At about 0.25 the difference is about 0.01 g; at 0.5 it is 0.03 to 0.07 g at the ends; at 1 it is 0.2 to 0.3 g.
- Train length over the element's length (the airtime stretch, or the loop's top half) orders the cases less well, because two elements of the same length can have very different radii.

## 5. Question (b): what changes if the design used the train's speed

A force-vector design turns the track by (asked force minus gravity) divided by the speed. Using the train's speed (when the chosen car is at that place) instead of the point's gives another shape. `redesign.py` finds it by repetition: design with the point, ride the train over the result, design again with that train's speed, ride again, until the crest moves by less than 2 cm (3 to 10 rounds). Two versions on the camelback (point design: crest 30.00 m above the entry, 62.1 m horizontally after the start of the pull-up, airtime stretch where the design force is below 0.1 g 50.7 m wide, crest speed 15.47 m/s):

**A. Same forces and durations as the point design** (as the program would do if the speed rule were switched under an existing project):

| Train | Design holds the force at | Crest height | Crest distance from the pull-up | Airtime stretch | Exit pitch | Exit height change |
|---|---|---|---|---|---|---|
| long | middle | 30.36 m (+0.36) | 62.4 m (+0.3) | 51.0 m (+0.3) | +2.0° | +3.3 m |
| long | front | 32.13 m (+2.13) | 66.8 m (+4.8) | 49.7 m (-1.1) | +10.6° | +15.6 m |
| long | rear | 29.99 m (-0.01) | 58.5 m (-3.5) | 51.9 m (+1.2) | +7.3° | +9.1 m |
| extreme | middle | 30.80 m (+0.80) | 62.7 m (+0.6) | 51.3 m (+0.6) | +4.9° | +7.3 m |
| extreme | front | 33.49 m (+3.49) | 69.5 m (+7.5) | 51.0 m (+0.2) | +30.9° | +31.2 m |
| extreme | rear | 31.35 m (+1.35) | 56.3 m (-5.8) | 49.7 m (-1.1) | +25.9° | +23.3 m |

The hill itself moves by decimetres for a middle-car design, but the exit is no longer level (2 to 5°), so everything after it would move: the knock-on problem of R5. Switching the speed rule under an existing project changes it; this is why the roadmap keeps the setting off for existing projects.

**B. Durations re-solved for the same goals** (crest 30 m, symmetric crest, level exit; the fall to the crest force, the crest hold and the hold after the drop are the unknowns):

| Train | Design holds the force at | Durations (s) | Crest distance from the pull-up | Airtime stretch | Crest speed of that car | Front / middle / rear lowest force and seconds below 0 g |
|---|---|---|---|---|---|---|
| (point design) | | 0.489 / 3.131 / 0.400 | 62.1 m | 50.7 m | 15.47 m/s | long: -0.60 g 2.64 s / -0.27 g 3.19 s / -0.58 g 2.37 s |
| long | middle | 0.472 / 3.176 / 0.395 | 62.5 m (+0.4) | 52.4 m (+1.6) | 15.90 m/s | -0.54 g 2.54 s / -0.20 g 3.28 s / -0.53 g 2.29 s |
| long | front | 0.400 / 3.199 / 0.331 | 67.0 m (+4.9) | 54.2 m (+3.5) | 16.70 m/s | -0.20 g 3.29 s / -0.48 g 2.11 s / -0.88 g 1.61 s |
| extreme | middle | 0.452 / 3.227 / 0.389 | 63.0 m (+0.9) | 54.2 m (+3.5) | 16.37 m/s | -0.72 g 2.29 s / -0.25 g 3.33 s / -0.69 g 2.17 s |
| extreme | front | 0.353 / 3.278 / 0.275 | 69.5 m (+7.4) | 57.9 m (+7.2) | 17.75 m/s | -0.20 g 3.37 s / -0.63 g 1.52 s / -1.09 g 1.56 s |

(For the 30 m train the "middle" design holds the force at the train's centre, between cars 5 and 6; the middle column of the last row is car 6, 1.5 m behind it, hence -0.25 g.)

What it means:
- **Holding the middle car changes a camelback by little**: the crest moves 0.4 m (20 m train) to 0.9 m (30 m train) and the airtime stretch grows 1.6 to 3.5 m, about 3 to 7 % of its width. That is the size of an ordinary design adjustment, and it buys the asked force at the middle car (-0.20 g instead of -0.27 g).
- **Holding the front car is a much bigger change** (crest 5 to 7 m further on, stretch 3.5 to 7 m wider) and makes the rear car worse (-0.88 g, -1.09 g).
- **No choice removes the spread.** The difference between the ends and the middle stays 0.3 to 0.5 g however the hill is shaped, because it comes from the train's length against the crest's radius, not from the design speed.
- **How sure:** high that the change is small for a middle-car design on a camelback like this one (the repetition converged; the force step it uses reproduces the program to 0.3 mm). Not established for tighter elements (a loop or a lift crest would change more, by the scaling of section 4.4) or for the time-against-distance argument of a section; only the camelback was redesigned.

## 6. Checks against independent results

- **Closed form, uniform train on a circular crest** (my derivation, standard energy mechanics): with cars at angles φᵢ = offsetᵢ / R from the top, the train's centre of mass is R (1 − mean cos φᵢ) below the top when the train is centred, so v²_train = v²_point + 2 g R (1 − mean cos φᵢ), and a car at φ feels cos φ − v² / (g R). For a continuous train of length ℓ this is R (1 − sin α / α) with α = ℓ / (2R), about ℓ² / (24 R), so the middle car's force changes by about ℓ² / (12 R²). The prototype on a 20 m radius crest without friction reproduces both the speed and the force to four decimals for 10, 20 and 30 m trains (`results.txt`: e.g. 30 m train, 12.2649 m/s and 0.2302 g from both). This checks the train code; the camelback's middle car follows the same formula to 0.025 g (section 4.4).
- **Kaschke and Cartarius, "Roller coaster dynamics: from point particles to a continuum model using Lagrange density"**, arXiv 2603.27230, March 2026 ([arXiv](https://arxiv.org/html/2603.27230); read through a page summary, confidence medium). A physics-education paper that goes from a point to a fixed-length train, to cars joined by springs, to a continuous elastic train, on a hill-shaped track. As summarised, its fixed-length train gives the outer cars a higher speed at the hilltop and so stronger airtime than the middle car, the same as here. With springs between cars it finds the rear car the most intense. No numbers in it were compared with mine.
- **Pendrill, "Student investigations of forces in a roller coaster loop"**, European Journal of Physics, 2013 ([PDF](https://physics.gu.se/LISEBERG/eng/pendrill_loop_2013.pdf); I could not read the PDF itself, only a search summary; confidence low to medium). According to the summary, it treats the loop with the train's centre of mass, finds the speed lowest when the middle of the train is at the top, says the difference between front, middle and back depends on train length over loop radius, and gives the extra tangential force at the ends in a form built on sin α / α, the same quantity as the closed form above. That is the same scaling variable as section 4.4.
- **Pendrill in Physics World** ([article](https://physicsworld.com/a/twists-turns-thrills-and-spills-the-physics-of-rollercoasters/), seen as a search summary; confidence medium) and her Helix note ([first_drop.pdf](https://tivoli.fysik.org/fileadmin/tivolifysik/Liseberg/helix/first_drop.pdf), search summary; low): front and back riders feel more weightlessness because both ends go over a hill faster; on the Helix's 12.5 m train the front row gets its strongest negative force early on the airtime hill and the back row late. Both agree with section 4.1. The same summary has her saying the middle sees the highest forces in valleys; that holds here only in a valley the whole train passes through symmetrically (the camelback's middle car sees less than the ends in its pull-up and pull-out, because the end cars meet them with the train elsewhere). Not resolved.
- **"The physics of roller coasters from a designer's perspective"** (ResearchGate, 2022; [page](https://www.researchgate.net/publication/362133320_The_Physics_of_Roller_Coasters_from_a_Designer's_Perspective); search summary only; low): faster front and rear cars over a hill give stronger negative force, more so for longer trains. Agrees qualitatively.

No published comparison with numbers for a real ride's front, middle and rear car against a point model was found.

## 7. What it means for the program

**For P2 (the train as a train).**
- Move it up to right after Stage 3, as the roadmap allows if R7 shows a difference that matters. It does: 0.2 to 0.5 g at the ends over hyper-style crests with real train lengths, about 1 g on a 20 m loop's top, and more than 1 g over a lift crest.
- Step one (ride the finished shape with the train, report every car) is the valuable step. Step two (design with the train's speed) changes shapes by decimetres for a middle-car design; build it after step one, aimed at the train's centre, and keep it off for existing projects (version A of section 5 shows that switching it under a project tilts every exit by a few degrees).
- The train needs car count and car length (pitch); the results are sensitive to both, through the train length. The owner will have to enter them (R1); the screen should say so.
- The chain matters: a train on a lift is not a point on a lift. P2 needs to know which sections are lifts and hold the train while its rear car is on one.

**For A4 (forces at every seat).** Most of the spread between seats comes from the car's place in the train, not from the seat's place in the car: 0.3 to 1 g between the front and rear car here. A4 built at the heartline of one point would miss it; A4 should be built on P2's first step, with the seat offsets inside a car added on top.

**For the ride sheet (A5) and its airtime numbers.** The point's airtime is the middle car's to within 0.05 s for a 20 m train, but the end cars of a 20 m train have about 0.6 to 0.9 s less time below 0 g on a camelback and up to a second of ejector airtime that the point does not show. Until P2, the sheet should say that its airtime is for the middle of the train. After P2 it should show the range over the cars (for example "below 0 g 2.4 to 3.2 s; ejector 0 to 1.0 s").

**For A6 (energy margin)** (R8 section 4.3, point 5): over a crest a train's speed is higher than the point's by the amounts in section 4 (0.1 to 1 m/s for a 10 to 30 m train on the camelback), so a point that just clears a crest leaves a train with some reserve when centred; but the train is slower than the point when its centre is in a dip. A stall check run with the point is on the safe side at crests and wrong in the other direction where the train straddles a valley.

## 8. What carries over, and what must be re-run in the program

**Carries over unchanged.** The shapes were computed by the program and the point model reproduces its speed to 0.0004 m/s and its forces to 0.04 g, so the numbers in section 4 are what a train model built into the program would give on these projects, under the same modelling choices. The projects in `r7-prototype/projects/` and the tables in `results.txt` can be P2's first tests: with these settings and trains, P2's step one should reproduce section 4's numbers to a few hundredths.

**Must be decided or re-run in the program.**
- **Where a car's mass sits.** Here, as in the program, at the point 0.1 × heartline above the rail (section 2.2; R8 section 1.3 reads it as a stand-in for the riders' centre of mass, but the code puts it 0.9 of the way from the heartline *toward the rail*). A real centre of mass is higher. Because it is the same for point and train it hardly changes the comparison, but P2 should choose deliberately (R8 section 4.4).
- **Car spacing along the centre line or the heartline.** Here along the centre line (the rail), which is how wheels are coupled. On a 1.1 m heartline the two differ by a few percent over tight curves.
- **Rigid coupling.** Real couplings are stiff but cars pitch on their wheels; the arXiv paper's spring model moves the extremes towards the rear car. A rigid train is the right first step; whether NoLimits 2 couples rigidly was not established.
- **Several rows per car.** A car of two rows has seats about a metre apart; that is A4's job.
- **Time against distance sections, roll and lateral force.** Only level turns were tried (no effect); a turn that climbs or falls while it turns would show the same height effect as a hill.
- **The redesign (b)** was done in a two-dimensional copy of the force step; in the program it must handle the force chaining between sections and the roll.

## 9. Could not establish

- **Car lengths and pitches** of any real train. None is published (R1); the lengths here are assumptions.
- **How NoLimits 2 simulates a train**: whether its cars are rigidly coupled, where it puts each car's mass, how it handles the chain at a lift crest. I did not run NL2. "What NoLimits 2 would show" in this note assumes it behaves like the train model.
- **A measured front/middle/rear comparison** on a real ride with numbers, and Pendrill's loop paper in full (only a search summary of it was available).
- **Whether the scaling factors** (0.3 and 0.15 of (train length / radius)² for the end cars) hold for other shapes; they were fitted to one camelback and one loop.
- **The effect on tighter elements of question (b)**: only the camelback was redesigned.
- **The program's force at a node that is exactly vertical** (section 3) was found, not investigated: whether the wrong value reaches anything but the printed and plotted force (for example the force a following section starts from) was not checked.

## Sources (accessed 2026-11-11)

- M. Kaschke, H. Cartarius, "Roller coaster dynamics: from point particles to a continuum model using Lagrange density", arXiv 2603.27230 (2026): https://arxiv.org/html/2603.27230
- A.-M. Pendrill, "Student investigations of forces in a roller coaster loop", European Journal of Physics (2013): https://physics.gu.se/LISEBERG/eng/pendrill_loop_2013.pdf
- Physics World, "Twists, turns, thrills and spills: the physics of rollercoasters": https://physicsworld.com/a/twists-turns-thrills-and-spills-the-physics-of-rollercoasters/
- A.-M. Pendrill, Helix first drop note: https://tivoli.fysik.org/fileadmin/tivolifysik/Liseberg/helix/first_drop.pdf
- "The Physics of Roller Coasters from a Designer's Perspective" (2022): https://www.researchgate.net/publication/362133320_The_Physics_of_Roller_Coasters_from_a_Designer's_Perspective
