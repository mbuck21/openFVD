# R6: Parametric elements (FR-10): research, a model, a first experiment

Research for issue 28 (FR-10, parametric elements such as a camelback hill), Stage 1. Written 2026-11-10 from the code on `claude/stage-1`, web sources (all accessed 2026-11-10) and a stand-alone Python prototype in `docs/research/r6-prototype/`. Nothing in the program was changed. Builds on `r5-fvd-workflow.md` (the knock-on effect, the goal-seeking solver, pins and slack pieces) and `r3-force-limits.md`.

## Summary

- **What an element is.** A named group of ordinary force sections, plus parameters the designer sets, unknowns the program solves, goals the result must meet, and checks. It expands into sections the program already has, so the physics and the exporters do not change. The architect's description holds; the next three points are where it needs correcting.
- **Finding 1: counting is not enough, the goals must be independent.** "Exit level at the entry height" is *one* goal, not two, for a hill whose drop mirrors its climb: without friction the second follows from the first, and with friction it can never be met (the exit lands 0.9 to 1.5 m low in the prototype at 30 m/s). The element needs a third unknown that breaks the mirror. The designer must be told which goal pairs work; a dimension count alone, as CAD sketch solvers do it, would call this case well-posed.
- **Finding 2: a fixed exit height fixes the exit speed only if the energy at the element's entry did not change.** True for an upstream reshaping (entry 1 m lower, same energy): the re-solved element's exit speed came back to within 0.004 m/s and the layout's end moved 0.1 m in height. False for a taller lift or a faster launch: with 3 % more entry speed the element exits level at the same height but 3 % faster, and the next, unsolved element ends 1.5 m high and 0.7° off. An element can promise its exit height or its exit speed, not both. Re-solved elements also change length, so the rest of the ride slides along the track (up to 24 m in the test); only a pin with a slack piece (R5) holds positions.
- **Finding 3: solving is easy once the solver starts from a real hill; finding that start is the hard part.** Damped Newton solved the camelback in 1 to 18 track runs (milliseconds each in C++) whenever its starting numbers gave a valid hill. Naive starting numbers often give a loop instead (4 of 6 in one test, most cells of the map), and from there Newton fails; a short scan for a valid start fixes it. Outside the reachable range there is simply no hill (too slow, crest too high, *or* crest too low for the chosen pull-up), and the designer must see "cannot reach, reachable is ...". "Exit level" must be counted in unwrapped angle, or loops count as level exits.
- **Recommendation.** Build elements as data-driven groups on top of the R5 goal-seeking solver (W2), one element first: the camelback, valley to valley, with three goals (exit level, crest height, exit height) and three unknowns. Elements stay live and re-solve on upstream change by default, with "convert to plain sections" one way. Store the plain sections as today plus an optional element record after the end-of-project mark, which the 2016 program skips (tested on today's loader).
- **The owner's choices:** which numbers define the first elements, whether elements hold position (by default: no, only their exit pose), the file approach (recommended: compatible extension, no version bump; this needs a decision record because it reads rule 4 narrowly), and which elements come first (recommended: camelback, then zero-g roll, then overbank turn).

---

## Part 1. Research

Confidence: **high** = two independent sources or a primary source read in full; **medium** = one good source, or a source seen only as a search summary but consistent with others; **low** = forum impression or a single weak source. "Opened" means the page itself was read; "summary" means only a search summary was seen. Labels: *manufacturer claim*, *measured*, *physics*, *rule of thumb* (community). All links accessed 2026-11-10.

### 1.1 How elements are defined in practice

**Little is published by the manufacturers.** No B&M, Intamin, RMC, Vekoma or Mack document describing how an element is parametrised was found (R5 reached the same conclusion for layout work). What is public is physics, a handful of accelerometer measurements by physics educators, and forum rules of thumb. Everything below is labelled accordingly.

**Camelback / airtime hill.**
- *Physics.* A point that moves under gravity alone follows a parabola, so a crest shaped as the free-flight parabola for its speed gives 0 g ("parabolic flight"). (Pendrill's Balder paper, [physics.gu.se pe5602](https://physics.gu.se/LISEBERG/eng/pe5602.pdf), summary; textbook physics; confidence high for the physics.)
- *Physics, derived here.* For a crest held at **constant** normal force *n* (in g), the speed and pitch obey an exact relation (Part 2.2): **v · |n − cos θ| = constant**. So the crest is a known curve for each *n*: at *n* = 0 it is the projectile parabola; for *n* < 0 it is tighter. The crest radius is **R = v_c² / (g (1 − n))**. Verified against the prototype's integrator to 1 part in 10⁴ (`results.txt`, check 0).
- *Measured.* On Liseberg's Helix (Mack) Pendrill's phone accelerometer read about 0 g and about −1 g on the airtime hills and about 4 g in the valleys (3.5 g at the first drop's bottom). No durations given. ([Physics World](https://physicsworld.com/a/twists-turns-thrills-and-spills-the-physics-of-rollercoasters/), opened; measured; high.)
- *Rule of thumb.* B&M hyper and giga coasters are described as "floater" airtime (near 0 g, around −0.2 g, up to −0.5 g at the crest), Intamin and RMC as "ejector" (stronger negative). Valleys are "mid-3s", up to about 4 g, 4 g called aggressive; posters disagree with each other (one gives −0.7 g on Griffon). ([coastercrazy.com, B&M g-forces for mega coasters](https://www.coastercrazy.com/forums/b-m-g-forces-for-mega-coasters-t20941.html), opened; rule of thumb; low to medium. CoasterForce and CoasterBuzz threads on B&M hypers and Fury 325 seen as summaries only; low.)
- *Not found:* a published accelerometer trace of any B&M camelback; airtime durations in seconds from a measurement. So **there is no sourced "reference B&M camelback"**; see 2.5, caveat "What a B&M camelback is".
- *The few governing numbers* (from the physics): entry speed (given by the layout), crest force *n*, the pitch at which the airtime starts (or equivalently its duration, or the crest speed), the valley force before and after, the onset rates, and either the crest height or the hill length. The relations are in 2.2.

**Vertical loop.**
- *Physics and measured.* A circular loop that is just weightless at the top gives 6 g felt at the bottom (5 g centripetal plus gravity); real loops are tighter at the top and wider at the bottom to cap the entry force. A radius that grows linearly with height gives constant centripetal acceleration. On a Vekoma Invertigo Pendrill measured about 4.5 g and 4.0 g at the bottom before and after the loop and about 2.2 g at the top. The clothoid (Euler spiral, curvature changing linearly with length) was first used by Stengel on Revolution (1976); Arrow used a more teardrop shape. (Pendrill, *Rollercoaster loop shapes*, Phys. Educ. 40 (2005) 517, [pe5601.pdf](https://physics.gu.se/LISEBERG/eng/pe5601.pdf), opened; physics and measured; high. Physics World, above: "about 4 g" entering and leaving teardrop loops; medium.)
- *Governing numbers:* entry speed, entry (bottom) force, top force, loop height (or top speed), the transition shape (clothoid or force-time curve), whether exit mirrors entry. *Relations:* top speed v_t² = v₀² − 2 g H (minus friction); top radius r_t = v_t² / (g (1 + n_t)) with *n_t* the seat force at the top, inverted. In force-vector terms a loop is a force section whose normal force rises to the entry value, falls to *n_t* over the top and rises again: the clothoid is not needed as a separate idea, it is one particular force-time curve.

**Zero-g roll.** *Physics:* with normal and lateral force both 0, the heartline is the ballistic parabola whatever the roll does, so the roll can be added on top of a 0 g crest without changing the path (2.3). *Rule of thumb:* the roll should ramp in and out (roll rate rising and falling, not constant), with the banking starting early. ([coastercrazy.com zero-g roll](https://coastercrazy.com/forums/zero-g-roll-t15586.html), summary; low.) No roll rates in degrees per second were found from any source. *Governing numbers:* entry speed and pitch, crest force (0 g or slightly negative), roll amount (360°), roll-rate shape and timing, exit pitch.

**Overbanked turn.** Bank beyond 90°, typically 100° to 120°; above about 135° it is usually counted as an inversion; Outlaw Run (RMC) is listed at 153°. ([Coasterpedia, over-banked curve](https://coasterpedia.net/wiki/Over-banked_curve), summary; rule of thumb; medium.) *Governing numbers:* heading change, peak bank, crest force, entry and exit pitch, lateral force (held near 0).

**Helix.** No sourced sustained force was found. *Physics:* a level turn with zero lateral force needs bank φ with **n = 1 / cos φ** and radius **r = v² / (g tan φ)**; at 3 g that is 70.5° of bank. *Governing numbers:* sustained force (or bank), turned angle, height change per turn.

**Dive loop, Immelmann, top hat, S-bend (wave turn), speed hill.** No sourced numbers were found. The Helix top hat reads about 0 g at its top (Physics World, measured). A community recipe builds a dive loop in FVD from two sections, the first with two roll and four normal-force transitions, lateral locked ([coastercrazy.com](https://coastercrazy.com/forums/basic-dive-loop-in-fvd-or-newton-2-t34210.html), summary; low). *Governing numbers, from the geometry:* dive loop and Immelmann: entry speed and force, height, the half-roll's timing against the half-loop, exit heading (180° turned). Top hat: entry force, crest force, height. S-bend: bank reversal (through ±90° on a wave turn) and its timing; R3's rule that fast reversals in x and y halve the allowed peak applies here. Speed hill: a camelback with a low, wide crest at high speed (R grows with v²).

### 1.2 How other tools offer elements

- **NoLimits 2, formula elements.** A small program turns a parameter *T* from 0 to 1 into X, Y, Z, bank and a roll-type flag. A "wizard" can ask the user questions (each with minimum, maximum, default, unit, decimals allowed) when the element is added. ([NL2 docs, formula element](https://nl2docs.arborarcade.com/pages/formulaelement.html), opened; high.) This is a parametric generator **of geometry**. The page does not say whether the answers can be changed after the element is placed; nothing suggests it re-solves against neighbours.
- **NoLimits 2, Force Vector Designer.** Three graphs (vertical, lateral, roll), total time, start speed, end speed reported only; settings can be saved and loaded (`.nl2fvd`); no goal, end point or solver ([NL2 docs](https://nl2docs.arborarcade.com/pages/forcevector.html), opened via R5 and again here; high. The `.nl2fvd` detail from the file-format page, summary; medium). *Impressions:* users find it weak beside FVD++, cannot get banking right in it, and want to save elements ([CoasterForce](https://coasterforce.com/forums/threads/nolimits2-noob-in-need.42301/), [CoasterCrazy](https://coastercrazy.com/forums/anyone-still-use-fvd-newton-fvd-t35220.html), [NoLimits Central](https://nolimitscentral.com/forum/topics/44), all summaries; low).
- **KexEdit.** A node graph; its optimizer changes one keyframe value by gradient descent until roll, pitch or yaw at the playhead hits a target, one target at a time; Copy Path reuses a shape. Its docs show no sub-graph, group or template feature (a "templates" topic is listed in a page footer with no page behind it). The `.kex` file keeps "all project data", versioned. ([optimizer](https://individualkex.github.io/KexEdit/user-guide/optimizer.html), [menu](https://individualkex.github.io/KexEdit/reference/menu.html), [shuttle coasters](https://individualkex.github.io/KexEdit/user-guide/shuttle-coasters.html), opened; high for what is there, medium for what is absent.)
- **newton2, FVD++ community.** No shared library of element files and no camelback tutorial were found; element "recipes" are written as instructions in forum posts (the dive loop above). FVD++ can import tracks from another project, which is the nearest thing to a template today. ([altlenny/openFVD](https://github.com/altlenny/openFVD), summary; medium.)
- **Games.** OpenRCT2 stores track designs as fixed pieces; newer elements could only be saved as designs from version 0.4.32 (2026-03-01). Reuse, not parameters. ([changelog](https://openrct2.io/download/changelog), summary; medium.) No parametric element presets were found in Planet Coaster, RCT3 or Ultimate Coaster 2.
- **Research.** Sequeira, Pombo, Ambrósio and others build tracks from a library of analytical segments with simple parameters (radius, length) ([pure.hud.ac.uk](https://pure.hud.ac.uk/en/publications/construction-of-three-dimensional-track-models-for-roller-coaster/), summary; medium); Schuttert (Twente 2022) tunes one parametrised element at a time with reinforcement learning (R5).
- **What is missing, by users' account** (impressions, low): saving and reusing elements, banking that comes out right, editing time and value separately. **No tool found** offers an element that re-solves against its neighbours, or a force-defined element with goals. The gap is real as far as searching can show.

### 1.3 Parametric design elsewhere: what transfers

- **Parametric CAD.** Sketch solvers show the state of each sketch: under-defined, fully defined, or over-defined and conflicting, by colour, with a constraint manager listing which constraint is in conflict ([Onshape help](https://cad.onshape.com/help/Content/Sketch/troubleshooting_sketch_geometry.htm), [constraint manager](https://www.onshape.com/en/resource-center/tech-tips/sketch-constraint-manager), summaries; high). SolidWorks "Break Link" turns a part into a dumb solid; it cannot be undone or relinked, although FeatureWorks can try to recognise features again ([SolidWorks help](https://help.solidworks.com/2026/English/SolidWorks/sldworks/t_break_link_from_original_file.htm), summary; medium to high). *Transfers:* the under/fully/over-defined display, the conflict list, and "convert is one-way, recognition is best-effort". *Does not transfer:* CAD constraints are mostly equations between geometric entities solved simultaneously; FVD elements are a forward integration, so the "constraints" are only satisfiable by shooting (below), and finding 1 shows that counting alone misleads.
- **Road and rail alignment.** Civil 3D: fixed, floating and free elements; a free spiral has a single geometric solution, so its length cannot be set by the user ([Autodesk help](https://knowledge.autodesk.com/support/civil-3d/learn-explore/caas/CloudHelp/cloudhelp/2020/ENU/Civil3D-UserGuide/files/GUID-39D91C0F-FA46-4AF4-BCFD-ED96156371AF-htm.html), summary; medium). Railway transition (clothoid) length comes from a limit on the rate of change of lateral acceleration, typically 0.3 to 0.5 m/s³: L ≥ v Δa / c ([Infrastructures 5(5) 43](https://doi.org/10.3390/infrastructures5050043), [Railway News](https://railwaynews.net/wiki/the-geometry-of-comfort-transition-curves-explained), summaries; medium). *Transfers:* "this piece absorbs change, that one is fixed", and a rate limit setting transition lengths, exactly as Experiment C does with g per second.
- **Animation rigs.** Blender's IK/FK switch is a keyed or driven influence value, and switching mid-animation needs a "snap" so the pose does not pop ([Blender manual](https://docs.blender.org/manual/en/latest/animation/constraints/tracking/ik_solver.html), [IK/FK switch](https://adammadej.com/posts/202407-ikfkswitch_blender/), summaries; medium). *Transfers:* switching an element between "solved" and "frozen" must not change the track at the moment of switching (convert at the current solution).
- **Shooting and continuation.** Multiple shooting (aiming at intermediate points) is more robust than single shooting, less sensitive to the first guess; too many shooting unknowns can shrink Newton's region of convergence ([arXiv 1701.06203](https://arxiv.org/pdf/1701.06203), summary; medium). Natural-parameter continuation fails at a fold, where two solutions meet and disappear; pseudo-arclength continuation passes through it ([Wikipedia, numerical continuation](https://en.wikipedia.org/wiki/Numerical_continuation); Kuznetsov's notes [nba.pdf](https://webspace.science.uu.nl/~kouzn101/NBA/nba.pdf); summaries; high). *Transfers:* an element re-solved after an upstream change should start from its previous solution (continuation), which keeps it on the same branch; at a fold the program must say "no solution beyond here", never jump branches silently.

### 1.4 Jerk and onset rate

- **No general jerk limit in the standards** (R3: F2291 sets onset conditions only for the headrest, 5 g/s; prone restraints, 15 g/s; and airtime into a hard pull-out, at least 133 ms from 0 to 2 g, an average of 15 g/s).
- Pendrill and Eager quote ISO 17929:2014 (*Biomechanical effects on amusement ride passengers*) as recommending a **maximum onset rate of 15 g/s and a minimum drop-off rate of 0.8 g/s** after a maximum-duration exposure, and say the CEN, ASTM and ISO documents have converged; they measured about 7 g/s on Valkyria's dive, and found no limit on snap. (*Velocity, acceleration, jerk, snap and vibration*, Phys. Educ. 55 (2020) 065012, [IOP](https://iopscience.iop.org/article/10.1088/1361-6552/aba732), opened by a research helper; Physics World repeats the two numbers, opened. Confidence medium: the ISO text itself was not seen, so the exact wording is second-hand.)
- Rohde advises designers to aim for 5 g/s and at most 10 g/s, with 15 g/s as the most a measured test should show (R3; advice, not a rule; medium).
- **So a jerk limit in FVD++ is the designer's own rule.** A sensible default is 5 g/s (Rohde's aim) with a warning above 10 and an error above 15. In FVD terms the onset of a transition of change ΔG over T seconds is *k* ΔG / T, with *k* the shape's peak slope: 1 for linear, 1.5 for cubic, 1.875 for quintic, π/2 for sinusoidal (from the formulas in `subfunc::getValue()`).
- F2291 defines onset as the slope of a least-squares line over 0.1 s (R3); a checker should use that window, not the node-to-node difference.

---

## Part 2. The model, thought through against the code

### 2.1 What the program gives an element to work with

These facts, all from the code, set the rules for any element design (R5 Part A has the full chain):

1. **Forward integration only.** `track::updateTrack()` re-integrates the edited section, then for each later section prepends the previous section's last node and calls `updateSection(0)`. An element is therefore a *shooting* problem: choose numbers, integrate, see where it ends.
2. **Transitions store changes, not end values.** On a recompute from node 0, the first normal-force transition is moved to start at the entry node's `forceNormal` (`translateValues(diff)` in `secforced::updateSection()`), and every later transition chains from the previous one's end (`func::translateValues()`). The entry node's `forceNormal` is the value *recomputed from the geometry* at the end of the previous section, not the value that section prescribed. The prototype shows this drift is small (in A1's solved camelback the crest section starts at 0.0004 g instead of 0.0 g, and the exit is 1.00002 g), but it is not zero, and over a chain of elements it adds up unless each element ends "at" a value (R5's C4).
3. **Turn rate = (prescribed force − gravity component) ÷ heartline speed**, the heartline speed being estimated from the *previous* step (`estVel`). So shape depends on speed (R5 A.2).
4. **Speed comes from energy at a point 0.9 × heartline height below the heart** (`vPosHeart(fHeart*0.9f)`), with friction charged on the rail length run so far (`fTotalLength * fFriction`) and air drag v³ × `fResistance` per step. The speed `fVel` is the speed of the rail point, which is what the position step advances by; the heartline moves faster on a crest and slower in a valley.
5. **Durations are quantised.** A section has `int(duration × 1000 + 0.5)` nodes, so a section's end moves in 1 ms steps; residuals a solver sees have tiny jumps. Finite-difference steps must be several ms (the prototype uses 10 ms) and tolerances no tighter than what one node moves (about 0.03 m at 30 m/s).
6. **Pitch is reported wrapped.** `mnode::getPitch()` is an `atan2` in ±90°, and `calcDirFromLast()` takes differences of it. A goal "exit pitch 0" read from `getPitch()` cannot tell a level exit from a level exit after a full loop. An element solver must track the unwrapped turned angle.
7. **The one existing "aim" is `tozero`**, which solves the roll speed so bank returns to zero (`subfunc::getValue()`, case `tozero`). Everything else is open-loop.
8. **Undo already chains.** `undoAction` has a `nextAction` pointer and `doUndo()`/`doRedo()` follow it, so a compound action ("edit + re-solve three elements") fits the existing undo model; F1 is turning every edit into a core function.

### 2.2 The camelback, precisely

**Sections.** Valley to valley, three force sections, time argument, lateral force 0, roll 0:

| Section | Normal-force transitions | What it does |
|---|---|---|
| S1 pull-up and climb | 1 → *G_p* cubic over *t₁*; hold *G_p* for *t_h*; *G_p* → *G_a* cubic over *t₂* | turns the track up, then lets it go into the airtime |
| S2 crest | hold *G_a* for *t_air* | the sustained airtime |
| S3 drop and pull-out | *G_a* → *G_p* cubic over *t₄*; hold *G_p* for *t_h*; *G_p* → 1 cubic over *t₅* | the mirror image (when *t₄* = *t₂*, *t₅* = *t₁*) |

Three sections rather than one so the crest can be named, shown and checked on its own; a single section with the same transitions computes the same track except for the force chaining at the two joins (2.1, item 2).

**Numbers.** Entry state (from upstream): speed *v₀*, height, pitch, force, roll, roll rate. Designer numbers: *G_a* (airtime), *G_p* (valley force), *t₁*, *t_h*, *t₂*, *t_air*, *t₄*, *t₅*, the transition shapes. Possible goals: exit pitch, exit height, crest height, exit speed, exit force, airtime duration (if defined as time below a threshold, it differs from *t_air* by part of the transitions), span or length, peak onset.

**Closed form for the crest.** In the vertical plane, with *n* the felt normal force in g, θ the pitch and *v* the speed of a point mass (heart offset 0, no friction):

- turn rate: dθ/dt = g (n − cos θ) / v (the force section's own rule, 2.1 item 3);
- energy: d(v²)/dt = −2 g v sin θ.

Dividing, d(ln v²)/dθ = −2 sin θ / (n − cos θ) = −2 d(ln|n − cos θ|)/dθ, so for constant *n*:

**v · |n − cos θ| = v_c (1 − n)**, with *v_c* the crest speed.

Everything about a constant-force crest follows: v(θ) = v_c (1 − n)/(cos θ − n); time from the crest to pitch θ_a, t = ∫ v_c (1 − n) / (g (cos θ − n)²) dθ; height below the crest, (v² − v_c²)/2g. At *n* = 0 this is the projectile (horizontal speed constant, airtime 2 v_c tan θ_a / g). Examples (point mass, no friction):

| crest speed (m/s) | n (g) | airtime starts at ±θ_a | airtime (s) | span (m) | crest above airtime start (m) | crest radius (m) |
|---|---|---|---|---|---|---|
| 10 | 0.0 | 20° | 0.74 | 7.4 | 0.68 | 10.2 |
| 10 | 0.0 | 40° | 1.71 | 17.1 | 3.59 | 10.2 |
| 10 | −0.5 | 40° | 1.07 | 10.3 | 2.06 | 6.8 |
| 15 | 0.0 | 40° | 2.57 | 38.5 | 8.08 | 22.9 |
| 15 | −0.5 | 40° | 1.60 | 23.2 | 4.63 | 15.3 |
| 15 | −1.0 | 40° | 1.16 | 16.6 | 3.24 | 11.5 |

So **at a given crest speed, airtime strength and airtime duration fix the crest's angle and height**; with the entry speed given, the crest height fixes the crest speed. The architect's "two of airtime strength, duration and height fix the third" is right *for the crest alone with the crest speed known*. For the whole element it is not the right count: the transitions add free numbers (*t₁*, *t_h*, *t₂*, *G_p*), each of which moves where the airtime starts. With all of those fixed, *one* number (the airtime strength) fixes everything else once the exit must be level.

**Where only a numerical solve works, and why.**
- *Transitions:* while *n* varies in time, dθ/dt = g(n(t) − cos θ)/v is a non-linear equation with no closed form for a cubic *n(t)*.
- *Friction:* the loss depends on path length, which depends on the shape; the invariant above breaks.
- *Heartline offset and the 0.9 term:* the program's speed is the rail point's, the energy is taken 0.9 × fHeart below the heart, and the turn uses the previous step's heart speed. With fHeart = 1.1 m the heart moves about 1.7 % faster than the rail on a 0 g crest at 25 m/s (R = 64 m); the closed form is a first guess, not an answer.
- *Time against distance:* in the time argument, durations are the natural unknowns; in the distance argument (`updateDistanceSection()`), the turn per metre goes as 1/v² (R5 A.2) and the element's length is fixed instead of its time. Both are solvable the same way; the unknowns change units.
- *Node quantisation* (2.1 item 5).

**Counting degrees of freedom against goals.** For the mirrored camelback, the unknowns and goals that were tried in the prototype:

| Designer gives | Program solves | Goals | Verdict |
|---|---|---|---|
| *G_a*, *G_p*, *t₁*, *t_h* | *t₂* (= *t₄*), *t_air* | exit level, crest height | **well-posed** inside the valid region (Experiment A1, A3) |
| same | *t₂* (= *t₄*), *t_air* | exit level, exit height = entry height | **degenerate**: one goal in disguise (A2) |
| same | *t₂*, *t_air*, *t₄* | exit level, crest height, exit height | **well-posed** (A2 last row, B) |
| *G_a*, onset limit *J*, *t_h* (so *t₁*, *t₂* follow from *J* and *G_p*) | *G_p*, *t_air* | exit level, crest height | well-posed; the valley force becomes the free number (C) |
| *G_a*, *G_p*, *t_air*, *t₁*, *t_h* | *t₂* | exit level | well-posed, but crest height is then an output |
| *G_a*, *t_air*, crest height, and *G_p* | *t₂* only | exit level, crest height | **over-determined** (two goals, one unknown): generally no solution |

The general rule: the solver needs as many *independent* goals as unknowns, and independence must be tested numerically (the Jacobian's condition number at the start point), not counted. The mirrored-hill case shows why: the exit height of a mirrored hill is (without friction) a function of its exit pitch, so the Jacobian has rank 1.

**Why the mirror ties exit height to exit pitch.** If the force-time curve is symmetric about the middle of the hold and the pitch is zero there, then with energy conserved the motion runs backwards the same as forwards: the second half is the mirror image of the first, so the exit is level *and* at the entry height. Conversely a level exit (on the same branch) needs the pitch zero at the middle. Friction breaks the symmetry one way: the train is slower on the way down, so the same forces turn it faster, the pull-out comes earlier in the descent and the exit is *lower* (0.9 to 1.5 m in A2's table, at default friction). It can never be raised by changing *t₂* and *t_air* together, because both move the climb and the drop equally.

### 2.3 Elements that turn or roll

**Zero-g roll.** Write it as a camelback whose crest section has *G_a* = 0 g, lateral force 0, and a roll function of 360° total. With normal and lateral force both 0, `forceVec` in `secforced::updateSection()` is gravity alone, so the turning is the same whatever the roll: the heartline is the projectile parabola, and the roll spins the train about it. The roll adds one goal (total roll 360°, roll rate back to 0 at the exit) and one unknown (the roll amount, solvable in closed form for a fixed roll shape: amount = 360° ÷ the shape's mean value × duration). The pitch goals are the camelback's. **The description holds.** It breaks as soon as the crest force is not 0: with *n* = −0.2 g the force points along the rolling normal, so the track yaws during the roll and the exit heading becomes a goal; that needs lateral force or an asymmetric roll as extra unknowns, and the problem becomes 3D and coupled.

**Overbanked turn or helix.** Write it as: roll-in (roll function), a hold at normal force *n* with lateral force 0, roll-out (`tozero` or a mirrored roll). In a force section **lateral force is an input**: prescribing 0 means "no lateral force", automatically. The bank is then not a shape choice but the steering: the normal force points along the banked normal, so the bank decides how much of it turns the track sideways and how much up. For a level helix the coupling is n cos φ = 1 (no pitch change), yaw rate = g n sin φ / v. Goals: heading change (for example 180°), exit pitch, exit height, exit bank 0, exit roll rate 0. Unknowns: roll amount, roll timing, hold duration, *n*. **The description holds**, but four things break or get harder:
- *Heading must be unwrapped too*, and it is undefined near vertical (pitch ±90°), where `getDirection()` is meaningless; an overbank at 120° with a steep crest comes close.
- *The entry roll persists.* Roll angle integrates on the inherited `fRoll`, and the roll function is chained from the entry roll speed (`rollFunc->funcList.at(0)->translateValues(diff)`). An element entered at 10° of bank turns differently for its whole length (R5 A.2). The element must either demand a plain entry or make the exit bank a goal (`tozero` already does that for the bank).
- *Roll orientation.* `bOrientation` EULER adds the yaw's contribution to the roll (`glm::dot(vDir, -Y) * fYawFromLast`), QUATERNION does not. An element must fix one; the same numbers give a different turn in the other.
- *Position goals become 3D:* x and z as well as height. A 180° turnaround that must land on a given line needs two more unknowns and is much more sensitive (lever arms). That is the R5 pin-and-slack problem, not the element's.

**Verdict on "one description".** The same four parts (sections, parameters, unknowns, goals) describe all three. What differs is the goal vocabulary (pitch, heading, bank, roll rate, height, position, all unwrapped), the need to fix the roll orientation and entry bank, and the conditioning: the turning elements couple their goals (bank changes both heading and pitch), so they need more Newton steps and a better first guess (the closed forms above give one).

### 2.4 Anchoring and the exit contract

The architect's claim: an element that promises its exit (level, unrolled, on heading, at a stated height) shields everything after it from upstream changes, because exit height fixes exit speed.

**What the energy line says.** In the program, v² = 2 (E − g (y_ref + μ L)), with E the specific energy carried from the anchor (minus drag), y_ref the height of the energy point, μ the friction value and L the rail length from the anchor. At a fixed exit height:

- **If the upstream change only reshapes the track** (a deeper drop, a different turn, same lift and launch), E is unchanged, and the exit speed changes only through μ ΔL: Δ(v²/2) = −μ g ΔL. With μ = 0.03, 10 m of extra track before the exit costs 2.9 m²/s², about 0.1 m/s at 30 m/s (0.33 %). Upstream length changes of a few metres are typical; so the contract restores the speed to 0.1 % or so. (Plus air drag, which is an order smaller at these speeds: 2×10⁻⁵ × v² is 0.018 m/s² at 30 m/s, against 0.29 m/s² for friction.)
- **If the upstream change alters the energy** (lift height, launch speed, anchor speed, a fixed-speed section's speed), E changes and **the exit speed changes fully**: +3 % entry speed gave +3.0 % (+0.84 m/s) at the exit of a camelback that kept its exit height (Experiment B). Everything after that element sees a faster train and changes shape (R5's knock-on), unless it too re-solves.

So: **an element can promise exit height or exit speed, not both**, unless it contains something that adds or removes energy (a lift, a launch, a brake, all fixed-speed sections in FVD). Promising exit *speed* means the exit height floats with the energy (y_exit = (E − v²/2)/g − μL), which moves everything after it up or down, but keeps its shape. Which is better depends on the change: for reshaping upstream changes the height promise wins (speed comes back for free); for energy changes the speed promise keeps the rest of the ride's shape and moves it vertically, which the designer may not accept either.

**What moves downstream when only the speed is approximately restored.** Two things:
1. *The position along the track.* A re-solved element has a different length and span (in Experiment B, 1 m for the same-energy change, 13 to 15 m per element and 22 to 24 m at the end of the two-element layout for ±3 % of entry speed), so everything after it slides along. Shape and forces after it are the same to the extent the speed is.
2. *The small speed difference.* 0.1 m/s at 30 m/s turns a later time-argument force element through about 0.3 % less angle (R5 A.3): a 60° pitch change becomes 59.8°. Over 100 m that is 0.3 m of height. Small, but not zero, and it compounds.

**When a position pin is needed, and what gives.** When the rest of the ride must stay *where it is* (a station, a structure, a footprint), the element's exit contract is not enough: its length change must be absorbed. That is R5's pin with a slack piece: a chosen section between the element and the pin (a straight's length, a curve's angle, a connector) is re-solved to meet the pin. The element contract and the pin compose cleanly: the element restores the pose and (approximately) the speed, so the slack only has to absorb a translation along the track and a tiny speed error, which a straight's length can do in the simplest case. Without the element contract, the slack would also have to absorb pitch, height and speed errors, which needs the six-unknown connector.

**Verdict.** The claim holds for reshaping upstream edits (to about 0.1 % in speed), with "the rest of the ride slides along" true and measurable. It does **not** hold for energy changes, which are among the most common edits (lift height). The design must say so in the element's panel ("keeps: exit height; exit speed follows the energy").

### 2.5 The caveats and edge cases in the issue

**No solution, or several.** Physics forbids some requests: a crest above the energy height (v₀²/2g minus friction), airtime too strong for the pull-up to have built enough angle, a hill that turns into a loop before its crest. In A3's maps the reachable crest heights at each speed and airtime form one interval; outside it there is no solution. Several solutions: none were found in A3's scan for the camelback as parametrised (crest height rises monotonically with *t₂* until the shape guard), but two classes of extra solutions exist in principle: (a) the same exit pitch after a full loop (excluded by using unwrapped angle and a pitch guard), (b) folds, where two solutions meet and vanish as a parameter changes (1.3). *What the designer sees*, for example: "cannot reach: closest is crest 23.4 m with exit pitch 0.0°; reachable crest heights at this speed and airtime: 9 to 24 m". The closest result is the end point of the scan (or the last good Newton iterate), never a silent jump. Several solutions: the one nearest the previous solution (continuation), and a "show the other shape" button.

**Solver behaviour.** Damped Newton with a forward-difference Jacobian converged in the prototype from starts across the valid region (A1) and failed only when the start was outside it (a loop or a stall), where every trial run is invalid and the line search cannot find a better point. Rules that follow: (1) start from the previous solution after any upstream change, so the element stays on its branch and changes continuously; (2) bound every unknown (durations ≥ 0.05 s, forces within the limits); (3) treat an invalid run (stall, loop, guard broken) as "worse", not as an error; (4) when the start itself is invalid, fall back to a 1D scan of the most influential unknown with the others solved inside, as A3 does, to find a valid start; (5) report the condition number of the Jacobian, and warn when it is large (a degenerate goal pair, 2.2). *Continuity in the entry state:* away from folds, Newton from the previous solution gives a solution that changes smoothly with the entry speed (Experiment B's re-solves, started from the previous solution, moved *t₂* by about 0.05 s for 3 % of speed and converged in 5 to 9 runs). At a fold the element must stop and say so.

**The entry state not matching the element's assumption.** The camelback assumes a level, unrolled entry at 1 g with no roll rate.
- *Pitched entry:* the first transition still starts at the entry force, and the hill is solved from there; the goals (exit level, heights) stay meaningful. Allowed, solved through.
- *Non-1 g entry:* because transitions store changes, an entry at 1.3 g shifts the whole force curve by 0.3 g, including the airtime (R5 A.2). **For elements, the force values must be targets ("go to 0.0 g"), not changes** (R5's C4): the element expands each transition as "change = target − value at its start", computed at expansion time. Then the airtime is 0.0 g whatever the entry. Continuity at the entry is automatic (the first transition starts at the entry value); continuity of the *rate* of force is not: if the previous section ends mid-transition (rising at 3 g/s), the element's cubic starts with zero slope, a kink in onset. The element should check and report it ("entry onset 3 g/s, element starts at 0").
- *Banked or rolling entry:* roll persists through force sections (2.3). The camelback should either refuse (with the reason) or include a roll-out to 0 at its start as a first transition. Recommendation: refuse in version 1, offer "add a roll-out" as a fix.
- *Exit end:* the element ends at its target exit force (1 g in a valley); what follows inherits it. The element should end on zero onset and zero roll rate, so the next section's start is insensitive (R5 A.4).

**What stays fixed when the entry changes.** Three possible behaviours: (a) forces fixed, shape changes (today's force sections); (b) shape fixed, forces change (R5's C3, a geometric section with the distance argument); (c) goals fixed, the program re-solves (an element). This is a per-element choice, with (c) as the default for elements, (a) after conversion to plain sections, and (b) a separate "freeze shape" action. Inside (c), the designer also chooses *which* goals are held (2.4: exit height *or* exit speed).

**Time or distance inside an element.** Time argument for force sections in version 1: it is what the owner uses, what `secforced::updateSection()` handles fully (the distance version derives the roll differently and the `tozero` transition exists only for time), and onset rates are naturally in g per second. Distance makes the element's length fixed and its shape more speed-sensitive (R5 A.2), which is the opposite of what an element wants.

**Friction and resistance.** The solve includes them exactly (it runs the real integration), so no model error. Their effects: exit lower than entry for a mirrored hill (2.2), and the μ ΔL speed drift (2.4).

**Heartline height.** Goals refer to the heartline (`vPos`) by default, since that is what the program computes and what the forces are felt at. A crest-height goal for the rails or for clearance is the heart height minus fHeart along the normal (vertical at the crest). Changing the track's fHeart re-solves every element, since the shape depends on it.

**Roll smoothing across an element boundary.** `smoothHandler` regions store absolute node numbers, so they slide when an upstream element changes length (R5 A.2), and `applyRollSmooth()` runs *after* the integration, changing roll on nodes the element's solve did not see. Rule: an element's solve and its goals are computed without smoothing; smoothing regions that overlap an element are flagged, and regions are re-anchored to sections, not node numbers (a fix that belongs to W1/W2, outside this unit).

**Live elements and undo.** One upstream edit can re-solve several elements, in order front to back (each element's entry is the previous one's exit). This is one compound undo step: the edit plus every re-solve, chained through `undoAction::nextAction`. The stored state per element is its solved unknowns (a few floats), so undo restores them without re-solving. A re-solve that fails leaves that element with its old numbers, marked "goal not met", and later elements re-solve against the result anyway (or stop, a setting).

**The saved file.** See Part 4.3 for the recommendation. The facts, checked in the code and by a test run:
- `projectWidget::loadProject()` reads "FVD", a five-character version, then `"TRC"` blocks until `"EOP"`, and stops. It never reads past `"EOP"` and does not check for the end of the file. An unknown version string gives "Unsupported File Version" and nothing loads. The function has not changed since 2014 (`git log -L`), so the 2016 binary very probably has the same code.
- **Test (2026-11-10):** `samples/baseline/four-sections.fvd` with 75 bytes of element record appended after `"EOP"` was loaded by today's build (`build/dist/FVD.exe --batch-export`): "Load Successfull", and all three exports byte-identical to those of the unmodified file. The re-saved file was the same as the plain file's re-save, without the appended bytes. The 2016 binary has no batch mode and was not tested; that is a one-minute owner check (Part 4.4).
- So a file can carry **the expanded plain sections as today, plus an optional element record after `"EOP"`**. Old programs open it as plain sections (and drop the record if they save). New programs read the record and re-attach the elements.
- *What breaks or must be designed:* (1) this is a format addition without a version bump, a narrow reading of rule 4 that needs the owner's agreement and a decision record; (2) the record must refer to sections by position and carry a fingerprint (for example a hash of each section's saved bytes) so a file edited elsewhere is detected, and the element then demotes to plain sections with a message; (3) the trailer must be a container of tagged, length-prefixed chunks, so later features (pins, goals, targets) can add their own and an older new-version program skips unknown chunks; (4) a project saved by the 2016 program silently loses its elements; (5) the batch mode's "re-save" must keep the record.
- The alternative, a version bump ("v0.80"), is clean but makes every new file unreadable by the 2016 program, including files without elements, unless the program writes "v0.77" whenever nothing new is used.

**Converting to plain sections, and back.** Converting is trivial (the sections exist already; drop the element record). Making live again is not possible in general (the hand-edited sections may no longer match any parameter set), as in CAD. A best-effort "recognise" is possible for exactly the element's structure: if the sections still have the element's transitions in the element's order, the parameters can be read back and the goals re-measured. Version 1: one-way, with a warning.

**Limits as inputs.** An onset limit *J* sets every transition's minimum duration (*k* ΔG / *J*). Then the transition durations can no longer be unknowns; another number must give (Experiment C: the valley force). The interaction with goals: a low *J* means long transitions, a longer and wider element, and at some point no solution (the transitions alone use more height or angle than the goals allow). Treat a limit as an *input* when the designer wants the program to pick lengths, and as a *check* (bound on the unknowns) otherwise; never both silently.

**Per-seat checks without real train dimensions.** On a camelback the front and rear cars cross the crest at different speeds and pitch rates: the rear car is pulled over the crest by the train and gets the stronger airtime (physics: when the rear car crests, most of the train is already descending, so the train is moving faster). Without car dimensions (D2 has none from manufacturers; R1), the check must assume a train length and say so, for example: "assumed 7 cars × 2.0 m pitch, seat 0.6 m in front of and behind the car centre; front and rear seats estimated". The element's goals stay at the heartline (centre of the train); per-seat values are checks, never goals, in version 1.

**What a "B&M camelback" is.** No source gives a reference. A working definition, labelled as community estimate: crest held at about −0.2 g (range 0 to −0.5), valleys about 3.5 g (up to 4), onset no more than 5 g/s, the airtime held over most of the hill, the hill mirror-symmetric. The owner decides the numbers (Part 4.4). The prototype's camelback has this structure, with 3.5 g valleys and airtime from +0.2 to −1.0 g.

**A description language for custom elements.** It must be readable by the designer, writable by the automation interface (T2) and the project-as-text (F3), and expand into ordinary sections. A sketch, in the same plain style as the project-as-text will use:

```
element "B&M-style camelback"  version 1
  parameters
    airtime       -0.2 g     range -1.0 .. 0.3     "force held over the crest"
    valley         3.5 g     range  2.0 .. 5.0     "force in both valleys"
    crest_height  25.0 m     above entry
    onset_limit    5.0 g/s
    valley_hold    0.4 s
  unknowns
    t_up    start 0.5 s    range 0.05 .. 6
    t_air   start 2.0 s    range 0 .. 12
    t_down  start t_up     range 0.05 .. 6
  sections
    force "pull-up and climb"  time
      normal  cubic to valley over ramp(valley - 1)
              hold for valley_hold
              cubic to airtime over t_up
    force "crest"  time
      normal  hold for t_air
    force "drop and pull-out"  time
      normal  cubic to valley over t_down
              hold for valley_hold
              cubic to 1 g over ramp(valley - 1)
  define ramp(dg) = 1.5 * dg / onset_limit
  goals
    exit pitch    = 0 deg           within 0.05 deg   (unwrapped)
    crest height  = crest_height    within 0.01 m
    exit height   = entry height    within 0.01 m
  checks
    pitch everywhere < 80 deg
    onset <= onset_limit
    forces within limits (A3), per seat if a train is chosen (A4)
  entry needs  roll 0, roll rate 0
  on upstream change  re-solve, keep goals
```

Every line is something the designer already knows from FVD++: transitions with a shape, a target and a duration ("to" is a target, not a change), goals, checks. The automation interface reads and writes the same structure as data (JSON or the project text format). A custom element is made by selecting sections, marking numbers as parameters or unknowns, ticking goals (the R5 C1 panel) and saving under a name; the program writes this text.

---

## Part 3. Prototype experiments (a 2D stand-in)

**What it is.** `fvd2d.py` is a small model of one point on the heartline moving in the vertical plane, stepped 1000 times per second the way `secforced::updateSection()` (time argument) does it: the same turn rule with the previous step's heartline speed, the same rail-point position step, the same energy line (0.9 × fHeart point, friction on rail length, v³ drag), the force recomputed from the geometry at each node and used as the next section's start, transitions stored as changes, node counts rounded to whole milliseconds. Program defaults: fHeart 1.1 m, friction 0.03, resistance 2e-5. `run_experiments.py` runs the experiments and writes `results.txt`; the tables below are copied from it (run time about 8 minutes: `py -3 docs/research/r6-prototype/run_experiments.py`).

**Simplifications** (each is a reason to re-run on the real core, 4.2): two dimensions, so no roll, lateral force or yaw; pitch kept as a continuous angle (the program's `getPitch()` wraps at ±90°); 64-bit floats instead of 32-bit; only time-argument force sections (no geometric, straight or curved sections, no distance argument, no time warp, no locked transitions, no smoothing); a stall stops the run (the program would produce NaN). **Check 0** compares the integrator with the closed form v·|n − cos θ| = constant (fHeart 0, no friction): the error is 1 part in 10⁴ after 1.5 s.

**The test element.** The camelback of 2.2: valley force *G_p* = 3.5 g, ramps *t₁* = 1.0 s (an onset of 3.75 g/s), 0.4 s holds at *G_p*, cubic transitions, drop mirroring the climb unless *t₄* is solved separately. A "valid" hill stays below 80° of pitch and does not stall. An *evaluation* or *run* is one integration of the element (about 10,000 steps). Solver: damped Newton, forward-difference Jacobian (steps of 0.01 s or 0.01 g), backtracking by halving, tolerances 0.05° and 0.05 m.

### Experiment A: one element, two unknowns, two goals

**A1. Entry 30 m/s, airtime 0.0 g, crest 38 m above the entry, from six starting guesses.**

| start t₂, t_air (s) | result | runs | t₂ (s) | t_air (s) | exit pitch (°) | crest (m) | exit height (m) | exit speed (m/s) |
|---|---|---|---|---|---|---|---|---|
| 0.5, 2.0 | start invalid (loops) | 1 | | | | | | |
| 0.2, 4.0 | converged | 7 | 0.438 | 4.092 | 0.02 | 38.00 | −1.10 | 28.69 |
| 1.0, 1.0 | start invalid (loops) | 1 | | | | | | |
| 0.1, 8.0 | converged | 11 | 0.438 | 4.093 | −0.01 | 38.00 | −1.14 | 28.70 |
| 2.0, 3.0 | start invalid (loops) | 1 | | | | | | |
| 0.6, 0.5 | start invalid (loops) | 1 | | | | | | |

From a valid start Newton converges in 7 to 11 runs, to the same answer. Four of six plausible-looking starts are not hills at all: with too short an airtime hold, the drop's pull-up turns the track over into a loop. Damped Newton cannot recover from a start where every nearby run is invalid. **The start must be found, not guessed.**

**A2. The goal pair "exit level at the entry height".** With the mirrored element and unknowns *t₂* (= *t₄*) and *t_air*, Newton failed both with friction (it wandered into invalid runs after 24 runs) and without (stuck after 12). The reason, from solving *t_air* for a level exit at fixed *t₂*:

| t₂ (s) | exit height, friction on (m) | exit height, no friction or drag (m) |
|---|---|---|
| 0.2 | −0.945 | −0.008 |
| 0.4 | −1.106 | −0.022 |
| 0.6 | −1.243 | 0.012 |
| 0.8 | −1.526 | −0.003 |

Without friction a level exit *is* an exit at the entry height (to node resolution): the second goal adds nothing. With friction a level exit is always 0.9 to 1.5 m low here, and changing *t₂* makes it worse: there is no solution. Freeing *t₄* (the drop no longer mirrors the climb) and adding the crest as a third goal **converged in 9 runs**: *t₂* = 0.438 s, *t₄* = 0.398 s, *t_air* = 4.081 s. The drop's transition is 40 ms shorter than the climb's, to make up for the speed lost to friction.

**A3. Where a solution exists: crest 25 m above the entry, exit level.** Each cell: number of solutions found by scanning *t₂* (0.05 to 4.00 s, *t_air* solved for a level exit at each) / the reachable crest range in m / Newton from the fixed start (0.5 s, 2.0 s) / Newton from the scan's point nearest 25 m (runs, or "fail").

| entry (m/s) | +0.2 g | 0.0 g | −0.2 g | −0.4 g | −0.6 g | −0.8 g | −1.0 g |
|---|---|---|---|---|---|---|---|
| 15 | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill |
| 20 | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill | no valid hill |
| 25 | 0 / 29–30 / fail / fail | 0 / 27–30 / fail / fail | 0 / 25–29 / fail / fail | 1 / 24–28 / fail / 4 | 1 / 22–27 / fail / 4 | 1 / 21–26 / fail / 1 | 1 / 21–25 / fail / 4 |
| 30 | 0 / 34–43 / fail / fail | 0 / 30–43 / fail / fail | 0 / 28–42 / fail / fail | 0 / 26–40 / fail / fail | 1 / 24–39 / 10 / 1 | 1 / 23–38 / 10 / 4 | 1 / 22–38 / 7 / 4 |
| 35 | 0 / 36–58 / fail / fail | 0 / 32–59 / fail / fail | 0 / 29–57 / fail / fail | 0 / 27–56 / fail / fail | 1 / 25–54 / 18 / 5 | 1 / 24–53 / 10 / 4 | 1 / 22–52 / 7 / 4 |
| 40 | 0 / 38–77 / fail / fail | 0 / 33–77 / fail / fail | 0 / 30–75 / fail / fail | 0 / 27–73 / fail / fail | 0 / 26–71 / fail / fail | 1 / 24–70 / 7 / 4 | 1 / 23–69 / 7 / 1 |

What it shows:
- **One solution or none, never two,** for this element in this range. The crest height rises steadily with *t₂* until the hill would pass 80° or loop.
- **"No solution" has two sides.** At 15 and 20 m/s a 3.5 g pull-up of this length loops or stalls before any hill forms. At 30 to 40 m/s with weak airtime, 25 m is *below* the smallest hill this pull-up makes (26 to 38 m): the request is too modest for the chosen valley force and ramp, not too ambitious. The panel must say which ("lower the valley force or lengthen the ramp"), not just "no solution".
- **The fixed start failed in 34 of 42 cells, including every 25 m/s cell that has a solution.** Started from the scan's nearest point, Newton converged in 1 to 5 runs in every cell with a solution, and failed (correctly) in every cell without one.
- The scan cost 27,288 runs for 42 cells, about 650 per cell. In C++ that is well under a second per cell, but not "live" on every keystroke; the scan is for a new element or a lost solution, continuation for everything else.

### Experiment B: the exit contract

Layout: camelback E1 (0.0 g airtime, crest at 38 m), camelback E2 (−0.6 g, crest at 28 m), then 2 s of level 1 g run-out; entry at height 0. Each element's contract: exit level at height 0 with its crest at the stated height; unknowns *t₂*, *t_air*, *t₄*. The baseline at 30 m/s solved in 42 runs, including finding the starts. Each change is solved from the previous solution. Differences from the baseline:

| change | case | E1 exit dx (m) | dy (m) | dpitch (°) | dv (m/s) | end dx (m) | dy (m) | dpitch (°) | dv (m/s) | runs |
|---|---|---|---|---|---|---|---|---|---|---|
| entry speed +3 % | nothing re-solves | 9.04 | 2.87 | 2.04 | −0.10 | −1.15 | 7.33 | 1.24 | −1.73 | 0 |
| | E1 re-solves | 12.77 | 0.02 | 0.01 | 0.84 | 20.15 | 1.52 | 0.72 | 0.28 | 9 |
| | E1 and E2 re-solve | 12.77 | 0.02 | 0.01 | 0.84 | 21.73 | −0.04 | −0.02 | 0.83 | 14 |
| entry speed −3 % | nothing re-solves | −9.20 | −3.15 | −2.30 | 0.22 | 3.20 | −8.50 | −1.28 | 2.03 | 0 |
| | E1 re-solves | −14.86 | −0.04 | −0.03 | −0.82 | −22.02 | −1.64 | −0.80 | −0.20 | 9 |
| | E1 and E2 re-solve | −14.86 | −0.04 | −0.03 | −0.82 | −24.05 | 0.06 | 0.04 | −0.83 | 14 |
| same energy, entry 1 m lower | nothing re-solves | 3.28 | 0.07 | 0.77 | −0.05 | −0.66 | 1.77 | 0.45 | −0.66 | 0 |
| | E1 re-solves | 0.93 | −0.04 | −0.03 | −0.004 | 1.03 | −0.12 | −0.02 | 0.02 | 5 |
| | E1 and E2 re-solve | 0.93 | −0.04 | −0.03 | −0.004 | 0.75 | 0.08 | 0.04 | −0.05 | 10 |

What it shows:
- **Same energy (a reshaping upstream):** re-solving E1 alone restores everything after it: exit speed within 0.004 m/s, the end of the layout within 0.12 m in height and 1 m along. The architect's claim holds here, and one element re-solving is enough.
- **Energy changed (±3 % entry speed):** E1 keeps its exit pose, but exits 0.84 m/s (3 %) faster or slower, as the energy line says it must. E2, not re-solved, then ends 1.5 to 1.6 m off in height and 0.7 to 0.8° in pitch. Only when every element re-solves is the pose restored; the speed difference runs on to the end, and the layout ends 22 to 24 m further or nearer. **The exit contract does not shield the rest of the ride from an energy change**; it turns the damage into a known speed change plus a shift along the track.
- "Nothing re-solves" is today's FVD++: 7 to 8.5 m of height error at the end from a 3 % speed change, and 1.8 m even from the same-energy change.
- Re-solving from the previous solution took 5 to 9 runs per element (continuation works). The tolerance had to be 0.05 m, not 0.01 m: at 0.01 m Newton stalled at 0.018 m, because the element's end moves in whole-millisecond node steps (2.1, item 5).

### Experiment C: transition lengths from an onset-rate limit

Every transition is as short as the limit *J* allows (cubic: 1.5 ΔG / *J*). The durations are then no longer free, so the valley force *G_p* and *t_air* become the unknowns; goals: exit level, crest 25 m above the entry. The start is found by scanning *G_p* (1.5 to 5.0 g in 0.25 g steps, *t_air* solved for a level exit at each), then Newton. Every one of the 32 cases converged; "runs" includes the scan, which is most of them. Selected rows (all 32 are in `results.txt`):

| airtime (g) | entry (m/s) | J (g/s) | runs | G_p (g) | t_air (s) | t₂ (s) | track length (m) | span (m) |
|---|---|---|---|---|---|---|---|---|
| 0.0 | 30 | 2 | 132 | 2.12 | 1.67 | 1.59 | 180.9 | 169.9 |
| 0.0 | 30 | 3 | 128 | 2.40 | 2.22 | 1.20 | 167.4 | 155.5 |
| 0.0 | 30 | 5 | 148 | 2.80 | 2.75 | 0.84 | 154.1 | 141.4 |
| 0.0 | 30 | 10 | 147 | 3.44 | 3.27 | 0.52 | 141.0 | 127.4 |
| −0.6 | 30 | 2 | 122 | 2.12 | 0.31 | 2.04 | 171.8 | 160.1 |
| −0.6 | 30 | 3 | 120 | 2.43 | 0.98 | 1.51 | 155.7 | 142.7 |
| −0.6 | 30 | 5 | 144 | 2.90 | 1.61 | 1.05 | 140.3 | 125.8 |
| −0.6 | 30 | 10 | 168 | 3.66 | 2.22 | 0.64 | 125.3 | 109.3 |
| 0.0 | 40 | 2 | 128 | 2.09 | 1.73 | 1.56 | 261.6 | 254.3 |
| 0.0 | 40 | 10 | 152 | 3.37 | 3.29 | 0.51 | 207.0 | 198.1 |
| −0.6 | 40 | 2 | 116 | 2.08 | 0.35 | 2.01 | 245.9 | 238.3 |
| −0.6 | 40 | 10 | 142 | 3.58 | 2.21 | 0.63 | 181.2 | 170.7 |

What it shows:
- At the same crest height, going from 10 g/s to 2 g/s makes the element **26 to 37 % longer**, with gentler valleys (3.4 to 3.8 g down to about 2.1 g).
- **The limit eats the airtime first.** For −0.6 g airtime at 2 g/s the sustained part shrinks to 0.3 s: most of the hill is spent getting into and out of the airtime. Ask for strong airtime, a low onset limit and a fixed height, and the answer is "almost no sustained airtime" before it is "no solution". The panel should show the airtime duration as a result next to the limit.
- The peak onset of every solved element equals *J* (`results.txt`, last column), as it must when the durations come from it. The real checker (A3) will measure onset over a 0.1 s window (R3), which reads somewhat lower than the peak slope of a short cubic; E3 in 4.2 measures by how much.

### What carries over to the real core, and what must be re-run

**Expected to carry over**, because they follow from the structure of the integration that the stand-in copies: the degenerate goal pair of a mirrored element and the friction miss; the need to find a valid start, and the failure of fixed starts; one-or-none solutions with two kinds of "none"; continuation from the previous solution converging in a handful of runs; the energy argument of Experiment B; node quantisation setting the tolerance; onset limits trading valley force and airtime against length.

**Must be re-run on the real core** (4.2): all the numbers (32-bit floats, the real force chaining at section joins, real run times); anything with roll, lateral force or yaw (zero-g roll, overbank, helix); the heartline offset when roll is present (`vLatHeart()` and `vDirHeart()` use the roll speed); and the exporters (an element's sections export exactly like hand-built ones by construction, but the baseline comparison must show it).

---

## Part 4. Towards a design

A provisional recommendation. It is meant to be revised by the experiments in 4.2 on the real core.

### 4.1 Provisional recommendation

**What is stored.** An element is a record beside the sections, not a new section type:
- the element type and version (for example "camelback v1"), and which consecutive sections it owns (by position, with a fingerprint);
- the parameter values, the unknowns' solved values and bounds, the goals with their targets and tolerances, the checks, the "keeps" choice (exit height or exit speed) and the update rule (re-solve or frozen);
- the transitions are written as *targets* ("to 0.0 g") in the element definition and expanded to the program's *changes* when the element is expanded, so the stored sections are exactly what today's program computes.
The element *definitions* (camelback, zero-g roll, ...) are data files under `data/`, like the train catalogue, in the description language of 2.5; the shipped ones and the designer's own are the same mechanism.

**The solver's rules.**
1. Damped Newton with a forward-difference Jacobian (steps of several ms for durations), backtracking, bounded unknowns; the R5 C1 solver, unchanged.
2. Start from the previous solution (continuation). On a new element, or when the previous solution is lost, first scan one unknown with the rest solved inside (as A3 does) and start from the valid point nearest the goals: fixed starting values failed in 34 of 42 cells of A3, the scan-started solve in none that had a solution.
3. Goals in unwrapped angles; a shape guard (for example pitch < 80° for a camelback) as part of the definition; stall or guard broken counts as "worse", not as an error.
4. Independence is checked, not counted: if the Jacobian at the start is near-singular, the panel says which goals depend on each other (the mirrored-hill case).
5. On failure: keep the old numbers, mark "goal not met: closest ...", and show the reachable range if a scan is cheap.
6. Elements re-solve front to back after an upstream edit, as one compound undo step.

**Live versus converted.** Live by default; one-way "convert to plain sections" (best-effort "recognise" later, if ever). A live element's sections are read-only in the section panel except through the element's parameters, so the two never disagree.

**The saved file.** Plain sections exactly as today, plus an element record in a tagged extension block after `"EOP"`, without a version bump, so projects with elements still open in the 2016 program (as plain sections). This needs the owner's agreement and a decision record, because it reads rule 4 narrowly ("a change to what the 2016 program reads"). If the owner prefers the strict reading, bump the version only when a file contains elements.

**Limits.** As checks always (A1 to A3). As inputs on request: an onset limit sets the transition lengths, and the definition then names which other number becomes the unknown (Experiment C: the valley force). The default onset limit is 5 g/s (Rohde's aim), warning above 10, error above 15 (ISO 17929 as quoted).

**Build order.**
1. *Needs first:* F1 (core without a window), the R5 C1 goal-seeking solver (W2), C4 targets instead of changes, A1 (findings) for the checks.
2. *First buildable slice:* the camelback only, valley to valley, time argument, no roll; the three goals and three unknowns of Experiment B; live re-solve on upstream change; the extension record in the file; the "convert" action; one panel. The analysis framework shows its onset and force checks. The automation interface (T2) gets "add element", "set parameter", "solve" as commands, so an agent can run the re-test protocols below at scale.
3. Then the zero-g roll (adds the roll goal; 2.3), then the overbank turn (adds heading, bank and roll orientation; the first 3D element), then loops and the rest.
4. Per-seat checks (A4) after the train model exists; they never change what the element solves for in version 1.

### 4.2 Experiments to re-run on the real core before the design is fixed

Each is a script against the window-less core (F1), on the baseline machinery. "Changes the design if" says which result would.

**E1: solver on one element (Experiment A on the real core).** Inputs: the camelback as in Part 3 (fHeart 1.1, friction 0.03, resistance 2e-5); entry 15 to 40 m/s in 5 m/s steps; airtime +0.2 to −1.0 g in 0.2 g steps; crest 25 m; and the six starting guesses of A1. Measure: converged or not, evaluations, wall time per solve, the scan's reachable crest range and number of solutions per cell, the exit force drift. Changes the design if: the scan-started solve fails in cells that have a solution (then the start search needs more than one scanned unknown); the scan takes more than about a second (then it needs a cheaper first guess, for example from the closed form of 2.2); a solve takes more than 0.2 s (then re-solving live on every edit is not viable and re-solve becomes a button); two solutions appear in normal use (then the panel needs a branch chooser).

**E2: the exit contract (Experiment B on the real core, plus 3D).** Inputs: a real layout of the owner's with two camelbacks after the first drop and a turn after them; changes: lift +1 m, lift −1 m, first drop 1 m deeper (same lift), anchor speed +3 %. Cases: nothing re-solves; first element re-solves; all re-solve; plus "keeps exit speed" instead of "keeps exit height". Measure: the C5 knock-on table (position, heading, pitch, bank, speed at every section end and at the station). Changes the design if: re-solving does not cut the downstream change by at least ten times for reshaping edits (the contract would not be worth its complexity); or the owner prefers the "keeps exit speed" behaviour for his typical edits (then that becomes the default).

**E3: limits as inputs (Experiment C on the real core).** Inputs: as Part 3 C, plus quintic transitions and an F2291-style 0.1 s onset window. Measure: element length, valley force, existence, and the onset the A3 checker reports. Changes the design if: the onset the checker measures differs from *k* ΔG / T by more than 10 % (then transition lengths must be solved against the checker, not computed from the shape).

**E4: a second, harder element (the issue's Experiment 4).** Inputs: a zero-g roll (camelback plus roll goal) and an overbank turn (heading 180°, exit level, exit bank 0) at entry 20 to 35 m/s, entries with 0° and 10° of bank, EULER and QUATERNION orientation. Measure: convergence, evaluations, heading/bank errors, behaviour near vertical. Changes the design if: the overbank needs more than about five unknowns or fails often near the crest (then turning elements need multiple shooting, aiming at the crest as an intermediate point, before they are offered).

**E5: the file record.** Inputs: a project with an element saved by the new program; open it in the 2016 `bin/win64/FVD.exe` (copy outside the repository), export, compare with the new program's export of the same project; re-save it in the 2016 program and open it in the new one. Changes the design if: the 2016 program does not open the file (then a version bump is the only route).

**E6: per-seat on a camelback (with A4).** Inputs: the solved camelback, the assumed train of 2.5. Measure: airtime at the front and rear seats against the heartline value. Changes the design if: the rear seat differs by more than about 0.3 g on typical hills (then "airtime at the rear seat" may need to be a goal option).

### 4.3 The saved-file question in one paragraph

Projects with elements can stay readable by the 2016 program if the element record is written after the end-of-project mark, which the 2016 loader never reads (shown with today's build, which has the same load function; the 2016 binary itself is an owner check, E5). The cost: a narrow reading of rule 4, a fingerprint per element, and the fact that the 2016 program silently drops elements if it saves the file. The alternative is a version bump, which makes those files unreadable by the 2016 program.

### 4.4 Choices that are the owner's

1. **Which numbers define the first element (the camelback).** Recommendation: the designer gives airtime force, valley force, crest height (above the entry) and an onset limit (a check, and the lower bound on every transition's duration); the program solves the climb and drop transition durations and the airtime duration so that the exit is level at the entry height (the three goals and unknowns of Experiment B). Alternative: give airtime *duration* instead of crest height (then the crest height is an output). And the default "B&M-style" numbers: −0.2 g crest, 3.5 g valleys, 5 g/s onset (community estimates, 1.1).
2. **Do elements hold position by default?** Recommendation: **no**. An element holds its exit pose (level, at its height, unrolled) and lets the rest of the ride slide along the track; holding a position is a pin with a slack piece (R5 C2), placed by the designer where he wants it. Also: when the lift or launch changes, should elements keep their exit *height* (speed follows the energy; recommended) or their exit *speed* (height follows)?
3. **The file format.** Recommendation: the compatible extension block after the end mark, no version bump, so files with elements still open in the 2016 program as plain sections. Alternative: a version bump whenever a file contains elements (strict reading of rule 4). Before deciding: open the E5 test file in the 2016 program (one minute).
4. **Which elements come first.** Recommendation: camelback, then zero-g roll, then overbanked turn; loops, dive loops and the rest later. Is there an element you build more often than these?
5. **Live or on a button.** Recommendation: live re-solve after each upstream edit if E1 shows a solve under 0.2 s; otherwise a "re-solve elements" button. (Same question as R5's question 2.)

---

## Could not establish

- **A reference B&M camelback.** No accelerometer trace, airtime duration or valley force from a measurement of any B&M hyper or giga was found; the numbers used are forum estimates.
- **Roll rates for zero-g and heartline rolls**, sustained helix forces, and any numbers for dive loops, Immelmanns, S-bends and speed hills.
- **The ISO 17929:2014 text.** The 15 g/s onset and 0.8 g/s drop-off values are quoted second-hand (Pendrill and Eager 2020).
- **Whether the 2016 binary ignores data after the end mark.** Its loader source (unchanged since 2014) does, and today's build was tested; the binary itself has no batch mode and was not run.
- **Whether NoLimits 2 formula-element parameters can be edited after placement**; its documentation does not say.
- **How manufacturers parametrise elements internally.** Nothing published was found.
- **Behaviour of the real core.** All numbers in Part 3 come from a 2D stand-in; roll, lateral force, yaw, the distance argument, smoothing and 32-bit floats are not modelled, and the timings are Python's, not the program's.
