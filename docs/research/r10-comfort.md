# R10: Comfort beyond the standard (jerk, roll, head)

Research for roadmap piece R10 (feature request FR-17). It feeds A7 (comfort measures, Stage 3). Written 2026-11-11; every link was accessed on that date unless it says otherwise. It builds on R3 (what the standards limit), R1 (the heartline) and R6 section 1.4 (jerk); it does not repeat them.

## Summary

1. **There is no published comfort limit for coaster jerk, roll rate or roll acceleration.** The only coaster numbers are the 5 g/s design aim (Rohde, through R3) and the 15 g/s onset ceiling that ISO 17929 is reported to carry (second-hand). A measured, well-liked dive-coaster valley peaked at about 7 g/s and its authors say it "feels very smooth", so 5 g/s is an aim and not a boundary. Everything A7 colours "smooth / noticeable / rough" is therefore the designer's own scale, and A7 should say so on screen.
2. **Other fields tolerate far less jerk, and that is the wrong yardstick.** Lifts and railways aim at about 0.2 m/s³ to 1 m/s³ (0.02 to 0.1 g/s); a coaster at 5 g/s is about 49 m/s³. Rail is still the best source for *how to compute* the measure (1 s windows, lateral jerk, roll velocity) and for the finding that the numbers explain only part of what riders say.
3. **Compute jerk the way the standards compute onset**: low-pass the force at 5 Hz (4-pole Butterworth, as R3), then take the least-squares slope over a 0.1 s window. A raw node-to-node difference of a model that has 1 ms kinks at section joins is noise.
4. **Roll rate is easy and roll acceleration is the useful one.** Roll acceleration times the height of the head above the roll axis is the sideways acceleration the head feels (formula and worked example in section 2). Heartlining matters because it shrinks that height from about 1.6 m (axis at the rail) to about 0.55 m, nearly a factor of three. No source gives a limit in deg/s or deg/s²; the proposal below is derived from the physics and flagged low confidence.
5. **Head motion is a lateral-jerk and roll-geometry problem, and part of it is invisible to a design tool.** Measured head accelerations on real coasters are far below injury thresholds (about 20 g peak, head injury criterion 9.4 against a threshold of 300). "Headbanging" is a comfort matter, blamed on abrupt lateral transitions, restraint gaps and rough track; the last two cannot be predicted from the centreline.
6. **Recommended order for A7: lateral jerk, roll acceleration at the head (sideways head acceleration), normal-force jerk.** Roll rate is a cheap fourth. Add a pacing count (force reversals per minute) as a cheap fifth, flagged as the designer's own idea with no source.

> **Caution.** These measures are the designer's own aids. Nothing here is a standard, a limit or a safety finding. Where a number has only a forum or a search summary behind it, the table says so.

---

## Sources and confidence

Confidence: **high** = two independent good sources agree or the source is a measurement/standard text seen; **medium** = one good source; **low** = one weak source, an enthusiast's estimate, a second-hand summary, or my own derivation (marked "derived").

| Key | Source | Notes |
|---|---|---|
| **PE** | A. Pendrill, D. Eager, *Velocity, acceleration, jerk, snap and vibration: forces in our bodies during a roller coaster ride*, Phys. Educ. 55 (2020) 065012. <https://iopscience.iop.org/article/10.1088/1361-6552/aba732> (open access; full text read as PDF) | Peer-reviewed, accelerometer data from Liseberg's Helix and Valkyria. Quotes ISO 17929 second-hand. |
| **EP** | D. Eager, A. Pendrill, N. Reistad, *Beyond velocity and acceleration: jerk, snap and higher derivatives*, Eur. J. Phys. 37 (2016) 065008. <https://opus.lib.uts.edu.au/bitstream/10453/56556/4/6EA34412-B9D5-4C21-B707-55A3F7C73DB1%20published.pdf> | Only seen as search snippets; used for one statement. |
| **R3** | `docs/research/r3-force-limits.md` (Rohde 2024, Cargill 2019) | The 5 / 10 / 15 g/s advice, the 5 Hz filter, the 0.1 s onset slope, the reversal rule. |
| **VTI** | Swedish National Road and Transport Research Institute, note 56A-2003, a European tilting-train comfort study describing ENV 12299. <https://vti.diva-portal.org/smash/get/diva2:670177/FULLTEXT01.pdf> (full text read) | Rail comfort index inputs and passenger-vote correlations. |
| **PWI** | P. Allen, A. Bevan, *Impact of the application of exceptional track design rules on ride comfort and track forces*, Permanent Way Institution talk, 2020. <https://www.thepwi.org/wp-content/uploads/2021/02/Presentation_200304_Passenger-comfort-design-analysis.pdf> (read) | UK rail trial and literature review. |
| **KONE** | G. Hawkins, *Ride and building comfort in vertical transportation*, KONE slides, 2023. <https://www.vlr.nl/wp-content/uploads/2024/10/TC-23-028-Info-sessie-ride-comfort.pdf> (read) | Lift measurement practice; says ISO 18738 sets no levels. |
| **EK** | electrical-knowhow.com, *Human effects on elevator design*. <http://www.electrical-knowhow.com/2019/03/Human-effects-on-elevator-design.html> (page would not load; figures taken from a search summary of it) | Lift jerk and acceleration guidance; low. |
| **HM** | Pfister, Chickola, Smith, *Head motions while riding roller coasters: implications for brain injury*, Am. J. Forensic Med. Pathol. 30 (2009) 339. <https://www.researchgate.net/publication/38078595_Head_Motions_While_Riding_Roller_Coasters> (abstract and secondary summaries) | Head linear and angular acceleration, three coasters. |
| **SM** | Smith, Meaney, *Head injury risk from roller coasters*, J. Neurotrauma 19 (2002). <https://www.seas.upenn.edu/~molneuro/pdfs/SmithMeaney2002_RollerCoasters.pdf> (PDF would not extract; used through the UBC summary) | Worst-case model of head accelerations. |
| **UBC** | UBC Wiki, *Injury biomechanics of the head with relation to roller coasters*. <https://wiki.ubc.ca/Documentation:FIB_book/Injury_Biomechanics_of_the_Head_with_Relation_to_Roller_Coasters> (read) | Table of the Arndt, Pfister and Kuo measurements. Student wiki; medium-low. |
| **KW** | Kuo, Wu et al., *Pilot findings of brain displacements and deformations during roller coaster rides*, J. Neurotrauma (2017). <http://biomechanics.stanford.edu/paper/JNEUROT17.pdf>, PubMed 28683585 (abstract-level) | Mouthguard measurements, two riders. |
| **ISO2631** | ISO 2631-1 comfort scale as printed in public papers, e.g. <https://pmc.ncbi.nlm.nih.gov/articles/PMC11134141> | Second-hand; paid standard. |
| **FORUM** | Headbanging threads: <https://themeparkreview.com/forum/topic/31639-over-the-shoulder-restraints-otsrs/>, <https://coasterforce.com/forums/threads/head-banging-help.30324/>, <https://forums.pointbuzz.com/Forums/Topic/avoiding-the-head-bang> | Enthusiast opinion; low. |
| **STOLL** | A. Stoll, *Human tolerance to positive G as determined by the physiological end points*, 1956. <https://spacemedicineassociation.org/download/history/history_files_1956/27004.pdf> (seen through a search summary only) | Aerospace tolerance against onset rate; low as used here. |

---

## 1. Jerk

### 1.1 What the amusement-ride sources say

| Value | Source | Confidence |
|---|---|---|
| No general jerk limit in ASTM F2291 / EN 13814. The conditions that exist: onset under 5 g/s for the headrest case, under 15 g/s for prone restraints, at least 133 ms from 0 to 2 g after airtime (average 15 g/s). | R3 | high |
| Aim 5 g/s (49 m/s³), at most 10 g/s (98 m/s³), 15 g/s (147 m/s³) the most a measured test should show. Advice from the task-group member, not a rule. | R3 (Rohde) | medium |
| ISO 17929:2014 is reported to recommend a maximum onset rate of 15 g/s and a minimum drop-off rate of 0.8 g/s after a maximum-length exposure. The standard's wording was not seen. | PE (also R6 1.4) | medium (the paper is peer-reviewed; the standard is second-hand) |
| PE says the amusement-ride standards "do limit" force and jerk but give no limit for snap (the rate of change of jerk). | PE section 4 | medium |
| Measured: the valley after Valkyria's first drop, from slightly negative g to nearly 4 g, reached a jerk of about 7 g/s (69 m/s³), which is about half the 15 g/s onset ceiling; the authors say it "feels very smooth". The snap in that valley was about +22 g/s² building up and about -32 g/s² at its end. | PE section 5.2 | medium (one ride, one sensor, filtering of the plotted trace not stated) |
| Measured: ringing in the accelerometer after fast changes, for example where the Helix train leaves the launch into the curve. It comes from the sensor's own resonance and is not rider jerk. | PE section 2.2 | medium |
| Designers avoid jerk and snap by using clothoid-style transitions in loops and helices, as road and rail engineers do. | PE, EP (snippet) | medium |
| Jerk and snap are defined in the rider's own coordinate system, not the world's. | PE section 5 | high |

What this means: the useful upper numbers (5, 10, 15 g/s) are all about the *normal* (vertical) direction and onset into a *sustained* force. For the sideways direction, and for short blips that are not part of a sustained event, no number was found.

### 1.2 Other fields, and why they are a different yardstick

Rail and lifts are designed for seated or standing passengers with no restraint who must not lose balance or spill coffee, so their numbers are one to two orders of magnitude lower. They matter here for method and for what the data show about explaining "rough".

| Field | Value | m/s³ | g/s | Source | Confidence |
|---|---|---|---|---|---|
| Lift, "entirely acceptable" vertical jerk | about 2.0 m/s³ | 2.0 | 0.20 | EK (second-hand search summary) | low |
| Lift, "totally unacceptable" | about 6.0 m/s³ | 6.0 | 0.61 | EK (same) | low |
| Lift in hospitals (suggested) | about 0.7 m/s³ | 0.7 | 0.07 | EK (same) | low |
| Lift, measured maximum jerk on one KONE test trace | 1.4 m/s³ | 1.4 | 0.14 | KONE (a figure printed on an example plot) | medium (a measurement, not a limit) |
| ISO 18738 (lifts) | measures only; "no reference to allowable limits"; levels are agreed between customer and supplier | n/a | n/a | KONE | high |
| Rail, lateral jerk thresholds in the literature | 0.5 to 2 m/s³, under 1 being more typical | 0.5-2 | 0.05-0.20 | PWI (literature review) | medium |
| Rail trial: lateral jerk above about 0.8 to 0.9 m/s³ likely to lower perceived comfort | 0.8-0.9 | 0.08-0.09 | PWI | medium |
| Rail high-speed practice: maximum lateral jerk about 0.5 m/s³, often 0.35 on high-speed lines | 0.35-0.5 | 0.04-0.05 | Railway News, <https://railwaynews.net/wiki/the-geometry-of-comfort-transition-curves-explained> (secondary; credits EN 13803) | low |

So a coaster's 5 g/s aim (49 m/s³) is about fifty to a hundred times the rail figure. The riders are strapped in, braced, expecting force, and exposed for seconds, so this is not a contradiction; it only means rail values must never be pasted in as coaster thresholds.

### 1.3 How to compute it so it means something

- **Rail practice (EN 12299 as described in VTI)**: lateral acceleration is averaged over 1 s, shifting by 0.1 s; jerk is the change of that averaged acceleration over two values 1 s apart, in percent g per second; roll velocity is also a 1 s average, in deg/s. The same note tried 0.1 s windows and found the 1 s lateral jerk and lateral acceleration "strongly correlated" (a coefficient of about 0.8, against about 0.5 for 0.1 s windows), so a long window mostly repeats the acceleration itself, while the jerk is the quantity that tells a designer how long a transition should be. Confidence medium (a research report describing a draft of the standard).
- **Amusement practice**: filter at 5 Hz (4-pole Butterworth, single pass), then onset is the slope of a least-squares line over 0.1 s (R3, medium). 5 Hz means anything faster than about 0.2 s is smoothed away, which is the intent: what the body feels as a "change" is slower than the vibration it also feels.
- **Why filtering matters**: differentiating amplifies noise (PE makes the point for jerk and snap). The program has no sensor noise but has the 1 ms kinks at section joins noted in R3, which a plain difference would turn into spikes of hundreds of g/s.
- **Proposal (derived, low as to the exact windows)**: compute jerk from the 5 Hz-filtered force with the 0.1 s least-squares slope, as the standard does for onset. Offer a second, longer window (0.5 s) as an option for "transition length" feel, because the rail result suggests short and long windows answer different questions. Do not use the 1 s rail window as the default: at coaster speeds a 1 s window spans 20 to 30 m of track.
- **Direction, sign and rider axes**: compute in the rider's axes (normal, lateral, longitudinal), as PE does, using the force values the program already has (A1).
- **Report the peak and where it is, not a mean.** Rail comfort indices use maxima for the same reason.

---

## 2. Roll

### 2.1 What is known

- **Rail has the same quantity under other names.** Roll velocity of the car body (deg/s, 1 s average) is one of the three inputs of the rail curve-transition comfort index, with lateral acceleration and lateral jerk (VTI, medium). The rate at which the track's cant (the rail height difference) changes is limited directly; see section 6.
- **Passenger votes explain little.** In the same tilting-train study the correlation of average comfort vote with lateral jerk was weak (R-squared about 0.1 to 0.3 on two runs) and weaker still with roll velocity (about 0.1) (VTI, figures read from the plots, medium). This is the honest benchmark for A7: even in a research trial with instrumented trains, no single measure explained more than a third of the votes. The PWI literature review says the same of lateral acceleration and jerk thresholds: wide ranges, low correlation between passengers (PWI, medium).
- **Coaster roll rate and roll acceleration limits: none found.** Searches of standards summaries, physics-education papers, designer papers and enthusiast writing returned qualitative statements only: the roll into a bank "can be sculpted precisely" and the difference between a jarring and a flowing turn is a few degrees of bank spread across a few metres of track (ThrillZing, <https://thrillzing.com/roller-coasters/maverick-cedar-point-around-the-bend/>, low, marketing-grade). The NoLimits 2 editor shows roll in deg/s (section 6) but no recommended range was found.
- **Snappy against smooth, qualitatively.** Enthusiasts attribute violent lateral movement to fast, badly shaped transitions on older rides (Vekoma, Arrow named in the forum thread) and say that even well-designed quick transitions on modern rides (Maverick named) can cause some (FORUM, low). Designers assume a constant heartline height and constant speed, and in reality both vary along the train (FORUM, low; consistent with R1).
- **Typical roll rates, derived, not measured.** A full 360 degree roll in 4 s averages 90 deg/s; with a smooth (sinusoidal-ramp) roll function the peak is about pi/2 times the mean, so about 140 deg/s. A transition that banks 60 degrees over 2 s averages 30 deg/s. These come from arithmetic on element durations and are low confidence as "typical".

### 2.2 How roll acceleration becomes sideways acceleration at the head

Roll about a longitudinal axis that passes through the heartline. A point on the rider at height *h* (metres) above that axis moves in a circle about it. For roll rate omega (rad/s) and roll acceleration alpha (rad/s²):

- sideways (tangential) acceleration of that point: a_side = alpha x h
- inward (centripetal) acceleration: a_in = omega² x h

(Standard rigid-body kinematics; my derivation, not a sourced statement. The head is moved sideways by alpha x h and pulled toward the axis by omega² x h. The gravity component also rotates with the roll; the program's lateral force already contains that, so only the *extra* terms are shown.)

The sideways term is what A7 should show. Two things control it: how fast the roll *speeds up* (alpha), and how far the head is from the axis (h). That is the reason for heartlining: the axis is put at the rider's body, not at the rail.

**Worked example.** A roll that speeds up from 0 to 40 deg/s in 0.5 s, so alpha is steady over that time.

- omega = 40 deg/s = 40 x 0.01745 = 0.698 rad/s
- alpha = 0.698 / 0.5 = 1.396 rad/s² (about 80 deg/s²)

Head 0.55 m above the roll axis (heartline at chest height, head about 0.5 to 0.6 m above it; the head height is my estimate, low confidence, and varies by rider, seat and train, see R1):

- a_side = 1.396 x 0.55 = 0.768 m/s² = 0.078 g
- a_in = 0.698² x 0.55 = 0.487 x 0.55 = 0.268 m/s² = 0.027 g

Same roll, axis at the rail (head about 1.6 m above it, as for a train that is not heartlined; the 1.6 m is my estimate, low confidence):

- a_side = 1.396 x 1.6 = 2.23 m/s² = 0.228 g
- a_in = 0.487 x 1.6 = 0.780 m/s² = 0.080 g

So the same roll history throws the head about 2.9 times (1.6 / 0.55) as hard sideways when rolled about the rail. If that alpha is reached in a few tenths of a second, the sideways jerk at the head is the sideways acceleration over that time: 0.078 g in 0.2 s is about 0.4 g/s; the rail-axis case 0.23 g in 0.2 s is about 1.1 g/s. These are small next to the 5 g/s normal-force aim, which is exactly why lateral head motion feels like "bang" rather than "force": it is small, sudden and in the direction the restraint lets the head move freely (section 3).

Two cautions:

1. The program's heartline height is a train-type setting (R1); the real roll axis for a rider is where the roll function is applied, so the head's offset *h* is a small vector from the heartline to the head, not a constant, and it changes sign with the seat, the rider's height and the head tilt.
2. A tight transition also swings the head if the roll axis is moving sideways (a lateral-force change); that is the lateral jerk of section 1, measured at the heartline, which A7 already computes.

---

## 3. Head and neck

- **Real head accelerations on coasters are far below injury levels.** The UBC summary lists: a helmet study across 71 coasters with a highest instantaneous value of about 20 g and a head injury criterion (HIC15) of 9.4; mouthpiece studies on three coasters with peak linear 10.2 g and 6.9 g, peak angular acceleration 1547 and 1847 rad/s², HIC15 4.1 and 1.6; a further study with 8.0 g, 290 rad/s², HIC15 2.7. The injury level quoted for minor injury is a HIC of 300, and about 4500 rad/s² is cited as the minimum angular acceleration for subdural bleeding (a 1974 study). A computer model of a looping coaster gave a HIC15 of about 11.3. (UBC, HM, SM; confidence medium: a student wiki table, with the underlying papers seen only through abstracts.) Plain reading: coaster head loads are an order of magnitude or more below injury figures; the matter for A7 is comfort.
- **A mouthguard pilot study in two riders** found peak changes in head angular velocity of 9.9 rad/s, higher than the 5.6 rad/s of soccer headers in the same study, and brain deformations the authors compare to mild soccer heading (KW, medium-low: two subjects). The authors and a 2025 pediatric review treat the long-term question as open. This is *not* a design limit and no threshold should be derived from it.
- **Head against the headrest.** PE says that a gap between head and headrest at launch gives "an unpleasant knock" to the back of the head and, in extreme rides, a mild brain injury risk; it explains that the head's own inertia and the body's damped, elastic structure let the head lag the train (PE section 2.3, medium). For design that is a reason to count *longitudinal* jerk at launches and brake runs, which FVD++ does not model yet.
- **Body resonances**: spine about 10 to 12 Hz, shoulders 4 to 5 Hz, abdomen 4 to 8 Hz, head in the axial direction 20 to 30 Hz, flexed legs about 2 Hz (PE Table 1, from a vibration text; medium). Everything above about 5 Hz is above the 5 Hz filter and below the program's own time step, so a force trace from the design tool cannot show resonance excitation.
- **Headbanging.** Enthusiast sources name three causes:
  1. *Abrupt lateral movement from fast or badly shaped transitions*, especially on older Vekoma and Arrow rides (FORUM, low). This is a design property and is what lateral jerk and roll acceleration measure.
  2. *Small flaws and fluctuations in the track and joints*, named for B&M (FORUM, low). A design tool cannot see this.
  3. *The restraint*: an over-the-shoulder restraint secures the body so the head is the only part that moves, and the gap between head and restraint (a couple of inches) sets how hard the knock is; vest-type restraints were introduced to reduce it (FORUM and Wikipedia's train article, low).
  There is no published threshold. Nothing found links headbanging to a particular g or g/s level.

Implication for A7: lateral jerk at the heartline plus the head's sideways acceleration from roll are the best available indicators of the design-controlled part. They will not detect track shuffle, wheel play or a gap in the restraint, and A7's help text should say so.

---

## 4. Vibration and roughness from the track and train

- **A design tool cannot predict it.** Vibration depends on the as-built track (rail joints, pitting, alignment), wheel condition, bogie and suspension design, structure stiffness and, on wooden coasters, how the track works under load. The design centreline has none of these. R3's note applies: the standard's filter at 5 Hz already removes the vibration band from the judged signal; PE and the vibration literature place the human sensitivity in a band from below 1 Hz (motion sickness) to well above 100 Hz (PE section 3; medium).
- **Standards for vibration** (ISO 2631 and the weighted RMS comfort scale: below 0.315 m/s² not uncomfortable; 0.315 to 0.63 a little; 0.5 to 1.0 fairly; 0.8 to 1.6 uncomfortable; 1.25 to 2.5 very; over 2.5 extremely) were written for long exposures in vehicles and are second-hand here (ISO2631, medium for the table, low for relevance to a 90 s ride). A7 should not use them.
- **Design choices that make roughness worse (derived, low):** sustained high lateral force and tight radii at speed press the wheels harder on the rails and lift the demands on the track and wheel tolerance; abrupt curvature changes (a step in curvature, not a smooth build-up) excite the structure and the train as well as the rider. That is the same reason the standards and PE recommend avoiding jerk and snap singularities (PE section 5, medium). The practical design rule is the one A7 already implements: avoid steps in force and curvature.
- **The rail analogy supports this.** Allen and Bevan found that raising cant and cant deficiency did not much degrade comfort *by itself*, but where the as-built alignment was poor the faster curving made the ride worse (PWI conclusions, medium). Geometry sets the potential; the build quality decides what the rider gets.

---

## 5. Sustained forces and physiological comfort

Only items with a source; all aerospace figures are for trained or untrained people in aircraft or centrifuges, with ranges differing between sources.

- **Positive g.** Untrained, seated, relaxed people typically reach greyout in the range of 4 to 5 g held for more than several seconds, blackout at about 4.5 to 6 g, and loss of consciousness in a few seconds at about 6 g (general-interest summaries; low). Faster onset leaves less time: Stoll's 1956 curves, as summarised, give about 2.5 s to greyout when 6 g is reached at 3.5 g/s, and about 4 g at 6 to 8 s (STOLL, low as read second-hand). The amusement-ride standards sit well inside this: R3 gives the +Gz curve, which keeps long exposures well under these figures.
- **Negative g.** Tolerance is much lower; about -2 to -3 g for short periods, with redout and a risk of eye or brain vessel damage at around -3 g (a patent and summaries; low). R3 gives the standard's -Gz curve (-2.0 g at 0.2 s down to -1.5 g for longer). Nothing sourced was found for how many seconds of mild negative g (for example -0.5 g airtime) riders find comfortable; riders seek it, so the comfort question is the onset into and out of it (the 133 ms and 15 g/s rule in R3 covers the worst case).
- **Pacing: how often forces reverse.** Designers discuss "events per minute" or the rhythm of airtime and positive g. No published definition, count or threshold was found. A count of force-direction changes per minute is a sensible *designer's* measure (section 7) but it has no source and no threshold.
- **Motion sickness** relates to slow motion (0.1 to 1 Hz) (PE section 3.1, medium). For a 1 to 3 minute ride this is rarely the limit, except for long lateral oscillation or rolling about 0.2 to 0.5 Hz, which A7 can show with the lateral-force plot rather than a measure.

---

## 6. What other design tools and studios show

- **NoLimits 2**: shows vertical, lateral and (in the force view) the forces along the track, radius combs for curvature, and roll in deg/s in the editor (CoasterCrazy thread on smoothing, <https://coastercrazy.com/forums/impossible-time-smoothing-at-point-of-connection-t32554.html>; CoasterForce forum; FVD++ 0.5 documentation, <http://lucasbosch.de/nolimits-tools/FVD++/older_versions/fvd++_0.5_documentation.pdf>). Users judge smoothness by eye on the radius comb and the force plot ("unruly jerk in the middle" is a forum phrase). No jerk read-out, no roll-acceleration read-out and no accelerometer-style comfort feature was found in public help (low: forum posts and search summaries; the NL2 manual itself was not read). The `docs/research/r2-minimum-radii.md` finding (no enforced limits) is consistent.
- **Newton2 and other force-vector tools**: nothing about comfort measures was found in public sources. Not established.
- **Rail alignment design is the same idea in other words.** A curve on a railway is entered through a transition with the cant (outer rail raised) increasing along the length, and the leftover sideways acceleration (cant deficiency, the part not cancelled by cant) rising too. The limits are on *rates*, not just the end values, which is jerk and roll in other units:

| Rail quantity | Meaning | Typical limit | Coaster equivalent | Source | Confidence |
|---|---|---|---|---|---|
| Cant deficiency, normal / exceptional | Uncompensated lateral acceleration | 100 / 150 mm, about 0.65 / 0.98 m/s² (derived with a 1.5 m reference width) | Lateral force | PWI (110 to 150 mm context) | medium |
| Rate of change of cant deficiency, EN 13803 | Lateral jerk | 75 mm/s normal, 95 mm/s exceptional, which is about 0.49 / 0.62 m/s³ (derived) | Lateral jerk | slideshare copy of EN 13803-1, <https://www.slideshare.net/slideshow/bdsen1380312010/42042408>, second-hand | low |
| Rate of change of cant, EN 13803 | Roll velocity of the track, mm of rail height per second | about 50 mm/s normal, 76 mm/s exceptional (the standard calls these indicative) | Roll rate; 50 mm/s on a 1.5 m reference width is about 1.9 deg/s (derived) | KTH slide and search summary (garbled OCR), second-hand | low |
| Other national practice | Germany, change of cant 50 to 60 mm/s; South Australia 35 mm/s | | | FRA report and a South Australian rule through a search summary | low |

The analogy: the roll of the track plays the part of cant, the lateral force the part of cant deficiency, and a clothoid-style transition keeps their rates down. The scale is not comparable: a rail transition rolls about 2 deg/s, a coaster transition 30 to 100 deg/s or more, because the rider is braced and the ride lasts seconds.

---

## 7. Proposal for A7

### 7.1 Measures to compute

All computed from the force trace in the rider's axes after the 5 Hz filter (R3), at every node (1 ms in the core), reported as the peak in each stretch and drawn along the track.

| # | Measure | Unit | How | What it tells the designer | Confidence in method |
|---|---|---|---|---|---|
| 1 | **Lateral jerk** at the heartline | g/s | slope of a least-squares line over 0.1 s of the 5 Hz-filtered lateral force; second option a 0.5 s window | How abruptly sideways force builds in a transition: the main cause of a "snappy" or headbanging feel | method high (R3 for the window and filter), thresholds low |
| 2 | **Normal-force jerk** | g/s | same | Abrupt pull-outs and pop-ups: the standards' onset concept | method high, thresholds medium |
| 3 | **Longitudinal jerk** | g/s | same, once A3 adds the along-track force | Launch and brake kicks; head against headrest (PE) | method high, thresholds none |
| 4 | **Roll rate** | deg/s | derivative of the bank angle, smoothed over 0.1 s | How fast the train turns over; large values at tight transitions | method medium, thresholds low |
| 5 | **Roll acceleration** | deg/s² | derivative of roll rate, over the same 0.1 s window | How quickly the roll starts and stops (the cause of the head being thrown) | method medium (derivative of a derivative: needs the filter), thresholds low |
| 6 | **Sideways acceleration at the head** | g | alpha x h from section 2.2, with a settable head offset h (default 0.55 m above the heartline) and optionally the full 3-D offset | The roll's contribution to the lateral force at the head; shows what heartlining buys | method medium (derived), thresholds low |
| 7 | **Sideways jerk at the head** | g/s | measure 6 differentiated, or lateral jerk at the head position | The "bang" | method medium, thresholds low |
| 8 | **Force reversals per minute** | count / min | zero-crossings of normal force minus 1 g with a dead band of 0.1 g, and the same for lateral | Pacing; has no source | method designer's choice, no thresholds |
| 9 | **Snap** (rate of change of jerk) | g/s² | derivative of measure 2 over 0.1 s | Shows singularities: the sign that a force curve has a corner (PE) | method medium, no thresholds |

### 7.2 Suggested defaults

Thresholds are the designer's, adjustable in the dialog; defaults are shipped only as a starting point and labelled with their source. None of them is a standard.

| Measure | Smooth | Noticeable | Rough | Basis | Confidence |
|---|---|---|---|---|---|
| Normal-force jerk | up to 5 g/s | 5 to 10 | over 10 (warn), over 15 (error) | Rohde's aim and ceiling (R3); 15 g/s ISO 17929 as reported in PE | medium |
| Lateral jerk | up to 2.5 g/s | 2.5 to 5 | over 5 | derived: half the normal figures, in proportion to the lateral limit being half the vertical one in the standard (R3: 3 g against 6 g at 0.2 s). A feel-based start, not measured | low |
| Longitudinal jerk | up to 5 g/s | 5 to 10 | over 10 | follows the normal-force figures; no source | low |
| Roll rate | up to 60 deg/s | 60 to 120 | over 120 | derived from element durations (section 2.1): a 360 degree roll in 6 s averages 60 deg/s, in 3 s averages 120 deg/s. Not a comfort finding | low |
| Roll acceleration | not set | not set | not set | no source or sensible derivation; show the plot, let the designer compare with a ride they know | none |
| Sideways acceleration at the head | up to 0.1 g | 0.1 to 0.25 | over 0.25 | derived from the worked example: a heartlined 40 deg/s roll in 0.5 s gives 0.08 g, the same roll about the rail 0.23 g; the standard's 0.2 s lateral limit is 3 g (R3), so these are small fractions of it | low |
| Sideways jerk at the head | up to 1 g/s | 1 to 2 | over 2 | derived from the same example | low |
| Snap | not set | not set | not set | PE gives two values from a smooth valley (about +22 and -32 g/s²), enough for a reference but not a threshold | none |
| Force reversals per minute | not set | not set | not set | no source | none |

Calibration is the honest way. **A7 should offer "mark this stretch as reference"**: the designer picks a part of a ride they know and like (a ride they have measured or ridden) and the program shows that section's values as the "smooth" line. This is the approach the rail authors took in the end: PWI concluded that comparing with the existing route's measured comfort was more appropriate than adopting literature thresholds, because they vary so widely (PWI).

### 7.3 The three that matter most

1. **Lateral jerk.** It is the quantity rail comfort practice uses as a main input, the one the standards' own onset concept extends to, and the best available proxy for the transition snap and headbanging riders describe. Thresholds uncertain; ranking by relative size along the track is robust.
2. **Roll acceleration, shown as sideways acceleration at the head.** It turns a roll history into the thing the head feels and demonstrates the benefit of heartlining. It needs the head offset as a setting.
3. **Normal-force jerk.** The only one with published numbers (5, 10, 15 g/s), and it catches abrupt entries to valleys and exits from airtime.

Roll rate is nearly free and reads naturally to designers who know NL2's deg/s, but it is a weaker predictor on its own (rail data show roll velocity explained the least of the three inputs, VTI).

### 7.4 Outer and end seats once the train is followed (Stage 6)

- Compute measures 1, 2, 6 and 7 at each seat's own position: the program will then know the position of the seat relative to the heartline, so the head offset *h* is the seat's actual lateral and vertical offset (the first and last car sit on the heartline curve at a different time, and an outer seat on a banked turn is further from the axis).
- Report the worst over the train, plus which car it is. Seats at the two ends see the same geometry at different speeds and a different roll-axis offset; which end is worse depends on the element (derived, no source found), so A7 should compute it, not assume it.
- Keep the 0.1 s window; at the train's own speed the car length (about 2 to 3 m) is a fraction of the window's distance, so no extra smoothing is needed.
- Not worth doing yet: per-rider body models, seat damping and headrest dynamics (no data).

### 7.5 What is missing and what is not worth it

Missing: longitudinal force (A3 adds it); head offset settings per train type (R1's data); a way to compare to a reference ride; the pacing count; snap. Not worth it: ISO 2631 weighted vibration, vibration spectra, resonance, any prediction of headbanging itself, motion sickness indices, or a single combined "comfort score" (rail's combined index explains little, and a single number hides which problem a designer should fix).

---

## Could not establish

- Any published limit or typical range for **coaster roll rate (deg/s) or roll acceleration (deg/s²)**, or for **lateral jerk**.
- The wording of **ISO 17929:2014** itself (15 g/s and 0.8 g/s are second-hand through one paper), and the filtering used in the cited accelerometer plots.
- The **jerk and roll values of any ride known as smooth against one known as rough**. The one measured valley (7 g/s) has no paired rough ride. Smoother-against-snappier comparisons of old and new rides are enthusiast opinion only.
- The **numerical coefficients and thresholds of the EN 12299 curve-transition index** for the current edition (an older draft's coefficients appear in VTI; they were not used because they describe passengers on a train, not coaster riders).
- Whether **NoLimits 2, Newton2 or other tools** show jerk or head-motion read-outs (public help and forums say no; the manuals themselves were not read).
- A **head-height distribution** by train type for the roll formula (R1 has heartline heights, not head heights); and a measurement of head lateral motion against lateral jerk or roll acceleration.
- A source for the **comfortable duration of mild negative g** or a **pacing count threshold**.
- The EN 13803 rate-of-change values were taken from a slide-share copy and a search summary; they should be checked against the standard before being quoted.
- The Smith and Meaney (2002) PDF and the Pfister (2009) full text would not extract; their numbers come from the UBC summary and abstracts.
