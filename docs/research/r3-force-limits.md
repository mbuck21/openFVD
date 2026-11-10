# R3: ride-force limits (ASTM F2291, EN 13814)

Research for roadmap piece R3 (feature request FR-4). It feeds A3 (force limits at the heartline) and A4 (forces at every seat). Sources were read on 2026-11-10. Every source link below was accessed on that date.

## Summary

1. Public sources give the whole set of ASTM F2291 acceleration-duration curves (+Gx, -Gx with its base, over-the-shoulder and prone lines, ±Gy, +Gz, -Gz with the extended bungee line, and +Gz after airtime). Two of the sources are independent renditions of the standard's own figures (Rohde 2024 for F2291-23b, and Cargill's 2019 slides). The restraint diagram also appears in ASTM's own free preview of F2291-21.
2. The rules around the curves can be stated: events under 0.2 s are ignored; the data is filtered at 5 Hz; duration is measured by slicing the trace at levels 0.1 g apart; two-axis combinations use quarter-ellipses ("eggs") whose radii are the 0.2 s limits; fast reversals in x and y halve the limits; and there are two rules for going from airtime into positive g.
3. Your memory is half right. The **restraint class** (1 to 5) is chosen *from* the accelerations, using the restraint determination diagram. Separately, the **-Gx (eyes front) limit** depends on the restraint: base case, over-the-shoulder or prone. That x-direction chart is the "max g against duration, depending on the restraint" that you remember.
4. FVD++ already stores two of the three components at the heartline, with the same sign as the standard and with gravity included. The third (along the track) is missing. A3 has to add it.
5. **Recommendation: buy ASTM F2291, but not yet.** The current edition is F2291-26b, US$121 from the ASTM store. Buy it when A3 starts, and check by hand the roughly 40 numbers listed under "What is missing or uncertain". Until then, A3 can be built and tested on the public numbers below, each tagged with its source and confidence. No library or reading-room route could be confirmed.

> **Caution.** This is a design aid for a hobby tool. It is not a certification, a ride analysis or an engineering sign-off. The numbers come from secondary sources that redraw or summarise copyrighted standards. They may be misread by a pixel or a decimal, and editions change every year. A real ride is checked by its manufacturer, an independent inspection body and measured tests (ASTM F2137), using the edition the jurisdiction requires. A green result from FVD++ means only "within the limits as transcribed here, at the points the program models".

## Sources

Short names used in the tables:

| Key | Source | Edition it describes | Notes |
|---|---|---|---|
| **R** | M. Rohde, *Some Details about the Development of Acceleration Limits for Amusement Rides*, 2nd ed., March 2024. <https://www.vdv-freizeittechnologie.de/files/bilder-vdv/pdf/Rohde-Accelerationlimits-2nd-edition-2024.pdf> | F2291-23b and EN 13814:2019 (stated on page 2) | Rohde is a professor emeritus and a member of the G-force task group. The paper redraws the F2291 figures with numbered axes and explains the rules. It is the main source here. |
| **C** | R. S. Cargill II (JP Research), *Amusement Rides: How much thrill is too much?*, PSPE seminar slides, September 2019. <https://pspe-philly.org/programs/PSPE_ASTM_%20presentation.pdf> | F2291, edition not stated (a 2019 talk; some slides are marked "Slides from ASTM F-24") | Shows the standard's Fig. 1 (axes), Fig. 6 (Gx), Fig. 11 (x-z egg), Fig. 2 (restraint diagram) and Appendix X2 (slicing method) as images. It does not show the Gy or Gz curves. |
| **A** | ASTM F2291-21, the publisher's 8-page preview. <https://www.normsplash.com/Samples/ASTM/912997733/ASTM-F2291-21-en.pdf> | F2291-21 | Definitions 3.1.1, 3.1.2 and 3.1.8; restraint clauses 6.4.3.1 to 6.4.3.6; Fig. 2 (restraint diagram). |
| **P** | T. P. Smith, *Human Factors Review of Restraint Failures on Mobile Amusement Rides*, US CPSC, February 2005. <https://www.cpsc.gov/s3fs-public/pdfs/amusrest.pdf> | F2291-04 (Table X1.1) | What each restraint class requires in hardware. |
| **S** | S. Sicat, *Wearable Technology for Design and Safety Evaluation of Rider Acceleration*, ISOES 2018. <https://isoes.info/wp-content/conferences/2018/Papers/Sicat.pdf> | cites F2137 and F2291 | Gives the filter specification only. |
| **W** | web.de, *Wie beeinflussen Achterbahnen unsere Körperfunktionen?*, updated 25 April 2026. <https://web.de/magazine/gesundheit/beeinflussen-achterbahnen-koerperfunktionen-41930898> | EN 13814 (popular press, citing TÜV NORD) | Weak, but independent: "6 g for one second". |
| **ST** | ASTM store pages for F2291-26b and F2291-23b. <https://store.astm.org/f2291-26b.html>, <https://store.astm.org/f2291-23b.html> | | Price, page count and edition list. |

Confidence: **high** means two independent sources agree. **Medium** means one good source, usually Rohde, where the number appears both in his text and on his redrawn axes, or where a second source agrees on the shape but not the exact time. **Low** means read off one drawing only, or taken from a weak source.

## 1. Coordinates and signs, and what FVD++ stores

### The standard's axes

F2291 uses a patron coordinate system fixed to the torso (C, slide "Acceleration is the Measure", reproducing F2291 Fig. 1; R section 4):

- **+X points forward**, out of the chest. **+Y points to the rider's left.** **+Z points up the spine**, toward the head. The Z axis follows the spine, with a tolerance of ±5° (R section 4).
- The limits apply to the **total acceleration felt, including gravity**. A rider sitting still on level track reads +1 g in Z (R, Fig. 6.1 note "gravity included"; A, Fig. 2 note). In engineering terms this is the *specific force*: the force the seat and restraint put on the rider, divided by body weight.
- Directions are named after where the body is pressed (R section 7.2.2):

| Component | Positive name | Felt as | Negative name | Felt as |
|---|---|---|---|---|
| Gx | +Gx "eyes back" | pressed back into the seat back (launch) | -Gx "eyes front" | thrown forward into the restraint (braking) |
| Gy | +Gy "eyes right" | pressed toward the right | -Gy "eyes left" | pressed toward the left |
| Gz | +Gz "eyes down" | pressed down into the seat (valley, loop entry) | -Gz "eyes up" | lifted out of the seat (airtime) |

The sign follows the acceleration, not the felt push. +Gy is an acceleration toward +Y (the rider's left), which presses the body to the right. So "eyes right" means the vehicle is turning left.

The measured values relate to the seat and its back angle, not to the direction of travel. A reclined seat turns the axes (R section 4 note; R section 7.2.2: the limits "represent accelerations of the seat rather than, strictly speaking, of the rider").

### What FVD++ stores (checked in the code)

Read in `core/mnode.h`, `core/mnode.cpp`, `core/secforced.cpp`, `core/track.cpp` and `renderer/trackmesh.cpp`:

- **The frame.** The anchor node is built with `vDir = (0,0,-1)`. The `mnode` constructor sets `vLat = (-vDir.z, 0, vDir.x) = (1,0,0)`, and `updateNorm()` gives `vNorm = vDir × vLat = (0,-1,0)`. World up is +Y. Facing -Z with +Y up, +X is the rider's **right**. So at zero roll, **`vLat` points to the rider's right and `vNorm` points down**, from the heart toward the rail.
- **Where the point is.** `vPos` is the **heartline**. `trackMesh` draws the heartline line at `curNode->vPos` and the rails at `vRelPos(-fHeart, ±railSpacing)`, which is `vPos + fHeart·vNorm`, that is `fHeart` metres *below* the heart. `fHeartDistFromLast` is the distance `vPos` moves. So `mnode::vPosHeart(fHeart)` is, despite its name, the rail (spine) point.
- **`forceNormal`.** In g, dimensionless, and gravity is included. The code sets `forceNormal = -dot(f, vNorm)`, where `f = (0,1,0) + a/g` is the specific force (lines 160 to 171 of `secforced.cpp`; the same formula is in `mnode::calcSmoothForces()`). It is +1 at rest on flat track, 0 in free fall and negative in airtime. It is felt at the heartline: the integrator turns the frame at `nForce/estVel`, where `estVel` is the speed of `vPos`.
- **`forceLateral`.** In g, gravity included: `forceLateral = -dot(f, vLat)`. It is positive when the specific force points to the rider's **left**, which happens in a left turn when the rider is pressed to the right.
- **The mapping.** With seats upright (no seat-back tilt), the patron axes are **X = `vDir`, Y = `-vLat`, Z = `-vNorm`**. This frame is right-handed: `vDir × (-vLat) = -vNorm`. Therefore:
  - **Gz = `forceNormal`**, same sign and same meaning.
  - **Gy = `forceLateral`**, same sign, provided `vLat` is the rider's right as derived above. The Gy limits and the Gy egg are symmetric, so a sign error here would mislabel left and right in a report but would not change any pass or fail.
  - **Gx is not stored.** A3 must compute it as `Gx = (dv/dt)/g + vDir.y`, with v the speed of the heartline point. This gives +Gx while speeding up, and on a slope at rest it gives the share of the rider's weight carried by the seat back. A cleaner route, which gives all three components consistently, is to take the second difference of `vPos` over time, add `(0, g, 0)`, divide by g and project on `vDir`, `-vLat` and `-vNorm`.
  - What the info panel and the graphs show is `forceNormal + smoothNormal` and `forceLateral + smoothLateral`, which include the roll-smoothing correction (`ui/mainwindow.cpp` line 533). A3 should check those sums, not the raw fields.
  - Nodes are 1/`F_HZ` = 1 ms apart in the force sections (`core/mnode.h`). A3 should confirm that the other section types also step one node per millisecond before it measures durations by counting nodes.
- **Two existing docs disagree with the code.** `docs/glossary.md` says `vLat` points to the rider's left and `vNorm` up out of the seat. `docs/codebase-map.md` says `vPos` is the spine and `vPosHeart()` is the heartline. Both are reversed (see the list at the end).
- **Measuring point.** For adults, the ASTM test (F2137) puts the accelerometer 12 to 16 in (30 to 41 cm) above the seat. The EN test puts it at 22 to 26 in, about the heartline (R chapter 9, quoting F2137 12.1.5.2 and 13.5.2). FVD++ computes at the heartline, which matches the EN position best.

## 2. The acceleration-duration limits

How to read the curves: each one gives, for a duration T from 0.2 s up to 90 s, the largest acceleration that may last for T. Between break points the line is straight (linear interpolation in g against seconds). After the last point the value holds until 90 s, which is the longest exposure the limits cover (R sections 7.1 and 7.2.3.1). The 0.2 s value applies to every event from 0.2 s to the first break.

All the numbers below were read from redrawn figures with labelled axes. Where a value is printed on the figure it is exact; times between grid lines are read to ±0.1 s.

### +Gz (eyes down)

| Direction | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|
| +Gz | 0.2 to 1.0 | 6.0 | R Fig. 7.4 and text p. 27 ("6 g ... 1 second"); W | F2291-23b; EN 13814 | high |
| +Gz | 2.0 | 4.0 (linear from 6.0 at 1.0 s) | R Fig. 7.4, text p. 27 | F2291-23b | medium |
| +Gz | 2.0 to 4.0 | 4.0 | R Fig. 7.4 | F2291-23b | medium |
| +Gz | 5.0 | 3.0 (linear from 4.0 at 4.0 s) | R Fig. 7.4 | F2291-23b | medium |
| +Gz | 5.0 to 11.8 | 3.0 | R Fig. 7.4 (the 11.8 s is printed) | F2291-23b | medium |
| +Gz | 12.0 to 40 | 2.0 (step from 3.0 between 11.8 and 12.0 s) | R Fig. 7.4 ("2.0 g, max. 40 s"), text p. 27 ("after 12 seconds 2 g") | F2291-23b | medium |
| +Gz | beyond 40 | about 1 g | R text p. 27 only ("after 40 seconds 1 g ... must be assured") | F2291-23b | low |

### +Gz after airtime ("push-pull")

This curve applies instead of the one above when the +Gz event follows 3 s or more of -Gz. It applies for up to 6 s after the airtime; after that the normal curve applies again (R sections 7.2.5.3 and 7.4.3.1 and Fig. 7.23, "copy from figure 8, F2291-23b").

| Direction | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|
| +Gz after ≥3 s of -Gz | 0.2 to 1.5 | 5.0 | R Fig. 7.4 and 7.23 | F2291-23b | low |
| | 2.0 | 4.0 (joins the normal curve between 1.5 and 2.0 s) | R Fig. 7.4 | F2291-23b | low |
| | 2.5 to 6.0 | 2.0 (linear from 4.0 at 2.0 s) | R Fig. 7.4 | F2291-23b | low |
| trigger | ≥ 3 s of -Gz before | | R section 7.4.3.1 ("simplified to 3 or more seconds") | F2291-23b | medium |

### -Gz (eyes up, airtime)

| Direction | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|
| -Gz | 0.2 | -2.0 | R Fig. 7.4 and Table 7.1 (minimum z: -2.0 g, base) | F2291-23b | medium |
| -Gz | 0.5 to 4.0 | -1.5 (linear from -2.0 at 0.2 s) | R Fig. 7.4 | F2291-23b | medium |
| -Gz | 7.0 onward | -1.1 (linear from -1.5 at 4.0 s; the -1.1 g is printed) | R Fig. 7.4 and text p. 28 | F2291-23b | medium |
| -Gz extended | 0.2 | -2.8 | R Fig. 7.4 and Table 7.1 ("Extended") | F2291-23b, ASTM only | medium |
| -Gz extended | 1.0 | -2.2 | R Fig. 7.4 (printed) | F2291-23b | medium |
| -Gz extended | 3.0 | -1.5, joining the base line | R Fig. 7.4, text p. 28 ("only acceptable for a time limit of 3 seconds") | F2291-23b | medium |

The extended -Gz line comes from bungee jumping, needs "special restraints" and is not part of EN 13814 (R section 7.2.5.4). A coaster check should use the base line unless the train type says otherwise.

### Gy (lateral, both directions)

| Direction | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|
| ±Gy | 0.2 to 1.0 | ±3.0 | R Fig. 7.1 and Table 7.1; C Fig. X2.6 (the "Y limit curve" starts flat at 3.0, no time scale) | F2291-23b; C unstated | high for the level, medium for the 1.0 s |
| ±Gy | 2.0 onward | ±2.0 (linear from 3.0 at 1.0 s) | R Fig. 7.1 ("2,0 g" printed); C Fig. X2.6 (falls to and holds 2.0) | as above | high for the level, medium for the 2.0 s |

Related rules: bench seats without dividers with sustained lateral above 0.7 g in one direction need a seating order from smaller to larger rider (R section 7.2.3.2). Sustained lateral at or above 0.5 g calls for "special consideration" of seats, padding and restraints (C, slide "Restraint Design", quoting F2291 6.4.4.1).

### +Gx (eyes back, launches)

| Direction | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|
| +Gx | 0.2 to 1.0 | 6.0 | R Fig. 7.2; C Fig. 6 | F2291-23b; C unstated | high |
| +Gx | 2.0 to 4.0 | 4.0 (linear from 6.0 at 1.0 s) | R Fig. 7.2; C Fig. 6 | both | high |
| +Gx | 5.0 to 11.8 | 3.0 (linear from 4.0 at 4.0 s) | R Fig. 7.2; C Fig. 6 (11.8 s printed in both) | both | high |
| +Gx | 12.0 onward | 2.5 | R Fig. 7.2; C Fig. 6 ("2.5 g" printed in both) | both | high |
| +Gx condition | above 1.5 g | a headrest is needed, unless the onset is below 5 g/s, in which case 2.0 g is allowed without one. Without a headrest, time above 1.5 g is at most 4 s. | R and C (notes on the figure) | both | high |

C labels this curve "Base Case with headrest". Rohde's worked example (R section 7.2.6) mentions "12 s (max. 2 g, not shown)", which contradicts both figures (2.5 g). The figures are taken here.

### -Gx (eyes front, brakes): this is the chart that depends on the restraint

| Direction | Restraint | Duration (s) | Limit (g) | Source(s) | Edition | Confidence |
|---|---|---|---|---|---|---|
| -Gx | base case | 0.2 | -2.0 | R Fig. 7.2; C Fig. 6 | both | high |
| -Gx | base case | 0.5 onward | -1.5 | R Fig. 7.2; C Fig. 6 (printed) | both | high (the 0.5 s is medium) |
| -Gx | over-the-shoulder | 0.2 onward | -2.0 | R Fig. 7.2; C Fig. 6 (printed) | both | high |
| -Gx | prone | 0.2 to 2.0 | -3.5 | R Fig. 7.2 and Table 7.1; C Fig. 6 | both | high |
| -Gx | prone | 3.0 to 4.0 | -2.5 (linear from -3.5 at 2.0 s) | R Fig. 7.2; C Fig. 6 | both | high |
| -Gx | prone | 5.0 onward | -2.0 (linear from -2.5 at 4.0 s; then follows the over-the-shoulder line) | R Fig. 7.2; C Fig. 6 | both | high |

Conditions (R and C, figure notes): an over-the-shoulder restraint must minimise forward motion. The prone limits may be used if the onset is below 15 g/s and the restraint is properly padded.

## 3. The rules around the curves

**Minimum duration.** Events shorter than 0.2 s ("impact" accelerations) are not covered by the curves (A 3.1.1 and 3.1.2; R section 3 and chapter 8). Confidence high. F2291 has a separate, informative method for short events (Appendix X11, "Delta-v" and average acceleration), and it is still under review (R chapter 8). It is not encodable yet.

**Filtering.** For the limit check, use a 4-pole, single-pass Butterworth low-pass filter with a 5 Hz corner frequency (R section 5.2; S). Confidence high. A (3.1.8) defines the corner frequency as the -3 dB point. EN 13814:2019 changed from 10 Hz to this filter (R section 5.2, citing EN 13814-2019 I.2.1). For the restraint diagram, filter at 1 Hz, and treat the diagram's lines as 0.1 g thick, so ±0.05 g counts as on the line (R section 5.2, citing F2291 X1.2). Confidence medium. The standard applies these filters to *measured* data. FVD++ computes its data, but the trace still has 1 ms kinks at section joins, so applying the same 5 Hz filter before checking is recommended. That is our choice, not the standard's.

**How duration is measured: slicing.** Cut the filtered trace at levels no more than 0.1 g apart. At each level, every stretch where the trace stays continuously beyond that level is one event. Its duration is the length of that stretch, and its value is the level. Then plot each (duration, level) pair against the curve; a pair above the curve fails (R section 3 "Isoacceleration", "Sub-event", and Fig. 7.8a; C, slides of F2291 Fig. X2.5 and X2.6). Confidence high. Note what this means: a 2 s valley that peaks at 4.5 g is judged by how long it stays above 4.0 g, above 4.1 g and so on, not by its total length.

**Two directions at once: the "eggs".** For each pair of axes, the instantaneous values must satisfy (a/A)² + (b/B)² ≤ 1. Here A and B are the **0.2 s limits** in the quadrant concerned: x from -2.0 (base, or -3.5 prone) to +6.0, y ±3.0, z from -2.0 (or -2.8 extended) to +6.0. The shape is therefore four quarter-ellipses, lopsided like an egg (R section 7.2.8.2, Table 7.1 and formulas (1) to (3), Fig. 7.15 citing F2291 Figs. 10, 11 and 16; C, F2291 Fig. 11). Confidence high for the x-z egg (two sources), medium for x-y and y-z (R only). Rohde also proposes a three-axis version, (ax/A)² + (ay/B)² + (az/C)² ≤ 1. It is **not** in either standard (R formula (4), p. 39), so the program may offer it only as an extra, clearly marked. The EN 13814:2004 rule that peaks within 0.3 s of each other must be superimposed was dropped in 2019 and was never in F2291 (R section 7.2.8.2).

**Reversals in x and y.** Consider two consecutive sustained events (each ≥ 0.2 s) in opposite directions in x, or in y. If their peaks are less than 0.2 s apart, each peak may be only 50 % of that direction's 0.2 s limit (R section 7.4.2, citing F2291 7.1.6.1). Confidence medium. In practice the check is that every such pair stays below 3.0 g in x (half of 6.0) and below 1.5 g in y. A stricter criterion is being discussed (peak-to-peak below 4.0 g in x, 4.75 g prone, 3.0 g in y, whenever the jerk exceeds 20 g/s) but it is not adopted (R pp. 44 and 45). Confidence low as a rule; record it only as a proposal.

**Airtime into positive g (z).** There are two separate rules, both in F2291 7.1.7 according to R section 5.3.2:

- *Push-pull:* after 3 s or more of -Gz, use the reduced +Gz curve for the next 6 s (section 2 above). Confidence medium.
- *Sustained weightlessness into a hard pull-out:* if Gz ≤ 0 for at least 200 ms and the following +Gz reaches 2 g, the rise from 0 to 2 g must take at least 133 ms, an average onset of no more than 15 g/s. Otherwise it fails (R section 7.4.3.2, flow chart Fig. 7.24 and examples Fig. 7.25). Confidence medium. Rohde's prose says "the transition must be fast", but his flow chart and worked examples fail the *fast* transitions; the flow chart is taken here.

**Jerk (onset).** Apart from the conditions above (5 g/s for the headrest, 15 g/s for prone, 15 g/s from airtime), the standards set no general jerk limit. F2291 defines how to compute it: the slope of a least-squares straight line over a 0.1 s window (R section 7.3, citing F2291 Fig. 9 and 7.1.4.8). Rohde advises designers to aim for 5 g/s, at most 10 g/s, with 15 g/s as the most a measured test should show. That is advice, not a rule. Confidence medium.

**Children (riders under 48 in, 1.22 m).** F2291 Appendix X8 is informative only. It scales the limits by a multiplier: 0.6 for a minimum rider height of 32 in (0.81 m) or less, rising in a straight line to 1.0 at 48 in. For z, the multiplier is applied to the difference from 1 g. For example, at 32 in the +Gz peak becomes 1 + 0.6 × 5 = 4 g (R section 7.5 and Fig. 7.28, the reproduction of F2291 Fig. X8.1). Confidence medium for the line, low for the "difference from 1 g" reading. A similar EN proposal (0.6 at 0.8 m rising to 1.0 at 1.2 m) has not been adopted (R Fig. 7.28). A family-coaster check can offer this as an option keyed by the minimum rider height.

**Other.** The curves assume a properly seated and restrained rider (C, quoting F2291) and normal operation, including emergency stops (R section 7.1). Rotational accelerations are not limited by either standard (R sections 7.1 and 7.6.2).

## 4. Restraint classes: what depends on what

**The dependency runs from accelerations to restraint.** You compute the sustained (1 Hz filtered) accelerations in the x-z plane at every point of the ride. Plot them on the restraint determination diagram, and the furthest area they reach sets the minimum class of restraint (A 6.4.3.2 and 6.4.3.3; R chapter 6; C slide "Restraint Classes"; P p. 6). Confidence high. Lateral acceleration is not part of the diagram (R chapter 6 note). The diagram is for "sustained acceleration" only (A 6.4.3.2). On a boundary, the lower area may be chosen (A Fig. 2 note 1).

**Separately, some limits depend on the restraint** (section 2): the -Gx line (base, over-the-shoulder, prone), the extended -Gz line ("special restraints"), and the +Gx headrest condition.

**The areas** were read from the diagram (A Fig. 2; C; R Fig. 6.1, all three agree), with ax as eyes back positive and az as eyes down positive, in g, gravity included. Confidence high for the grid values; the corners of the diagonal lines are approximate, ±0.05 g.

| Area, and minimum class | Region (sustained ax, az) |
|---|---|
| 1, no restraint required (Class 1) | az ≥ +0.2 and ax ≥ about -0.2. The left edge is ax = -0.2 down to about az = +1.05; then a diagonal to about (0, +0.7); then ax = +0.2 down to az = +0.2. Sitting still in an upright seat, (0, +1.0), falls in Area 1 or on its edge. |
| 2, Class 2 (or none if riders have handholds and footrests, A 6.4.3.5) | az ≥ +0.2 and -0.7 ≤ ax < the Area 1 edge |
| 3, Class 3 | az ≥ +0.2 and -1.2 ≤ ax < -0.7; also the band 0 to +0.2 in az for ax ≥ 0, narrowing to a diagonal from (-0.7, +0.2) to (0, 0) for -0.7 < ax < 0 |
| 4, Class 4 | -1.8 ≤ ax < -1.2 with az ≥ 0; the strip between az = 0 and the lower edge of Area 3 for -1.2 ≤ ax < 0 (that edge is az = +0.2 for ax up to -0.7, then the diagonal); and, for ax > 0, a band from az = 0 down to a line from (0, 0) to (+0.7, -0.2), then az = -0.2 |
| 5, Class 5 | everything else: ax < -1.8 g, or az < 0 (with ax ≤ 0), or az below the Area 4 band (with ax > 0) |

The practical result for coasters: **any sustained airtime (Gz below 0 for 0.2 s or more) puts the ride in Area 5, so it needs a Class 5 restraint.** The P report (based on F2291-04) says that sitting still corresponds to Class 2; the current diagram puts that point in Area 1. Editions differ here, and it does not matter for coasters.

**What each class means in hardware** (P, the table based on F2291-04 Table X1.1; A 6.4.3.4 and 6.4.3.5 confirm Classes 1 and 2 in F2291-21). Confidence high for Classes 1 and 2, medium for 3 to 5 (one source, an older edition).

| Class | Per rider? | Final position | Who latches / unlatches | Indication | Redundancy |
|---|---|---|---|---|---|
| 1 | no restraint | | | | |
| 2 | individual or shared | fixed or adjustable | rider or operator / rider or operator | none | not required |
| 3 | individual or shared | adjustable | rider or operator / rider or operator | operator's visual check; a failure of the primary system detectable within one ride cycle | not required |
| 4 | individual | adjustable | locks automatically / operator only | operator's visual check | redundant locking |
| 5 | individual | adjustable | locks automatically / operator only | external indication; a fault stops the cycle or prevents the start | redundant locking, **and** two restraints or one fail-safe restraint |

**Mapping to coaster hardware.** The class is about locking, redundancy and monitoring. It does not say lap bar or over-the-shoulder. A modern individually locked, redundantly locked and monitored lap bar can be Class 5. Lap bar versus over-the-shoulder versus vest is decided in the ride's patron restraint and containment analysis, and it shows up in the limits through the -Gx line (and the headrest note for +Gx). I could not find a public statement in F2291 that names restraint *shapes* for each class. So the train type (roadmap D2) should carry two separate fields: `restraint_class` (1 to 5) and `restraint_kind` (lap bar, over-the-shoulder, vest, prone, none), and the check takes the -Gx line from `restraint_kind`.

## 5. EN 13814 and ISO 17842

- **Same curves.** EN 13814-1:2019 took over the F2291 acceleration limits as an informative Annex I. It is informative because the research reports behind the limits are not public (R pp. 2, 4 and 37). The current edition is EN 13814-1:2019+A1:2024. ISO 17842-1:2023 added an informative Annex I aligned with F2291-21 (search-result summary of the iTeh catalogue page; not opened). Confidence medium.
- **Same restraint diagram** (R chapter 6: "harmonized between EN13814 and ASTM F2291"). Confidence medium.
- **Differences** (all from R, confidence medium):
  - EN has no extended -Gz (-2.8 g) line, because bungee jumping is outside EN.
  - EN calls onset "jerk".
  - EN 13814:2004 had a 10 Hz filter and the rule that peaks within 0.3 s of each other are superimposed. The 2019 edition moved to the 5 Hz Butterworth filter and dropped the 0.3 s rule.
  - The EN test puts the accelerometer at about the heartline (22 to 26 in above the seat for adults), the ASTM test lower (12 to 16 in).
  - EN has no children's multiplier yet.
  - EN puts the notes under the figures; ASTM is moving to do the same.
- **Not established:** whether EN 13814-1:2019+A1:2024 or ISO 17842-1:2023 changed any break point relative to F2291-23b. No price for the EN was obtained (it is sold through national standards bodies).

## 6. What FVD++ can encode

### Proposed data shape

One file per limit set (JSON or YAML, read by the A1 analysis framework), with the source and confidence on every number, so the owner can replace values from his own copy without touching code. Units: acceleration in g, specific force including gravity, patron axes as in section 1; time in s.

```json
{
  "id": "astm-f2291-23b-public",
  "standard": "ASTM F2291",
  "edition": "23b",
  "transcribed_from": "Rohde 2024 (R), Cargill 2019 (C); see docs/research/r3-force-limits.md",
  "units": { "acceleration": "g (specific force, gravity included)", "time": "s" },
  "filter": { "kind": "butterworth", "order": 4, "passes": 1, "corner_hz": 5.0 },
  "restraint_filter": { "kind": "butterworth", "order": 4, "passes": 1, "corner_hz": 1.0, "line_tolerance_g": 0.05 },
  "min_event_s": 0.2,
  "max_duration_s": 90.0,
  "slice_step_g": 0.1,
  "curves": {
    "z_pos":            { "points": [[0.2, 6.0], [1.0, 6.0], [2.0, 4.0], [4.0, 4.0], [5.0, 3.0], [11.8, 3.0], [12.0, 2.0], [40.0, 2.0]], "after_last": "hold", "confidence": "medium" },
    "z_pos_after_neg":  { "points": [[0.2, 5.0], [1.5, 5.0], [2.0, 4.0], [2.5, 2.0], [6.0, 2.0]], "trigger_neg_z_s": 3.0, "window_s": 6.0, "confidence": "low" },
    "z_neg":            { "points": [[0.2, -2.0], [0.5, -1.5], [4.0, -1.5], [7.0, -1.1]], "after_last": "hold", "confidence": "medium" },
    "z_neg_extended":   { "points": [[0.2, -2.8], [1.0, -2.2], [3.0, -1.5]], "then": "z_neg", "confidence": "medium" },
    "y":                { "points": [[0.2, 3.0], [1.0, 3.0], [2.0, 2.0]], "after_last": "hold", "symmetric": true, "confidence": "medium" },
    "x_pos":            { "points": [[0.2, 6.0], [1.0, 6.0], [2.0, 4.0], [4.0, 4.0], [5.0, 3.0], [11.8, 3.0], [12.0, 2.5]], "after_last": "hold", "confidence": "high" },
    "x_neg_base":       { "points": [[0.2, -2.0], [0.5, -1.5]], "after_last": "hold", "confidence": "high" },
    "x_neg_otsr":       { "points": [[0.2, -2.0]], "after_last": "hold", "confidence": "high" },
    "x_neg_prone":      { "points": [[0.2, -3.5], [2.0, -3.5], [3.0, -2.5], [4.0, -2.5], [5.0, -2.0]], "after_last": "hold", "confidence": "high" }
  },
  "interpolation": "linear in g against s between points",
  "combination": { "rule": "quarter-ellipse per quadrant, (a/A)^2 + (b/B)^2 <= 1, A and B = 0.2 s limits", "pairs": ["xy", "xz", "yz"], "three_axis": "optional, not in the standard" },
  "reversal_xy": { "max_peak_gap_s": 0.2, "factor": 0.5, "min_event_s": 0.2 },
  "weightless_to_pos_z": { "min_weightless_s": 0.2, "threshold_g": 2.0, "min_rise_s": 0.133 },
  "conditions": [
    { "applies": "x_pos above 1.5 g", "needs": "headrest, unless onset < 5 g/s (then 2.0 g without); without headrest max 4 s above 1.5 g" },
    { "applies": "x_neg_prone", "needs": "onset < 15 g/s and padded restraint" }
  ],
  "children": { "kind": "multiplier_by_min_height_in", "points": [[32, 0.6], [48, 1.0]], "z_applies_to": "difference from 1 g", "informative": true, "confidence": "medium" },
  "restraint_areas": {
    "axes": ["x", "z"],
    "polygons": "Areas 1 to 4 as polygons in (ax, az), Area 5 = outside; lower area on a boundary",
    "confidence": "high for grid values, approximate on the diagonals"
  }
}
```

The train type (D2) then carries `limit_set`, `restraint_class`, `restraint_kind` (selects the `x_neg_*` line, and whether `z_neg_extended` is allowed), `min_rider_height_in` (selects the children's multiplier) and, for reclined or flying seats, `seat_pitch_deg` to turn the track frame into the patron frame.

### What the check would do (for A3)

For each of x, y and z: filter at 5 Hz, slice, find events of 0.2 s or more, and compare each (duration, level) pair with the curve. Then: the three egg checks on the instantaneous filtered values; the reversal check in x and y; the push-pull and weightless-to-positive checks in z; and the restraint area from 1 Hz data, compared with the train type's class. Each failure becomes a finding: where on the track, the value, the limit, which rule, and the confidence of the rule.

### What is missing or uncertain (the list to check against a bought copy)

1. Every **Gz** break point: 1.0, 2.0, 4.0, 5.0, 11.8 and 12.0 s, and the 6/4/3/2 g levels. These come from one source (R), with the 6 g / 1 s also in W.
2. What applies to +Gz **beyond 40 s**.
3. The **push-pull** curve: 5.0 g up to 1.5 s, 2.0 g from 2.5 to 6 s. Low confidence; read off a small drawing.
4. The **-Gz** times: 0.5, 4.0 and 7.0 s. The extended line: 1.0 and 3.0 s.
5. The **Gy** times: 1.0 and 2.0 s. The levels are confirmed.
6. The **-Gx base case**: whether it reaches -1.5 g at 0.5 s.
7. The exact **egg** definition: the instantaneous 5 Hz values; whether the radii are always the 0.2 s values or change with duration; and which -x and -z radius applies for over-the-shoulder, prone and extended restraints.
8. The **reversal** clause (7.1.6.1): the exact wording of "consecutive", how the 0.2 s between peaks is measured, and whether 50 % applies to both events.
9. The **weightless-to-positive** rule: the 200 ms, 2 g and 133 ms values, and its clause number.
10. The **children's** multiplier: how it applies to z and to the negative limits.
11. The **restraint diagram**: the exact corners of the diagonals, and the current text of the Class 3 to 5 requirements (the source is the 2004 edition).
12. Any change between F2291-23b (the edition R describes) and the current F2291-26b. Clause numbers have moved: in 26b, acceleration limits are section 6 and restraints section 5 (ST abstract).

## Should the owner buy ASTM F2291?

- **Price and where.** F2291-26b, the active edition, 73 pages, US$121 from the ASTM store (ST). F2291-23b, the edition Rohde describes, is listed as historical at US$145 (ST). ASTM revises F2291 several times a year (ST lists -23a, -24, -25, -25a to -25c, -26, -26a, -26b).
- **Cheaper legitimate routes.** None confirmed.
  - The ASTM Reading Room page could not be fetched (403). As far as I know it covers standards incorporated into US federal regulations, and F2291 is adopted by states, so it is probably not there. Not established.
  - Universities with an ASTM Compass subscription may give walk-in access. Not checked for any library near the owner.
  - A copy of the **2006** edition is posted on archive.org by Public.Resource.Org under its "incorporated into law" collection (<https://ia800607.us.archive.org/34/items/gov.law.astm.f2291.2006/astm.f2291.2006.pdf>). I did not read it. It is the 2006 edition, many revisions out of date.
- **Recommendation: buy the current edition when A3 starts, not now.** For a hobby design aid, the public numbers are good enough to build and test the check: the +Gx, -Gx and restraint diagram are high confidence, the rest medium. But the Gz curves come from a single source, and a design check must not be silently wrong. US$121 is small next to that risk, and it also settles the current edition. When the copy arrives, the owner transcribes the 12 items above into the limit file by hand, with the edition and clause for each. A3 can then raise every value from medium to high, or correct it.
- **One caution on licence.** The ASTM store page carries a notice that using AI on ASTM standards is prohibited and can lead to suspended access. So the owner should read the PDF himself and type the numbers into the limit file, rather than handing the PDF to Claude.
- **If he does not buy:** the check ships with the structure and the public numbers, labelled with source and confidence, and the program says plainly that the limits are transcribed from secondary sources.

## 7. Forces at every seat (scope for A4)

**Kinematics.** Take a reference point O on the car (for example the heartline point at the car's centre). It has acceleration **a_O** in the world frame. Let **R** be the car's orientation (columns X forward, Y left, Z up). Let **ω** and **α** be the car's angular velocity and angular acceleration, written in the car frame. A seat at fixed offset **r** from O (in the car frame, for example r = (0, ±0.6, 0) m for the outer seats of a four-across row) then has, in the car frame:

  **f_seat = f_O + (α × r + ω × (ω × r)) / g**,  with  **f_O = Rᵀ (a_O − g_vec) / g**

where g_vec = (0, −9.81, 0) m/s² in FVD++ world axes, so f_O is the specific force at O (what FVD++ already computes at the heartline). The components of f_seat are Gx, Gy and Gz at that seat. Rohde uses the same two terms, αR and ω²R, for the head (R section 7.6.2, Figs. 7.29 to 7.31). ω and α come from the change of the car frame between nodes, not from the track's roll angle alone.

**Size of the effect** (my own illustration, not from a source). A roll rate of 180°/s (π rad/s) at r = 0.75 m gives ω²r ≈ 7.4 m/s² ≈ 0.75 g, pressing the outer riders outward. If that roll rate builds up in 0.5 s, then α ≈ 6.3 rad/s², and αr ≈ 0.48 g: up on one side, down on the other. These are not small next to a -1.1 g airtime limit or a ±2 g lateral limit, so the outer seats of wide trains matter in fast rolls.

**Which seats are critical.**

- **Outer seats of wide rows** in rolls, as above.
- **Front and rear cars.** Rohde notes that accelerations differ between the front and rear of a train, and that tests are done in the front, middle and rear cars (R sections 5.3.1 and 9). The physics, in my own words: the whole train moves at one speed at a time, but each car sits at a different point on the track. So the rear car crosses a crest when the train is already going faster and gets more airtime, and the front car enters a valley or pull-out at a different speed from the rear. The front and rear cars also see the largest pitch and yaw rates relative to the train's centre.
- **Seat height** (r_z). It changes the rotational terms in pitch and roll, and the head sits higher still (R section 7.6.2).

**What A4 needs that does not exist.** A train model (number of cars, car length, the position of the bogies, seat offsets per car, from D2). One speed for the whole train at each instant (from energy, using the train's centre of mass rather than a single point). Each car's frame from its two bogie positions on the track. Then the formula above per seat, and the same checks as A3 per seat.

## Could not establish

- A second independent source for the **Gz** and **-Gz** break-point times, and for the Gy times. The levels are corroborated; the times are not.
- The exact **push-pull** curve, and what happens to +Gz **beyond 40 s**.
- Whether the **current F2291-26b** changed any number or rule relative to F2291-23b.
- Whether **EN 13814-1:2019+A1:2024** or **ISO 17842-1:2023** differ in any number from F2291. Also the EN's price.
- The exact **egg radii** for the over-the-shoulder, prone and extended cases, and whether the radii depend on duration.
- The current wording of the **Class 3 to 5** hardware requirements (the source is the 2004 table), and any public mapping from restraint *shape* (lap bar, over-the-shoulder, vest) to class.
- Whether the **ASTM Reading Room** or a nearby library offers F2291 for free.
- A firm rule for **events under 0.2 s**: the Delta-v method in Appendix X11 is informative and under review.
- That every FVD++ section type steps **one node per millisecond**. It is true for forced sections; A3 should check the others.
- The F2291 2006 text on archive.org was not read, so nothing here comes from it.
