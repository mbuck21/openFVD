# R2: Minimum curve radii by track system and train

Research for FR-3 (minimum curve radius check). Written 2026-11-10; all links accessed 2026-11-10.

## Summary

1. **Almost no manufacturer publishes minimum radii.** Intamin's brochures say their data are "indicative and general", and none of the searches (brochures, theses, forums, patents) turned up a minimum-radius table for Mack, Vekoma, B&M, Intamin, RMC, Gerstlauer, GCI, Arrow or Schwarzkopf. Per-system numbers in this document are therefore thin, mostly low confidence, and the gaps are listed at the end.
2. **The limit is not one number.** It comes from three places: the rail fabrication (a bending limit), the train (wheel assemblies, car pitch, coupler angle, clearance), and the ride forces (speed squared over radius). For a fast coaster the force limit binds long before the fabrication limit; for slow compact rides (wild mouse, kiddie, spinning) the train and rail limits bind.
3. **The program already has most of what the check needs on the heartline**: `mnode::fFlexion()` is the heartline's total bend in degrees per metre. It does not split into vertical and lateral parts, has no sign (crest versus valley), and says nothing about twist or the spine/rail radius. The check (A2) must add those.
4. **NoLimits 2 enforces no minimum radius.** Its editor shows radius combs (vertical and horizontal) and a g-force comb, and colours force limits, but it does not refuse or flag a radius as too tight (as far as the public help and forum material show).
5. **Proposed data shape:** per train type, `min_radius_vertical_convex_m`, `min_radius_vertical_concave_m`, `min_radius_horizontal_m`, `max_twist_deg_per_m`, a `radius_reference` (`heartline` | `spine`), and per-value `source` and `confidence`. Every field is optional, because most values will be unknown; the check then skips that limit and says so.

---

## 1. What physically sets the minimum radius

This section is engineering reasoning plus the few sources found. Where a statement is my own derivation rather than a sourced fact, it says "derived".

### 1.1 Rail and track fabrication

- Tubular steel rail and the spine/box beams are bent in a bending machine. Ordinary tube-bending guidance (not coaster-specific) puts the practical limit at a few times the tube diameter, from about 2 to 3 diameters for small thin tube, 3 to 5 for medium tube and 5 to 10 for large heavy tube ([etune-tube blog](https://www.etune-tube.com/blog/what-is-the-bending-radius-limit-for-stainless-steel-hollow-tubes-1090900.html), general tube guidance, low relevance to coaster rail). A rail tube a few inches across therefore bends down to a radius well under a metre to a few metres on its own. **Conclusion (derived): for the rail itself, fabrication is almost never the binding limit for anything but the tightest family and kiddie layouts.** The binding fabrication limit is the track segment as a welded unit: a three-pipe truss or a box spine is built in straight or gently curved sections, and the spine and ties set how tight a section can be made.
- Track structure matters in another way: the rail on the inside of a curve is shorter than the one on the outside, and the two rails of one track are about 1 m or more apart (about 1.0 to 1.5 m between rail centres on most modern steel tracks; no single source found, low confidence). Where the heartline radius is R and the rail is a distance d from the heartline, the rail radius is about R plus or minus d. So the rail (or the centre spine) is tighter than the heartline on the inside of a turn and also when the heartline is above the track in a valley or below in a crest. A check must say which line it measures.
- Three-pipe truss (Intamin, Gerstlauer, Mack variants) and single-spine three-pipe (Vekoma) are described in [Väisänen, Design of Roller Coasters, Aalto 2018](https://aaltodoc.aalto.fi/items/47131e75-c748-4dd3-9a44-6cfe966043f0) (confidence medium for the construction types; the thesis gives no radius figures that I could read, the full PDF was not retrievable by the tool).

### 1.2 The train

- **Wheel assemblies.** A common design uses road wheels, side-friction wheels and up-stop wheels around each rail; one popular system has 6 wheels per bogie (Väisänen, same link as above). The wheels must roll on a curved rail without binding or climbing, so the lengths of the wheel carrier (its wheelbase) and the sideways play set a minimum radius.
- **Car pitch and coupler angle (derived).** Two points on a car a distance L apart (the bogie spacing) on a curve of radius R differ in heading by about L/R radians. Between two coupled cars, the angle the coupler or articulation must take is about the pitch from one car's bogie to the next car's bogie divided by R. Example (arithmetic only, not a real car): a 2 m bogie-to-bogie pitch on a 5 m radius is 0.4 rad, about 23 degrees. The real limit is the coupler's travel and the clearance between car bodies, which are manufacturer values I could not find.
- **Clearance between cars** also depends on the car body shape: on the inside of a tight turn adjacent car corners close up; on the outside they open up and the gap can show the coupler.
- **Vertical.** Through a crest the middle of the car lifts away from the rail at the bogies, and through a valley the car body presses toward the rail; wheel carriers have limited vertical articulation, so car length and bogie pitch also bound vertical radii. Railway analogue: the Wikipedia page notes a crest curve that is too tight can make a train leave the track, and a valley too tight can make it plough into the rails ([Minimum railway curve radius](https://en.wikipedia.org/wiki/Minimum_railway_curve_radius), railway source, qualitative only).
- The railway handbooks treat minimum horizontal radius and minimum vertical (sag) curvature as properties of the vehicle, set by articulation joints, truck pivots and coupler alignment ([TCRP Report 57](https://onlinepubs.trb.org/onlinepubs/tcrp/tcrp_rpt_57-a.pdf)). A coaster train is the same kind of problem at a much smaller scale; the idea is useful, the numbers are not transferable.

### 1.3 Ride forces

- For a point travelling at speed v on a circular arc of radius R the centripetal acceleration is v squared over R. The vertical acceleration at a crest or valley is therefore (derived):
  - crest (convex, bump): vertical g = 1 minus v squared / (R times 9.81); R for a chosen vertical g is v squared / (9.81 times (1 minus g)). Zero-g airtime needs R = v squared / 9.81. At 25 m/s (90 km/h) that is about 64 m.
  - valley (concave, dip): vertical g = 1 plus v squared / (R times 9.81). At 25 m/s and a 5 g limit (4 g extra) the minimum radius is about 16 m.
  - horizontal curve: lateral acceleration is v squared / R in the horizontal plane, reduced to zero by banking so that the net force stays in the seat; the radius limit is then set by the combined normal force.
- A [CoasterCrazy thread](https://coastercrazy.com/forums/anyone-still-use-fvd-newton-fvd-t35220.html) gives no radius guidance; the formula above was suggested by a search summary and is standard physics.
- Radius is not the only force term: the **rate** at which curvature builds up (jerk) is what riders feel as "abrupt". Clothoid loops and transition curves exist for this reason ([Vertical loop](https://en.wikipedia.org/wiki/Vertical_loop), qualitative). This is the domain of FR-4 (ride-force limits) and is out of scope here; a radius check should only catch geometry the track or train cannot take.

### 1.4 Combined curves, roll rate and twist

- A banked turn bends the track about two axes at once. The curvature vector of the heartline can be split in the train's frame into a vertical part (pitch curvature, bends the track up and down relative to the seat) and a lateral part (yaw curvature relative to the seat). The two add as a vector, so the total curvature is the square root of the sum of squares, not the sum. In a steeply banked turn "vertical" and "lateral" in the train's frame are very different from world up and sideways.
- **Twist (roll rate along the track)** is the third limit. The rails and the spine must twist along their length as the track banks, so the spine/rail carries a twist in degrees per metre, and the wheels on both rails have to follow it. At a given speed the roll rate in degrees per second is the twist per metre times speed. No manufacturer value was found. This program's view has a "roll speed" colouring; that is a rate in time, not in length.
- For the combined case the practical check is: the vector curvature of the rail line (not just the heartline) against the allowed radius of the wheel-carrier geometry, plus the twist per metre.

---

## 2. Numbers

Almost every manufacturer value is missing. The table below lists what was found. "Kind" values: published (manufacturer or standards source), attested (stated in a reliable secondary source), rule of thumb (community or textbook), estimate from a real element.

| Track system / train | Quantity | Value or range | Kind | Source | Confidence |
|---|---|---|---|---|---|
| Intamin (all systems) | any minimum radius | none published; brochures give length, height, speed, capacity only and say the data are "indicative and general" | published (absence) | [Intamin wooden coaster flyer](https://www.intamin.com/wp-content/uploads/2019/09/IAR-2020-Flyer-RollerCoaster-wooden-coaster.pdf) | high |
| Mack, Vekoma, B&M, RMC, Gerstlauer, GCI, Arrow, Schwarzkopf | any minimum radius | none found in any searched source | n/a | searches listed in "Could not establish" | n/a |
| Modern vertical loops (steel, clothoid), all makers | radius of curvature at top | about one third of loop height | rule of thumb | [Physics Classroom coaster project PDF](https://www.physicsclassroom.com/iPad/pdfs/cmDirections.pdf) | low |
| Modern vertical loops, all makers | radius of curvature at bottom | about two thirds of loop height | rule of thumb | same | low |
| Full Throttle (Intamin launch coaster) loop | height and equivalent radius | height 160 ft (49 m); a student design document gives a loop diameter of 38.75 m (radius about 19.4 m) | estimate from a real element | [Vertical loop](https://en.wikipedia.org/wiki/Vertical_loop); [Rollercoaster Design (SlideShare)](https://www.slideshare.net/slideshow/rollercoaster-design-63377900/63377900) | low |
| Vertical loops, textbook values | top radius used in physics problems | 12 to 15 m (illustrative, not a ride) | rule of thumb | [Vaia (OpenStax College Physics problems)](https://www.vaia.com/en-us/textbooks/physics/college-physics-1-edition/chapter-6/problem-31-modern-roller-coasters-have-vertical-loops-like-t/) | low |
| Great American Revolution (Schwarzkopf, 1976) loop | radius used in a textbook problem | 6.3 m (textbook value, not verified for the ride) | estimate from a real element | [Vaia (Essential University Physics problem)](https://www.vaia.com/en-us/textbooks/physics/essential-university-physics-2-edition/chapter-5/problem-39-riders-on-the-great-american-revolution-loop-the-/) | low |
| Flip Flap Railway (1895, circular loop) | loop diameter, radius | 25 ft (7.6 m) diameter, so about 3.8 m radius; the radius was so tight that it caused neck injuries | estimate from a real element | [Coaster101](https://www.coaster101.com/2025/04/28/flip-flap-railway/); [Wikipedia](https://en.wikipedia.org/wiki/Flip_Flap_Railway) | medium (diameter), low (g figures vary 6 to 12 g) |
| SooperDooperLooper (Schwarzkopf, 1977) | loop height | 57 ft (17 m) | attested | [Wikipedia, SooperDooperLooper](https://en.wikipedia.org/wiki/SooperDooperLooper) (via search summary) | low |
| Mack wild mouse (e.g. Tiger Express, Hersheypark, Carowinds) | hairpin radius | no figure found; the 180-degree hairpins are flat (unbanked) and fit in a footprint of about 40 by 20 m for a 14.5 m high ride | estimate from a real element (qualitative) | [Preston & Barbieri](https://www.prestonbarbieri.com/rides-bumper-cars/roller-coaster/wild-mouse/) | low |
| NoLimits 2 "Classic" 4-seat car | minimum turn radius when curved through stations | 20 ft (6.1 m) per a Steam community post; I could not open the post itself, only a search summary of it | rule of thumb | [Steam NL2 discussions](https://steamcommunity.com/app/301320/discussions/0/624075566959764456) (via search summary; may be the wrong thread) | low |
| NoLimits 2 community example | 180-degree turn with a radius of 30 ft (9.1 m) | single forum example | rule of thumb | [CoasterCrazy: smooth turn](https://www.coastercrazy.com/forums/can-t-make-a-smooth-turn-t36078.html) (via search summary) | low |
| Any tube rail | bend limit in diameters | 2 to 3 (small, thin), 3 to 5 (1 to 3 in), 5 to 10 (over 3 in, thick) times the tube diameter | published (general tube bending, not coaster) | [etune-tube blog](https://www.etune-tube.com/blog/what-is-the-bending-radius-limit-for-stainless-steel-hollow-tubes-1090900.html) | low for coasters |

### What the table means for the check

- **Do not invent per-manufacturer numbers.** The honest data for D2 today is: the field exists, most values are blank, and a handful of low-confidence defaults can be used.
- **Force-derived limits can be computed rather than looked up.** Given the train's top speed and the ride-force limits (FR-4), the minimum radius follows from the formulas in 1.3. That is more defensible than a table of unsourced radii and is already what the NL2 g-force comb does.
- **Where the owner can measure.** Radii of real coasters could possibly be measured from NL2 recreations of real rides, if the owner has them; this was not checked (see "Could not establish").

---

## 3. What NoLimits 2 enforces or warns about

- The editor offers display options **Radius Comb XY** (orange sticks for vertical radius, red for horizontal radius), **Radius Comb Single** (longer bar means smaller radius) and a **G-Force Comb** (g-forces computed from speed and track radius, colour by intensity) ([NL2 help: Editor Mode](http://nolimitscoaster.com/nolimits2/help/pages/editor.html), high confidence for the feature names). The product page also advertises curvature and g-forces shown in real time ([NL2 info](https://www.nolimitscoaster.com/index.php/info), medium).
- Force limits: the editor shows yellow and red force limits; the Steam guide on track stress compensation says the author keeps the red limit at 4 g and adjusts the yellow one, and suggests changing tie spacing or spine thickness where forces are high ([Steam guide, via search summary because the page returned an error when fetched](https://steamcommunity.com/sharedfiles/filedetails/?id=621122797), low confidence).
- **No minimum-radius warning was found.** The help page says nothing about a minimum radius and neither do the forum posts found. The assessment is "NL2 shows radius and force but does not refuse or flag a radius as too tight", with medium confidence (absence of evidence in public pages; the NL2 manual itself was not available).
- Consequence for FR-3: a track that exports to NL2 without complaint can still be physically impossible, so FVD++ would be adding value over NL2 here.

---

## 4. What the program already knows

(From `core/mnode.h`, `core/mnode.cpp`, `core/section.cpp`, `docs/codebase-map.md` and `docs/glossary.md`.)

- **Per node:** `mnode::fFlexion()` returns `fTrackAngleFromLast / fDistFromLast`: degrees of direction change per metre of travel between the previous node and this one. `fTrackAngleFromLast` is set in `section::calcDirFromLast()` from the pitch and yaw change of the direction vector **of the heartline** (`vDirHeart(parent->fHeart)`), combined as sqrt((cos(pitch) times yaw change) squared plus (pitch change) squared); `fDistFromLast` is the distance between the heartline positions of the two nodes. So the program has the **heartline's** total curvature in degrees per metre, without a sign, and a radius follows as R = 180 / (pi times fFlexion) metres. (`mnode::exportNode` already computes a radius on its own from speed and the same angle, `radius = fVel / angle / F_HZ`, for the bezier export; that is a speed-derived value, not a geometry one.)
- **Not available per node:** the sign (crest versus valley, left versus right); the split into the train's vertical and lateral parts (the program holds `fPitchFromLast` and `fYawFromLast`, which are world-frame pitch and yaw changes, and `fDirFromLast`, the direction in which the heartline is bending relative to the seat, from which a split could be made but which I did not verify as correct for large roll); the radius of the spine (centre line `vPos`) or of each rail; twist per metre (only `fRollSpeed` in degrees per second is stored, from which twist per metre is roll speed divided by speed, so it is available but not as a stored quantity).
- **What the check (A2) would have to add:**
  1. Radius at the track centre and at each rail: offset the heartline by `fHeart` along the node normal to get the spine, and by half the rail gauge sideways for the rails, then compute the discrete curvature from three successive positions.
  2. The vertical and lateral components of curvature in the train's frame, using the node's normal and lateral vectors, with sign.
  3. Twist per metre of the track: change of roll between nodes divided by the distance, noting the roll angle must be unwrapped across 360 degrees.
  4. Smoothing: node spacing and noise in the discrete angle mean radius computed from a single node pair is noisy at tight radii; the check should average over a window about as long as one car.

---

## 5. Proposed data shape for the limits on a train type

These fields are for the D2 train-type data file. All numbers in SI-based units; all optional; each carries its own provenance.

```yaml
radius_limits:
  reference: spine          # heartline | spine | rail_inner  -- the line every value below applies to
  min_vertical_convex_m:    # crest, track curving down away from the train
    value: null
    source: null            # URL or "unknown"
    confidence: unknown     # high | medium | low | unknown
    kind: null              # published | attested | rule-of-thumb | estimate | derived
  min_vertical_concave_m:   # valley, track curving up under the train
    value: null
    ...
  min_horizontal_m:         # flat turn
    value: null
    ...
  min_combined_m:           # total radius of the combined curvature vector; if absent, the check uses the smaller of the three above
    value: null
    ...
  max_twist_deg_per_m:      # roll change along the track
    value: null
    ...
  window_m: 3.0             # averaging length for the radius check, about one car pitch
```

Notes:

- **Why a reference line field:** the same train has a different "radius" depending on whether it is measured at the heartline, the spine or the inner rail (section 1.1). Storing which line the number refers to avoids silent errors. The check converts: it measures at the stated line.
- **Why convex and concave separately:** the train and the track fail differently (wheels lifting off versus bottoming), and published figures for the two differ when they exist.
- **Why not one number:** most real values are unknown. A missing value means "no check", and the report says "not checked: no limit for this train", which is better than a made-up default.
- **Optional extra fields** that help derive a limit when none is stored: `car_pitch_m` (bogie-to-bogie distance), `max_coupler_angle_deg`, `top_speed_ms`. With these the check can compute a limit from section 1.2 and mark it `derived`.
- Units: metres, degrees per metre. Convert nothing in the file; the loader reads SI.

---

## Could not establish

- **Minimum radii for any named manufacturer system** (Mack, Vekoma, B&M, Intamin, RMC, Gerstlauer, GCI, Arrow, Schwarzkopf, family, wild mouse, kiddie). No brochure, patent, thesis or forum text found gave one. The Väisänen thesis (Aalto 2018, [handle](https://aaltodoc.aalto.fi/bitstream/handle/123456789/33706/master_V%C3%A4is%C3%A4nen_Antti_2018.pdf)) is the most promising source for track and bogie construction but could not be read in full by the tool (HTTP 403 for the PDF); it should be read by hand.
- **Coupler angle limits and clearance between cars** for any train. Only the geometry (1.2) is given.
- **Twist limits (degrees per metre)** for any track; neither a published nor a community value was found.
- **Measured radii of real elements** other than a few loops; nothing for helices, wild-mouse hairpins or kiddie turns beyond qualitative remarks. A route that could work: measure NL2 recreations of real rides or published layout drawings, with the owner's help.
- **NL2's internal limits.** The help pages describe displays only. Whether the NL2 train physics (not editor) refuses or misbehaves on a too-tight radius needs a test in the program itself (build a tight curve with each style and train and watch the train).
- **ASTM F2291 or other standards**: the relevant parts are paid documents and were not read; nothing here should be taken as coming from them.
- Several rows in section 2 rest on search-summary text of pages I could not open directly (marked in the table); they should be re-checked before use as defaults.
