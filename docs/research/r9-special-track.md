# R9: Special track (lifts, launches, brakes, swing launches, switches)

Research for FR-16 (roadmap pieces P3 lifts, launches and brakes; P4 swing launches; P5 switches and transfer track; and O1 for the station and transport sections). Written 2026-11-11; every web link was accessed 2026-11-11. Confidence is high, medium or low per claim. "Help" means the official NoLimits 2 (NL2) help at `https://nolimitscoaster.com/nolimits2/help/`. R4 already covers what NL2 sections and blocks are; this note builds on it and does not repeat it.

How to read the numbers. Ride speeds, times and lengths come mostly from Wikipedia, Coasterpedia and park or maker pages, which in turn quote the maker. Almost no maker publishes a thrust curve, so **every average acceleration below is derived by me from a published speed and time** (average = speed change divided by time), and the arithmetic is shown. A derived average is not a peak.

## Summary

1. **Lifts.** A chain lift moves at a constant speed: about 1.2 to 1.5 m/s on older rides (forum, low), 4.3 m/s on a modern big chain lift (Leviathan, 15.5 km/h, medium), and 6 to 8 m/s on a cable lift (Millennium Force about 13.5 to 15 mph, Skyrush 26 ft/s; medium to low). Typical angle 20 to 45 degrees; vertical lifts exist. At constant speed the rider is pressed into the seat back with g times sin of the slope: 0.5 g at 30 degrees, 0.71 g at 45, 1.0 g at 90 (derived, high). Anti-rollback is required wherever a train is driven (EN 13814 as quoted in a thesis, medium).
2. **Launches.** Hydraulic and LSM launches of the big rides average 1.0 to 1.7 g over 2 to 5 s (Xcelerator 1.63 g, Kingda Ka 1.67 g, Formula Rossa 1.39 g, Red Force 1.02 g; derived, medium). Compressed air is the extreme at 3.3 g (Do-Dodonpa). Gentler family and themed LSM launches average 0.4 to 0.9 g. The thrust is **not constant**: an LSM is limited by power at speed so the force falls as the train gets faster (a measured 2.4 MW LSM started near 1.5 g, average about 0.9 g), and a hydraulic launch loses pressure as the gas expands (measured, high).
3. **Brakes and trims.** A magnetic brake pulls in proportion to speed, so it brakes hardest at the entry and fades to nothing; it cannot hold a train. A friction (fin or pinch) brake has a force set by clamp force and can hold. Design limits quoted from EN 13814: 0.5 g in normal operation, 0.7 g in an emergency (medium). The rider limit in ASTM F2291 is 1.5 g sustained (R3). A speed-proportional trim takes off the **same speed difference whatever the entry speed** (derived, medium), which is the right way to specify a trim: "take off 5 m/s".
4. **Swing launches.** Top Thrill 2 passes: 74 mph forward, 101 mph backward up the spike, 120 mph forward over the top hat (medium). In height terms (v squared over 2g) that is 56 m, 104 m and 147 m of energy. The track is shaped for the last, fastest forward pass; the earlier passes run at 38 percent and 71 percent of its centripetal force on the same curves (derived).
5. **Switches.** Kinds: sliding table, rotating or pivoting table, vertical rotation, a drop track (Hagrid's, 5.2 m), and the new high-speed "quick switch" passed with the train rolling (Toutatis, Pantheon, Hagrid's, Top Thrill 2). No source gives a geometry limit or a cycle time. NL2 has switches and transfer tables, but only as pre-built special tracks.
6. **NL2.** Lifts, brakes, transport, launch (an option, not a section type), switches and transfer tables all exist with speed, acceleration and deceleration settings, but **the help gives no units** and no default values. No import format carries a section type (not `.nl2elem`, not the spline CSV). After an export the owner builds every special section again by hand in NL2.
7. **Proposed P3 defaults** are in section 7: chain lift 3 m/s (maximum 8), launch average 1.0 g (soft maximum 1.7), block brake 0.5 g (maximum 0.7), trim 0.3 g (maximum 1.0), transport 2.8 m/s.

---

## 1. Lift hills

### 1.1 Speeds

| Item | Value | Source | Confidence |
|---|---|---|---|
| Chain lift, modern large steel (Leviathan, Canada's Wonderland, 93 m lift) | 15.5 km/h = 4.3 m/s | [Wikipedia: Leviathan](https://en.wikipedia.org/wiki/Leviathan_(Canada's_Wonderland)) | medium (encyclopedia, no maker figure) |
| Chain lift, older AC-driven rides | 4 to 5 ft/s = 1.2 to 1.5 m/s | one poster on an [Ultimate Rollercoaster forum thread](https://www.ultimaterollercoaster.com/forums/roller-coasters-theme-parks/165543) | low (forum) |
| Fastest chain lifts | about 9 mph = 4.0 m/s | same thread, another poster | low |
| Cable lift, Millennium Force (Intamin, 45 degree lift, 310 ft) | 13.5 mph (6.0 m/s) from [Coaster101](https://www.coaster101.com/2020/08/31/20-in-20-millennium-force-at-cedar-point/); 15 mph (6.7 m/s) from [CoasterCrazy](http://www.coastercrazy.com/16899/Millennium-Force) and the forum thread above | medium to low; no maker figure |
| Cable lift, Skyrush (Intamin, 200 ft) | 26 ft/s = 7.9 m/s | [Wikipedia: Skyrush](https://en.wikipedia.org/wiki/Skyrush) (as returned by the search; page not opened) | low |
| Transport (tyre or chain) in stations and brake runs | about 10 km/h = 2.8 m/s | [Väisänen, Design of Roller Coasters, Aalto 2018](https://aaltodoc.aalto.fi/server/api/core/bitstreams/5e6a206c-13c5-41b6-87f6-da0bc4ec7aef/content), section 2.4 | medium |
| Chain lift speed range quoted for a video game's builder | 4 to 14 mph | [Steam guide](https://steamcommunity.com/sharedfiles/filedetails/?id=887920899) | not used (game guide) |

Cross-check (my arithmetic). Millennium Force: a 310 ft (94.5 m) rise at 45 degrees is a lift 94.5 / sin 45 = 133.6 m long. At 6.03 m/s that is 22.2 s, which matches "a little over 20 seconds" in Coaster101. So the 13.5 mph figure is consistent; the 15 mph figure (6.7 m/s) would give 19.9 s, also close. Both are plausible, so the cable lift speed is known to about 10 percent.

Some rides slow the chain toward the top to reduce chain stress and positive g ([Wikipedia: Goliath, Six Flags Magic Mountain](https://en.wikipedia.org/wiki/Goliath_(Six_Flags_Magic_Mountain)), medium); a lift with a variable speed is therefore not exotic. Newer drives are speed-regulated, so a following train can slow when the block ahead is occupied ([forum](https://www.ultimaterollercoaster.com/forums/roller-coasters-theme-parks/165543), low).

### 1.2 Angles

- Early lifts were shallow, 20 to 30 degrees; today "any angle" is possible ([Coaster101: lift hills](https://www.coaster101.com/2013/09/25/coasters-101-lift-hills/), medium). A forum view puts 26 to 30 degrees as usual and 45 degrees as very steep for a conventional ride ([Frontier forum](https://forums.frontier.co.uk/threads/tutorial-making-a-realistic-lift-hill.489819/), low). Millennium Force's cable lift is 45 degrees and Goliath (Six Flags Great America) is a 45 degree wooden lift built in a small space ([Wikipedia](https://en.wikipedia.org/wiki/Goliath_(Six_Flags_Great_America))). Vertical chain lifts exist: Fahrenheit (121 ft, [Wikipedia](https://en.wikipedia.org/wiki/Fahrenheit_(roller_coaster))), HangTime (150 ft, [Wikipedia](https://en.wikipedia.org/wiki/HangTime_(roller_coaster))), Impulse (98 ft).
- Confidence: the common range 20 to 45 degrees is **medium to low** (no maker figure was found). Vertical lifts are well documented (high).
- Lifts and other track-mounted machinery need a straight piece of track: "most lift hills allow only pitch change", with yaw changes rare (Väisänen, layout chapter, medium). NL2 treats lift straights the same way and has a "Make Straight 2D" command for them ([Track Tab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html), high).

### 1.3 How the train engages, rides and leaves a chain lift

All from Väisänen section 2.4 and the [Wikipedia lift hill page](https://en.wikipedia.org/wiki/Lift_hill) unless stated; confidence medium.

- The chain runs in a trough between the rails and loops over cog wheels driven by an electric motor. A hinged **chain dog** under the car hooks a chain pin. Because the dog is hinged, it rides over the first pin as the train arrives and cannot slide back ([Crazy Worm description](https://www.huvipuisto.net/ukwormtech.html), low: a kiddie ride). The train therefore arrives at a speed at or below the chain speed and is picked up by a small bump when the dog catches. There is no published figure for that bump; the speed match is a simple catch-up, not a controlled merge (low, my reading).
- The chain only pushes. At the crest the train goes over the top faster than the chain and the dog simply drops away. This is why a chain lift needs no controlled release.
- **Anti-rollback**: a ratchet rack beside the chain with a pawl on the train. The standard requires, wherever a train is driven by an external device, either safety devices or automatically acting brakes to prevent running backwards (EN 13814 clause 5.4.3.5, quoted by Väisänen; the standard is paid and is not reproduced here). Silent versions use friction or eddy currents (Tulireki, Wildfire). Intamin's vertical lifts use two chains that can hold or move the train; Gerstlauer and Zamperla use permanent-magnet stators that let the train descend slowly (all Väisänen).
- **Tyre-drive (friction wheel) lifts**: motorised wheels pinch a plate under the train, can push or hold it, and need no anti-rollback chain ([Wikipedia](https://en.wikipedia.org/wiki/Lift_hill), medium). Example: Rabalder at Liseberg (Väisänen figure 10). No speed was found.
- **Linear motor lifts**: Lightning Rod (Dollywood) launched its train up the lift, 0 to 45 mph in 7.7 s before reaching 192 ft ([Wikipedia: Lightning Rod](https://en.wikipedia.org/wiki/Lightning_Rod_(roller_coaster)) via search summary, low to medium). Derived average: 20.1 m/s over 7.7 s = 2.6 m/s squared = 0.27 g along the track. The LSM lift was replaced by a chain lift in 2024 (same source).

### 1.4 The along-track force a rider feels on a lift

Derived (physics, high). At constant speed on a slope of angle θ the train does not accelerate, so the seat supports the rider against gravity only. Per unit weight, the component along the track is sin θ, **pushing the rider forward into the seat back** (the "eyes back", +Gx direction of R3), and the component pressing into the seat is cos θ.

| Slope | Along track (+Gx) | Into the seat (Gz) |
|---|---|---|
| 20 degrees | 0.34 g | 0.94 g |
| 30 degrees | 0.50 g | 0.87 g |
| 45 degrees | 0.71 g | 0.71 g |
| 90 degrees | 1.00 g | 0.00 g |

When the chain picks the train up, or the lift speeds up or slows, a/g adds to or subtracts from the +Gx. The only measured lift data found is [Pendrill 2013, Eur. J. Phys. 34, 1379](https://physics.gu.se/LISEBERG/eng/pendrill_loop_2013.pdf), which notes that lifts, launches and brakes are the places where an along-track force exists. A lift at 30 degrees with a 0.5 g load sits well inside the R3 +Gx limit (4 g beyond a few seconds, 2.5 g sustained).

---

## 2. Launches

### 2.1 Types

| Type | How it works | Example | Source | Confidence |
|---|---|---|---|---|
| Hydraulic | Oil pumps compress nitrogen in accumulators to about 300 bar; on release the oil spins hydraulic motors that wind a cable pulling a catch-car. Pressurising takes about 45 s, a launch 2 to 4 s. Seven 500 hp pumps, four accumulators and 32 motors on Kingda Ka (peak 20,800 hp, 15.5 MW); one pump, one accumulator and eight motors on a 47 mph ride. | Xcelerator, Kingda Ka, Formula Rossa | [Wikipedia: Accelerator Coaster](https://en.wikipedia.org/wiki/Accelerator_Coaster), [Pendrill 2008](https://physics.gu.se/LISEBERG/eng/pe8_5_003.pdf) | high for the principle, medium for the counts |
| LSM (linear synchronous motor) | Stator coils on the track, permanent magnets (or coils) on the train; the track field travels and the train locks to it. Thrust controlled by current. More than 700 stator modules on Falcons Flight. | Taron, VelociCoaster, Red Force | [Veltman et al., Maglev 2002](https://www.researchgate.net/publication/228555976_Control_of_a_24_MW_Linear_Synchronous_Motor_for_launching_roller-coasters) (read through its abstract in a search summary; the page itself returned 403), [Wikipedia: Falcons Flight](https://en.wikipedia.org/wiki/Falcons_Flight) | medium |
| LIM (linear induction motor) | Coils on the track induce current in a reaction plate on the train. Used by older Premier rides. | Mr. Freeze (116 LIMs) | [Six Flags wiki](https://sixflags.fandom.com/wiki/Mr._Freeze:_Reverse_Blast), [Wikipedia](https://en.wikipedia.org/wiki/Mr._Freeze_(roller_coaster)) | medium |
| Compressed air | Air cylinder drives a catch-car. Record accelerations. | Do-Dodonpa (closed 2024), Maxx Force | [Wikipedia: Do-Dodonpa](https://en.wikipedia.org/wiki/Do-Dodonpa), [Wikipedia: Maxx Force](https://en.wikipedia.org/wiki/Maxx_Force) | medium |
| Tyre (drive wheels) | Rows of motorised rubber wheels pinch a plate under the train. About 220 to 230 motors and 8 MW to launch one train on the Incredible Hulk. | Incredible Hulk Coaster, Zamperla and Gerstlauer tyre launches | [Coasterpedia](https://coasterpedia.net/wiki/Incredible_Hulk) | medium |
| Flywheel, spring, counterweight, cable | Stored energy released through a clutch or catapult. Older shuttle loops. | Revolution, Montezooma's Revenge | [Wikipedia: Launched roller coaster](https://en.wikipedia.org/wiki/Launched_roller_coaster) (flagged by Wikipedia as needing more citations) | low |

### 2.2 Example rides, published figures and the average acceleration derived from them

Average = (speed change in m/s) / (time in s); in g divide by 9.807. A constant-acceleration launch of time t reaches speed v in the distance d = v t / 2, listed as "distance if constant" for a cross-check against the published launch length.

| Ride (type) | Published | Average acceleration | Distance if constant | Published launch length | Source | Confidence |
|---|---|---|---|---|---|---|
| Xcelerator (hydraulic) | 82 mph (36.66 m/s) in 2.3 s | 36.66 / 2.3 = 15.9 m/s squared = **1.63 g** | 42 m | 157 ft = 48 m | [Wikipedia](https://en.wikipedia.org/wiki/Xcelerator) | medium |
| Kingda Ka (hydraulic) | 128 mph (57.22 m/s) in 3.5 s | 57.22 / 3.5 = 16.3 m/s squared = **1.67 g** (Coasterpedia states about 1.67 g) | 100 m | not found | [Wikipedia](https://en.wikipedia.org/wiki/Kingda_Ka), [Coasterpedia](https://coasterpedia.net/wiki/Kingda_Ka) | medium |
| Formula Rossa (hydraulic) | 240 km/h (66.67 m/s) in 4.9 s | 66.67 / 4.9 = 13.6 m/s squared = **1.39 g** (Wikipedia: up to 1.7 g) | 163 m | 400 m (park maintenance director) | [Wikipedia](https://en.wikipedia.org/wiki/Formula_Rossa) | medium |
| Red Force (LSM) | 180 km/h (50 m/s) in 5 s | 50 / 5 = 10.0 m/s squared = **1.02 g** (PortAventura: 1.35 g peak) | 125 m | not found | [Wikipedia](https://en.wikipedia.org/wiki/Red_Force_(roller_coaster)), [PortAventura](https://www.portaventuraworld.com/blog/en/red-force-height-speed-history) | medium |
| Do-Dodonpa (air) | 180 km/h (50 m/s) in 1.56 s | 50 / 1.56 = 32.1 m/s squared = **3.27 g** (sources give 3.3 to 4.3 g, probably the ride's total) | 39 m | not found | [Wikipedia](https://en.wikipedia.org/wiki/Do-Dodonpa), [GIGAZINE](https://gigazine.net/gsc_news/en/20170706-fujikyu-do-dodonpa/) | medium |
| Maxx Force (air) | 78 mph (34.87 m/s) in 1.8 s | 34.87 / 1.8 = 19.4 m/s squared = **1.98 g** | 31 m | not found | [Wikipedia](https://en.wikipedia.org/wiki/Maxx_Force) | medium |
| Superman Escape / Tower of Terror II (LSM) | 100 mph (44.70 m/s) in 7 s | 44.70 / 7 = 6.39 m/s squared = **0.65 g** | 156 m | 206 m tunnel (Dreamworld) | [Wikipedia](https://en.wikipedia.org/wiki/Tower_of_Terror_(Dreamworld)) | medium |
| Rock 'n' Roller Coaster (LSM) | 57 mph (25.48 m/s) in 2.8 s | 25.48 / 2.8 = 9.10 m/s squared = **0.93 g** | 36 m | 200 ft = 61 m | [Ultimate Rollercoaster](https://www.ultimaterollercoaster.com/coasters/rocknroller_disneymgm) | medium |
| VelociCoaster, launch 1 (LSM) | 0 to 50 mph (22.35 m/s) in 2 s | **1.14 g** | 22 m | not found | [Wikipedia](https://en.wikipedia.org/wiki/VelociCoaster) | medium |
| VelociCoaster, boost (LSM) | 40 to 70 mph (13.41 m/s change) in 2.4 s | 13.41 / 2.4 = 5.59 m/s squared = **0.57 g** | n/a | not found | same | medium |
| Incredible Hulk (tyres) | 0 to 40 mph (17.88 m/s) in 2 s | **0.91 g** | 18 m | tunnel about 150 ft | [Coasterpedia](https://coasterpedia.net/wiki/Incredible_Hulk) | medium |
| Mr. Freeze (LIM) | 0 to 70 mph (31.29 m/s) in 3.8 s | **0.84 g** | 59 m | not found | [Six Flags wiki](https://sixflags.fandom.com/wiki/Mr._Freeze:_Reverse_Blast) | low to medium |
| Hagrid's, launch 3 (LSM) | 0 to 32 mph in 3.5 s | **0.42 g** | | | [Coasterpedia](https://coasterpedia.net/wiki/Hagrid's_Magical_Creatures_Motorbike_Adventure) | medium |
| Hagrid's, launch 7 (LSM) | 0 to 50 mph in 4.0 s | **0.57 g** | | | same | medium |
| Lightning Rod, lift launch (LSM) | 0 to 45 mph (20.12 m/s) in 7.7 s | **0.27 g** | | 192 ft climb | see section 1.3 | low to medium |

Arithmetic check (two recomputed). Xcelerator: 82 mph x 0.44704 = 36.66 m/s; 36.66 / 2.3 = 15.94 m/s squared; 15.94 / 9.807 = 1.63 g. Red Force: 180 km/h = 50.0 m/s; 50.0 / 5 = 10.0 m/s squared = 1.02 g. Also: Dreamworld's 206 m tunnel versus 156 m of constant acceleration shows the published length is the whole run-out zone, not the powered part. The same holds for Formula Rossa (400 m listed against 163 m). **A published launch length is therefore an upper bound on the powered length. The speed and the time are the usable numbers.** Wikipedia also says only about three quarters of a hydraulic launch track is usable for launching, because the catch-car needs room to stop (about 20 m on a 100 km/h model).

A second-order cross-check: Rock 'n' Roller Coaster's 61 m against 36 m of constant acceleration suggests either a long run-out or a launch that is front-loaded (see 2.3).

### 2.3 How the acceleration is shaped over the launch

**Onset.** Both families build the thrust gradually, not as a step. A hydraulic launch begins when the valves open, about 5 s after the launch dog is released (Wikipedia: Accelerator Coaster); a measured Kanonen launch shows force dropping from the start, with wiggles from the cable winding onto the drum ([Pendrill 2008](https://physics.gu.se/LISEBERG/eng/pe8_5_003.pdf), via search summary and read PDF, high). For an LSM, the controller ramps current in about 0.2 s ([Veltman et al.](https://www.researchgate.net/publication/228555976_Control_of_a_24_MW_Linear_Synchronous_Motor_for_launching_roller-coasters): 1.5 g reached in 0.2 s; low to medium, abstract only).

**Hydraulic: the force falls during the launch.** The nitrogen expands as oil leaves, so pressure drops from about 300 to about 250 bar, and Pendrill's accelerometer traces on Kanonen and Speed Monster show the acceleration falling with it (high, measured). Wikipedia's Accelerator Coaster page calls it "constant acceleration" and its Launched roller coaster page says the opposite about LIM/LSM; the measured data and the physics side with Pendrill. Treat a hydraulic launch as **front-loaded with a modest fall (the pressure drop is about 17 percent)**.

**LSM: constant force, then constant power.** (Physics, with the cited paper.) The power a drive can deliver is limited by the voltage and current the converter and the stator supply. Power is force times speed, so the maximum force available is:

- up to a **base speed**, force can be held at its current-limited maximum (constant thrust, constant acceleration for a constant mass);
- above the base speed, the voltage limit binds and the force can only fall as 1/v (**constant power**; the same two-regime picture is described for motors in general in [this NASA linear-launcher report](https://ntrs.nasa.gov/api/citations/20080023418/downloads/20080023418.pdf), and "field weakening" above rated speed in [this motor paper](https://arxiv.org/pdf/1405.6815)).

The one coaster-specific source, a 2.4 MW LSM (Veltman et al., Maglev 2002; read through search summaries because the paper pages refused access), states that the train goes from rest to 25 m/s in a little under 3 s over about 40 m, with an initial acceleration of about 1.5 g reached in 0.2 s, and that acceleration falls beyond the speed of maximum power, giving an economic constant-power operation for most of the launch (low to medium). My arithmetic: average = 25 / 2.8 = 8.9 m/s squared = 0.91 g; constant acceleration would need 35 m, which fits the 40 m; the **peak is 1.65 times the average**. For constant power P and mass m the thrust at speed v is P / v, so acceleration is a = P / (m v); with F = P / v the acceleration ratio between 10 m/s and 25 m/s is 2.5. A ride such as Red Force (average 1.02 g, quoted peak 1.35 g) sits at a peak-to-average ratio of 1.3, which agrees in direction with this picture (medium, a comparison of two numbers from different sources).

A measured LSM launch: [Smartphones and Newton's first law in escalators and roller coasters](https://www.researchgate.net/publication/339798263_Smartphones_and_Newton's_first_law_in_escalators_and_roller_coasters) (Pendrill, Phys. Educ.) reports 24 stators in a second launch, a maximum possible acceleration of 0.9 g, a specified launch speed of 23.5 m/s and measured acceleration falling from 0.33 g to 0.14 g during the launch (low, secondhand through a search summary and the ride is unnamed).

**Practical consequence for P3.** A launch section should not carry a single "acceleration" number only. Offer: (a) constant acceleration (simple; matches NL2 and most published averages), and (b) an optional force profile of "peak acceleration up to a base speed, then constant power", described by peak g and base speed. The model must say what it does with the fall.

**Rolling versus standing launches.** A standing launch starts from rest, usually in the station or on a dedicated launch track (Kingda Ka, Xcelerator, Rock 'n' Roller Coaster). A rolling launch starts with the train already moving, so there is no stop and no restraint-check before it; Coasterpedia flags Taron's first launch after a slow U-turn and the Hagrid's launches as rolling, and Red Force is also listed as a rolling LSM launch ([Coasterpedia: Hagrid's](https://coasterpedia.net/wiki/Hagrid's_Magical_Creatures_Motorbike_Adventure), [Taron](https://en.wikipedia.org/wiki/Taron_(roller_coaster)), [Coasterpedia: Red Force](https://coasterpedia.net/wiki/Red_Force); medium to low: the site's use of "rolling" is not defined and its table marks even a 0 to 20 mph launch as rolling). A boost is a launch that starts from a nonzero speed and adds to it (VelociCoaster 40 to 70 mph; Hagrid's boosts at 22 to 30 and 27 to 35 mph). For the designer both are the same thing: a section where the speed rises by an amount set by the thrust and the section length; only the entry speed differs.

**Multi-pass launches** are covered in section 4.

---

## 3. Brakes and trims

### 3.1 Types and why the force depends on speed

| Type | Force | Holds a train? | Source | Confidence |
|---|---|---|---|---|
| Magnetic (eddy-current): magnets on the train pass copper or brass fins on the track, or the reverse | Roughly proportional to speed and to how much of the magnet array overlaps the fin; tapers off as the train slows; the force itself does not depend on the control system | No: it goes to zero as the train stops | [Pendrill et al., Phys. Educ. 47, 728](https://physics.gu.se/LISEBERG/eng/magn_brakes.pdf) (read, high); [Väisänen](https://aaltodoc.aalto.fi/server/api/core/bitstreams/5e6a206c-13c5-41b6-87f6-da0bc4ec7aef/content) | high |
| Friction, fin or skid | Set by the clamp force times the friction coefficient: Väisänen notes the coefficient is constant, so the force is controlled by the applied force, which the ride computer sets pneumatically or electrically | Yes | Väisänen section 2.4 | medium |
| Friction, pinch (double jaw) | As above, with jaws on both sides of a fin | Yes | same | medium |
| Tyre or transport | Motorised wheels push or slow the train at a set speed; also used to move a train at about 10 km/h | Yes at low speed | same | medium |

Brakes are "closed by default" (spring-closed, opened by air or electric power) so a power failure stops the train (Väisänen, medium). A magnetic brake plus a friction brake is the usual pair: magnets for the speed, friction for the last metres and the station hold ([Wikipedia: Brake (roller coaster)](https://en.wikipedia.org/wiki/Brake_(roller_coaster)), medium).

**The physics of the magnetic brake (high, from Pendrill et al.).** The eddy-current force on a magnet moving past a conductor is proportional to the conductivity, to the field squared and to the speed, F proportional to σ B² v. For a linear brake it is also proportional to the length of fin that is adjacent to magnets, which grows as the train enters. Pendrill models this as a = -k v s (s the distance into the brake) and fits Kanonen (Liseberg) with k = 0.15 per metre per second, entry speed v0 of about 12 m/s, a 9.8 m train of about 7 tonnes, nine fin pairs 7.2 m long, a slope of 4.3 degrees. A heavier train stops more slowly. Brass with less zinc conducts better and brakes harder.

Two simple limiting cases (my derivation, medium). With the whole train already over the fins, the force is proportional to speed alone, a = -c v with c a constant in 1/s. Then v = v0 - c s: **the speed drops linearly with distance, and the speed taken off by a brake of length L is c L, independent of the entry speed**. A peak deceleration a0 at entry means c = a0 / v0 and the train needs a distance v0 squared over a0 (reached asymptotically; the last part is done by a friction brake). Example: v0 = 20 m/s, a0 = 0.5 g = 4.9 m/s squared gives c = 0.245 per s, a time constant 4.1 s, a length of about 82 m. A real brake is not infinite and starts gently. Using Pendrill's own model for Kanonen (long-train, horizontal, a = -k v s): speed v = v0 - k s squared / 2, the train stops after s = sqrt(2 v0 / k) = 12.6 m, and the peak deceleration, at s = sqrt(2 v0 / (3k)) = 7.3 m, is a = (2/3) v0 sqrt(2 k v0 / 3) = 8.8 m/s squared, **0.89 g**, with an average of 144 / (2 x 12.6) = 5.7 m/s squared = 0.58 g. (Medium: from the paper's model with its fitted constants; the paper's text does not state the peak g, and the real brake is 7.2 m long followed by braking wheels that make up the difference.) Pendrill et al. note that at high speed the force stops rising and can fall (patent [US 6,062,350](https://patents.google.com/patent/US6062350A/en), via search summary, low).

### 3.2 Decelerations

| Item | Value | Source | Confidence |
|---|---|---|---|
| Block brakes designed long enough to stop a train in normal operation | **4.9 m/s squared = 0.5 g** | EN 13814 clause 5.4.3.4, as quoted by Väisänen, chapter 3 (the standard is paid; I did not read it) | medium |
| Same, emergency (a failure, a wet or loaded train) | **7.0 m/s squared = 0.7 g** | same | medium |
| Rider limit, eyes front (-Gx), base restraint | 2.0 g at 0.2 s falling to 1.5 g from 0.5 s on; over-the-shoulder 2.0 g; prone 3.5 g falling to 2.0 g | ASTM F2291, as tabulated in [R3](r3-force-limits.md) section 2 | high |
| Maximum track tilt at places where a train may stop (brakes) | 25 degrees | EN 13814 clause 5.4.3.1, as quoted by Väisänen | medium |
| Kanonen magnetic brake, derived | peak 0.89 g, average 0.58 g | section 3.1 | medium |
| Trim brake strength | "some of the strongest g-forces on the layout" on VelociCoaster; no figure | [ThrillZing](https://thrillzing.com/roller-coasters/magnetic-coaster-brakes-explained/) | low |
| Emergency stop on a block brake in a design calculation | the same 0.7 g, with a full train and the worst friction | Väisänen | medium |

No source gives a "comfortable" figure. The reading I would defend is this: the **comfortable** band is the 0.5 g normal-operation design value; the **usual maximum** for a designed block brake is the 0.7 g emergency value; and the **hard limit** for a rider is the ASTM -Gx value (1.5 g for a sustained base-restraint load). A strong magnetic trim can exceed 0.7 g at its entry, because it is a trim and it only fires when the train arrives faster than designed.

### 3.3 Brake-run lengths

Constant deceleration a from speed v needs the length v squared over 2a (derived, high):

| Entry speed | At 0.3 g (2.9 m/s squared) | At 0.5 g (4.9 m/s squared) | At 0.7 g (6.9 m/s squared) |
|---|---|---|---|
| 10 m/s (36 km/h) | 17 m | 10.2 m | 7.3 m |
| 15 m/s (54 km/h) | 38 m | 23 m | 16 m |
| 20 m/s (72 km/h) | 68 m | 41 m | 29 m |
| 25 m/s (90 km/h) | 106 m | 64 m | 45 m |

Real lengths found: Kanonen magnetic brake 7.2 m of fins plus braking wheels, from about 12 m/s (Pendrill). The blocks must also be as long as a train (Väisänen). That is the only real brake-run length found; manufacturer figures are not published (low).

### 3.4 Block brakes, the final brake run and trims

- A **block brake** can stop and hold a train; a **trim brake** only slows it ([Wikipedia: Brake](https://en.wikipedia.org/wiki/Brake_(roller_coaster)), Väisänen, high). The final brake run is the last block brake before the station.
- **How a trim is used**: the ride computer measures the train's speed with sensors before the trim and engages or disengages brake units so that it leaves at the required exit speed (Väisänen section 2.4, medium). Trims are put in either at the design stage or later where trains are faster than planned, to hold the g-forces of a following element, to cut wear and to make every ride feel the same by removing the speed spread between a light and a heavy train (Väisänen, medium).
- **Speed taken off** (one anecdote): a B&M trim on Patriot takes a train from about 30 mph to 5 to 10 mph in about a second, according to one rider ([Theme Park Review](https://themeparkreview.com/forum/topic/15036-bampm-magnetic-brakes/), low, a forum anecdote and quite possibly exaggerated; if true, 25 mph = 11 m/s in 1 s is 1.1 g). Millennium Force and Fury 325 use magnetic trims to shave off just enough speed to keep airtime on the following hills (same search summary, low).
- **Specifying a trim for P3**: a speed-proportional magnetic trim removes the same speed difference at any entry speed (section 3.1), so the clean user input is "speed to remove" (m/s) plus an upper deceleration limit, from which the program computes the length (length = Δv / c, c = peak deceleration / entry speed). A friction trim has a constant deceleration; the input is deceleration and a target exit speed, the program computes the length. For NL2 a trim is simply a brake in Trim Brake mode with a speed limit (section 6).

---

## 4. Swing launches

### 4.1 What they are and how they work, pass by pass

A swing (or multi-pass) launch uses one length of LSM track to put energy into the train several times: the train is launched, rises up a spike or hill, stops, falls back through the same track now moving the other way, is launched again, goes up the other side, and so on, finishing with a pass that carries it over the top. Each pass adds energy; the train needs less thrust per pass than a single launch would, so the motors can be smaller (Zamperla on Top Thrill 2: the LSM is less powerful than the old hydraulic launch, and makes up for it with extra passes; [Coaster101](https://www.coaster101.com/2023/08/01/cedar-point-officially-announces-top-thrill-2/), medium).

| Ride | Passes and speeds (published) | Spike / hill | Source | Confidence |
|---|---|---|---|---|
| Top Thrill 2 (Zamperla LSM, Cedar Point) | 3 passes: forward at 74 mph, backward at 101 mph up the new spike, forward at 120 mph over the original top hat | spike and top hat both 420 ft (128 m) | [Wikipedia](https://en.wikipedia.org/wiki/Top_Thrill_2), [Coaster101](https://www.coaster101.com/2023/08/01/cedar-point-officially-announces-top-thrill-2/) | medium (one review gives 72 mph for the first) |
| Pantheon (Intamin LSM, Busch Gardens Williamsburg) | 4 launches: a rolling launch to 36 mph, then a three-pass swing launch between a vertical spike and the 178 ft top hat: 50 mph, backward 61 mph, forward 67 mph; top speed 73 mph after the drop | spike and top hat, 3,328 ft of track | [Coasterpedia](https://coasterpedia.net/wiki/Pantheon), [Wikipedia](https://en.wikipedia.org/wiki/Pantheon_(roller_coaster)) | low to medium (Coasterpedia gives no times; whether the figures are the speed at the end of each pass is my reading) |
| Toutatis (Intamin LSM, Parc Astérix) | same three-launch pattern after a high-speed switch; top speed 107 km/h | 51 m high | [Coasterpedia](https://coasterpedia.net/wiki/Toutatis), [Wikipedia](https://en.wikipedia.org/wiki/Toutatis_(roller_coaster)) | medium |
| Hagrid's Motorbike Adventure (Intamin LSM, Universal) | 7 launches; a 70 degree, 65 ft spike up which the train is launched backwards, then falls through a sixth launch; top speed 50 mph | spike 65 ft (20 m) | [Wikipedia](https://en.wikipedia.org/wiki/Hagrid's_Magical_Creatures_Motorbike_Adventure), [Coasterpedia](https://coasterpedia.net/wiki/Hagrid's_Magical_Creatures_Motorbike_Adventure) | medium |
| Mr. Freeze: Reverse Blast (Premier LIM, Six Flags) | one backward pass through a 190 ft tunnel, up a spike (218 ft St. Louis, 236 ft Texas), a second LIM set pushes the train almost to the top, it falls back forward through the course | spike 218 to 236 ft | [Six Flags wiki](https://sixflags.fandom.com/wiki/Mr._Freeze:_Reverse_Blast), [Captain Coaster](https://captaincoaster.com/en/coasters/472/mr-freeze-reverse-blast-six-flags-st-louis) | low to medium |
| Superman Escape / Tower of Terror II (Intamin LSM) | 1 pass, backward, 100 mph, up a vertical 415 ft tower (Superman) or 115 m (Dreamworld); falls back to the station | tower | [Wikipedia](https://en.wikipedia.org/wiki/Superman:_Escape_from_Krypton), [Wikipedia: Dreamworld](https://en.wikipedia.org/wiki/Tower_of_Terror_(Dreamworld)) | medium |

Launch track length for swing launches was not found for any of them.

### 4.2 What the published speeds say about energy, pass by pass

Energy per unit mass is v squared over 2; dividing by g gives the height the train could climb with no losses, h = v squared / 2g. Top Thrill 2 (derived, my arithmetic; one value recomputed: 101 mph x 0.44704 = 45.15 m/s, 45.15 squared = 2,038, / 19.61 = 103.9 m):

| Pass | Speed | Frictionless climb height | Fraction of pass-3 centripetal force (v squared ratio) |
|---|---|---|---|
| 1 forward | 74 mph = 33.1 m/s | 55.8 m (183 ft) | 0.38 |
| 2 backward | 101 mph = 45.2 m/s | 103.9 m (341 ft) | 0.71 |
| 3 forward | 120 mph = 53.6 m/s | 146.7 m (481 ft) | 1.00 |

Pass 3's 481 ft against the 420 ft top hat leaves 61 ft of margin for friction, air drag and arriving with speed to spare, which is plausible. Pass 2's 341 ft is less than the 420 ft spike, which suggests either that pass 2 does not go to the very top of the spike, or that the LSM continues to push on the spike (as the Mr. Freeze description says the second LIM set does) so that 101 mph is not the full energy. I could not tell which (low). Pass 1 "rolls back partway up the tower" (Wikipedia), which agrees with 183 ft being far below 420 ft.

### 4.3 What a designer would need to specify one

From the above and from how NL2 does it (section 6): (a) the **number of passes**; (b) for each pass its direction and either the **speed at the end of the launch track** or the thrust (as a constant acceleration, or a peak plus a base speed as in 2.3) and the length of powered track; (c) the **launch track length** and which part of it is powered on each pass (the powered stretch may differ); (d) the **spike or hill**: its height and angle (70 degrees on Hagrid's, vertical on Top Thrill 2 and Pantheon), and for a rollback the stopping distance on the way back; (e) the **final pass's speed** at the point where the train leaves the launch track, since that sets the energy for everything after it; (f) the deceleration used to catch the train after a pass if it returns to the station or must be parked (the LSM can brake the train, as at Dreamworld: "the LSM slows the returning car").

### 4.4 Notes for the designers of P4

The program fixes the forces for one speed at each point, so the track shape is designed for one chosen pass and the others are checked.

- **Choose the last, fastest pass that goes over the top.** It is the pass that must carry the energy for the rest of the ride; the layout after it follows from its end speed. The spike and the launch-to-spike transition are the shared geometry and will see all the passes.
- **The check on the others** is mostly lower speed. On a curve of fixed shape the centripetal part of the force scales with speed squared, so Top Thrill 2's backward pass sees about 71 percent, and the first pass about 38 percent, of the pass-3 value on the same curve (derived, high). Lower speed is not automatically safe. A crest that holds the rider at about 0 g at the design speed throws less up at a lower speed, so the rider is pressed down more, but the train may then fail to reach the top, and that is the energy-margin check of FR-13. A valley's pull falls with speed, so it is gentler.
- **Direction reversal** changes the longitudinal force sign, the order in which the transitions are met (so the onset of a force that is gentle forwards is the offset backwards), and who is at the front. A transition tuned for jerk in one direction is not necessarily smooth reversed (derived, medium).
- **Speed is not set by the designer alone**: each pass ends with the speed the thrust and track length deliver. The designer who gives launch speeds as inputs gets the reverse problem: the thrust needed. Both sides should be shown.

---

## 5. Switches and transfer track

### 5.1 Kinds

| Kind | How it works | Used for | Source | Confidence |
|---|---|---|---|---|
| Sliding (translating) transfer table | A track piece the length of a train, with the train stopped on it, slides sideways to line up with a storage track. Cedar Point uses this on most of its rides; very large B&M rides carry the train on the sliding piece | Storage of spare trains, maintenance bays | [Coasterpedia: Transfer track](https://coasterpedia.net/wiki/Transfer_track), [Väisänen](https://aaltodoc.aalto.fi/server/api/core/bitstreams/5e6a206c-13c5-41b6-87f6-da0bc4ec7aef/content), maintenance-bay passage | medium |
| Rotating or pivoting track | A section turns about a pivot to line up with a different branch | Storage; some rides use it to switch between bays | same | medium to low |
| Vertical transfer | The moving piece goes up or down; Mack YoungStar and spinner coasters store trains on vertical transfers | Storage | [CoasterForce thread](https://coasterforce.com/forums/threads/odd-transfer-tracks.44225/), via search summary | low |
| Plain switch (fork or merge) | A fixed fork where a short moving piece sends the train one of two ways | Maintenance bay, dual stations (Tatsu, Storm Runner, Tron Lightcycle Power Run branch to parallel stations) | Väisänen, station-capacity passage | medium |
| Drop track | The track disconnects from the circuit and drops 5.2 m (17 ft) with the train, reconnecting to lower track | Hagrid's: a capacity doubler (two sides) and a show element | [Wikipedia](https://en.wikipedia.org/wiki/Hagrid's_Magical_Creatures_Motorbike_Adventure) | medium |
| **High-speed ("quick") switch** | A switch passed with the train rolling: no stopping. Used on Toutatis, Pantheon, Hagrid's and Top Thrill 2 so that a multi-pass launch does not have to slow the ride | Switch launches: one track lets a train swing while the next train uses the other track | [Coasterpedia: Toutatis](https://coasterpedia.net/wiki/Toutatis), [Amusement Insider via search summary](http://www.amusementinsider.com/2019/06/hagrids-magical-creatures-motorbike.html), [Coaster101](https://www.coaster101.com/2023/08/01/cedar-point-officially-announces-top-thrill-2/) | medium to low |

The recent Mack and Vekoma high-speed switches asked about in the brief: I found no source that names one (the search for Mack and Vekoma switch tracks returned ordinary two-train rides). Only Intamin (Toutatis, Pantheon, Hagrid's) and Zamperla (Top Thrill 2) are documented. Coasterpedia says Toutatis is one of three rides with a multi-pass launch track and a high-speed switch (Toutatis, Pantheon, Hagrid's); Top Thrill 2 is a fourth, per Coaster101.

### 5.2 Speeds, geometry limits, cycle times

- **Speeds at the switch.** No source gives them. The only inference: the Hagrid's boost that follows its switch begins at 21 mph (9.4 m/s) and ends at 41 mph (Coasterpedia's table), so the train crosses the switch at about 21 mph (low). The Hagrid's final launch goes up to 50 mph.
- **Geometry limits.** Nothing found. My reasoning (derived, low): a moving piece is rigid, so it can only mate with the fixed track at each of its positions if it is straight (translating) or has a constant curvature (pivoting about a point at the centre of the arc); it need not be straight for a pivoting piece but must keep a constant radius. A fixed-length shift sideways by the track spacing means the approach on each side is a straight at the switch and the diverging curves begin beyond it. Väisänen: "Most current track-mounted devices require a straight piece of track to align the equipment" (medium), and track-mounted devices are normally on straights.
- **Cycle times.** Not found for any real switch or table. NL2 gives no times either (R4).

### 5.3 What they are used for

Storage and maintenance transfer (more than two trains usually means a transfer track and several bays, Väisänen); dual-station branching (Tatsu, Storm Runner); reversing and swing launches (the switch lets a launched train go back over a shared track without meeting the following train: Top Thrill 2, Pantheon, Toutatis, Hagrid's); and show elements (Hagrid's drop track). The three block rules that matter for P5: each switch position is one track, a transfer brake is a block brake in NL2 (R4), and a switch is not a block of its own (not documented).

---

## 6. What NoLimits 2 can take

### 6.1 Native sections and their parameters

All from the official help; the help names every setting but **states no units, defaults or ranges for any of them** (high). R4 has the full section table; the parts that matter here:

| Item | Settings | Page |
|---|---|---|
| Lift | Speed, acceleration, deceleration, drive device (chain, friction wheels, none; some coaster styles use LIM/LSM instead), has anti-rollback, silent, no-block mode, shuttle gentle second pass release, dive coaster drop release, motor location, extra block length | [liftsettings](https://nolimitscoaster.com/nolimits2/help/pages/liftsettings.html) |
| Brake | Block brake or trim brake; complete stop and wait time; brake type (friction or magnetic or hidden); speed limit and hysteresis (the brake engages above limit plus hysteresis and releases below limit minus hysteresis; the example uses 15 and 5); **deceleration**; optional transport device (speed, acceleration, deceleration, friction wheels or linear motor); **launch** checkbox with its own acceleration and a maximum speed; trigger position and trigger car | [brakesettings](https://nolimitscoaster.com/nolimits2/help/pages/brakesettings.html) |
| Station | Brake device with deceleration; transport device; **launch** with acceleration and maximum speed; **shuttle mode** with a pass counter; shuttle backwards start (needs a lift behind the station) | [stationsettings](https://nolimitscoaster.com/nolimits2/help/pages/stationsettings.html) |
| Transport | Speed, acceleration, deceleration, type (friction wheels, linear motor, hidden); **shuttle mode**: each entry reduces a pass counter by 1, the section keeps accelerating toward **Maximum Speed** until the train leaves, at 0 it stops accelerating; optional "speeding down" to a given speed | [transportsettings](https://nolimitscoaster.com/nolimits2/help/pages/transportsettings.html) |
| Storage | Optional transport device and type | [storagesettings](https://nolimitscoaster.com/nolimits2/help/pages/storagesettings.html) |
| Switch track | Fork and merge; default and current track; direction (left or right, straight or left, straight or right); style: horizontal shift, vertical rotation, horizontal rotation | [switchtracksettings](https://nolimitscoaster.com/nolimits2/help/pages/switchtracksettings.html), [elementtab](https://nolimitscoaster.com/nolimits2/help/pages/elementtab.html) |
| Transfer table | Number of tracks, default and current track, length, distance between tracks; transfer brakes are always block brakes | [transfertablesettings](https://nolimitscoaster.com/nolimits2/help/pages/transfertablesettings.html) |

How this answers the brief:

- **Lifts**: speed plus acceleration and deceleration, with a drive device. Matches P3 directly (a constant speed, an end-of-lift speed match through the acceleration/deceleration limits).
- **Launches** are not a section type. They are the "Launch" checkbox on a station or brake (and a transport section accelerating to a maximum speed): an **acceleration and a maximum speed**. That is a constant-acceleration model with a cap. NL2 has no published thrust curve, constant-power region or base speed. Using a different shape in FVD++ than NL2 will make the two programs disagree on the speed at the end of the launch even when the average matches.
- **Brakes and trims**: speed limit, hysteresis and deceleration; trim is a mode. NL2's brake is therefore a constant-deceleration friction-type model with a speed target, not a magnetic brake whose force falls with speed (unless "magnetic" only changes the look; the help does not say).
- **Swing launches**: the closest native feature is **shuttle mode** (a pass counter and a maximum speed) on a transport or station section. The forum advice found: set the launch track to the maximum speed you want and then tune acceleration, deceleration and the number of passes until the performance looks right ([Steam discussion via search summary](https://steamcommunity.com/app/301320/discussions/0/6320322363298289844/), low). So in NL2 the speed on each pass is an outcome of the acceleration and distance, not an input.
- **Switches and transfer tables** exist as special tracks, in play mode controlled from the Switches tab of the Ride Control Panel, which works only in Manual Block or Full Manual mode ([Play Mode](https://nolimitscoaster.com/nolimits2/help/pages/play.html), medium via search summary). Automatic Block Mode therefore does not drive them (consistent with R4: switch control is by script).

### 6.2 What the user must set up by hand after an export

R4 found that `.nl2elem` carries only vertices and roll points. The file-format page lists the formats and **none of them carries a section type**: `.nl2elem` (vertex and roll points), `.nl2formelem`, the spline CSV (position and front, left and up vectors per point), NL1 `.nltrack` and `.nlelem` which NL2 can import, the supports XML, `.nl2fvd` (NL2's own Force Vector Designer settings) and `.nl2park` (binary, structure not public) ([File formats](https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html), high). The Track Tab shows that sections are made in NL2: select a range of track, click Section Type and choose it ([Track Tab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html), high). Switch tracks and transfer tables are not made from imported geometry; they are placed as pre-built special tracks from the editor ([Element Tab](https://nolimitscoaster.com/nolimits2/help/pages/elementtab.html), high).

So after every export the owner must, inside NL2, by hand:

1. Select each range of track and set its section type: station, lift, brake (block or trim), transport, storage.
2. Type in the speed, acceleration and deceleration of each lift and transport, the launch acceleration and maximum speed, and the brake speed limit, hysteresis and deceleration. The units are not documented, so the owner must find them by testing (or by reading the Telemetry page).
3. Place switch tracks and transfer tables from the special-track list, join them to the main line, and set tracks and styles; and write the control script.
4. Set the number of shuttle passes for a swing launch.
5. Straighten lift and brake pieces (the "Make Straight 2D" command exists for lifts).

What FVD++ can do to help without any format change: write the section ranges and every number out in a text file or on the screen next to the export (a "setup sheet" with vertex ranges, speeds and decelerations), and note the units NL2 uses once the owner has tested them. Whether the NL2 element exporter or a newer NL2 build accepts any hidden section tag was not tested (see "Could not establish").

---

## 7. Defaults and limits to propose for P3

Units: SI. "Soft maximum" means the program warns; "hard maximum" means the program refuses. Confidence per row is for the value, not for the sources behind it.

| Quantity | Proposed default | Soft maximum | Hard maximum | Basis | Confidence |
|---|---|---|---|---|---|
| Chain lift speed | 3.0 m/s | 4.5 m/s | 8 m/s (cable) | Leviathan 4.3 m/s; older 1.2 to 1.5 m/s; Millennium Force 6 to 6.7 m/s; Skyrush 7.9 m/s | medium to low |
| Cable lift speed | 6.0 m/s | 8.0 m/s | 8.5 m/s | as above | low |
| Lift slope | 30 degrees | 45 degrees | 90 degrees | Coaster101, Frontier, Fahrenheit | medium to low |
| Lift acceleration (picking up and slowing) | 0.1 g | 0.3 g | ASTM +Gx and -Gx | no source: set low so the catch-up is gentle; the rider limit is the ASTM table | low |
| Transport (station, brake-run tyres) | 2.8 m/s (10 km/h) | 3.5 m/s | 6 m/s | Väisänen | medium |
| Launch, average acceleration (default shape: constant) | 1.0 g | 1.7 g | per ASTM +Gx at the launch's duration (R3: 6 g at 1 s, 4 g at 4 s, 3 g at 12 s, 2.5 g sustained) | Table 2.2: 0.3 to 1.1 g for LSM and tyre rides, 1.4 to 1.7 g hydraulic and big LSM, 2.0 to 3.3 g air | medium |
| Launch, peak-to-average ratio for the optional power-limited LSM shape | 1.5 | 1.7 | | 2.4 MW LSM 1.65; Red Force 1.3 | low |
| Launch maximum speed | set by the user | none | none | rides to 249 km/h (Falcons Flight, Wikipedia) and 240 km/h (Formula Rossa) | medium |
| Block brake deceleration (stop) | 0.5 g | 0.7 g | 1.5 g (ASTM -Gx) | EN 13814 as quoted; ASTM via R3 | medium |
| Trim deceleration | 0.3 g | 0.7 g | 1.0 g | trims fire less often and with less margin; Kanonen peak 0.9 g is a magnetic stop | low |
| Trim as "speed to take off" | user input | | | magnetic trim removes a fixed Δv, section 3.1 | medium (derived) |
| Friction brake coefficient | not needed | | | the clamp force is a machine setting; a design needs only the deceleration | |
| Brake slope tilt (roll at a stopping place) | 0 degrees | 15 degrees | 25 degrees | EN 13814 5.4.3.1 | medium |
| Brake / trim minimum length | train length for a block brake | | | block must be as long as a train (Väisänen) | medium |

Rules for the program, from the sources, for the P3 designers:

- A powered section (lift, launch, brake with transport) must be **straight, or change pitch only**: roll and yaw are restrained by the machine. Offer this as a check, not a refusal (medium: Väisänen; R2 style: "check and say which line it measures").
- Show the along-track force in g next to the other forces. On a slope the rider's along-track force is the section's acceleration over g **plus** sin of the slope angle (section 1.4).
- For a launch, report the **speed at the end** and the **energy height** v squared over 2g, since that is what the designer actually needs (for example Kingda Ka: 57.2 m/s gives v squared over 2g = 167 m = 548 ft, against a 456 ft tower, the margin being friction, drag and the speed left at the top; derived, medium).
- Because NL2 has no units documented, export nothing numeric for these sections without a "unit check" in the setup sheet.

---

## 8. Could not establish

- Launch lengths for most rides (Kingda Ka, Red Force, Taron, Do-Dodonpa, Maxx Force, Pantheon, Top Thrill 2) and the thrust-versus-distance curve of any real LSM or hydraulic launch other than the Veltman 2.4 MW LSM (abstract only, the paper pages returned 403) and Pendrill's measured hydraulic force fall (no peak number in the text).
- Any peak (not average) acceleration from the maker. Peak figures quoted (1.35 g Red Force, 1.7 g Formula Rossa, 4.5 g Superman Escape's launch, 3.3 to 4.3 g Do-Dodonpa) are inconsistent, and several are the ride's total g, not the launch's. The 4.5 g that the Superman and Tower of Terror II pages give does not fit a 0.65 g launch average; I take it to be the peak of the vertical pull-up (low).
- Lift speeds from a maker, and lift angles from a maker. The numbers here are from encyclopedia pages, enthusiast sites and one forum. The tyre-lift and LSM-lift speeds were not found.
- How a chain dog picks a train up: speed difference, bump size, any figure for the catch-up jerk.
- The text of EN 13814 (0.5 g and 0.7 g come second-hand through one master's thesis) and the full ASTM F2291 wording (R3 has the tables). Nothing in either standard that sets a minimum brake length.
- A "comfortable" brake deceleration; a deceleration for trims on named rides; the force-versus-speed curve of a commercial magnetic brake at high speed (a patent says it peaks and then falls; not read).
- Speeds at which a high-speed switch is passed, the switch piece's geometry (straight or curved), the time it takes to move, and a list of Mack, Vekoma and B&M switches. Only Intamin and Zamperla examples were found; the brief's "recent Mack and Vekoma" switches were not found.
- Cycle times of any transfer table.
- Swing launch pass-by-pass times, thrust and powered track lengths; whether the published pass speeds are the speed at the end of the launch or the peak.
- Whether NL2 labels "magnetic" brakes with a speed-proportional force, what unit any NL2 speed, acceleration or deceleration setting uses, and the NL2 defaults. The help does not say and the owner must test; the same for what the Switches tab does in each mode.
- Whether any NL2 import path (the `.nl2elem` importer, the CSV import or a newer build) accepts a section tag; this was not tested against a real NL2 export.
- Superman Escape versus Tower of Terror II: the two Wikipedia pages give the same launch (100 mph, 7 s); I treated them as one family and took the tunnel length from the Dreamworld page.
