# R8: Friction, resistance and margins

Research for FR-13 (energy margin), and a pointer for FR-11 (the train as a train). Written 2026-11-11; all links accessed 2026-11-11.

## Summary

1. **What the two numbers are (from the code, section 1).** `fFriction` is the height the train loses per metre it travels, so it is a rolling-resistance coefficient: the retarding force is `fFriction` times the train's weight. The default is 0.03 (a slope of 1.7 degrees), which costs 0.294 m/s² on level track at any speed. `fResistance` is a drag term: the deceleration is `fResistance` times the speed squared, in units of 1/m. The default is 2e-5, which costs 0.008 m/s² at 20 m/s. Neither number knows the train's mass; both are "per kilogram" already.
2. **The defaults are sensible for friction and low for drag.** Published and forum values for a real polyurethane-wheeled train's rolling coefficient are 0.009 to 0.018 (one source, method unknown); the NoLimits 2 fits made by users are 0.026 to 0.031 for steel and 0.016 to 0.021 for wood. The program's 0.03 therefore sits at the top of the physical range and in the middle of the simulator-fitted range, so it most likely carries bearing, side-wheel and other losses too. A back-of-envelope drag figure for a full train is 1.3e-4 to 6.5e-4 1/m, which is 6 to 30 times the default 2e-5. At 30 m/s that drag is as big as the rolling term (section 2).
3. **Real rides get slower when cold, new, empty or short of grease, and faster when hot, broken-in or full.** Operators publish minimum temperatures of roughly 0 to 10 degrees C (32 to 50 F) by ride, and test new rides with water-filled dummies. No source gave a measured size of the cold effect, so any low/high band is a judgement (sections 2 and 3).
4. **No designer's margin figure was found.** The check should report what it can compute exactly (speed and energy over each crest in the worst case) and let the owner choose the pass mark (section 4).
5. **Recommended band** (section 4): relative to the project's own settings, friction at 0.67, 1.0 and 1.33 times, plus a drag set for the empty-train, cold-air case. Every number in the band is low or medium confidence. Calibrating one real ride against its recorded lap time or speeds would be worth more than any further desk research.

---

## 1. What the program's two numbers are

### 1.1 The code

The speed update in `core/secforced.cpp` (lines 153 to 159; the same two lines appear in `secstraight.cpp`, `seccurved.cpp`, `secgeometric.cpp` and `secbezier.cpp`) runs once per node, and nodes are `1/F_HZ` = 1/1000 s apart (`core/mnode.h`):

```
fEnergy -= v*v*v / F_HZ * fResistance
v        = sqrt( 2 * ( fEnergy - F_G * ( height + fTotalLength * fFriction ) ) )
```

and when the section is given a fixed speed (`bSpeed` false) the energy is reset from the speed: `fEnergy = 0.5*v*v + F_G*(height + fTotalLength*fFriction)`. The anchor node starts with `fEnergy = 0.5*v*v + F_G*height` (`core/track.cpp` line 45 and 952). `F_G` is 9.80665 m/s² (the code also uses the literal 9.80665 in places).

Defaults, from the constructor in `core/track.cpp` lines 47 and 48: `fFriction = 0.03f` and `fResistance = 2e-5`.

### 1.2 The algebra in plain steps

`fEnergy` is energy per kilogram (J/kg): kinetic energy `v²/2` plus potential energy `g·h`. Nothing in the code multiplies by a mass, so mass has already cancelled.

**Friction.** Solve the second line for `v²/2`:

`v²/2 = fEnergy − g·h − g·fFriction·s`, where `s` is the distance travelled along the track (`fTotalLength`).

So the energy available for speed goes down by `g·fFriction` for every metre travelled, on top of the usual `g·h`. The same energy would be lost climbing a height of `fFriction × s`. That is the first reading: *`fFriction` is the height lost per metre of track* (metres per metre, no unit). The second reading: a force `F` over distance `s` removes work `F·s` from a mass `m`, so per kilogram it removes `(F/m)·s`. Setting `(F/m) = g·fFriction` gives `F = fFriction × m × g`: *the retarding force is `fFriction` times the weight*, which is exactly the definition of a rolling-resistance coefficient. NoLimits Central's FVD++ friction thread gives the same reading in the program author's words, that the value "tells you how much energy the train loses per traveled track-length" ([forum thread](https://nolimitscentral.com/forum/topics/59?page=1), forum statement by the program's author, confidence medium).

**Resistance.** Each step takes `v³/F_HZ · fResistance` out of `fEnergy`. One step lasts `dt = 1/F_HZ`, so per second the energy loss is `fResistance · v³`. Power per kilogram is deceleration times speed, `a·v`, so `a = fResistance · v²`: *a drag deceleration proportional to the speed squared*. Its units: `a` is m/s², `v²` is m²/s², so `fResistance` is **1/m**. Compare physical drag: force = ½·ρ·Cd·A·v², so deceleration = (½·ρ·Cd·A / m)·v², giving

**`fResistance` = ½ · air density · drag area (Cd·A) / train mass.**

It is a drag-area-to-mass ratio, so it depends on the train's mass, unlike `fFriction`.

**Worked example with the defaults: level track, 20 m/s.** Height is constant, so only the two losses act.
- Friction: 0.03 × 9.80665 = **0.2942 m/s²**.
- Drag: 2e-5 × 20² = **0.0080 m/s²**.
- Total: **0.302 m/s²**; over one second the speed falls about 0.30 m/s.
- Cross-check on the energy form over 100 m: half the speed squared falls by about (0.2942 + 0.0080) × 100 = 30.2 m²/s², from 200 to 169.8, so the speed is sqrt(2 × 169.8) = **18.43 m/s** (friction alone would give 18.47). With the code's own line, 100 m at 20 m/s is 5 s, i.e. 5000 steps, and each step removes `20³/1000 × 2e-5 = 1.6e-4` J/kg from the energy at the start, which agrees with 0.008 × 20 = 0.16 W/kg.
- In slope terms: 0.03 is a slope of atan(0.03) = 1.72 degrees, the same figure the program's author quotes (forum thread above, medium).

This reading was checked by hand only; I did not run the program.

### 1.3 Things the model does not do (derived from the code)

- **The height is that of a point at 0.9 of the heartline offset** (`vPosHeart(0.9*fHeart)`, and `fPosHearty(0.9*...)` at the start). `fHeart` is the heartline's distance from the track; the 0.9 point is a stand-in for the centre of mass of the riders and train. Two things follow. First, the energy bookkeeping follows that point's height, so through an inversion (the point swings above and below the rail) the speed follows the point, not the rail. Second, an empty train's centre of mass sits lower than a full one's (riders raise it), and the model has one point for both. How much lower is not published; I found no source (section 5).
- **The distance in the friction term is the heartline's length at `fHeart`** (`fTotalLength`, built from `vPosHeart(parent->fHeart)` steps), not the rail's length and not the length of the 0.9 point. On a valley or a tight turn the lengths differ slightly (derived, a few percent at most for the radii in R2).
- **Friction does not scale with the force felt.** Real rolling resistance is the coefficient times the force pressing wheel against rail, which is about 4 times the weight in a 4 g valley and nearly nothing in airtime (derived from the definition). The program charges the same per metre everywhere. Over a whole ride the load averages out near one weight, but the loss is shifted from the valleys to the crests compared with a real train. For a stall check at a crest this error is on the safe side only if friction before the crest is a bit high; the check should not rely on it.
- **No speed-dependent rolling term and no hysteresis.** Real wheels' resistance varies with speed and temperature (section 2); the program's is constant.
- **Nothing depends on mass,** so it cannot represent an empty train against a full one:
  - Rolling resistance on the wheels is proportional to weight, so to first order the coefficient is the same empty and full (derived). The parts that do not scale with weight, such as grease drag in the bearings, seals, magnetic-brake drag when empty, are a larger share of a light train's energy. Practitioner sources say empty trains are more affected by brake drag and grease ([bgtsafari, general-audience blog](https://bgtsafari.com/2026/08/11/roller-coaster-testing-includes-water-filled-test-dummies-and-kids-trying-to-wiggle-out-of-restraints/), confidence low).
  - Air drag per kilogram is inversely proportional to mass. A train 35 percent lighter (an assumed empty against full ratio of 4,500 to 7,000 kg) has a `fResistance` about 55 percent higher.
  - The centre-of-mass height differs (above).

---

## 2. Real values

How each number was obtained is in brackets: measured, fitted, assumed, quoted.

### 2.1 Rolling resistance of coaster wheels

| Value | What | Source | How obtained | Confidence |
|---|---|---|---|---|
| 0.009 to 0.018 (of the supported load) | Polyurethane and nylon wheels on steel rail, "typical" | [Coaster101, Coasters-101 wheel design](https://www.coaster101.com/2011/10/24/coasters-101-wheel-design/) (enthusiast article by Nick Weisenberger, 2011, updated 2024); repeated, with the same formula `F = R·M·g`, by [Coaster Me This](https://coastermethis.com/friction-affect-roller-coaster/) | Quoted; no method or test stated | Low to medium |
| 0.010 to 0.017 | Same wheels | Two US patents on roller coaster wheel assemblies (6,375,243 and 6,598,919), via a web search summary only. I could not read the scans myself | Quoted in the patents; basis unknown | Low (second-hand) |
| 0.0005 to 0.002 | Steel wheel on steel rail (railway) | A simulation page's remark, and general engineering knowledge; not a coaster source | Quoted | Low |
| 0.03 | Generic 95A polyurethane caster wheel on a hard floor | [CasterHQ](https://casterhq.com/blogs/engineering-specifications/rolling-resistance) | Industrial data sheet value, not for coaster wheel geometry | Low relevance |

Findings:
- The only coaster-specific range is **0.009 to 0.018**, a factor of two. Nylon is harder and rolls with less resistance and a rougher ride; polyurethane is softer, smoother and rolls with more resistance (Coaster101 and Coaster Me This, qualitative).
- Those figures are for the road wheels alone. A coaster carries side-friction and up-stop wheels as well, which add load only in turns and in airtime. Together with bearings and seals this plausibly pushes the effective whole-train coefficient above the road-wheel value. That is my reasoning, not a published number.
- **Wooden coasters.** I found no published rolling-resistance number for wooden-coaster trains. Forum claims are that wooden trains run with less friction than steel ones because of looser wheel geometry (TylerE and maxamaxam in a [CoasterCrazy thread, 2010](https://www.coastercrazy.com/forums/newton-2-friction-settings-t22740.html), forum opinion, low). Modern timber trains (GCI, Gravity Group Timberliner) ride on steel-strap-topped wood; whether they use polyurethane or steel wheels was not established (section 5).

### 2.2 How it changes (temperature, compound, load, lubrication, track)

- **Temperature.** Every operator source says cold raises friction: wheel against rail, and the grease in the bearings. [McGowan Allied](https://mcgowanallied.com/ride-maintenance/) (a maintenance contractor's article): cold weather "creates increased friction", grease may not warm enough, and newer ride computers block operation below 40 F (4.4 C). Confidence medium for the direction, none for the size. No source gave a number per degree for coaster wheels. The one figure found was for pneumatic road tyres, about 1 percent more rolling resistance per degree C of cooling, taken from a literature review that I could only read through a search summary ([KTH literature study, 2022](https://kth.diva-portal.org/smash/get/diva2:1678831/FULLTEXT01.pdf)). It is for a different material and geometry; use as an order of magnitude only (20 C of cooling would give about 20 percent more), confidence low.
- **Wheel compound.** A general-audience site says parks may fit nylon on cold mornings to avoid valleying and polyurethane when warm ([Attractions Near Me](https://www.attractionsnearme.co.uk/ride-safety/roller-coaster-wheels/); confidence low) and Coaster Me This says parks may mix wheel types, or test one type and switch after break-in. This is the one direct statement that the compound is an operational lever; it is not backed by figures.
- **Load.** Rolling resistance scales with load, so a full train rolls with about the same coefficient as an empty one, with the non-proportional terms above. A forum reader and the author agreed that friction rises with weight ([Coaster101, speed article](https://www.coaster101.com/2010/09/27/coasters101-what-influences-train-speed/), qualitative).
- **Lubrication.** The same article: too little grease speeds the train and too much slows it, even to a stop; grease is thick when cold and thin when hot, so a train is slowest empty in the cold and fastest full in the heat. Track lubrication is mostly a wooden-coaster practice. Qualitative, low; one anecdote of grease on the first third of a lift hill each morning at one park.
- **Break-in.** New ride, new wheels and track run slower and pick up speed as wheels wear in and bearings warm. At one named coaster early settings had to be changed because heat made the train faster (Wikipedia on Time Traveler and Mental Floss, via a search summary; low).
- **Track condition and wind.** Strong headwinds can stall a train at a crest, which is why parks close rides in high wind (Coaster101 speed article; qualitative).

### 2.3 Air drag of coaster trains

- A frontal-area drag model, `F = ½ρ·Cd·A·v²`, is standard. Published coaster numbers are teaching values, not measurements:
  - Cd about 0.8 for a traditional train, with the note that it differs by train design: [Mid-America Adventure physics day student manual](https://midamericaadventure.enchantedparks.com/wp-content/uploads/sites/8/2026/04/physics-day-student-manual-st-louis-2024_rev2.pdf) (an educational handout; assumed value, low).
  - Cd 0.8 to 1.2 and frontal area 3 to 5 m²: [World of Physics blog](https://world-of-physics.com/blog/physics-of-roller-coasters-loops-drops-g-forces/) (blog; assumed, low).
  - Cd 1.8, borrowed from a passenger-train table, admitted to be a stand-in: [UAF student project](http://ffden-2.phys.uaf.edu/webproj/212_spring_2015/Zachary_Milbradt/zachary_milbradt_212/drag_force.html) (assumed, low).
- Coaster101 notes that a single car is hit harder by drag than a long train because it has less weight for the same frontal area, and that on fast twisting track cars can fan out and show more area (qualitative).
- Air density is about 1.20 kg/m³ at 20 C and about 1.31 at -10 C (ideal-gas scaling, derived), so a cold morning adds up to 9 percent drag compared with 20 C.

**Numbers I derived** (assumptions marked; train masses are round numbers I chose, not published values: 7,000 kg full, 4,500 kg empty, a difference of about 2,400 kg, i.e. 32 riders at 75 kg):

| Case | Cd·A (m²) | ρ (kg/m³) | Mass (kg) | `fResistance` (1/m) | Deceleration at 20 m/s | at 30 m/s |
|---|---|---|---|---|---|---|
| Full, low drag | 1.5 | 1.2 | 7,000 | 1.3e-4 | 0.05 | 0.12 |
| Full, nominal | 3.0 | 1.2 | 7,000 | 2.6e-4 | 0.10 | 0.23 |
| Empty, nominal | 3.0 | 1.2 | 4,500 | 4.0e-4 | 0.16 | 0.36 |
| Empty, cold air, large | 4.5 | 1.3 | 4,500 | 6.5e-4 | 0.26 | 0.58 |

(deceleration in m/s²). The Cd·A range of 1.5 to 4.5 m² is my assumption from the frontal-area and Cd values above; a train shelters its rear cars, which argues for the low end, and the fanning-out remark for the high end. Confidence low.

Rolling resistance for comparison: 0.009 to 0.018 gives 0.09 to 0.18 m/s² on level track; the program's 0.03 gives 0.29. **Drag equals rolling resistance at v = sqrt(μ·g / k).** With 0.015 and 2.6e-4 that is 24 m/s; with 0.03 and 4e-4 it is 27 m/s. So on a fast ride the drag, not the wheels, dominates the loss at speed, and the program's default 2e-5 understates it.

### 2.4 What measured rides say

- Pendrill and Eager (Physics Education 55, 2020) measured forces on a real ride with accelerometers, and say air and rolling losses are "surprisingly small", so speeds can be got from the height drop ([IOP page](https://iopscience.iop.org/article/10.1088/1361-6552/aba732)). I could read only the search summary, not the paper; it gives no coefficients. Confidence low. Her loop paper on Liseberg's Kanonen ([PDF](https://physics.gu.se/LISEBERG/eng/pendrill_loop_2013.pdf)) timed the train over the top; I could not read the PDF, so no number was taken.
- **No measured coaster rolling or drag coefficient was found in the peer-reviewed physics-education literature.** Park "physics day" workbooks have students estimate energy lost between two points; the Busch Gardens one gives an example of about 9 percent and says its figures are not accelerometer-tested ([Busch Gardens workbook](https://buschgardens.com/tampa/-/media/commercial/busch-gardens-tampa-bay/files/physics-day/new-2024-2025---physics-day-workbook-(2).pdf), via a search summary, low). A blog says friction and drag take 15 to 25 percent of the initial potential energy by the end of a ride ([World of Physics](https://world-of-physics.com/blog/physics-of-roller-coasters-loops-drops-g-forces/), unsourced, low).
- One forum account, a former operator on a CoasterBuzz thread, said the control system showed a full train about 6 to 8 mph (2.7 to 3.6 m/s) faster than an empty one at the same mid-course points ([CoasterBuzz](https://coasterbuzz.com/Forums/Topic/roller-coaster-physics-question), anecdote about one unnamed ride, via a search summary; low). It is the only number found for empty against full.

### 2.5 What simulators assume

- **NoLimits 2 has friction and air-resistance settings only in the Professional edition.** There the friction can be set to custom values or turned off, as can air resistance. The NL2 documentation of the two fields was not found; the best source is a fan reverse-engineering thread ([NoLimits Central, "The Official(?) FVD++ Friction Guide"](https://nolimitscentral.com/forum/topics/59?page=1); forum, fitted values, confidence medium for steel and wood, low for drag):
  - FVD++ 0.75 changed its friction input to NL2's scale, which is why older values are about ten times larger. 
  - Designer values posted in 2018 (CoolioJoe, "not set in stone"): B&M hyper 0.029, B&M inverted 0.031, Gerstlauer Infinity 0.030, Mack launched 0.030, Intamin rocket 0.030, Gravity Group Timberliner 0.021.
  - Fan fits listed by Archy (mostly before the scale change, in the same units as above): GCI Millennium Flyer and Gravity Group wooden trains 0.0157; B&M and most Vekoma and Intamin sit-down trains 0.029 to 0.034; one-car Maurer X 0.047, three cars 0.016, so friction changes with the number of cars.
  - 2024 tests by Aeneas (200 m of track at three start speeds): **steel 0.026, wood 0.017** under NL2 and NL2.2; the same fitting gave a drag value of 1e-4 for NL2.2 and 2e-5 for the older NL2. The thread labels it "m/s²"; by the dimensions in section 1 it is 1/m.
  - AllegroCrab (2019) estimated the NL2 air-resistance parameter at about 1e-4 against the program's 2e-5 default, and called its effect "negligible" (about 1 km/h over a 1 km layout). My own arithmetic does not reproduce that: at a steady 25 m/s, 1e-4 × 25² × 1,000 m is 62 J/kg, about 6 m of height, which is more like 10 km/h off a 25 m/s train. The forum claim is probably too small by that arithmetic; I did not run NL2 to check.
- An older CoasterCrazy post (TylerE, 2010) said 0.0195 "works ok for woodies" in Newton/NL1 units, which fits the later 0.017 to 0.021 wooden-coaster fits (forum, low).
- The program's default 0.03 equals the NL2-style steel values above (0.026 to 0.031), and 2e-5 equals NL2's old air-resistance fit, so **the defaults correspond to a typical steel coaster in NoLimits 2, not to measured physics.**

---

## 3. How rides are commissioned for the worst case

- **Operating temperature limits.** Practitioner sources (a mix of manufacturer rules, park signs and forums) put minimum operating temperatures between about 0 and 10 C (32 to 50 F), differing by ride: Silver Dollar City's posted minimums run from about 32 F (Outlaw Run) to 41 F (Time Traveler), and Universal Orlando lists 35 F (VelociCoaster), 40 F (Hagrid's) and 50 F (Incredible Hulk). The sources are forum posts and a touring blog read through search summaries ([CoasterBuzz minimum temperature thread](https://coasterbuzz.com/Forums/Topic/minimum-operating-temperature), [TouringPlans](https://touringplans.com/blog/touring-universal-orlando-in-cold-weather/)); I could not open the first (it returned an access error) and have not seen a manufacturer document. An old forum post claims wooden coasters open from about 35 F and steel ones from 45 to 50 F (unverified; low). Newer ride computers refuse to run below 40 F (4.4 C) according to the maintenance article cited in 2.2 (medium). **The cold limit is therefore a controlled input of the ride, not a surprise,** and the check's cold case should sit near the ride's stated minimum.
- **Valleying and rollback.** The failure the check looks for: a train that has too little energy to clear a crest, which parks call valleying (the train stops in a low spot between hills) or rollback. Coaster Me This says designers try to prevent it with conservative energy modelling and testing, with block systems, brake runs sized for the worst case and trim brakes to cut the peak speed (no source or numbers; low). Coaster101's "speed envelope": the slowest case is an empty train in cold weather, the fastest a full train in the heat (qualitative, low). Wind adds to the picture: a headwind at a crest can stall a train.
- **Test runs.** New rides are tested in stages: a pull-through test by winch, the train empty, then trains loaded with water-filled dummies for dozens or hundreds of laps with sensors and cameras, ending with an inspection ([bgtsafari](https://bgtsafari.com/2026/08/11/roller-coaster-testing-includes-water-filled-test-dummies-and-kids-trying-to-wiggle-out-of-restraints/), a general blog; the same description from a second source via a search summary; medium for the practice, nothing on the numbers). Coaster101 adds that dummies matter especially when breaking in a new ride in cold, wind or with new components. Empty runs cover the stall case; loaded runs cover over-speed.
- **Why new rides run slower.** Wheels and bearings are stiff and the grease is thick until the parts have run in (qualitative; see 2.2). Per the same sources, this passes within days to weeks of operation.
- **Margin.** No numeric design margin was published in any source I could find: not for speed over a crest, not for energy, not for the empty-train case. The only margin-like statements are structural (brake runs, block sections, trim brakes). I did not find, for example, a rule such as "keep at least X m/s over every crest". The ASTM and EN standards are paid and were not consulted (section 5).

---

## 4. Recommendation for the check

### 4.1 What the program's defaults mean, and the approach

The check (A6) rides the finished shape (P1) with different friction and resistance and reports the reserve. The design computation must not be re-run with other friction, because it would reshape the hills (FR-13, architect's note). Two choices:

1. **Make the nominal case the project's own `fFriction` and `fResistance`,** so that the nominal run reproduces the design (P1's own test), and make the low and high cases multiples of them with the floors below. This keeps the check coherent with whatever the owner already calibrated against NoLimits.
2. **Show the absolute values it used,** so the owner can see them, and let the owner overwrite them. Both low and high values below are judgement.

### 4.2 Proposed values

Friction is in m/m; resistance is in 1/m. "Low" is the fast case (warm, broken-in, full train: where speed is highest); "high" is the slow case (cold, new, empty: where a stall happens). The band follows the stall case first.

| Quantity | Type | Low | Nominal | High | Confidence and basis |
|---|---|---|---|---|---|
| `fFriction` | Steel | 0.020 | 0.030 | 0.040 | Nominal: medium; NL2 fits 0.026 to 0.031 ([NL Central](https://nolimitscentral.com/forum/topics/59?page=1)) and the program default. Low: low; closer to the physical 0.009 to 0.018 ([Coaster101](https://www.coaster101.com/2011/10/24/coasters-101-wheel-design/)) plus bearings. High: low; nominal plus a third, my judgement for a cold, empty train, order of magnitude supported by the tyre-literature 1 percent per degree (20 C colder is +20 percent) plus the grease and the empty-train terms |
| `fFriction` | Wood | 0.014 | 0.019 | 0.026 | Nominal: medium; NL2 fit 0.017, forum 0.0195, Timberliner 0.021, Millennium Flyer 0.0157 ([NL Central](https://nolimitscentral.com/forum/topics/59?page=1), [CoasterCrazy](https://www.coastercrazy.com/forums/newton-2-friction-settings-t22740.html)). Low and high: low; similar factors (0.74 and 1.37 of nominal) to steel |
| `fResistance` | Low case | 2e-5 | | | Low; the program default and NL2's older fit |
| `fResistance` | Nominal | | 1.3e-4 | | Low; NL2.2 fit 1e-4 (forum) and the full-train physical estimate of 1.3e-4 to 2.6e-4 (section 2.3) |
| `fResistance` | High (empty, cold) | | | 4e-4 to 6.5e-4 | Low; derived in 2.3 from assumed Cd·A of 3 to 4.5 m² and an empty mass of 4,500 kg. Replace it when FR-11 gives the real train |

Notes on the table:
- **Project-relative rule.** If the project's own `fFriction` is, say, 0.026, use 0.026 × (0.67, 1, 1.33) rather than the absolute row; use the absolute rows only when the project value is unknown.
- **Do not count drag twice.** The NL2 fits fit friction and drag together on one set of tests, so a high friction already contains some drag. When the high drag case is run on top of a nominal friction that was fitted with drag in it, the stall case is a little too pessimistic. That is acceptable for a safety check but should be said in the report.
- **Wood against steel.** The wooden values lean on NL2's fits and on forum opinion; I did not find an engineering source that says a real wooden train rolls more freely than a steel one.
- **What the defaults correspond to.** `fFriction` 0.03 and `fResistance` 2e-5: the middle of the NL2 steel scale and NL2's older drag fit. Physically, 0.03 is about 2 to 3 times the road-wheel coefficient and 2e-5 is 6 to 30 times below the drag estimate; the first probably absorbs part of the second at moderate speeds. At 30 m/s and above the default understates the loss.

### 4.3 What the check should report

For each case (low, nominal, high), along the whole ride:
1. **The speed band** along the track: lowest and highest speed at each position, in m/s (and km/h or mph as the project's unit).
2. **At each crest** (each local maximum of the 0.9 heartline point's height): the speed over it in the high case (the lowest speed), and the **reserve in metres of height**, `v²/(2g)`, meaning the extra hill it could still climb, and as a percentage of the energy the ride has spent up to that point. Both are exact outputs of the model, with no tuning.
3. **The first point where the high case stalls or rolls back:** where `fEnergy − g(h + s·fFriction)` becomes negative, so that the square root in the code would fail. Report its distance, the crest, and how short of energy it is (as metres of height and as a drop in friction that would have cleared it).
4. **The sensitivity,** i.e. the change in crest speed for a 10 percent rise in friction, so the owner sees which crest is fragile.
5. **A pass mark chosen by the owner.** No published design margin exists (section 3), so the default should be a visible placeholder (for example "warn below 3 m/s over a crest in the high case"), labelled as the program's choice, not a standard. A point model needs more than zero speed: a real train that is 15 to 20 m long has cars still climbing while others descend (FR-11), so a point stalling at 0 m/s may in reality get over or may roll back from the middle; that question belongs to the train model.
6. **State the assumptions** on the same page: the three sets of values, the model's blind spots (constant friction per metre; the 0.9 point; no train length), so that a pass is not read as a guarantee.

Recommended calibration, not research: the owner can check any design against a real ride whose lap time or speed trap numbers are known, and adjust the nominal friction until the model matches; that single check is worth more than the table above.

### 4.4 Empty against full, once the program follows a whole train (FR-11)

With a whole train the two numbers can be built from physical data instead of lumped:
- `fFriction` = the wheel coefficient μ + (fixed loss force F0 / weight). Per case, F0 (grease drag, seals, magnetic-brake drag when empty) is a fixed force, so its share grows when the train is lighter.
- `fResistance` = ½ρ·Cd·A / m, evaluated separately for the empty and full mass.
- Each car gets its own heartline point, so that the centre of mass of the empty train is not the same as that of the loaded one.
- Needed inputs (not published for any train, per R1): car mass empty and loaded, number of cars, Cd·A of the train, and seat-load assumptions. The owner may have to enter them; the catalogue has none yet.
- Until then, the high case in 4.2 stands in for the empty train, and the low case for the full.

---

## 5. Could not establish

- **A measured rolling-resistance coefficient for coaster wheels** from a test (coast-down, accelerometer or tribometer). The 0.009 to 0.018 range comes from an enthusiast article without a method, and the patent figures (0.010 to 0.017) were read only through a search summary.
- **Any manufacturer figure:** wheel compound, rolling coefficient, Cd·A of a train, train mass empty or loaded, or a stated design margin. Not found for Intamin, B&M, Mack, Vekoma, Gerstlauer, GCI, Gravity Group, RMC.
- **A temperature curve for coaster wheels** (polyurethane or nylon) or for coaster grease; only the direction (colder is slower) and a tyre-literature 1 percent per degree for a different material.
- **Wooden coasters:** whether trains use steel or polyurethane wheels on the steel strap, and any measured coefficient. The statement that wood rolls freer than steel rests on forum opinion.
- **Pendrill's papers:** I found no measured loss figures; the Physics Education 2020 paper and the Kanonen loop PDF could not be read in full by my tools (summary and an unreadable PDF).
- **NoLimits 2's own documentation** of its friction and air-resistance fields, how it treats train mass, and whether it models a wheel-load-dependent resistance. Only fan reverse-engineering was found; I did not run NL2.
- **The size of the empty-against-full difference** on any real ride beyond one anecdote (6 to 8 mph).
- **Operating minimum temperatures from manufacturer documents;** the figures here come from forums and a touring blog, and the CoasterBuzz thread returned an access error when opened directly.
- **Designers' margins** (speed over crests, energy reserve) and any rule for them. ASTM F2291 and EN 13814 are paid standards; I did not consult them and cite no clause.
- **Test-dummy details:** the masses used and the number of cycles per ride, beyond "dozens or hundreds of laps".
- **Where the train's centre of mass lies** for empty and for loaded trains relative to the 0.9 heartline point.
