# R12: How NoLimits 2 simulates a train

Research for FR-11 (simulate the train as a train), written 2026-11-13. It answers the open point at the end of `r7-train-against-point.md` ("how NoLimits 2 simulates a train ... was not established") and extends `r8-friction-and-margins.md` (what the program's friction and drag numbers mean) and `r9-special-track.md` (NoLimits 2 sections). All web pages were opened on 2026-11-13 unless the text says "search summary only". Nothing in the program was changed and no program was downloaded or run.

How to read the confidence labels:

- **High:** the official help, the scripting API reference or the telemetry reference says so.
- **Medium:** the developer or a moderator says so, or two independent users agree, or a reader of NoLimits 2's own file format reports it.
- **Low:** one forum claim, or an inference from official text.

"Verified" below means high. Forum claims are always named as forum claims.

## Summary

NoLimits 2 (NL2) does not publish its train physics. What the public sources do establish is small, and it is mostly negative.

1. **There is no mass anywhere you can reach.** The scripting API for `Train`, `Coaster` and `Simulator` has no mass, force or friction method (high). The coaster properties page mentions no mass (high). The `.nl2park` reader library lists a car as colour only (medium). A physics teacher asking on Steam got the answer that the physics may not use mass explicitly (low). This fits the program's model, which is per kilogram already (R8 section 1.2).
2. **Resistance is two numbers, set per coaster by a "Physics Model".** The coaster's Mode tab has a Physics Model setting; the Professional licence allows custom values (high). The park-file reader names the custom values `constFrictionParameter` and `airResistanceParameter`, and lists five models: NL 2.0, NL 1.6, NL 1.0, NL 2.0 with custom friction, NL 2.2 (medium). That is the same pair as FVD++'s `fFriction` and `fResistance`. The FVD++ author says he changed the program to "NL2 style" parameters in v0.75 (medium). Nothing found says the friction depends on wheel load or lateral load.
3. **Fitted values exist but come from one tester.** On a level 200 m track at three speeds, Aeneas fitted friction 0.026 m/m for steel and 0.017 for wood, and drag 2e-5 for the NL 2.0 model and 1e-4 for the NL 2.2 model (forum, Nov 2024, low). Older posts say 0.025 to 0.027 for steel (low). The program's defaults (0.03 and 2e-5) are therefore a little high on friction and equal to the NL 2.0 drag, five times below NL 2.2's.
4. **The train is made of cars at their own places, but nothing says how they are coupled.** The API gives a position and orientation for every car and every bogie (high). The park format has a "zero car" per train (medium). Brakes trigger on the "first car" or the "middle car" (high). A forum tester says the centre of mass over a lift crest drops as the train gets longer (low), which is the whole-train energy effect R7 modelled. No source describes slack, springs or how the lift releases the train.
5. **The forces shown belong to one selected seat.** The F2 panel shows vertical, lateral and along-track force "affecting the current selected seat" (high), and you change seat with C and V. The telemetry carries the current train, car and seat and one speed and one g-vector, but its reference does not say which seat they belong to, and gives no units (high that it is silent). One user logs six seats (front, middle, back, left, right) through telemetry for his ride sheets (medium, one user).

**Recommendation for the FVD++ train model** (details in section 8):

- **Mass.** Keep the model per kilogram and give every car the same mass. Do not add a mass setting. Nothing in NL2 can be matched with one. The only mass effect NL2 can have is the train's shape (how its mass is spread along the track), and that comes from car count and car length, not a mass.
- **Friction.** Keep `fFriction` as a constant loss per metre of track, applied to the whole train (equivalent to a force proportional to weight, independent of load). This is what the evidence supports and what R8 and R7 already do. Take defaults from NL2: about 0.026 for steel styles and 0.017 for wood, marked as one tester's fit. Let the owner overwrite them, and mark them to be replaced by experiment E1 below.
- **Drag.** Keep `fResistance` as `a = fResistance x v^2`, and note that NL2 has two values for it (2e-5 and 1e-4, depending on the physics model). The owner should pick the model NL2 uses in his project (the default for new coasters is NL 2.2, medium) and the program should offer both numbers.
- **Mapping.** FVD++ `fFriction` to NL2 `constFrictionParameter` (m/m), FVD++ `fResistance` to NL2 `airResistanceParameter` (1/m). Both mappings are medium confidence for meaning, and the units of NL2's dialog are not published (see "Could not establish"). Experiment E5 settles it if the owner has the Professional licence.
- **Check against NL2 before trusting any of it.** The five experiments in section 7 need no scripting.

---

## 1. Mass

| Claim | Source | Confidence |
|---|---|---|
| The scripting API has no mass, force, g-force or friction method on `Train` (14 methods: bogie and car count, car and bogie matrix and orientation, speed, harness state, train index, lashed-to-track flag, onboard sound, coaster). | [API: Train](https://nl2docs.arborarcade.com/api/com/nolimitscoaster/Train.html), and the same list on the official host [nolimitscoaster.com](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Train.html) (only the method names were read there) | High |
| `Coaster` (21 methods) has train access (`getTrainAt`, `getTrainCount`, `findNearestTrain`), blocks, sections, special tracks, emergency stop. None is about friction, mass or seats. | [API: Coaster](https://nl2docs.arborarcade.com/api/com/nolimitscoaster/Coaster.html) | High |
| `Simulator` has time, weather, terrain and viewer methods and no physics settings. | [API: Simulator](https://nl2docs.arborarcade.com/api/com/nolimitscoaster/Simulator.html) | High |
| The Coaster Properties help page (Mode tab, Trains tab) mentions no mass and no air resistance. It does list the number of trains and cars per train. | [Coaster Properties](https://nolimitscoaster.com/nolimits2/help/pages/coasterproperties.html) | High |
| In the `.nl2park` file a `Car` holds only an individual colour. A `Train` holds its cars, a zero car, a start block and two flags. No mass field. | [libnolimits, Car](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_car.html), [Train](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_train.html). The library is a third-party reader; its own README says the documentation is incomplete. | Medium |
| A physics teacher asked how to find the train's mass. A user replied that mass values may not exist because the physics "may not use mass explicitly", and the asker noted that mass cancels when friction is ignored. | [Steam thread, Feb 2020](https://steamcommunity.com/app/301320/discussions/0/1747897987699931419/) | Low |
| Added mass "pushing against air" had no visible effect; a longer train moved the centre of mass over a lift hill. | Aeneas, [NoLimits Central friction thread](https://nolimitscentral.com/forum/topics/59?page=2), Nov 2024 | Low |

Answers:

- **Mass per car, train mass, rider mass:** none is exposed, in the editor help, the API or the park file. The program cannot match a mass to NL2 because there is none to set.
- **Can the user change it?** Not found. No loading or rider setting was found either; the help names the harness state (open or closed) and nothing about riders or weight.
- **Empty against full:** not established. Because no weight is visible, the safest reading is that NL2 runs one train the same way every time. Experiment E1 (car count) tests the part of this that can be tested.
- **Where mass would change the result:** in a model with friction as a force proportional to weight or drag as a force in newtons, mass cancels or enters only through drag per kilogram (R8 section 1.2). NL2's drag parameter is documented only as a per-length number (section 2), which is consistent with a mass already folded in.

## 2. Friction and air drag

### 2.1 What the official sources say

- The Mode tab has a **Physics Model** setting, "the friction and physics setting for the coaster". Most coasters use the standard model; old coasters may need the 1.6 or 1.0 one; the Professional licence allows custom physics settings ([Coaster Properties](https://nolimitscoaster.com/nolimits2/help/pages/coasterproperties.html), high). The page text I received said "NL 2.0" as the standard name. The update log says version 2.2.0.0 "added new default physics model v2.2" ([CoasterCrazy update log](https://coastercrazy.com/forums/nolimits-2-update-log-t34113.html), forum copy of the changelog, medium). I could not tell from the help alone which of the two is called "standard" today.
- The Steam page of the Professional licence lists "Set custom friction parameters in the coaster properties" ([Steam](https://store.steampowered.com/app/305170/NoLimits_2_Roller_Coaster_Simulation__Professional_License/), high).
- The 2.0.2.0 changelog line "Reduced chain and anti-rollback friction on lift sections" shows that lifts have a friction of their own (changelog copy, medium).
- The help says nothing about wheel load, bearings, rolling friction against air drag, or units.

### 2.2 What the file reader shows

The libnolimits `Mode` class has `PhysicsModel` with five values: `NoLimits2` (0), `NoLimits16` (1), `NoLimits1` (2), `NoLimits2CustomFriction` (3), `NoLimits22` (4). Its `CustomFriction` class has exactly two `double` values, `constFrictionParameter` and `airResistanceParameter`, with no units or descriptions ([Mode](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_mode.html), [CustomFriction](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_custom_friction.html)). Medium: it is a reader written from NL2's files, not NL2's own documentation, and the names are the reader's.

### 2.3 What users measured (all forum, low unless two agree)

| Claim | Who and where | Notes |
|---|---|---|
| The program's friction number means energy lost per metre of track, and 0.03 needs a slope of about 1.7 degrees to roll without losing speed. | altlenny (the FVD++ author), [NoLimits Central](https://nolimitscentral.com/forum/topics/59?page=2), Jan 2018 | Medium. Same reading as R8. |
| FVD++ 0.75 changed the friction numbers to "NL2 style". | altlenny, same thread; also the [openFVD changelog](https://github.com/altlenny/openFVD/blob/master/readme.txt) ("NL2 style friction parameters", v0.75) | Medium to high for the changelog line, medium for what "NL2 style" means. |
| The NL2 tooltip for the "Air Resistance Parameter" reads "speed^2 friction simulation parameter" with units 1/m. The poster's estimate for NL2 was about 1e-4 against FVD++'s 2e-5, "negligible" (about 1 km/h over 1 km). | AllegroCrab, same thread, Oct 2019 | Low: one reader quoting a tooltip. If true, 1/m matches R8's reading. |
| Level-track fits (200 m, starts at 45, 90, 180 km/h): friction 0.026 steel and 0.017 wood, the same under NL 2.0 and NL 2.2; drag 2e-5 (NL 2.0) and 1e-4 (NL 2.2) for both steel and wood. He wrote the drag unit as m/s^2, which conflicts with 1/m; the thread does not settle it. | Aeneas, same thread, Nov 2024 | Low. One tester, method only partly given. His 1e-4 agrees with AllegroCrab's 1.0e-4. |
| Under NL 2.2 all steel styles give the same friction, and all woodies the same. | Aeneas, same thread | Low. |
| An older list ("N2" values) varies with the style and car count: 0.0313 for 8-car B&M hyper, 0.0471 / 0.0392 / 0.0313 for 6 / 7 / 8-car B&M twister, 0.0157 for a 12-car GCI wooden coaster and a 6-car Gravity Group wooden coaster. Copied from "an old cache", many gaps. | Archy's list, quoted by Mr. E, same thread, Jan 2018 | Low. The numbers fall with car count in two of three series, which would mean the old model was per-train, not per-kilogram. Not checked. |
| FVD++'s 0.03 is higher than NL2's default; lowering FVD++ to about 0.025 to 0.027, or setting NL2 custom friction to 0.03, brings them closer. | Projektion, [CoasterCrazy FVD++](https://coastercrazy.com/forums/fvd-t34726.html), Dec 2014; and a post in the [g-force control thread](https://www.coastercrazy.com/forums/fvd-g-force-control-t34590.html), Oct 2014 | Low to medium (several users, but old: NL2 before version 2.2). |
| A user who tried 0.027 still saw the train 8.9 km/h too fast through the first magnetic brake (42 km/h in FVD++ against 50.9 km/h in NL2). | mijke, same g-force thread | Low. Cause unknown; the thread blames friction and speed differences. |
| Even without Pro, "custom" friction in NL2 defaults to 0.03. | Emonadeo, [NoLimits Central FVD thread](https://nolimitscentral.com/forum/topics/44?page=3), Apr 2018 | Low. Conflicts with the 0.026 fit. |
| Wooden coasters have less friction than steel (looser wheel clearance). | TylerE, maxamaxam, [CoasterCrazy, 2010](https://coastercrazy.com/forums/newton-2-friction-settings-t22740.html) (this is about the older NoLimits 1 and Newton 2 era) | Low; agrees with Aeneas's 0.017 against 0.026. |

### 2.4 Answers

- **Model:** most likely a constant-per-weight loss plus a v^2 drag term, as FVD++ has. This rests on the shape of the custom dialog (two numbers), the tooltip report and the level-track fits, not on any statement about the model by the developer. Confidence medium-low.
- **Load-dependent friction (higher in a high-g valley), lateral load:** not found in any source. The two-number dialog gives no room for it, but a built-in model could still use it. Experiment E2 tests it.
- **Custom setting:** exists in the Professional licence, Mode tab, Physics Model (high). Two parameters (medium). Units not published (high that none are). Defaults per style: not published; the only numbers are the forum fits above.
- **Coast-down fit formula:** none beyond the forum fits. For FVD++: energy per kilogram lost is `g x constFriction x s` plus `airResistance x v^3 x dt`, which is R8's reading and reproduces Aeneas's setup.

## 3. The train as a body

| Claim | Source | Confidence |
|---|---|---|
| Every train has `getCarCount()`, and every car and bogie has its own position and orientation (front, top, right vectors). Index runs front to back. | [API: Train](https://nl2docs.arborarcade.com/api/com/nolimitscoaster/Train.html) | High that the cars are placed separately. The API does not say how those places are computed. |
| A train has a "zero car" in the park file. | [libnolimits Train](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_train.html) | Medium. It says a reference car exists; nothing says it is the front or the middle, or that it matters to the physics. |
| The brake's trigger is "First Car" or "Middle Car" with a position slider along the section. | [Brake Section Settings](https://nolimitscoaster.com/nolimits2/help/pages/brakesettings.html) | High for the setting; what exactly it triggers on (the start of braking only, or the whole force?) is not stated. |
| The centre of mass over a lift hill drops a little as the train gets longer. | Aeneas, [NoLimits Central](https://nolimitscentral.com/forum/topics/59?page=2), Nov 2024 | Low. A tester's observation, no numbers. Fits a train whose speed comes from all cars' heights (R7). |
| The front car of the train enters a hill faster than FVD++ predicts and the back cars feel more extreme forces, so designers "always underestimate". Hills designed at 0 g gave about -0.5 g in the middle seats and -1.1 g in the end seats. | mijke, tiepilot35, Projektion, [CoasterCrazy g-force thread](https://www.coastercrazy.com/forums/fvd-g-force-control-t34590.html), Oct 2014 | Low to medium: three users agree. It fits R7's numbers (-0.45 g front, -0.27 g middle at a -0.2 g crest for a 9-car train), but the posts do not separate train length from friction. |
| A 4-car train had lateral forces more than 1 g off the whole way along, with speed within 0.5 km/h; the cause was a missing heartline offset. | Rheel, [NoLimits Central](https://nolimitscentral.com/forum/topics/44?page=3), Apr 2018 | Low. Useful as a warning: a heartline mistake looks like a physics difference. |

Answers:

- **One speed for the train?** `Train.getSpeed()` returns one number per train (high), so the train has one speed along its track. Whether the cars are rigidly coupled, have slack, or are springs: **not established**. The API gives no coupling, only positions.
- **Each car at its own place, mass on the track or above it:** cars have their own places (high); where each car's mass sits (rail, heartline, centre above the rail) is **not established**. The heartline offset is a property of the track style and is typed in as a spline position (centre of rails, heartline of the style, custom), medium ([Mode in libnolimits](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_mode.html), R1).
- **Zero position:** a "zero car" exists per train (medium). Which car it is: not established.

## 4. Lifts, brakes and launches

Only the settings are published; the behaviour is not.

| Device | What the help says | Units | Per car or per train |
|---|---|---|---|
| Lift | Speed "controls how fast the train moves while under power of the lift", Acceleration and Deceleration "control the force used to speed up or slow down the train". Drive device: chain, friction wheels, none. Anti-rollback option. | Not stated | Not stated |
| Transport | Speed, Acceleration, Deceleration. In shuttle mode it keeps accelerating toward Maximum Speed until the train leaves. | Not stated | Not stated |
| Brake | Speed Limit, Hysteresis (engage above limit plus hysteresis, release below limit minus hysteresis), Deceleration; trigger car (first or middle) and a position slider. Optional transport device, optional launch with its own Acceleration and Max Speed. | Not stated | Trigger car chosen; the force itself not stated |
| Station launch | Launch acceleration "configured separately from the normal transport setting", Max Speed limits launch speed. | Not stated | Not stated |

Sources: [Lift](https://nolimitscoaster.com/nolimits2/help/pages/liftsettings.html), [Transport](https://nolimitscoaster.com/nolimits2/help/pages/transportsettings.html), [Brake](https://nolimitscoaster.com/nolimits2/help/pages/brakesettings.html), [Station](https://nolimitscoaster.com/nolimits2/help/pages/stationsettings.html), [Track Tab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html) (all high). R9 section 6 covers the same pages.

Answers:

- **Acceleration or force, set speed?** The words are "force" for acceleration and deceleration and a target "speed". The numbers carry no units. The most defensible reading is: a constant acceleration toward a target speed, as R9 concluded (medium-low).
- **Does the train run ahead of the chain over a lift crest?** Not found for NL2. Real chain dogs release when gravity pulls the train faster than the chain (R7 modelled this). The help says the lift stops the train "near the top" if the next block is blocked. Whether NL2's lift holds the whole train at the chain speed past the crest is not established. Experiment E4 tests it.
- **Per car or whole train:** only the brake's trigger car is documented (first or middle). Whether a device acts on every car that is over it is not established.

## 5. The forces NL2 shows

### 5.1 On screen

The Simulation Panel (F2) shows the speed "of the selected train", the vertical g "affecting the current selected seat" (positive pushes the rider down), the lateral g (positive pulls the rider left) and an acceleration "forward and backward forces affecting the rider" (positive pushes the rider backward) ([Play Mode](https://nolimitscoaster.com/nolimits2/help/pages/play.html), high). T changes train, C changes car, V changes seat. The Editor's G-Force Comb "uses the speed and track radius to calculate g-forces" ([Editor Mode](https://nolimitscoaster.com/nolimits2/help/pages/editor.html), high), which is a point-train estimate in the editor, not the simulated ride. The help gives no unit for g or the acceleration, does not say where in the seat the force is taken, and does not say how the on-screen value is computed. A user's guess is "by radian of the spline and the speed of the train" ([Steam, Füchschen](https://steamcommunity.com/app/301320/discussions/0/619569608626815249), low); the user adds that external rides have no g calculation at all (same post).

The Setup page has a Metric or Imperial unit switch on its Others tab; it names speed units only (km/h or mph) ([Setup](https://nolimitscoaster.com/nolimits2/help/pages/setup.html), high).

### 5.2 Telemetry

The telemetry server (TCP, default port 15151, switched on in Setup, Others tab) answers "Get Telemetry" with a 76-byte message, all big-endian ([Telemetry Interface](https://nolimitscoaster.com/nolimits2/help/pages/telemetry.html), high):

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | State flags: in play mode, braking, paused, VR on |
| 4 | int32 | Rendered frame number (new data when it changes) |
| 8 | int32 | View mode (off ride, ride view, external ride view) |
| 12, 16 | int32 | Current coaster, coaster style ID |
| 20, 24, 28 | int32 | Current train, current car, current seat |
| 32 | float32 | Speed (unit not stated) |
| 36, 40, 44 | float32 | Position x, y, z (unit not stated) |
| 48 to 60 | float32 | Rotation quaternion x, y, z, w |
| 64, 68, 72 | float32 | G-force x, y, z (unit not stated) |

- **Which seat do speed and g belong to?** The reference lists current train, car and seat in the same message and has a "Select Seat" request (id 29), so they probably belong to the selected seat. The document does not say so (the page is silent: high; the inference: low).
- **Time:** there is no time stamp. A moderator suggested using the frame number plus the frame-rate limiter to get "close to time" ([Steam thread](https://steamcommunity.com/app/301320/discussions/0/2570942028831071187/), medium). The same page reports that speed has no direction and g has no rotation.
- **Per car and per seat:** you can select any seat through the telemetry, so a log of any seat is possible in principle. One user logs "6 specific points in the train" (front, middle and back, left and right) with `nl2telemetry` into CSV for ride sheets and gets front/middle/back vertical g (for example -1 / 3.2, -0.5 / 3.1, -1 / 3.4 g on a 32-seat ride) ([Clem, NoLimits Central](https://nolimitscentral.com/forum/topics/412), medium, one user). This is the nearest thing to a published NL2 per-seat measurement.
- **Tools:** the official docs give the protocol; a community client, [py_nl2telemetry](https://github.com/bestdani/py_nl2telemetry), logs g-forces to CSV and plots them live (README read; fields and units not documented there). A Steam thread recommends it for a time-against-g graph ([Steam, May 2019](https://steamcommunity.com/app/301320/discussions/0/1648791520843138246/), low). I did not run it.

## 6. Train geometry

- **Car counts:** set in the Trains tab of the coaster properties. Some styles have a fixed maximum, others a minimum ([Coaster Properties](https://nolimitscoaster.com/nolimits2/help/pages/coasterproperties.html), high). Counts and seating per style are in R1 and `data/traintypes.json`.
- **Car length, car pitch:** **no number was found** in the help, the forums or any search. R1 already said the same. The API can give them: `getCarOrientationAndPosition(index, ...)` returns the position of each car, so a short script that prints the distance between car 0 and car 1 on a straight gives the pitch (high that the method exists; not done here). Without scripting: select seat 1 of car 1, then seat 1 of car 2 through the telemetry (Select Seat) and read the position x, y, z on a straight (medium-low: needs the position unit, which the reference does not state).
- **Station length:** a forum rule says station length in metres is the number of gates plus 7 ([CoasterCrazy, search summary only](https://www.coastercrazy.com/forums/station-lengths-t5448.html), low). It tells nothing about train length.
- **A table:** cannot be sourced. The numbers would come from the API method above, one run per style, and need NL2 to be open on the owner's PC.

## 7. Experiments for the owner

All five use test tracks FVD++ can export (R6, R7). Every position and speed reading below is a request to the owner: no unit is documented for the sections' Speed, or for telemetry speed and position, so **step 0 of every experiment is to check the unit** (see E0).

**E0. Units check (10 minutes, do this once).** Build a straight level track of 200 m. Put a Transport section at the start. Run F2 and read the speed. Set the section's Speed to a few values and write down what F2 shows after the train settles; this tells you whether the setting is m/s, km/h or mph and whether F2 follows the Metric or Imperial switch (Setup, Others). Run the telemetry client once and write down speed against F2 speed. Answers: units of Speed, F2 and telemetry speed.

**E1. Coast-down on a level straight, two speeds, two car counts (friction against drag, and does car count matter).**

- Build: a dead level straight 800 m long (an FVD++ project with one straight section at constant height), a Transport section on the first 50 m, then plain Track. Use the same coaster style, physics model left as is.
- Steps: set the Transport speed to about 10 m/s (use E0 to know the setting), run; then about 40 m/s. Run each speed with 2 cars and with 9 cars (Trains tab). Four runs. Let the train leave the transport at its set speed and coast.
- Record: with the telemetry client, speed and position x, y, z for every frame in the selected seat of the middle car, from the end of the transport to where the train stops or the track ends. If you have no telemetry client, film the F2 panel and read speed every 100 m using the Editor's comb marks; this is rougher.
- Read: compute distance along the track from the positions, then `v^2/2` against distance. The slope of `v^2/2` is `-(g x friction + drag x v^2)`: at 10 m/s the drag part is negligible, so the slope gives friction directly; at 40 m/s the extra slope gives drag (2e-5 x 1600 = 0.032 m/s^2 against 1e-4 x 1600 = 0.16 m/s^2, against 0.26 m/s^2 for friction 0.026). If the 2-car and 9-car runs give the same slope in the same units, car count and mass do not matter; if the 9-car slope is different, resistance depends on train length (the Archy list suggests it might).

**E2. A valley against a level (is friction load-dependent).**

- Build: two tracks with the same entry speed. (a) 400 m level. (b) The same 400 m with, in the middle, a flat circular dip (radius R large, for example a 1.5 g to 4 g valley held for 60 m, entering and leaving at the same height). Use the Transport section to give the same entry speed.
- Record: speed at the same two positions before and after the dip (use the position field), for each of two dip strengths, 1.5 g and 4 g.
- Read: height-corrected energy loss per metre through the dip (`(v1^2 - v2^2)/2` divided by distance, with the heights equal) against the loss per metre on the level. A dip loss per metre that rises with the g (about 4 times at 4 g) means friction follows wheel load; equal loss means a constant per-metre loss as in FVD++.

**E3. The same hill, 2 cars against 9 cars (is speed the whole train's energy).**

- Build: R7's camelback (`docs/research/r7-prototype/projects/camelback.fvd`, exported as an NL2 element; entry speed 30 m/s as in R7, or the transport set to the same). Run it with 2 cars and with 9 cars of the same style.
- Record: with the telemetry client, speed and vertical g at the front seat of car 1, the middle seat, and the last seat, each in its own run (Select Seat or C and V). Note the position of the crest from the track.
- Read: speed when the middle of the train is at the crest. R7 predicts about 0.1 m/s above the point for 4 short cars and 0.46 m/s for a 9-car train at this hill, and vertical g at the crest of about -0.26 / -0.22 / -0.24 g (front / middle / rear, 4 cars) and -0.45 / -0.27 / -0.40 g (9 cars). If NL2's front and rear seats read roughly that and the 2-car train is close to the middle, the train is a coupled body with one speed. If every seat reads the same as the point, NL2 is simulating a point.

**E4. Lift crest and brake trigger car (what the train does over a lift crest and where a brake starts).**

- Build: the lift, crest and drop of R7's `liftcrest-0.60` project with a Lift section (chain) ending at the crest, 9 cars.
- Record (a): telemetry speed of the front seat of the first car, the middle car and the last car over the crest, in one run per seat. Compare with the Lift's set speed. If the rear car goes faster than the chain when it passes the crest, the train is not held to the chain once gravity is stronger (R7 predicted the rear car at 9 to 12 m/s against a 3.5 m/s chain).
- Build (b): on a level straight, a Trim Brake with Speed Limit below the train speed and a position slider at 0, with trigger car First Car, then Middle Car, then (if offered) Middle Car with 9 cars.
- Record (b): position of the first-car seat when the speed starts to fall. If First Car and Middle Car start at positions about half a train apart, the braking is triggered by that car; check whether the speed then falls the same in each case (force on the whole train) or the whole train brakes only while a car is over it.

**E5. Custom friction calibration (needs the Professional licence).**

- Build: the E1 track, 10 m/s start and 40 m/s start.
- Steps: in Coaster Properties, Mode tab, Physics Model, choose the custom friction model; type friction 0.03 and the air resistance 0. Run at 10 m/s. Then friction 0 and air resistance 1e-4, run at 40 m/s. Then the default model: read the slope as in E1.
- Read: with friction 0.03 and no drag the slope of `v^2/2` against distance must be 0.294 m/s^2 if the number is a loss in metres per metre (R8). With drag only, 1e-4 at 40 m/s must give 0.16 m/s^2 if it is a 1/m number. A different slope gives the unit NL2 really uses. The same run shows whether the default NL 2.0 and NL 2.2 models give the 0.026 and 2e-5 / 1e-4 that Aeneas found.

Without the Professional licence, E1 alone gives the default values; E5 is skipped.

## 8. What the model should do (how the summary's recommendation follows)

| FVD++ today | NL2 | What to do |
|---|---|---|
| Energy per kilogram, mass cancels (R8) | No mass visible | Keep. Add no mass setting. Say in the help that NL2 has none either. |
| `fFriction`, loss of `g x fFriction` per metre of track, independent of load (default 0.03) | Friction parameter of the physics model (constant, m/m; 0.026 steel, 0.017 wood by one fit); custom value in Pro | Keep the form. Offer style defaults 0.026 and 0.017 as "fitted by one NL2 user, to be replaced by experiment E1". Per metre of track, not per second. |
| `fResistance`, `a = fResistance x v^2` (default 2e-5) | Air resistance parameter (1/m by one tooltip report): 2e-5 in the NL 2.0 model, 1e-4 in NL 2.2 | Offer the two as presets for NL2's physics model. Do not call either "air": it is a fit. |
| Whole-train energy over all cars (R7 model) | Cars at separate positions; one train speed; centre-of-mass effect reported by a tester | Keep the R7 model as the working hypothesis. E3 tests it. |
| Forces per car, one speed | Forces per selected seat | Report forces at every car, the same as NL2's per-seat read-out, and let the owner pick the car for the design (R7: the middle car). |
| Lift holds the train at chain speed only while gravity is weaker (R7) | Not established | Keep R7's rule until E4. |
| Brake and launch acting per car over the device (R7 left open) | First or middle car trigger; no units | Add a "trigger car" option to a trim brake only after E4. |

Everything in the table's right-hand column is for matching NL2, not for explaining real trains. The real-train numbers are in R8.

---

## Could not establish

- **Units of the Custom Friction dialog and of the Lift, Transport, Brake, Station Speed, Acceleration and Deceleration settings.** The help has none (high that it is silent). Experiments E0 and E5.
- **How NL2 couples the cars** (rigid, slack, springs), and **where each car's mass sits** (rail, heartline, above the rail). Nothing public. E3 tests the result, not the cause.
- **Whether friction depends on wheel load or lateral load.** No source says. E2.
- **Whether car count or mass changes the friction** (the old "N2" list varies with car count, a 2024 tester said no). E1.
- **Which of "NL 2.0" and "NL 2.2" is the default today and what they differ in** beyond the two drag numbers. The help text I received names NL 2.0 as the standard; the changelog says 2.2 became the default in 2.2.0.0. I could not open a page that settles it.
- **What happens over a lift crest** (does the train run ahead of the chain), and **whether a device acts on every car over it.** E4.
- **Which seat the telemetry speed and g belong to, the units of telemetry speed, position and g.** The reference is silent. E0 and E3.
- **How the on-screen g is computed** (from the path curvature and train speed, or from the car's motion). A user's guess only.
- **Car length and pitch for every style.** No source. The API can give it with a short script (section 6).
- **How accurate the fits are.** Aeneas's table has no error bars or run details; the Archy list is from an "old cache".
- **The official page for the NL 2.2 physics model, the developer's own statements, any thesis or paper that measured NL2.** I found none: searches for "NoLimits 2" with physics, validation, thesis, coast-down and mass returned no source that measured NL2. Pages searched and not opened: the CoasterCrazy "one and only NoLimits 2 topic" (about 90 pages), the Theme Park Review help thread and the NoLimits Exchange site were seen only as search results, not read, so a statement of the developer could still exist there.

## Sources

Official (high):

- [NL2 help: Coaster Properties](https://nolimitscoaster.com/nolimits2/help/pages/coasterproperties.html), accessed 2026-11-13. Physics Model, Trains tab. (The fetch tool returned a summary, not the raw page.)
- [NL2 help: Play Mode](https://nolimitscoaster.com/nolimits2/help/pages/play.html), 2026-11-13. F2 panel, seat keys.
- [NL2 help: Editor Mode](https://nolimitscoaster.com/nolimits2/help/pages/editor.html), 2026-11-13. Speed and G-Force combs.
- [NL2 help: Track Tab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html), [Lift](https://nolimitscoaster.com/nolimits2/help/pages/liftsettings.html), [Transport](https://nolimitscoaster.com/nolimits2/help/pages/transportsettings.html), [Brake](https://nolimitscoaster.com/nolimits2/help/pages/brakesettings.html), [Station](https://nolimitscoaster.com/nolimits2/help/pages/stationsettings.html), [Setup](https://nolimitscoaster.com/nolimits2/help/pages/setup.html), all 2026-11-13.
- [NL2 help: Telemetry Interface](https://nolimitscoaster.com/nolimits2/help/pages/telemetry.html), 2026-11-13.
- [NL2 help: Scripting API basics](https://nolimitscoaster.com/nolimits2/help/pages/scriptingbasics.html), 2026-11-13. API class pages read on the mirror [nl2docs.arborarcade.com](https://nl2docs.arborarcade.com/api/index.html) (Train, Coaster, Simulator), which I believe is a copy of the official reference; the Train page on the official host showed the same 14 method names.
- [Steam: NL2 Professional License](https://store.steampowered.com/app/305170/NoLimits_2_Roller_Coaster_Simulation__Professional_License/), 2026-11-13.

Medium:

- [libnolimits docs: Mode, CustomFriction, Train, Car, Brake](https://geforcefan.github.io/libnolimits/class_no_limits_1_1_no_limits2_1_1_mode.html) (third-party `.nl2park` reader), 2026-11-13.
- [CoasterCrazy NoLimits 2 update log](https://coastercrazy.com/forums/nolimits-2-update-log-t34113.html) (forum copy of the changelog, no dates on the lines I used), 2026-11-13.
- [openFVD readme changelog](https://github.com/altlenny/openFVD/blob/master/readme.txt), 2026-11-13.
- [NoLimits Central: layouts of Clem](https://nolimitscentral.com/forum/topics/412), 2026-11-13.

Forum (low unless stated above):

- [NoLimits Central, The Official(?) FVD++ Friction Guide](https://nolimitscentral.com/forum/topics/59?page=1) and [page 2](https://nolimitscentral.com/forum/topics/59?page=2), 2026-11-13.
- [NoLimits Central, Official FVD thread, page 3](https://nolimitscentral.com/forum/topics/44?page=3), 2026-11-13.
- [CoasterCrazy: FVD++](https://coastercrazy.com/forums/fvd-t34726.html), [FVD++ g-force control](https://www.coastercrazy.com/forums/fvd-g-force-control-t34590.html), [Newton 2 friction settings](https://coastercrazy.com/forums/newton-2-friction-settings-t22740.html), [Telemetry specs](https://coastercrazy.com/forums/telemetry-specs-t37254.html) (no useful content), 2026-11-13.
- [Steam: mass of train, friction losses](https://steamcommunity.com/app/301320/discussions/0/1747897987699931419/), [telemetry velocity](https://steamcommunity.com/app/301320/discussions/0/2570942028831071187/), [G force charts](https://steamcommunity.com/app/301320/discussions/0/1648791520843138246/), [a thread on rotating external track and g-force](https://steamcommunity.com/app/301320/discussions/0/619569608626815249), 2026-11-13.
- [py_nl2telemetry](https://github.com/bestdani/py_nl2telemetry), 2026-11-13 (README only).
- Seen as search summary only, not opened: [CoasterCrazy station lengths](https://www.coastercrazy.com/forums/station-lengths-t5448.html).
