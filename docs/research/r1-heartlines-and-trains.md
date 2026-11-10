# R1: Heartlines and the train catalogue

Research for FR-1 and FR-9 (roadmap piece R1). All web pages were read on 2026-11-10. Written for the train data file (D2) and the picker (D3).

## Summary

1. The owner's table is a community table, not a manufacturer one. Its numbers are the vertical distance in metres from the centre of the rails to the heartline of each NoLimits 2 train (author's own words, source S1). FVD++ draws its rails the same distance from the heartline, so the numbers can be typed in unchanged (section 1).
2. No independent second source exists for any of the 38 values. The nearest things to confirmation are the author's statement that they are "exactly what NL2 shows", other forum members agreeing, and two other forum users who work with a Wing Coaster heartline of 0.5 m. I rate the table "likely" as a whole, and the Mack Launched value (1.5 m) and the plain Hyper value (0.9 m) "unverified".
3. The table is incomplete for NoLimits 2 as it is now. Four styles added later are missing (Gerstlauer Infinity, Infinity 2, Hybrid, Hybrid Launched), and the Vekoma MK1101 train is paid add-on content. No heartline for any of them was found.
4. For the real manufacturers there are almost no published heartline figures. Seating layouts and train lengths are well documented and are in the catalogue. Real-model heartlines are therefore the NoLimits 2 value for the nearest train, or an estimate, and are marked as such.
5. For the data file: keep one row per NoLimits 2 train and one per real model, with the `heartline_confidence` column carried through, and treat everything but "verified" as a default the designer may override. Nothing in the catalogue is "verified" in the strict sense, because no value has a second independent source.

Source keys used below (S1 and so on) are listed in "Sources" at the end.

## 1. The datum: what the number is measured from

**NoLimits 2.** The author of the table says the figures are "the distance between the Center of Rail and the heartline", and a second poster in the same thread clarified that it is vertical, "not horizontal" (S1). NoLimits 2 itself applies one heartline offset to a whole track, or 0 for "center of rails" (S3, S8-style forum descriptions). The official editor help says only that with the heartline "at track level" the spline shows directly between the rails, and otherwise the rails are drawn offset from the spline (S2). The Wing Coaster example in S3 fits this reading: a track built at a heartline of 0.5 m, imported into a Wing Coaster train, becomes a heartline of 0 m, because the train's own offset is 0.5 m. The help page and the Professional file-format page (S6) do not give per-train heartline values.

**FVD++ (what the code does).** I read `core/mnode.h`, `core/mnode.cpp` and `renderer/trackmesh.cpp`:

- Each node's position `vPos` is the point the integration follows. Forces are computed on the path `vPosHeart(fHeart) = vPos + fHeart * vNorm`.
- `vNorm` points down from an upright rider. `forceNormal` is `-dot(forceVec, vNorm)` and the roll is `atan(vLat.y, -vNorm.y)`, so an upright train has `vNorm.y = -1`.
- The rails and spine are drawn at `vRelPos(-fHeart, x)` (`renderer/trackmesh.cpp`, around line 1360: pipe offset `-trackData->fHeart`), which equals `vPos + fHeart * vNorm`. In plain words: the rails are drawn `fHeart` metres below the path that the force calculation follows. A positive `fHeart` is a train sitting on top of the rails, a negative one hangs below them. That is the same sign convention as the table.
- The NoLimits 2 export writes the path with the heartline offset applied through the exporter (`exportNL2Track`, `exportNode`), and the dialog offers "export without heartlining".

**Can the table be typed in unchanged?** Yes for the datum and the sign. Both measure heartline height above the rail centre line, positive upward. I did not run the program; this is from reading the code. Two cautions: (a) the glossary in `docs/glossary.md` describes `vPos` as the "spine" and the heartline as above it; the code reads the other way round (the path is the heartline and the drawn rails are offset from it), though the distance and its sign come out the same. That is a wording matter for the lead to decide on. (b) Rail-centre means the middle of the rail tube. The mesh adds extra `spineHeight` for some track styles, so the drawn spine of a box or double-spine style sits further below; that does not change the heartline height.

Confidence: datum in NoLimits 2 "likely" (one authoritative-sounding forum statement, S1, consistent with S2, S3); datum in FVD++ "verified" by reading the code.

## 2. The 38 entries

### What could be confirmed

- The table's author wrote: "The heartlines I provided are EXACTLY what the NL2 shows", and he found them by placing each coaster with "Heartline of current style" checked, estimating against the grid, then adjusting "Custom C.o.R. Offset-Y" until they matched (S1). So the values are measured from NoLimits 2's own train offsets by eye on a grid, not from manufacturer data. Expect an accuracy of a few centimetres at best.
- Other forum members called the chart correct (S1). The thread has 27 posts; the chart itself is an attached image that I could not read. The only numbers quoted in the text are 4D Coaster 3.117 ft and Inverted Coaster (4 seats across) 3.609 ft, which match the owner's table (0.95 m and -1.1 m). The feet and metre columns are the same numbers converted (S1).
- A user disputed the chart because 3.6 ft "would kill riders" as a real heartline; the author and another member replied that this misreads what is measured (S1). That dispute is about the meaning, not the numbers.
- Twisted Wing Coaster 0.5 m: two further forum posts about FVD++ and NoLimits 2 use a Wing Coaster heartline of 0.5 m (a track built at -0.5 m or -1.0 m is shifted by the train's offset) (S3, S4). This is the only value with a use independent of the table. Confidence "likely".
- No value could be checked against NoLimits 2's own data files, a manual, or a manufacturer. I did not find the chart anywhere except the CoasterCrazy thread, and no later correction is recorded in it (last post seen: September 2015).

### Changes since 2.1.0.0 (current version is about 2.6)

From the update log (S5) and the 2.6 announcement (S7):

- 2.1.0.0 added the Twisted Wing Coaster. The table already contains it, so the table is from 2.1 or later.
- 2.1.5.0 added modern double and single spine options to the Rocket Coaster style (a visual change). 2.0.8.0 improved the dive coaster trains (earlier than 2.1). 2.2.0.3 fixed the Wing Coaster eddy brake and the chain on twisted styles.
- 2.2.0.0 added the Gerstlauer Infinity Coaster and Infinity 2 Coaster.
- 2.6 added Hybrid Coaster (standard) and Hybrid Coaster (launched), bringing the total to 43 styles. 2.6.8.0 added modern spine types for the Euro-Fighter and Infinity styles. The Vekoma MK1101 is a paid add-on (S7, via search summary; not read in full).
- The log mentions no change to any train's heartline offset. I found no statement either way; "no change recorded" is not proof that none happened.

### Missing from the owner's table

The official style ID list (S6) has styles the table lacks:

| NL2 id | Style | Status |
|---|---|---|
| 73 | Gerstlauer Infinity | added 2.2, heartline unknown |
| 74 | Gerstlauer Infinity 2 | added 2.2, heartline unknown |
| 80 | Hybrid Coaster | added 2.6, heartline unknown |
| 81 | Hybrid Coaster (Launched) | added 2.6, heartline unknown |
| 24 | Dive Coaster (10-seat) without water scoops | probably the same as the dive entry, unconfirmed |
| n/a | Vekoma MK1101 (paid add-on) | heartline unknown |

Name mapping: the official list calls styles "Twisted Inverted" (4), "Twisted Hyper" (8), "Twisted Flyer" (23), "Hyper 4-seat staggered" (63) and "... with water scoops" (64), "Maurer Spinning" and "Zamperla Spinning" (S6). The owner's table uses different display names ("Inverted Coaster (4 Seats Across)", "Hyper Coaster (4 Seats Across)", "Twisted Flying Coaster", and so on). I could not pair the four hyper variants one to one from text alone; the owner's "(4 Seats Across with Scoops)" and "(4 Seats Staggered)" do not match the official "staggered" and "staggered with water scoops" names exactly. This needs a look in the running NoLimits 2 and is listed under "Could not establish". The `nl2_name` column keeps the owner's names.

### The three doubtful values

- **Mack Launched 1.5 m.** No second source. It is 0.4 m above the 1.1 m of nearly every other sit-down train in the table. Real Mack launch cars seat two across in two rows with the second row slightly higher (S12 Blue Fire), which could explain a higher figure for the rear row but not obviously 1.5 m. Rated "unverified". The cheapest check is to place the Mack Launch style in NoLimits 2 and read the offset with the author's method.
- **Twisted Wing Coaster 0.5 m.** Supported by two independent uses (S3, S4). It is far lower than other seated trains, which fits the wing layout, where riders sit beside the rail with the seats lower relative to it; that physical reason is my inference, not sourced. Rated "likely".
- **Hyper Coaster 0.9 m vs 1.1 m for the four-across variants.** No second source. Real hypers differ: B&M hypers seat two across in two rows, B&M mini-hypers four across in one row, Intamin hypers two across in two rows (S12). Which of those the plain NL2 "Hyper" represents was not established. Rated "unverified".
- One more worth a look, which the owner did not list: Vekoma Flying Dutchman is +0.8 m (positive) while Twisted Flying Coaster is -1.1 m. Whether a flying-dutchman train sits above or below its rail was not established; check the sign in NoLimits 2.

## 3. Real models

Findings per manufacturer. "NL2 proxy" means the owner's value for the closest NoLimits 2 train, used only as a starting estimate. No manufacturer publishes a heartline height in anything I found; one forum poster notes that the heartline offset varies by vehicle design and is a modelling convention, not the riders' actual hearts (S11).

Method for the estimates in general (not sourced, labelled "estimate" in the catalogue): heartline height above rail centre is roughly the height of the seat pan above the rail centre plus the height of a seated rider's chest above the seat, about 0.5 to 0.6 m. For a typical sit-down car with the seat pan 0.5 m above the rail centre that gives about 1.0 to 1.1 m, which agrees with the many 1.0 and 1.1 values in the owner's table. I found no published seat-pan heights, so this is a plausibility check on the NL2 values, not a derivation of new ones.

**Mack Rides.** Launch coaster (Blue Fire type): 2 across, 2 rows, 4 riders per car, 5 cars per train on Blue Fire and Manta, 4 cars on Copperhead Strike, lap bar pulled from above, rear row slightly higher on Blue Fire (S12). Xtreme Spinning Coaster: 4 cars of 4 riders in two rows of two on The Ride to Happiness, with an eddy-current brake controlling the spin (S12). Powered Coaster and Big Dipper: product pages exist; no seating or train data found. Mack hyper: listed as a model; no train data found. NL2 has the Mack Launch style (id 62) only.

**Vekoma, newer.** The Sit-Down Thrill Coaster (MK-1101 track): sleek trains, over-the-shoulder lap bars, usually 4 to 7 cars, 16 to 28 riders (S10), so 4 per car. Bermuda Blitz (renamed Lech Coaster) and Space Warp use the same family; Space Warp at Formuła has 4 cars of 4 in two rows of two (S12). Tilt coasters use the same gauge and trains as the Sit-Down Thrill Coaster (S10). Track gauge: the old MK-1200 name is the gauge in millimetres, 1200 mm; the MK-900 is 900 mm and the MK-700 is 700 mm (S11). The gauge of the MK-1101 itself was not found. Suspended Thrill Coaster (2020 successor to the Suspended Looping Coaster): no seating data found. Flying Dutchman: 6 cars of 4 seats in one row (24 riders) on Batwing; F.L.Y. is the 2020 redesign, with no seat counts found (S12). Boomerang: 7 cars, 2 across in 2 rows, 28 riders on The Flying Cobras, which is also described as the first with MK-1212 trains (S12). Family Boomerang and Super Boomerang: described, no seating found.

**Bolliger and Mabillard.** Hyper coaster: 2 across in 2 rows, 8 cars of 4 (32 riders) on Diamondback, Shambhala and Behemoth, single lap bar per seat; the Apollo's Chariot train of 1999 introduced B&M's lap bars (S12). Mini-hyper: 4 across in a single row, 9 cars (36 riders) at La Ronde's Goliath (S12). Inverted: 4 across in one row, over-the-shoulder, 7 to 8 cars (28 to 32 riders) (S12). Floorless: 6 to 8 cars, 4 across, over-the-shoulder (S12). Dive coaster: 6, 7, 8 or 10 across, two to three cars (S12). Track gauge: a fan tutorial puts B&M at about 4 ft (about 1219 mm), explicitly an estimate (S11). Wing, flying and stand-up trains: no seating data found.

**Intamin.** LSM Blitz / Launch Coaster: 2 across in 2 rows, lap bar; 4 cars (16 riders) on Taron, 3 cars (12 riders) on Red Force; Intamin's own pages list 12, 16 and 20 seat trains with over-the-shoulder lap bars (S12). Hot Racer: nothing found; the related Jet Rescue has 8 cars of two side by side, 16 riders, lap bar (S12). Hyper, Mega-Lite and Accelerator trains: nothing found. Track gauge: nothing found. Intamin and Schwarzkopf mount wheels on the outside of the rails (S11).

**Rocky Mountain Construction.** I-Box: 6 cars, 2 across in 2 rows, 24 riders (New Texas Giant, Iron Rattler, Wildcat's Revenge), individual lap bar and seat belt (S12). Raptor single rail: inline seating, 1 across; Wonder Woman Golden Lasso has 8 cars of 1 (8 riders) and Jersey Devil Coaster 12 cars of 1 (S12). T-Rex: seats two across, none announced when the source was written, no train data. Topper Track: RMC's wooden-style trains run on it; no train data. The only heartline remark is an unanswered 2017 forum question that says the RMC train "looks pretty close to standard PTC's" (S9).

**Gerstlauer.** Euro-Fighter: single-car trains, 4 across in 2 rows (8 riders), over-the-shoulder; Dare Devil Dive is a lap-bar variant, 2 across in 3 rows (S12). Infinity: separate cars seating 4 across in one row, up to 8 rows offered (none over 5 built), lap bars (S12).

**GCI.** Nothing found on trains, seats per bench or restraints. NL2 has none labelled GCI; the wooden styles (1.0 m) are the proxy.

## Catalogue

Columns: id, name, manufacturer, nl2_name, heartline_m, heartline_confidence, heartline_source, seats_across, rows_per_car, cars_per_train, car_length_m, restraint, notes. Empty cell = unknown. Rows `nl2-*` are the owner's 38 entries with his value. Rows `real-*` are real models. Seat data in `nl2-*` rows is filled only where the name states it. "S1" and the other keys are in "Sources"; the URL is given in full for the heartline value. Seat and restraint sources for `real-*` rows are named in the notes by source key. No car length was found for any train.

| id | name | manufacturer | nl2_name | heartline_m | heartline_confidence | heartline_source | seats_across | rows_per_car | cars_per_train | car_length_m | restraint | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| nl2-4d-coaster | 4D Coaster | Arrow/S&S type | 4D Coaster | 0.95 | likely | https://coastercrazy.com/forums/no-limits-2-heartlines-t35035.html | | | | | | 3.117 ft re-quoted by a second poster in S1; same chart |
| nl2-classic-steel-looping | Classic Steel Looping Coaster | | Classic Steel Looping Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-classic-steel-looping-modern | Classic Steel Looping Coaster (Modern) | | Classic Steel Looping Coaster (Modern) | 1.1 | likely | owner's table | | | | | | |
| nl2-corkscrew | Corkscrew Coaster | | Corkscrew Coaster | 0.65 | likely | owner's table | | | | | | |
| nl2-gerstlauer-bobsled | Gerstlauer Bobsled Coaster | Gerstlauer | Gerstlauer Bobsled Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-gerstlauer-eurofighter | Gerstlauer Euro-Fighter | Gerstlauer | Gerstlauer Euro-Fighter | 1.1 | likely | owner's table | 4 | 2 | 1 | | over-the-shoulder | seating from real rides, S12 |
| nl2-gerstlauer-eurofighter-2 | Gerstlauer Euro-Fighter 2 | Gerstlauer | Gerstlauer Euro-Fighter 2 | 1.1 | likely | owner's table | | | | | | |
| nl2-gerstlauer-spinning | Gerstlauer Spinning Coaster | Gerstlauer | Gerstlauer Spinning Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-gravity-group-timberliner | Gravity Group Timberliner | Gravity Group | Gravity Group Timberliner | 1.0 | likely | owner's table | | | | | | |
| nl2-hyper | Hyper Coaster | | Hyper Coaster | 0.9 | unverified | owner's table | | | | | | doubtful vs 1.1 for the 4-across variants; see section 2 |
| nl2-hyper-4-across | Hyper Coaster (4 Seats Across) | | Hyper Coaster (4 Seats Across) | 1.1 | likely | owner's table | 4 | | | | | |
| nl2-hyper-4-across-scoops | Hyper Coaster (4 Seats Across with Scoops) | | Hyper Coaster (4 Seats Across with Scoops) | 1.1 | likely | owner's table | 4 | | | | | name does not match the official "staggered with water scoops" exactly (S6) |
| nl2-hyper-4-staggered | Hyper Coaster (4 Seats Staggered) | | Hyper Coaster (4 Seats Staggered) | 1.1 | likely | owner's table | 4 | | | | | |
| nl2-inverted-2-across | Inverted Coaster (2 Seats Across) | | Inverted Coaster (2 Seats Across) | -0.9 | likely | owner's table | 2 | | | | | |
| nl2-inverted-4-across | Inverted Coaster (4 Seats Across) | Bolliger & Mabillard type | Inverted Coaster (4 Seats Across) | -1.1 | likely | https://coastercrazy.com/forums/no-limits-2-heartlines-t35035.html | 4 | 1 | | | over-the-shoulder | 3.609 ft re-quoted by a second poster in S1; seating from S12 |
| nl2-inverted-face-to-face | Inverted Face-to-Face Coaster | Vekoma Invertigo type | Inverted Face-to-Face Coaster | -0.9 | likely | owner's table | | | | | | |
| nl2-inverted-impulse | Inverted Impulse Coaster | Intamin type | Inverted Impulse Coaster | -1.16 | likely | owner's table | | | | | | |
| nl2-lim-launched | LIM Launched Coaster | Premier Rides type | LIM Launched Coaster | 1.0 | likely | owner's table | | | | | | |
| nl2-mack-launched | Mack Launched Coaster | Mack Rides | Mack Launched Coaster | 1.5 | unverified | owner's table | 2 | 2 | 5 | | lap bar | doubtful; seating from S12 (Blue Fire) |
| nl2-maurer-spinning | Mauer Söhne Spinning Coaster | Maurer | Mauer Söhne Spinning Coaster | 1.15 | likely | owner's table | | | | | | |
| nl2-maurer-xcar | Mauer Söhne X-Car Coaster | Maurer | Mauer Söhne X-Car Coaster | 1.295 | likely | owner's table | | | | | | |
| nl2-rocket | Rocket Coaster | | Rocket Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-suspended | Suspended Coaster | Vekoma type | Suspended Coaster | -1.7 | likely | owner's table | | | | | | |
| nl2-twisted-dive | Twisted Dive Coaster | Bolliger & Mabillard type | Twisted Dive Coaster | 1.8 | likely | owner's table | 10 | | | | | official list has a 10-seat dive and one without water scoops (S6) |
| nl2-twisted-floorless | Twisted Floorless Coaster | Bolliger & Mabillard type | Twisted Floorless Coaster | 1.3 | likely | owner's table | 4 | 1 | | | over-the-shoulder | seating from S12 |
| nl2-twisted-flying | Twisted Flying Coaster | Bolliger & Mabillard type | Twisted Flying Coaster | -1.1 | likely | owner's table | | | | | | |
| nl2-twisted-sitdown | Twisted Sitdown Coaster | Bolliger & Mabillard type | Twisted Sitdown Coaster | 1.2 | likely | owner's table | | | | | | |
| nl2-twisted-standup | Twisted Stand-Up Coaster | Bolliger & Mabillard type | Twisted Stand-Up Coaster | 1.6 | likely | owner's table | | | | | | |
| nl2-twisted-wing | Twisted Wing Coaster | Bolliger & Mabillard type | Twisted Wing Coaster | 0.5 | likely | https://nolimitscentral.com/forum/topics/573 | | | | | | independent use of 0.5 m in S3 and S4; added in NL2 2.1.0.0 |
| nl2-vekoma-flying-dutchman | Vekoma Flying Dutchman | Vekoma | Vekoma Flying Dutchman | 0.8 | unverified | owner's table | 4 | 1 | 6 | | | sign worth checking in NL2; seating from Batwing, S12 |
| nl2-vekoma-minetrain | Vekoma Minetrain Coaster | Vekoma | Vekoma Minetrain Coaster | 1.2 | likely | owner's table | | | | | | |
| nl2-vekoma-minetrain-loco | Vekoma Minetrain Coaster with Locomotive | Vekoma | Vekoma Minetrain Coaster with Locomotive | 1.2 | likely | owner's table | | | | | | |
| nl2-vekoma-motorbike | Vekoma Motorbike Coaster | Vekoma | Vekoma Motorbike Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-wooden-classic-4 | Wooden Coaster (Classic 4-Seat) | | Wooden Coaster (Classic 4-Seat) | 1.0 | likely | owner's table | | | | | | |
| nl2-wooden-classic-6 | Wooden Coaster (Classic 6-Seat) | | Wooden Coaster (Classic 6-Seat) | 1.0 | likely | owner's table | | | | | | |
| nl2-wooden-trailered-2 | Wooden Coaster (Trailered 2-Seat) | | Wooden Coaster (Trailered 2-Seat) | 1.0 | likely | owner's table | | | | | | |
| nl2-wooden-trailered-4 | Wooden Coaster (Trailered 4-Seat) | | Wooden Coaster (Trailered 4-Seat) | 1.0 | likely | owner's table | | | | | | |
| nl2-zamperla-twister | Zamperla Twister Coaster | Zamperla | Zamperla Twister Coaster | 1.1 | likely | owner's table | | | | | | |
| nl2-gerstlauer-infinity | Gerstlauer Infinity Coaster | Gerstlauer | Gerstlauer Infinity | | unverified | | 4 | 1 | 5 | | lap bar | NL2 id 73, added 2.2; not in owner's table; seating from S12 |
| nl2-gerstlauer-infinity-2 | Gerstlauer Infinity 2 Coaster | Gerstlauer | Gerstlauer Infinity 2 | | unverified | | 4 | 1 | | | lap bar | NL2 id 74, added 2.2; not in owner's table |
| nl2-hybrid | Hybrid Coaster | | Hybrid Coaster | | unverified | | | | | | | NL2 id 80, added 2.6; not in owner's table |
| nl2-hybrid-launched | Hybrid Coaster (Launched) | | Hybrid Coaster (Launched) | | unverified | | | | | | | NL2 id 81, added 2.6; not in owner's table |
| real-mack-launch | Mack Launch Coaster (Blue Fire type) | Mack Rides | Mack Launched Coaster | 1.5 | unverified | owner's table | 2 | 2 | 5 | | lap bar | 5 cars on Blue Fire and Manta, 4 on Copperhead Strike; rear row slightly higher (S12 Blue Fire, Manta, Copperhead Strike) |
| real-mack-hyper | Mack Hyper Coaster | Mack Rides | | 1.1 | estimate | NL2 proxy: Hyper 4-across, owner's table | | | | | | listed as a Mack model on RCDB; no train data found |
| real-mack-big-dipper | Mack Big Dipper | Mack Rides | Wooden Coaster (Classic 4-Seat) | 1.0 | estimate | NL2 proxy: wooden, owner's table | | | | | | Mack lists it as a current product; no train data found |
| real-mack-xtreme-spinning | Mack Xtreme Spinning Coaster | Mack Rides | Gerstlauer Spinning Coaster | 1.1 | estimate | NL2 proxy, owner's table | 2 | 2 | 4 | | | 4 riders per car; S12 The Ride to Happiness |
| real-mack-powered | Mack Powered Coaster | Mack Rides | | | unverified | | | | | | | drive-tyre cars, mine-train themed; no train data found (S12) |
| real-vekoma-sit-down-thrill | Vekoma Sit-Down Thrill Coaster (MK-1101) | Vekoma | | 1.1 | estimate | NL2 proxy: Classic Steel Looping (Modern), owner's table | | | 5 | | over-the-shoulder lap bar | 4 to 7 cars, 16 to 28 riders, so 4 riders per car (S10); the NL2 MK1101 add-on has its own train, heartline unknown |
| real-vekoma-bermuda-blitz | Vekoma Bermuda Blitz / Lech Coaster | Vekoma | | 1.1 | estimate | NL2 proxy as above | | | | | over-the-shoulder lap bar | same family as sit-down thrill coaster (S10) |
| real-vekoma-tilt | Vekoma Tilt Coaster | Vekoma | | 1.1 | estimate | NL2 proxy as above | | | | | | same gauge and trains as the sit-down thrill coaster (S10) |
| real-vekoma-space-warp | Vekoma Space Warp Launch Coaster | Vekoma | | 1.1 | estimate | NL2 proxy as above | 2 | 2 | 4 | | | 16 riders (S12 Formuła) |
| real-vekoma-suspended-thrill | Vekoma Suspended Thrill Coaster | Vekoma | Suspended Coaster | -1.7 | estimate | NL2 proxy: Suspended, owner's table | | | | | | 2020 successor of the Suspended Looping Coaster; no train data found (S12) |
| real-vekoma-boomerang | Vekoma Boomerang | Vekoma | | | unverified | | 2 | 2 | 7 | | | 28 riders on The Flying Cobras; MK-1212 trains (S12) |
| real-vekoma-family-boomerang | Vekoma Family Boomerang | Vekoma | | | unverified | | | | | | | trains similar to the Junior Coaster (S12); no seating found |
| real-vekoma-super-boomerang | Vekoma Super Boomerang | Vekoma | | | unverified | | | | | | | no seating found |
| real-vekoma-fly | Vekoma F.L.Y. / Flying Dutchman | Vekoma | Vekoma Flying Dutchman | 0.8 | unverified | owner's table | 4 | 1 | 6 | | | Batwing, a Flying Dutchman, has 6 cars of 4 in a row (S12); F.L.Y. seat count not found |
| real-vekoma-gauge | Vekoma track gauges | Vekoma | | | | | | | | | | gauge in mm: MK-1200 = 1200, MK-900 = 900, MK-700 = 700 (S11); not a train, for the gauge column of D2 |
| real-bm-hyper | B&M Hyper / Giga Coaster | Bolliger & Mabillard | Hyper Coaster | 1.1 | estimate | NL2 proxy: 0.9 or 1.1, mapping unconfirmed | 2 | 2 | 8 | | lap bar | 32 riders on Diamondback, Shambhala, Behemoth (S12) |
| real-bm-mini-hyper | B&M Mini Hyper | Bolliger & Mabillard | Hyper Coaster (4 Seats Across) | 1.1 | estimate | NL2 proxy, owner's table | 4 | 1 | 9 | | lap bar | 36 riders at La Ronde Goliath (S12) |
| real-bm-inverted | B&M Inverted Coaster | Bolliger & Mabillard | Inverted Coaster (4 Seats Across) | -1.1 | likely | https://coastercrazy.com/forums/no-limits-2-heartlines-t35035.html | 4 | 1 | 8 | | over-the-shoulder | 7 to 8 cars, 28 to 32 riders (S12); track gauge about 1219 mm, fan estimate (S11) |
| real-bm-floorless | B&M Floorless Coaster | Bolliger & Mabillard | Twisted Floorless Coaster | 1.3 | likely | owner's table | 4 | 1 | 8 | | over-the-shoulder | 6 to 8 cars (S12) |
| real-bm-dive | B&M Dive Coaster | Bolliger & Mabillard | Twisted Dive Coaster | 1.8 | likely | owner's table | 10 | 1 | 3 | | | 6, 7, 8 or 10 across, 2 to 3 cars (S12) |
| real-bm-wing | B&M Wing Coaster | Bolliger & Mabillard | Twisted Wing Coaster | 0.5 | likely | https://nolimitscentral.com/forum/topics/573 | | | | | | no seating data found |
| real-bm-flying | B&M Flying Coaster | Bolliger & Mabillard | Twisted Flying Coaster | -1.1 | likely | owner's table | | | | | | no seating data found |
| real-bm-standup | B&M Stand-Up / Surf Coaster | Bolliger & Mabillard | Twisted Stand-Up Coaster | 1.6 | likely | owner's table | | | | | | no seating data found |
| real-bm-family-inverted | B&M Family Inverted | Bolliger & Mabillard | | | unverified | | | | | | | not researched beyond the search; no data found |
| real-intamin-lsm-blitz | Intamin LSM / Blitz Launch Coaster | Intamin | LIM Launched Coaster | 1.0 | estimate | NL2 proxy, owner's table (LIM launched is Premier-style, so weak) | 2 | 2 | 4 | | lap bar | 4 cars on Taron, 3 on Red Force (S12); Intamin pages list 12, 16, 20-seat trains with over-the-shoulder lap bars |
| real-intamin-hyper | Intamin Hyper / Giga / Mega-Lite | Intamin | Hyper Coaster | 0.9 | estimate | NL2 proxy: 0.9 or 1.1, mapping unconfirmed | 2 | 2 | | | | no train data found |
| real-intamin-accelerator | Intamin Accelerator Coaster | Intamin | Rocket Coaster | 1.1 | estimate | NL2 proxy, owner's table | | | | | | which real train the NL2 Rocket Coaster models was not confirmed |
| real-intamin-impulse | Intamin Impulse | Intamin | Inverted Impulse Coaster | -1.16 | likely | owner's table | | | | | | |
| real-intamin-hot-racer | Intamin Hot Racer | Intamin | | | unverified | | | | | | | nothing found; related Jet Rescue: 2 side by side, 8 cars, lap bar (S12) |
| real-rmc-ibox | RMC I-Box | Rocky Mountain Construction | Hybrid Coaster | 1.0 | estimate | forum remark that RMC trains look close to standard PTC wooden trains (S9); NL2 wooden 1.0, owner's table | 2 | 2 | 6 | | lap bar and seat belt | 24 riders (S12); NL2 Hybrid is not confirmed as an I-Box |
| real-rmc-topper | RMC Topper Track | Rocky Mountain Construction | Wooden Coaster (Classic 4-Seat) | 1.0 | estimate | NL2 proxy, owner's table | | | | | | wooden-coaster trains run on it (S12); no train data found |
| real-rmc-raptor | RMC Raptor single rail | Rocky Mountain Construction | | | unverified | | 1 | 1 | 8 | | | 1 rider per car (S12 Wonder Woman, Jersey Devil) |
| real-rmc-trex | RMC T-Rex | Rocky Mountain Construction | | | unverified | | 2 | | | | | two seats wide (S12); no train data found |
| real-gerstlauer-infinity | Gerstlauer Infinity Coaster | Gerstlauer | Gerstlauer Infinity | | unverified | | 4 | 1 | 5 | | lap bar | up to 8 rows offered, none over 5 built (S12) |
| real-gerstlauer-eurofighter | Gerstlauer Euro-Fighter | Gerstlauer | Gerstlauer Euro-Fighter | 1.1 | likely | owner's table | 4 | 2 | 1 | | over-the-shoulder | S12 |
| real-gci-wooden | GCI wooden coaster | Great Coasters International | Wooden Coaster (Classic 4-Seat) | 1.0 | estimate | NL2 proxy, owner's table | | | | | | nothing found on trains or restraints |

## Could not establish

- Any independent confirmation (second source) of the 38 values, other than the Wing Coaster 0.5 m. The chart image in the CoasterCrazy thread was not readable by my tools, so I compared the owner's text version only against the two numbers quoted in the thread's text.
- Whether NoLimits 2 versions after 2.1 changed any train's heartline offset. The update log mentions none, which is not proof.
- The heartline for Gerstlauer Infinity, Infinity 2, Hybrid, Hybrid Launched and the paid Vekoma MK1101 train.
- Which NoLimits 2 train each real model maps to for hypers, Rocket Coaster, Hybrid, and the four hyper variants' exact pairing with the official names.
- Whether the Mack Launched 1.5 m, the plain Hyper 0.9 m and the Flying Dutchman +0.8 m are right. The cheapest check is to read each offset directly in NoLimits 2 with the author's method (Custom C.o.R. Offset-Y) and compare.
- Any manufacturer-published heartline figure, seat-pan height, car length or wheelbase. None was found for any model.
- Track gauge for Mack, Intamin, RMC and the MK-1101 (only Vekoma MK-700, MK-900, MK-1200 names and a fan estimate for B&M, S11).
- Seat counts for B&M wing, flying and stand-up trains, Mack hyper, Big Dipper and powered trains, Vekoma Family Boomerang, Super Boomerang, Suspended Thrill Coaster, F.L.Y., Intamin hyper, Mega-Lite, Accelerator, Hot Racer, RMC Topper and GCI trains.
- The Aalto thesis on roller coaster design (it discusses FVD++ heartline height) could not be read; the server returned 403.
- Wikipedia's NoLimits page was fetched but its summary did not describe NoLimits 2 train styles, so it is not used.

## Sources (accessed 2026-11-10)

- S1: https://coastercrazy.com/forums/no-limits-2-heartlines-t35035.html (the table's author, datum, endorsements, dispute)
- S2: http://nolimitscoaster.com/nolimits2/help/pages/editor.html (official editor help, spline display and heartline)
- S3: https://nolimitscentral.com/forum/topics/573 (Wing Coaster 0.5 m in practice)
- S4: https://coastercrazy.com/forums/concepts-of-in-line-twist-for-fvd-t36854.html (seen as a search summary only, not opened in full)
- S5: https://coastercrazy.com/forums/nolimits-2-update-log-t34113.html (update log)
- S6: https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html (official style ID list)
- S7: https://www.nolimitscoaster.com/index.php/26-newupdate2600released (2.6, 43 styles; seen as a search summary only)
- S8: https://coastercrazy.com/forums/heartlines-t34283.html (heartline discussion, Arrow 0, no per-train values)
- S9: https://coastercrazy.com/forums/rmc-heartline-t37106.html (the unanswered RMC question)
- S10: https://coasterpedia.net/wiki/Sit_Down_Thrill_Coaster_(Vekoma_product)
- S11: https://coasterbuzz.com/Forums/Topic/vekoma-custom-mk-900 (Vekoma gauges), https://themeparkreview.com/forum/topic/21249-roller-coaster-wheel-assembly/ (B&M gauge estimate), https://coasterbuzz.com/Forums/Topic/heart-line-solarplexus-line---what-line-is-it-really (heartline offset varies by design)
- S12: ride and product pages, seen as search summaries: https://en.wikipedia.org/wiki/Blue_Fire, https://en.wikipedia.org/wiki/Manta_(SeaWorld_San_Diego), https://en.wikipedia.org/wiki/Copperhead_Strike, https://en.wikipedia.org/wiki/Diamondback_(Kings_Island), https://en.wikipedia.org/wiki/Shambhala_(roller_coaster), https://en.wikipedia.org/wiki/Behemoth_(roller_coaster), https://en.wikipedia.org/wiki/Goliath_(La_Ronde), https://coasterpedia.net/wiki/Hyper_Coaster_(Bolliger_%26_Mabillard_product), https://en.wikipedia.org/wiki/Patriot_(Worlds_of_Fun), https://en.wikipedia.org/wiki/Black_Mamba_(roller_coaster), https://en.wikipedia.org/wiki/Floorless_Coaster, https://en.wikipedia.org/wiki/Dive_Coaster, https://en.wikipedia.org/wiki/Taron_(roller_coaster), https://en.wikipedia.org/wiki/Red_Force_(roller_coaster), https://en.wikipedia.org/wiki/Jet_Rescue, https://www.intamin.com/products-list/lsm-launch-coaster/, https://en.wikipedia.org/wiki/Iron_Rattler, https://en.wikipedia.org/wiki/Wonder_Woman_Golden_Lasso_Coaster, https://en.wikipedia.org/wiki/Jersey_Devil_Coaster, https://en.wikipedia.org/wiki/The_Ride_to_Happiness, https://coasterpedia.net/wiki/Xtreme_Spinning_Coaster, https://en.wikipedia.org/wiki/Mystic_(roller_coaster), https://en.wikipedia.org/wiki/Adrenaline_Peak, https://en.wikipedia.org/wiki/Dare_Devil_Dive, https://en.wikipedia.org/wiki/Batwing_(roller_coaster), https://en.wikipedia.org/wiki/The_Flying_Cobras, https://en.wikipedia.org/wiki/Formu%C5%82a_(Energylandia), https://coasterpedia.net/wiki/Powered_Coaster_(Mack_Rides_product)
