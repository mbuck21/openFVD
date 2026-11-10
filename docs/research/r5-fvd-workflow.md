# R5: How force vector design is done, where the time goes, and what to build

Roadmap piece R5 (FR-7, with FR-5). Written 2026-11-10 from the code on `claude/stage-1`, the owner's answer about his own work, and web research (sources accessed 2026-11-10). No code was changed.

## Summary

**Diagnosis.** FVD++ computes the track forwards: each section starts from the previous section's last node (position, direction, bank, speed, force level), and force sections turn by force ÷ speed, so an early edit moves *and reshapes* everything after it. The designer's problem has both ends fixed (station to station), and today he solves it by hand, by trial and error.

**Shortlist, in order:**
1. **Pinned points with a slack piece:** fix a point. After an early edit, a chosen "slack" piece is re-solved so the track meets the pin again, and nothing after the pin moves (pain 1).
2. **Goal-seeking:** a solver adjusts the values you choose until a section ends at a height, heading, bank or position (pain 2). It is built first, because it is the engine number 1 runs on.
3. **Keep shape or keep forces, per element:** a frozen element keeps its shape when the speed changes. 4. **Targets instead of increments:** transitions that end *at* a g or bank value. 5. **A knock-on view:** what moved, and by how much. 6. **Limits shown live** (A1 to A3).

**Questions for the owner** (in full at the end): the ranking; re-solve on a button or live; which values the program may change; old projects and file versions; a real project to test on.

---

## Part A: why an early change knocks on, from the code

### A.1 The chain

`track::updateTrack(index, iNode)` (`core/track.cpp`) is the whole recompute. It re-integrates the changed section from the changed node. Then, for every later section, it copies the previous section's last node in as the new section's first node (`lNodes.prepend(... lNodes[size-1])`) and calls `updateSection(0)`. Nothing else connects one section to the next. The node copied across (`mnode`, `core/mnode.h`) carries:

- **pose:** position `vPos`, direction `vDir`, lateral `vLat`, normal `vNorm`, roll `fRoll`;
- **motion:** speed `fVel`, energy `fEnergy` and running length `fTotalLength`;
- **the rates and forces at that instant:** `forceNormal`, `forceLateral`, `fRollSpeed`, `fPitchFromLast`, `fYawFromLast`.

All of it becomes the next section's starting state. A section has no stored end state and no way to ask for one. The one exception is the `tozero` roll transition (`subfunc::getValue()`, `core/subfunction.cpp`), which solves for the roll speed that brings the bank back to zero at its end. It is the only "aim at a target" behaviour in the program.

The integration is forward-only: section *k* is computed from section *k−1*. In numerical terms the program solves an *initial value problem*. The designer has a *boundary value problem*: the station is fixed at both ends, and often there are fixed points in between (a lift top, a turn around a building).

### A.2 What each kind of section does with the state it inherits

**Force section (`secforced::updateSection()`, time argument).** Each millisecond it builds the force the rider should feel from `normForce->getValue(t)` and `latForce->getValue(t)`, subtracts gravity, and turns the direction by an angle of (force ÷ speed ÷ 1000) about the lateral axis (pitching) and the normal axis (yawing) (the two `glm::angleAxis(... / estVel ...)` and `(... / prevNode->fVel ...)` lines). So:

- **The rate of turning is the force divided by the speed.** At the same g, a faster train turns more slowly. The radius is speed² ÷ acceleration.
- **The duration is fixed in seconds,** so the length of the section is speed × time. A faster train makes a longer element that also turns through a smaller angle.
- **Gravity is in world coordinates** (the `- glm::vec3(0,1,0)` term). Pitch the section's start up or down, or bank it, and the same force curve gives a different shape, because the part of the force that gravity supplies has changed.
- **The force curve is relative to the entry force.** On recompute from node 0, the first transition's start value is set to the entry node's force (`diff = lNodes[0].forceNormal;` then `normForce->funcList.at(0)->translateValues(diff)`, likewise lateral; `subfunc::translateValues()` sets `startValue`). `func::translateValues()` then chains every later transition from the previous one's end value. A transition stores a *change* (`symArg`), not an end value. So if the section before ends at 1.3 g instead of 1.0 g, this whole section's normal-force curve runs 0.3 g higher, and it ends 0.3 g higher, which carries the offset into the next force section too.
- **Roll speed is chained the same way** (`rollFunc->funcList.at(0)->translateValues(diff)` with the entry roll speed), and the bank angle itself is integrated on top of the inherited `fRoll` (`curNode->setRoll(...)` adds an increment). A bank that is 10° different at entry stays 10° different for the whole section, unless `tozero` is used. Because the prescribed forces act along the banked normal and lateral axes, a different bank also means the section turns in a different direction.

**Force section, distance argument (`secforced::updateDistanceSection()`).** The functions are read at metres travelled instead of seconds, so the element's **length** no longer changes with speed. The turning still does: the angle per node is still force ÷ speed per step, so per metre it is force ÷ speed². A distance-argument force section is **not** speed-proof in shape. It fixes the length, but a faster train still turns through less angle, and the effect is stronger than with the time argument (1/v² per metre against 1/v per second).

**Geometric section (`secgeometric::updateSection()`).** Here the two functions hold pitch change and yaw change (degrees per second, or per metre with the distance argument), applied with `changePitch()` and `changeYaw()` in world terms. Forces are worked out afterwards from the geometry.

- With the **time argument**, the total pitch and yaw turned are fixed (rate × seconds), so the element's exit direction does not depend on speed, but its size does (length = speed × time).
- With the **distance argument**, the turn per metre is fixed, so the shape is fixed and only the forces change with speed.
- One catch in both modes: the first transition starts from the *inherited* turn rate (`diff = lNodes[0].getPitchChange()` then `translateValues(diff)`; with the distance argument `getPitchChange()/fVel`; the same for yaw). If the section before ends while still turning, and its rate changes with an upstream edit, so does this section's start.

**Straight section (`secstraight::updateSection()`).** It has a fixed heartline length and no turning. The forces are simply gravity resolved into the frame (`forceNormal = -vNorm.y`). It moves and turns with its entry pose but never changes shape. The roll function runs over metres and chains roll speed like the others.

**Curved section (`seccurved::updateSection()`).** It has a fixed angle, radius and lead-in/out. The step per node is speed ÷ radius, but the loop runs until the fixed angle is reached, so the **shape is fixed** whatever the speed. Only the forces change.

**Speed (`bSpeed`).** Every section type ends each step with the same energy update. With `bSpeed` on, speed comes from energy minus height minus friction × *total track length so far* (`fTotalLength`, counted from the anchor) minus air drag. With `bSpeed` off, speed is held at `fVel` and the energy is reset to match. Two consequences:

- A **fixed-speed section is an energy firewall.** Nothing upstream changes the speed after it. Lifts, launches and brakes built this way isolate everything after them from speed changes before them. In the constructors, new straight and curved sections start with fixed speed (`bSpeed = 0`, 10 m/s), while force and geometric sections coast (`bSpeed = 1`).
- **Length upstream costs speed downstream.** Friction (`fFriction`, default 0.03) is charged on the cumulative length. Ten extra metres early in the ride take about 0.3 m of "height" of energy from everything later. That is small but not zero, and it adds up on a long layout.

**Smoothing.** A "custom region" smoothing entry stores absolute node numbers (`smoothHandler::fromNode`/`toNode`, `core/smoothhandler.cpp`). If an upstream section gains or loses nodes, the region silently slides to a different stretch of track. Smoothing attached to a section follows that section.

### A.3 Which early edit disturbs what downstream

Read the table as "I change something in section *k*; what happens to the sections after it?"

| Early edit | Changes at the end of *k* | Effect on a later force section (time) | On a later geometric (distance), straight or curved section |
|---|---|---|---|
| Move the anchor (x, y, z) or turn its yaw | nothing inside the computation: `track::startPos` and `startYaw` are offsets applied to the finished track; the anchor node itself stays at the origin | the whole track moves and turns rigidly; shape, speed and forces unchanged | same |
| Change the anchor speed | speed everywhere until the first fixed-speed section | changes shape (see the speed row) | same shape, different forces |
| Change a section's length or duration, or a force that only adds a straight-ish run | position, possibly height | moves; if the height changed, the speed changes, so it **changes shape** | moves; forces change with speed |
| Change a pitch at the end of *k* (exit angle) | direction not about the vertical | **changes shape** (gravity now acts differently), plus a lever-arm movement that grows with distance | turned rigidly, but it can now run into the ground or a structure |
| Change the exit bank angle | roll | **changes direction of turning** for the whole rest of the force chain, because forces are applied in the banked frame and the bank offset persists | banked differently; forces change |
| Change the exit force (end of *k* at 1.3 g instead of 1.0 g) | `forceNormal`/`forceLateral` | **whole force curve shifted by the difference**, and so are all later force sections, until a straight, curved or geometric section re-derives the force from geometry | none on shape |
| Change the exit roll rate or turn rate | `fRollSpeed`, pitch/yaw rate | first transition starts from the new rate | geometric sections start from the new rate |
| Change anything that changes the height or length before a coasting section | speed | turns through a different angle (rate ∝ 1/speed), different length (time) and radius (speed²/g); heading and pitch at exit change, and the error grows with every later element | same shape, different forces |
| Same, but a fixed-speed section lies in between | speed only up to that section | after the fixed-speed section, speed is unaffected; pose effects still pass | — |

**A worked example.** An airtime hill in a time-argument force section at 20 m/s, with 0 g at the crest, pitches over at g ÷ v, about 28° per second. Make the drop before it 1 m deeper, so the hill sits 1 m lower, and the train arrives about 2.4% faster (v² grows by 2 × 9.81 × 1). The hill now turns roughly 2.4% less: a hill designed to go from +30° to −30° exits at roughly −28.6° instead of −30°. Every later section starts 1.4° shallower and 2.4% faster. Over the next 100 m of track that alone puts the layout about 2.4 m higher than before, and every later force element changes its shape again for the new speed. This is the knock-on the owner describes, and it is built into the method: it is not a bug.

### A.4 What already limits the damage

- **Fixed-speed sections** cut the speed knock-on (not the pose knock-on).
- **Geometric sections with the distance argument, straights and curves** keep their shape whatever the speed. The forces change instead.
- **Geometric sections with the time argument** keep their exit direction whatever the speed.
- **The `tozero` roll transition** keeps the exit bank at zero, which stops the bank offset spreading.
- **A locked transition** (`subfunc::locked`, `func::changeLength()`) stretches to fill its section, so within one section one piece absorbs duration changes.
- **Ending sections in a "plain" state** (level, unbanked, 1 g, no roll rate) makes the next section's start insensitive to small changes. The FVD++ documentation's own tip that a roll graph which does not return to zero carries roll rate into the next section is the same advice (see B.1).

What does *not* help, despite the name: the distance argument on a **force** section (see A.2).

### A.5 Where the time goes (owner's answer, read against A.3)

1. **Knock-on.** After an early edit, every element after it has moved, and many have changed shape (all coasting force sections). The owner then re-tunes them one by one from front to back, because fixing element 3 changes the start of element 4. Each fix is a manual small "shooting" problem.
2. **Hitting a target.** Adjusting forces and durations until a section ends at a height, heading or position, or the circuit closes, is the same problem without the early edit. The program shows the result after each try, but it never moves towards the target by itself.

Both are a boundary value problem solved by hand. Banking for lateral forces, building standard elements, and editing chores are real costs but, by the owner's own ranking, secondary. They rank below unless they serve the two above.

---

## Part B: how others do it

Sources accessed 2026-11-10. "Impression" marks forum opinion, not fact. Where a page was seen only as a search summary, it says so.

### B.1 Hobby tools

**FVD++ (this program).**
- The upstream README lists the four section types, the transition shapes with time warp, the NoLimits exporters and the visualisations. ([altlenny/openFVD](https://github.com/altlenny/openFVD))
- The four open upstream issues are about building and macOS, glm, and the dead NoLimits Exchange. None asks for goal-seeking. ([issues](https://github.com/altlenny/openFVD/issues))
- The FVD++ manuals by Lucas Bosch (versions 0.31, 0.5 and 0.6; [0.6](http://lucasbosch.de/nolimits-tools/FVD++/fvd++_0.6_documentation.pdf)) describe the anchor (position, roll, pitch, yaw) as what the end of the track must match to close a circuit. They also say that a roll graph which does not return to zero carries roll rate into the next section. Seen through search summaries only; the 0.6 PDF did not load.
- The LiquidFVD fork adds autosave, a dark theme, graph dragging and a music timeline, with no solver. ([Vivent-Group/LiquidFVD](https://github.com/Vivent-Group/LiquidFVD))
- *Impressions:*
  - A NoLimits Central FVD thread advises shaping yaw first, then pitch, then bank in geometric sections, and says that banking is hardest because geometric sections ignore rider forces. ([nolimitscentral.com/forum/topics/44](https://nolimitscentral.com/forum/topics/44))
  - A CoasterCrazy thread reports different g-forces in NoLimits 2 after export and blames FVD++'s default friction of 0.03, which is higher than NoLimits 2's. ([coastercrazy.com](https://coastercrazy.com/forums/fvd-t34726.html))

**newton2** (Keith "Entropy" Cohn, 2009, for NoLimits 1). It built track from prescribed forces, "the way real coaster designers do", and its author made YouTube tutorials on lifts, loops and cobra rolls. ([Coaster101, 2009](https://www.coaster101.com/2009/12/08/very-cool-no-limits-tool/)) No manual was found. FVD++ and KexEdit both credit it.

**NoLimits 2's editor.**
- The track is a B-spline control polygon of vertices with roll points; a circuit is closed by connecting plug and socket. ([NL2 help, tutorial 4](https://nolimitscoaster.com/nolimits2/help/pages/tut4.html))
- NL2 also has formula elements and a built-in Force Vector Designer. ([NL2 docs, editor](https://nl2docs.arborarcade.com/pages/editor.html))
- The Force Vector Designer has three graphs (vertical force, lateral force, roll), a total time, a constant-speed option, a start speed, and an end speed that it only reports. It has **no target, end point or solver**. ([NL2 docs, force vector designer](https://nl2docs.arborarcade.com/pages/forcevector.html), checked directly)
- *Impressions:* users say the built-in designer cannot take a starting bank or forces, so pieces are joined at zero-bank points. The usual workflow is said to be hybrid: complex elements in FVD, the rest by hand or with formulas, and the circuit closed by hand. ([coastercrazy.com](https://coastercrazy.com/forums/how-do-you-use-the-built-in-fvd-t32513.html))

**KexEdit** (IndividualKex, MIT licence, Unity; version 0.11.1 devlog dated September 2025). ([GitHub](https://github.com/IndividualKex/KexEdit), [docs](https://individualkex.github.io/KexEdit/), [itch.io](https://individualkex.itch.io/kexedit)) It is the newest FVD tool and the most relevant one.
- **A node graph instead of a list** ([node reference](https://individualkex.github.io/KexEdit/reference/node-graph.html), checked directly):
  - Force, Geometric and Curved sections each take an *Anchor* (a full train state) in and give an Anchor out, with a time or distance duration.
  - *Copy Path* re-uses a shape and recomputes its forces from a new anchor. This is "keep shape" (C3).
  - *Bridge* joins two anchors with a Catmull-Rom spline, used to close circuits.
  - A section's input can be promoted to its own Anchor node, which detaches it from whatever came before.
- **An optimizer** ([optimizer](https://individualkex.github.io/KexEdit/user-guide/optimizer.html), checked directly): gradient descent changes **one keyframe value** until the **roll, pitch or yaw at the playhead** matches a target (for example roll back to 0°, level out, an exact turn angle). The docs recommend three keyframes, with the middle one optimised. They do not say what happens when the target cannot be reached.
- **Closing circuits** ([guide](https://individualkex.github.io/KexEdit/user-guide/complete-circuits.html)): copy the start anchor, place it near the end, and bridge to it. The guide says the bridge is basic and complex joins need manual sections.
- **Not found in its docs:** a position or height target, several targets at once, pins that hold later track still, parametric elements, or automatic banking.

**Others.**
- rcphys is a proof of concept: acceleration vectors edited in source code, an OBJ mesh out. ([pardinensis/rcphys](https://github.com/pardinensis/rcphys))
- Ian Kopack's MATLAB script turns distance, vertical and lateral g, and roll into a centreline with an NL2 export. ([iankopack.com](https://iankopack.com/2021/05/12/force-vector-design-roller-coaster-centerline-modelling-part-ii/), seen as a search summary)
- Planet Coaster 2 has smoothing tools and g-force heat maps, but banking is manual. ([planetcoaster.com](https://www.planetcoaster.com/player-guides/coaster-ride-building))
- Searches found no documentation for other FVD-style tools. That does not prove none exist.

**What the hobby tools show.** Every tool found integrates forwards. Only KexEdit has any solver, and it handles one keyframe against one angle. Nobody offers a position target, a pin that keeps later track still, or a solved connector that matches direction, bank and forces at both ends. Users cope by splitting the ride into pieces joined at "plain" states and closing by hand. No first-hand complaint about the knock-on effect was found in public text (Reddit and Discord could not be read), so the owner's own answer is the main evidence for pain 1.

### B.2 Outside the hobby

**How manufacturers work.**
- IB Stengel describes its own work as simulating motion, accelerations and loads with software it writes itself. ([rcstengel.com](https://www.rcstengel.com/en/elemente/zeitleiste/ueber-uns))
- Stengel's clothoid loop, with curvature changing smoothly and tightest at the top, was computer-calculated in the 1970s. ([Wikipedia](https://en.wikipedia.org/wiki/Werner_Stengel), [Theme Park Dispatch](https://themeparkdispatch.com/articles/roller-coaster-loop-shape/))
- Intamin, on moving its stations, brakes and lifts to the ProVI CAD system, said that before it "if something changed somewhere, we had to adjust everything again by hand". With one database, it says, change management became simple. It does not use ProVI for loops or other complex elements. ([provi-cad.de, Intamin reference](https://www.provi-cad.de/en/references/intamin-amusement-rides/), checked directly) This is the professional version of pain 1, and even there the fix covers only the simple parts.
- A Coaster101 overview says engineers use general CAD (AutoCAD, CATIA, SolidWorks, Inventor) plus in-house programs, and iterate several layout variants for a client. ([Coaster101, 2015](https://www.coaster101.com/2015/11/03/coasters-101-what-software-do-roller-coaster-engineers-use/))
- B&M, RMC, Vekoma, Mack and Gerstlauer descriptions of their process were not found.

**Research on coaster track.** All of it treats geometry as the input and forces as the output, or optimises one element:
- Pombo and Ambrósio model a given track as splines for multibody dynamics. (*Modelling tracks for roller coaster dynamics*, IJVD 45(4), 2007, [pure.hud.ac.uk](https://pure.hud.ac.uk/en/publications/modelling-tracks-for-roller-coaster-dynamics/))
- Their group builds tracks from a chain of "analytical segments" defined by a few parameters, which is the same data model as FVD's section list. ([pure.hud.ac.uk](https://pure.hud.ac.uk/en/publications/construction-of-three-dimensional-track-models-for-roller-coaster/))
- A 2018 IEEE paper derives the bank from curvature and speed so the lateral force is zero. ([IEEE 8627334](https://ieeexplore.ieee.org/document/8627334/), abstract only)
- A 2022 Twente master's thesis tunes one parametrised element at a time with reinforcement learning and says traditional geometric and force-vector design is inflexible. It does not close circuits. ([Schuttert 2022](https://essay.utwente.nl/fileshare/file/93826/Schuttert_ME_AMDA.pdf))
- An Aalto master's thesis describes industry practice as heartline splines checked for accelerations. ([Väisänen 2018](https://aaltodoc.aalto.fi/items/47131e75-c748-4dd3-9a44-6cfe966043f0))
- No published method was found that prescribes forces **and** hits an end state or closes a circuit. The gap looks real, but that is not proven.

**Road and rail alignment: fixed, floating and free.** Civil 3D gives each line, curve or spiral of an alignment one of three types ([Autodesk help: tangency constraints](https://help.autodesk.com/cloudhelp/2018/ENU/Civil3D-UserGuide/files/GUID-3F377231-BB24-4EDF-9453-A8859B08782E.htm); [EnvisionCAD tip](https://envisioncad.com/civil-3d-tip-alignments-types-and-tangency-constraints/)):
- **fixed:** placed by its own points;
- **floating:** tangent to one neighbour;
- **free:** tangent to both neighbours, with its size *computed*.

Move a fixed element and only the free and floating ones attached to it re-solve; the rest of the alignment stays where it is. Railway alignments are also anchored by absolute intersection points with a radius and transition length at each, so the geometry is not an accumulation of increments. ([Li et al. 2019](https://onlinelibrary.wiley.com/doi/10.1111/mice.12392)) **This is the clearest model for pain 1:** some pieces are fixed in the world, and some pieces are designated to absorb change.

**Parametric CAD and animation.**
- CAD sketch solvers count the degrees of freedom left and say whether a sketch is under-, fully or over-constrained. ([Onshape forum](https://forum.onshape.com/discussion/25425/what-does-fully-defined-mean))
- Animation rigs pin a hand or foot to a target and solve the joint angles back up the chain, which is inverse kinematics. FABRIK is a fast iterative solver for it. (Aristidou and Lasenby, 2011, [PDF](https://www.andreasaristidou.com/publications/papers/FABRIK.pdf))
- FVD today is "forward kinematics" only: you set each joint and see where the end lands.

**Numerical methods.**
- The **shooting method** turns a boundary value problem into a search: guess the free parameters, integrate forwards, measure how far the end misses, correct with Newton's method using a Jacobian from n + 1 trial runs, and repeat. Poor first guesses can fail; damped Newton, or **multiple shooting** (aim at intermediate points), make it robust. ([UMD ASTR415 notes](https://pages.astro.umd.edu/~ricotti/NEWWEB/teaching/ASTR415/class17.pdf))
- This is exactly what the owner does by hand.
- **Clothoid connectors:** one clothoid joining two given points with given directions in the plane can be found quickly and reliably (Bertolazzi and Frego, *G1 fitting with clothoids*, 2015, [arXiv](https://arxiv.org/pdf/1209.0910), [code](https://github.com/ebertolazzi/Clothoids)). Matching curvature too needs up to three pieces and can fail (same authors, [2018](https://www.sciencedirect.com/science/article/pii/S0377042718301924)).
- 3D Euler spirals exist in the literature. ([Harary and Tal](https://www.sciencedirect.com/science/article/pii/S0925772111000770))
- These give good first guesses for a solved connector.

---

## Part C: directions

Each direction gives: what the designer sees and does; how it attacks the knock-on effect (pain 1) and hitting a target (pain 2); what it needs from the roadmap; its size; its risks; and a cheap first trial. Roadmap pieces: **F1** core without a window, **A1** analysis framework, **T2** automation interface, **D2** train data.

### C1. Goal-seeking: aim a section at an end state  — **keep, rank 2, build first**

**What he sees.** A "Goal" panel on any section, or on a run of sections.
```
Section 7 "turnaround"   Goal:  [x] end height   = 12.0 m
                                [x] end heading = 180 deg
                                [ ] end pitch / bank / speed / position
          Free to change:       [x] duration of transition 3
                                [x] change of transition 2 (normal g)
          [Solve]   -> solved in 6 tries, 0.2 s; end height 12.00 m, heading 180.0 deg
                       normal g peak 3.4 g (limit 4.0)   [Accept] [Undo]
```
He ticks the goals (any of: end height, heading, pitch, bank, speed, or full position), ticks the same number of free values (a transition's duration, its change in g or degrees, a straight's length), and presses Solve. Each solved value lands in the normal spin box, and the solve is one undo step.

**How it works.** A shooting solve: re-run the section's `updateSection()` with small changes to the free values, build the Jacobian, take damped Newton steps, stop when the end state is within tolerance. Re-integrating one section of a few seconds costs about the same as one edit today.

**Pains.** Pain 2 directly. It is also the engine for C2 and C4.

**Needs.** F1, so the solver can call the core without the widgets and as one undo command. Optional: A1 (show limits on the solved result), T2 (so an agent can call it). D2 is not needed.

**Size.** Medium: the solver plus the panel.

**Risks.**
- No solution exists (asking a 2-second transition to turn 270°) or there are several. The panel must say "cannot reach; closest is ..." rather than produce garbage.
- Badly scaled problems (metres against degrees against g).
- Goals that need more free values than were ticked. Show the count, as CAD sketch solvers do.

**Cheap trial.** Before any UI: a small script (Python, or C++ against the core once F1 lands) that takes the baseline project's force section, frees two transition values, and solves for end height and heading. Measure the tries, the time and the failure cases.

### C2. Pinned points with a slack piece — **keep, rank 1**

**What he sees.** He right-clicks a section end (or a point in the 3D view) and picks *Pin here*. The pin stores the absolute pose there: position, heading, pitch and bank, and optionally the force level and roll rate, so the join stays smooth. Between the previous pin and this one, he marks one section, or one new *connector* section, as **slack**. Now he edits an early element:

```
 before edit:  [lift]-[drop]-[hill]-[turn*]-[helix]-[PIN]-[airtime]-...-[station PIN]
                                       ^ slack
 after edit of the drop:
               [lift]-[drop']-[hill']-[turn*']-[helix']-[PIN]-[airtime]-... unchanged
               the drop changed; the hill and helix follow as today;
               the slack turn is re-solved so the PIN is met again;
               everything after the PIN is exactly as before
```

**Why it is powerful here.** By A.2, the state after a point is pose plus speed plus force and roll rate. If the pose at the pin is restored, the **height is restored, so the speed is restored too**, except for the friction on the length that changed and air drag. That is a small difference, and the slack piece can also be asked to absorb it. So restoring the pose, and matching force level and bank at the join, makes everything after the pin identical. The knock-on is cut at every pin. The circuit closing is the same thing: the station is a pin.

**The slack piece, three flavours (choose per pin):**
1. **An existing section's own values** are solved (durations, changes), as in C1 but aimed at the pin. This keeps the designer's element type, but it needs at least six free values for a full pose, which is hard to choose well.
2. **A connector section** whose shape is solved to join two given states, like Civil 3D's "free" element or KexEdit's Bridge, but matching direction, bank and force at both ends. A first version could be a geometric section whose pitch, yaw and roll-rate curves come from a small family (for example each a sum of a few smooth bumps), solved by Newton, with a planar clothoid fit as the first guess.
3. **A straight or curve whose length or angle is solved.** This is the simplest, for pins that only need to move along the track.

**Pains.** Pain 1 above all; pain 2 for circuits and fixed points.

**Needs.** C1's solver; F1. A1 strongly recommended: the slack piece's forces must be shown against limits, because the program, not the designer, chose them. T2 is useful for testing it at scale.

**Size.** Large: pins in the model and the file, the connector, the UI for marking slack, and the re-solve rules (when to re-solve; what happens on failure).

**Trade-off and risks.**
- The slack piece gives up detail by definition. Its shape and forces are whatever meets the pin. The designer chooses where that happens, ideally somewhere he cares little about (a turnaround, a transfer run).
- If the change is large, the slack piece may become unrideable or impossible. The program must then say so and leave the old track in place.
- Pins add fields to the `.fvd` file: a format version bump with a load path (rule 4).

**Cheap trial.** Flavour 3 first, on a real project of the owner's: pin the station, let one straight's length and one curve's angle be the slack, and show that an edit to the first drop re-closes the circuit within a few centimetres in position and a tenth of a degree. If even that needs more than about two seconds per edit, or the owner finds that the slack he would allow is never enough, the idea is in trouble (see C9).

### C3. Keep shape or keep forces, per element — **keep, rank 3**

**What he sees.** An element-level switch: **Fixed: forces** (today's force section) or **Fixed: shape**. Switching to "shape" freezes the element's current geometry: its curvature, turn and bank as functions of distance. After that, an upstream speed change leaves its shape alone and its forces change instead, shown in the plot and against the limits.

**How.** Either convert the force section's computed nodes into a geometric section with the distance argument (pitch, yaw and roll per metre sampled from the nodes), or add a "fixed path" section like KexEdit's Copy Path, placed relative to its entry pose. The first needs no new section type. Watch the inherited start rate (A.2): the frozen element should hold its own start rates, not inherit them.

**Pains.** Pain 1: the shape no longer changes downstream; only the position does, and C2's pins handle that. Combined with C2, an early edit changes the edited element, the elements explicitly left as "forces", and the slack, and nothing else.

**Needs.** F1 only for the clean version. A1 helps by showing what the frozen element's forces became.

**Size.** Small to medium.

**Trade-off.** It gives up the exact force profile of the frozen element, which is the point of FVD. That is acceptable for elements whose look matters more than their feel (a big hill seen from the midway), and wrong for an airtime hill whose 0 g is the whole point. The owner chooses per element.

**Cheap trial.** Convert one element of a baseline project by script (sample its nodes, write a geometric section), lower the lift by 2 m, and compare the shape and the force changes.

### C4. Targets instead of increments — **keep, rank 4 (small, early)**

**What he sees.** A transition can be set to end **at** a value ("end at 1.0 g", "end at 0° bank", "end at 0 lateral") instead of "change by". Today's `tozero` already does this for roll speed. This extends it to force and bank, with any end value.

**Why.** A.2 shows the force curves and bank are chained as increments. An early change to an exit force or bank therefore shifts every later force section by the same amount. Ending each element at a stated value stops the offset spreading, at no cost in detail: the designer usually meant "end at 1 g" anyway.

**Pains.** Pain 1 (the force and bank offsets); a little of pain 2.

**Needs.** F1 only to do it cleanly; it is a change in `subfunc`/`func` (`translateValues()`) and the transition panel. A per-transition flag is a **file format change** (version bump and load path).

**Size.** Small.

**Risks.** The meaning of a "change" in an old project must not move: old files load as "change by". It does not stop the speed knock-on, which is the larger one.

**Cheap trial.** Count in the owner's projects how often a force section starts from a force that is not a round number. That shows how much of his re-tuning is this offset.

### C5. A knock-on view — **keep, rank 5 (small, early, supports all)**

**What he sees.** After any edit, the old track is shown as a faint ghost, and a small table lists, for each later section end and for the closing point back to the anchor: how far it moved, how much its heading, pitch and bank changed, and how much its speed changed. Optionally the track is coloured by how much each part moved.

```
 Section   moved    heading  pitch   bank   speed      Close-up to station:
 4 hill    0.0 m    0.0      0.0     0.0    +0.0       position  3.8 m
 5 turn    1.2 m   +1.9     -0.4     0.0    +0.5 m/s   heading   +2.6 deg
 6 helix   3.1 m   +2.6     -1.4     0.0    +0.5 m/s   pitch     -0.3 deg
```

**Pains.** It makes pain 1 visible and measurable. It does not fix it, but it shows where the damage stopped and is the read-out C1 and C2 report against. It also gives the owner and the tests a number for "how much did this edit disturb".

**Needs.** Nothing beyond today's model; it is cleaner after F1. The ghost needs a second mesh in `renderer/`.

**Size.** Small for the table; medium with the ghost.

**Cheap trial.** The table is only a comparison of two node lists. It can be printed by the batch mode (B1/F2) for two versions of a project.

### C6. Limits shown live — **keep, rank 6 (already on the roadmap as A1 to A3)**

Radius and force limits marked on the view and plots as the designer edits. These do not attack either pain directly, but every solver result above needs them: the owner will only trust a solved slack piece if a glance shows it is inside the limits. Needs A1, A2, A3 and D2. Already planned; nothing extra here except "make the solver report the same findings".

### C7. Parametric elements — **change: fold into C1 as presets; rank below the shortlist**

An airtime hill, loop, zero-g roll or overbank given by a few numbers. As stand-alone generators they speed up building elements, which the owner did not name as a pain. Their real value here is that an element defined by **intent** ("a zero-g roll that exits level at the same heading") can be re-expanded when its entry state changes. That is C1 with a stored goal. So: build them later as C1 goal presets (an element type, its free values and its goals), not as a separate system. Size: medium per family of elements. Risk: they limit shapes, which costs detail if they are the only way in; they must expand into ordinary editable sections.

### C8. Distance or energy as the argument — **reject as a cure; keep what exists**

The distance argument already exists. By A.2 it fixes an element's length but **not** its shape, because force-section turning per metre goes as 1/speed². "Energy-based" variants run into the same physics: the same g at a different speed is a different curve. The speed-proof option is to fix the shape (C3), not to change the clock. A one-paragraph note in the program's help, plus showing which sections are speed-sensitive (C5), is worth more than a new parameterisation.

### C9. Re-fit downstream automatically — **change: keep only as a mode of C2 and C3**

The idea: after an early edit, re-time or re-scale later sections automatically so each keeps its own shape or end state. Done to every section at once, this hides from the designer what changed, and it has many solutions. Done per element, with a stated goal, it is C1 with stored goals ("this section keeps its exit heading"), re-solved in order from front to back. Keep it as an option on elements that have C1 goals ("re-solve on upstream change") and as C2's slack. Do not build a global auto-refit.

### C10. Banking for a target lateral force — **keep, lower**

In force sections the lateral force is prescribed already. Auto-banking matters for geometric, curved and straight sections and for the slack piece in C2, whose bank should be solved for low lateral force rather than left arbitrary. Building it as a goal type in C1 ("bank so lateral = 0 throughout") covers that. Not ranked on its own: the owner did not pick it.

### C11. Layout tools: clearance, footprint, circuit closing — **split**

Closing the circuit is C2 with the station as a pin, and it ranks with C2. Clearance between track and track, and a footprint to stay inside, are checks for A1 (findings shown on the view). Useful, but not the owner's top pains. Later wave.

### C12. A coaster as text, with an agent as co-designer — **keep, as a test bench and a later wave**

An agent through T2 can already do C1's trial and error, only slower, and it can propose layouts from a description. Its best use early on is as a **test bench**: drive thousands of edit-and-solve cycles on the owner's projects through the batch mode (F2/T2) to show that C1 and C2 converge, and to find failure cases, before the owner ever touches them. As a designer it depends on C1: an agent that cannot aim is just a faster hand. Needs T2, T3, A1.

### C13. Own additions considered

- **Build from both ends and meet in the middle** (multiple shooting). Integrate one stretch forwards from the station and another backwards from a pin, and join them with a C2 connector. It is the general form of C2; keep it as the way C2 handles several pins, not as a separate feature.
- **Variants side by side.** The industry iterates several layouts for a client (B.2). Keeping two versions of a section and switching between them is useful but does not cut knock-on time. Later.
- **Edit as a "change set", reviewed before applying.** An early edit plus its re-solves shown as one proposal, with C5's table and C6's findings, then accepted or undone as one step. This is how C2 should present itself, so it is folded into C2.

### Ranking and the honest trade-offs

| Rank | Direction | Pain | Size | Gives up |
|---|---|---|---|---|
| 1 | C2 Pinned points with a slack piece | 1 (and circuit closing) | large | the slack piece's shape, chosen by the solver |
| 2 | C1 Goal-seeking (built first; engine for 1) | 2 | medium | the exact value of the numbers you let it solve |
| 3 | C3 Keep shape or keep forces | 1 | small to medium | exact forces in frozen elements |
| 4 | C4 Targets instead of increments | 1 (offsets) | small | nothing, if old files load unchanged |
| 5 | C5 Knock-on view | 1 (visibility) | small to medium | nothing |
| 6 | C6 Limits live (A1 to A3) | trust in 1 and 2 | already planned | nothing |

**Build order:** C5 and C4 are small and early (W1 after F1). C1 next, as the engine. Then C2 on top of C1, and C3 beside it (W2). C6 arrives through A1 to A3.

### The top two in more detail

**C2, pinned points with a slack piece.**
- *First buildable version:* one pin per track, the station (closing the circuit). The slack is a straight's length plus a curve's angle and radius, or a single geometric connector whose three rate curves are each a two-bump family (six unknowns for a pose: three position, heading, pitch, bank). It is re-solved when the designer presses "Re-close", not live. The result is shown with C5's table and the connector's peak forces.
- *What a week of agent work could show the owner:* given F1 and C1's solver, a command-line demonstration on one of his projects. Change the first drop by 1 m and print before and after: the circuit re-closed to under 5 cm and 0.1°, the connector's peak forces, and every section after the connector unchanged. Then a screen recording of the same in the window if the core is ready.
- *What would prove it worthless:* on the owner's real layouts, the slack he is willing to give up cannot absorb typical early edits without unrideable forces; or he finds he would rather re-tune by hand than accept a solved piece. A third failure would be solve times above a couple of seconds on a long track, which kills the "edit and see" feel.

**C1, goal-seeking.**
- *First buildable version:* one section; goals among end height, heading, pitch and bank; free values among a transition's change and duration; equal numbers of each; damped Newton with finite-difference Jacobian; a clear "cannot reach" message showing the closest result; one undo step.
- *What a week could show:* the baseline project's force section solved to end level at a chosen height and heading, both in the batch mode and as a panel. A table of a hundred random goals: how many converged, in how many tries and how long.
- *What would prove it worthless:* frequent failure to converge on the kind of elements the owner builds (inversions, where heading and bank are poorly defined near vertical), or solved values that are technically right but ugly (spiky transitions). In that case the goals need a smoothness preference, which makes it a small optimisation rather than a solve. That is more work, but it is not a dead end.

---

## Questions for the owner

1. **Ranking.** Is the order right: pins with slack first, goal-seeking second (built first because pins need it), then freezing shapes, end-at-a-value transitions and the knock-on view?
2. **How much would you let the program change for you?** When an early edit breaks a pin, should the program (a) re-solve when you press a button and show you the result to accept, or (b) re-solve live on every edit? And which values may it touch: only a connector piece you mark, also lengths and durations of your own sections, or also force values?
3. **When it cannot reach the pin within limits:** stop and leave the old track, or give the closest result and mark the problem?
4. **Keep shape or keep forces:** for most of your elements, which matters more when the speed changes?
5. **Old projects.** Projects you saved must keep opening and computing exactly as before (that is assumed). Is it acceptable that a project saved *with* the new features (pins, goals, end-at-a-value) no longer opens in the 2016 FVD.exe?
6. **A test case.** Can you share one of your projects where an early change cost you the most time, and say what you changed? It becomes the yardstick for C1 and C2.

---

## Could not establish

- **First-hand complaints about the knock-on effect** in public. Reddit, YouTube descriptions and Discord could not be read, and no forum quote on it was found. The owner's answer is the main evidence.
- **The FVD++ 0.5 and 0.6 manuals** were seen through search summaries only (the 0.6 PDF did not load). The "anchor is what you must match" and "roll rate carries over" points are therefore second-hand.
- **Whether newton2 or any tool ever had a position or circuit-closing solver.** None was found; that is not proof that none existed.
- **How B&M, RMC, Vekoma, Mack or Gerstlauer iterate a layout**, and what Stengel's in-house software does beyond the firm's own one-line description.
- **Solve times.** They are estimated from the integration scheme (each try re-integrates the free stretch at 1000 nodes per second of ride), not measured. The current recompute time on the owner's largest project is not known; it should be measured with the batch mode before C2's "live" option is promised.
- **Convergence near vertical.** Heading and bank are poorly defined when the track points straight up or down (`secgeometric` flips its pitch sign at |roll| ≥ 90°). Whether goals placed inside inversions solve reliably is unknown until tried.
- **The details of the IEEE 2018 paper** (authors, year), from its abstract only. KexEdit's optimizer behaviour when a target is unreachable is not documented.
