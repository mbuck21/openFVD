# N1: why the rebuilt program differs from the 2016 binary on one of the owner's projects

Investigation for Stage 1 (`docs/roadmap.md`). Written 2026-11-10 on branch `claude/n1-2016-difference`, from `claude/stage-1`. The program was not changed; every build with a change in it described below was private and thrown away.

## Summary

> **Superseded in one point by `n2-matching-the-2016-maths.md`:** the 2016 library does not compute `asinh` as "double precision, then round". It squares the input in single precision first and does the rest in double. On the owner's project and the loop project the two agree; on the force-chain project they do not. N2 has the exact formula, and the change that was merged uses it.

1. **Cause:** the C runtime's `asinhf()`. Shaping a transition with a **negative tension** calls `asinh()` on a `float` (`subfunc::applyTension()`, `core/subfunction.cpp` line 384). The rebuilt program gets that from Windows' Universal C Runtime (UCRT), which is up to 2 units off in the last place and differs from the correctly rounded answer for about a quarter of the arguments. The 2016 binary carried its own copy (statically linked from MinGW), and behaves as if it worked in double precision and rounded.
2. **How a last-bit difference becomes 74 mm:** a force section starts from the force the previous section *measured* at its last node (`secforced::updateSection()`, `core/secforced.cpp` lines 75 to 82), not from the force it was told to reach. That measurement is a difference of two directions one millisecond apart and carries float noise; a last-bit change upstream moved it from 1.999992 g to 2.000283 g. The whole 7-second normal-force curve of the next section is lifted by that 0.00029 g, which bends the track by about 70 mm over 7 seconds.
3. **How sure:** proven. A private build in which only that one call goes through double precision reproduces the 2016 binary's NoLimits 2 export, Spline export and re-saved project of the owner's project **byte for byte**. Doing the same for one `asin()` (`section::calcDirFromLast()`, whose result only the Tangent exporter and the flexion display read) makes the Tangent export match too, and also removes the one 0.975 mm difference the baseline has always had to tolerate (`four-sections` Tangent export). The six baseline projects still match exactly.
4. **Effect:** on the owner's project, exports differ by up to 74 mm per coordinate (84 mm at the end, heading 2 milliradians apart, on 235 m of track). Re-saving the project in the rebuilt program changes stored values, but those values are recomputed on every load, so **nothing is lost by saving**: the 2016-like build opens the rebuilt program's re-save and writes the 2016 exports exactly.
5. **Recommendation:** make the rebuilt program compute `asinh` (and the Tangent exporter's `asin`) in double precision and round to `float`, two lines. That brings it back to the 2016 binary on all seven projects with no tolerance, and makes the result independent of whichever C runtime a later build uses.
6. **The owner's decision:** yes or no to that two-line change. It changes what the rebuilt program writes today for projects with negative tension (back toward the 2016 program), so it is his call.

---

## 1. Reproduction

Built from `claude/stage-1` at `991395f` with `tools\build-windows.cmd`: MSYS2 UCRT64, GCC 16.2, Qt 5.15.19, glm 1.0.3, `-O2 -march=nocona`.

| Check | Result |
|---|---|
| `FVD.exe --batch-export` on a copy of the owner's project, outputs compared with `cmp` against the `.ref.` files | all four files identical |
| Same, run four times | identical every time: the output is deterministic |
| `tools/compare_exports.py` 2016 against rebuilt | Spline: 111 segments, all differ; NoLimits 2: largest difference 74.134 mm |
| Rebuilt program's exports of the original and of its own re-save | identical |
| `py -3 tools/run-baseline.py` on the clean build | `PASS` |

**"Function got parameter out of bounds" is not the lead.** The baseline projects print the same warning: all twelve baseline project files (the six projects and their six 2016 re-saves) log between 10 and 26 such lines each (for example `x = 90.000366` for `four-sections`, `x = 4.300000` for `gen-normal-shapes`). It comes from `subfunc::getValue()` being asked for the value at the section's last node, which lies a rounding error past the function's end; the value is clamped to the end and nothing else happens.

## 2. What is in the owner's project

Decoded with a throwaway script that follows `track::loadTrack()`, `secstraight::loadSection()`, `secforced::loadSection()`, `func::loadFunction()` and `subfunc::loadSubFunc()`. One track, three sections:

| # | Kind | What matters here |
|---|---|---|
| 1 | Straight | 16 m. |
| 2 | Force, time argument, fixed speed | 3.2 s. Roll and lateral functions locked (they stretch to the section's end). The normal-force function has six cubic transitions; **two have a negative tension, -1.2 and -1.6**, on transitions that change the force by 2.5 g each. It ends holding 2.0 g. |
| 3 | Force, time argument, physics speed | 7.0 s. Normal-force function of six transitions, starting at whatever force section 2 ends with. This is the function at byte offset 774. |

The project also has smoothing entries; the integer that goes from 12585 to 12485 on saving, in both programs, sits among them and is about the track's node count (a range clamped to the track's end).

One more oddity, not the cause: section 3's roll function is 7.3 s long while the section is 7.0 s (its length is set by the normal-force function, the shortest unlocked one), so the roll function's last 0.3 s are never used.

**Correction to the brief.** The six values that the rebuilt program changes at offsets 794/827, 860/893 and 926/959 are not transition boundaries: each is the **start value** (in g) of a transition of section 3's normal force (`startValue` sits 12 bytes into a transition record). The function's three levels, 2.0 g, 0.1 g and 2.9 g, each moved by +0.00029 g. The boundaries are untouched; the change at offsets 889 and 918 (4.999999 s to 4.9999986 s) is a boundary, and both programs make it (section 7).

**What is unusual compared with the baseline:** negative tension of more than 1 on a transition with a large change. Otherwise nothing stands out: the locked functions stretch as they should, all values are finite, there is no distance argument, no zero-length transition. The baseline has only two tensions: +0.5 (which calls `sinhf()`, which agreed with the double result on every argument tested) and -0.3 on a 6 m pitch transition of `gen-geometric-distance` with a change of -0.5 (where `asinhf()` does differ in the last bit for about 30% of the arguments in that range, but with a change that small the difference does not survive into the computed track).

## 3. Where the difference starts

The NoLimits 2 exports agree to the printed precision (seven digits) up to vertex 13 (about 40 m along); the first differences appear at vertex 14, about 0.9 s into section 2. That is inside the first transition with a negative tension (0.7 to 1.1 s). (Before that, only the lateral `x` values differ, at sizes of 1e-29 m: rounding noise in the exporter's spline fit around zero, with no meaning.)

The rebuilt program imports these maths functions from the UCRT (`objdump -p build/release/FVD.exe`): `acosf asinf asinhf atan2f cosf sinf sinhf tanf` and others. The 2016 binary imports from `msvcrt.dll` only the double-precision `acos asin atan sinh tan log10`; everything else, including `asinhf`, is linked into the executable from the MinGW runtime of the time.

A small test program, compiled with the same toolchain, compared each UCRT `float` function with the double function rounded to `float`, over a million random arguments:

| Function | Differs in | Largest difference |
|---|---|---|
| `asinhf` on [-2.5, 2.5] | 25.0% of arguments | 2 units in the last place |
| `asinhf` on [-0.32, 0.32] | 34.5% | |
| `asinf` on [-1, 1] | 6.4% | 1 unit |
| `acosf` on [-1, 1] | 1.9% | 1 unit |
| `sinf`, `cosf` on [-7, 7] | 0.12%, 0.11% | |
| `tanf`, `expf`, `atanf`, `atan2f` | 0.02% or less | |
| `sinhf` on [-10, 10] | 0 | |

For the two tensioned transitions of section 2, with the arguments the program actually evaluates (one per millisecond), 91 of 400 and 126 of 501 nodes get a different warped argument.

## 4. The decisive experiment

Private builds, each differing from the clean build only as stated:

| Build | Change | Owner's project against the 2016 files | Baseline (`run-baseline.py`) |
|---|---|---|---|
| clean | none | all four differ | `PASS`; `four-sections` Tangent differs by 0.9747 mm (inside its 1 mm tolerance) |
| A | `core/subfunction.cpp:384`: `x = (float)::asinh((double)x)/tensionArg;` | NoLimits 2, Spline and re-saved project **identical**; Tangent: 2 segments differ by 0.0019 mm | `PASS`, unchanged |
| A + tan | A, and `tan` in `mnode::exportNode()` in double | as A | |
| A + asin | A, and both `asin` calls in `section::calcDirFromLast()` (`core/section.cpp` lines 293 and 295) in double | **all four identical** | `PASS`; `four-sections` Tangent now **identical** (0 segments differ) |

So the exact lines are:

- `core/subfunction.cpp`, line 384, in `subfunc::applyTension()`: `x = asinh(x)/tensionArg;`. With `using namespace std;` and a `float` argument this calls `asinhf`. This is what changes the geometry.
- `core/section.cpp`, lines 293 and 295, in `section::calcDirFromLast()`: `asin(curDirHeart.y)` (and `prevDirHeart.y`) calls `asinf`. The result feeds only `fTrackAngleFromLast`, which only the Tangent exporter (`mnode::exportNode()`, the handle length `radius * tan(angle / 2)`) and the flexion display read. This is also the explanation for the tolerance in `samples/baseline/tolerances.json`: it was never the tangent near its pole on its own, but `asinf` feeding it.

The value that ends up different in the saved file is written in `secforced::updateSection()`, `core/secforced.cpp` lines 75 to 82: `diff = lNodes[0].forceNormal` (the first node of section 3 is the last node of section 2), then `normForce->funcList.at(0)->translateValues(diff)` and `normForce->translateValues(...)`, which re-chain every later transition's start from the one before. `forceNormal` of that node was computed at line 170 from `fPitchFromLast` and `fYawFromLast` (the change of pitch and heading over one millisecond, in degrees, `section::calcDirFromLast()`). At 2 g and the speeds here that change is about 0.02 degrees per node, while a `float` pitch of tens of degrees is only resolved to a few millionths of a degree; a relative noise of around 1e-4 in the measured force is what one should expect, and that is the 0.00029 g seen.

**What this rules out.** Because swapping one library call reproduces the 2016 output bit for bit (and does not disturb the six projects that already matched), nothing else on this file's path differs between the two programs: not uninitialised memory, not floating-point contraction or excess precision, not glm 0.9.5 against 1.0.3, not Qt 5.6 against 5.15. For that reason the `-O0`, sanitizer, `-ffp-contract=off` and glm 0.9.5 builds proposed in the brief were not run; they could only have moved the result away from both programs.

## 5. A synthetic project that shows it, and the experiment for the 2016 binary

Two projects in `samples/investigations/`, each a baseline project with a few numbers changed (`samples/investigations/README.md`). Neither contains anything of the owner's project.

- `n1-tension-force-chain.fvd`: `gen-normal-shapes.fvd` (a straight, then two force sections back to back, like the owner's) with the third transition of the first force section given a change of -2.5 g and a tension of -1.4.
- `n1-tension-loop.fvd`: `four-sections.fvd` with the force section's normal-force transition given a tension of -1.2.

The clean build (current behaviour) against build A + asin (the 2016 hypothesis):

| File | `n1-tension-force-chain` | `n1-tension-loop` |
|---|---|---|
| NoLimits 2 | largest difference 0.240 mm | 2795.4 mm |
| Spline | 222 of 226 segments, 0.221 mm | 23 of 23, 1851.3 mm, roll 3.14 rad |
| Tangent | 215 of 226, 0.214 mm | 8 of 23, 5085.8 mm |
| Re-saved project | second force section's normal-force start (byte offset 695): current `BF 80 03 6F` (-1.0001048), hypothesis `BF 80 03 71` (-1.0001050) | geometric section's three start values (offsets 504, 545, 586): current `C0 3D EA BD`, `43 D9 AB 25`, `40 55 61 00` (-2.967, 435.3, 3.334); hypothesis `41 1F 55 EC`, `45 63 1F 5F`, `41 46 DA 40` (9.958, 3633.96, 12.428) |

The loop case is extreme because the geometric section inherits the pitch and yaw *rates* measured at the top of a tight loop, where the heading is nearly undefined; the same kind of inheritance as above, with far more amplification. It makes the test unmistakable.

**Experiment for the lead (2016 binary on screen):** open each of the two files with File, Load; export NoLimits 2, Tangent and Spline with the dialog defaults (all sections); File, Save As. Then compare each 2016 file with the rebuilt program's batch export of the same project using `tools/compare_exports.py`, and look at the bytes above in the 2016 re-save.

- **Expected (the hypothesis holds):** every comparison says NO MATCH with the sizes in the table, and the 2016 re-save carries the "hypothesis" bytes. A build with the two-line change would then match the 2016 files exactly.
- **If the 2016 files instead match the rebuilt program's:** the 2016 runtime's `asinhf` gives the same last bits as the UCRT on these arguments, and the owner's project differs for a reason not found here. (Unlikely: it would contradict the byte-exact match of section 4.)
- **If they match neither:** a third behaviour; the 2016 `asinhf` is neither correctly rounded nor the UCRT one, and the fix would have to copy its algorithm.

## 6. What is right

Neither result is physically "right" and neither is a bug in the author's intent. The author meant each force section to continue smoothly from the force the train actually had at the end of the previous section, and both programs do that. The value they continue from is a numerical measurement that carries noise of about 1e-4 of its size; the design value here was exactly 2.0 g, the 2016 program happened to measure 1.999992 g, the rebuilt one 2.000283 g. Both are accidents of rounding; the 2016 one is closer only by luck.

What *is* better in the 2016 program is the maths library: its `asinh` result is the correctly rounded one, and the UCRT's is not. Computing in double and rounding gives the correctly rounded `float` on any reasonable C runtime, so it is the more portable choice, not merely an imitation of 2016.

**Practical effect for the owner:**

- On this project: 74 mm largest single coordinate, 84 mm at the end of 235 m, end heading 2.0 milliradians apart. Small for a ride, but enough that a NoLimits layout built around the 2016 export would not line up with a new export to the millimetre.
- On a longer layout: the offset applies to one section (it grows roughly with the square of that section's duration), then the next section re-measures; but the position and heading difference carries on, so after this section it would grow by about 2 mm per metre of following track (an estimate from the end heading, not measured). Where a geometric section starts at a near-vertical point, the inherited rates are ill-conditioned and the difference can be metres (the synthetic loop).
- Which projects are affected: only those with a negative tension, and among those it is chance whether a last-bit difference survives into the track. In sweeps on a copy of `gen-normal-shapes`, one tension value in twelve changed the result on a transition with a -0.5 g change, and one in eight with a -2.5 g change. The owner's project has two such transitions and was hit.
- **Old files are safe.** The start values that change on saving are derived data: on every load `secforced::updateSection()` overwrites them from the measured force. Proven: build A + asin opened the rebuilt program's re-save of the owner's project and wrote the 2016 binary's three exports exactly.

## 7. Two smaller findings on the way

- **Transition boundaries drift by one bit on each of the first two saves, in both programs.** `func::loadFunction()` (`core/function.cpp` line 170) appends each transition with `appendSubFunction(1, i-1)`, which recomputes every later boundary as `prev->maxArgument + cur->maxArgument - cur->minArgument` (line 79). In `float`, `a + (b - a)` is not always `b`, so 4.999999 s became 4.9999986 s on the first save (2016 and rebuilt alike), and the rebuilt program's second save moves the next one from 5.999999 s to 5.9999995 s; after that it is stable. The geometry does not change (exports identical). Harmless, but it is why a re-save is never byte-identical to an older file.
- **`acosf` (1.9% of arguments differ) is used by `glm::angle()` in the roll of segments flagged RelRoll** (`mnode::exportNode()`, `section::exportSection()`, export only). No project tested so far hits a difference, but it is the same kind of risk and the same remedy would apply.

## 8. Recommendation

| Option | What happens | Risk |
|---|---|---|
| **1. Two-line change (recommended):** compute `asinh` in `subfunc::applyTension()` and `asin` in `section::calcDirFromLast()` in double precision and round to `float` | The rebuilt program matches the 2016 binary exactly on all seven projects tested, the owner's included; the 1 mm tolerance for `four-sections` can be removed; results stop depending on the C runtime | Projects saved with today's rebuilt program open as they would in 2016 (the changed values are recomputed on load). A project with negative tension exported today with the rebuilt program would come out slightly different after the change (back to the 2016 result). |
| 2. Leave it and document it | Nothing changes; the owner's project and any like it differ from 2016 by centimetres | The baseline cannot cover negative tension with a large change without a tolerance; the difference may grow on longer layouts. |
| 3. Remove the amplifier: start each force section from the previous section's *designed* end force instead of the measured one | The result would no longer swing with rounding | Changes the geometry of every project with two force sections in a row, by small amounts, relative to 2016. A product decision for later (`docs/maybe-later.md`), not for Stage 1. |

Option 1 makes the program behave like the 2016 reference rather than differently from it, so it fits Stage 1's aim, but it does change what the current rebuild writes; hence it needs the owner's yes. If yes, it goes in as its own piece with a test: the two synthetic projects here become baseline projects once the lead has their 2016 files, and the four-sections tolerance comes out of `tolerances.json`.

**The question for the owner:** "Shall the rebuilt program compute these two maths functions the way the 2016 program did, so that your project TheDays (and any project with a negative tension) comes out exactly as it did in the 2016 program?" Recommended answer: yes.

## Could not establish

- **The 2016 binary's own `asinhf` and `asinf`.** It is statically linked; its source (the MinGW runtime of 2016) was not looked at. That it behaves as "double, then round" is inferred from the byte-exact matches (seven projects; the two tensioned transitions alone evaluate about 900 arguments, of which over 200 differ between the two candidate behaviours). The experiment in section 5 tests it directly.
- **Which program saved the owner's project in April 2025.** The start values in the file are exactly what the 2016 maths computes, so it was the 2016 binary or a build with the same maths library.
- **Effect on a full circuit:** estimated from the end heading, not measured on a longer project.
- The 2016 binary was not run in this unit (it has no batch mode and would open a window).
