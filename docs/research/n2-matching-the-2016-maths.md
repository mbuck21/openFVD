# N2: computing `asinh` and `asin` exactly as the 2016 binary did

Investigation and change for Stage 1 (`docs/roadmap.md`), continuing N1 (`docs/research/n1-difference-from-2016-on-an-owner-project.md`). Written 2026-11-10 on branch `claude/n2-match-2016-maths`, from `claude/stage-1`. Built with MSYS2 UCRT64 (GCC 16.2, Qt 5.15.19, glm 1.0.3).

## Summary

1. **What the 2016 library did.** The 2016 binary's `asinhf` is mingw-w64's formula `log1p(|x| + x*x / (sqrt(x*x + 1) + 1))`, with **`x*x` rounded to `float`** and everything after it in double, then rounded to `float`. Rounding the square early is what makes it disagree with the correctly rounded answer on some arguments (0.14 % of the floats in [-3, 3]) and with Windows' runtime on others (5.6 %). Its `asinf` gives, on every argument, the double-precision `asin` rounded to `float`. Both were read from the 2016 binary's own machine code and then checked by running that machine code.
2. **Candidates** (section 3): today's runtime matches one of the three test projects; N1's "double, then round" matches two; the lead's "divide in double too" matches none; **the 2016 formula matches all three, in all four files each** (NoLimits 2, Tangent, Spline, re-saved project), and also on every one of the 4 294 967 296 possible `float` arguments when compared with the 2016 machine code.
3. **The change:** a new `core/fvdmath.h/.cpp` with two functions, `fvdmath::asinh2016()` and `fvdmath::asin2016()`, in plain double arithmetic (no x87, no assembly, no dependence on the processor's precision setting); called from `subfunc::applyTension()` (one line) and `section::calcDirFromLast()` (three calls on two lines).
4. **Result:** the owner's project matches its 2016 files exactly (four MATCH, no tolerance); so do all eight baseline projects, including the two negative-tension projects now added to the baseline; the `four-sections` Tangent tolerance is gone; the undo check and the generator check pass; a new test pins 364 results.
5. **Audit** (section 4): besides these two, the core calls `acosf`, `atan2f`, `cosf`, `sinf`, `tanf`, `sinhf`, `sqrtf` on floats and `cos`, `sin`, `exp`, `pow`, `sqrt` on doubles. On real projects, `atan2f` and `cosf` give a different last bit from the 2016 binary's on up to 39 and 52 calls per project; **none of those differences reached a file** in any of the nine projects. `sinhf`, `sqrtf` never differ.
6. **Still exposed, not changed:** `atan2f`, `cosf`, `sinf`, `tanf`, `acosf` (all reproducible: a private build returning the 2016 values for all of them still matched all nine projects) and the double `cos`, `sin`, `exp`, `pow`, `sqrt` (the 2016 versions used x87 extended precision; not reproduced). There is no evidence yet that any of them changes a file; they are listed so that the next unexplained difference starts here.
7. **The owner's decision:** yes or no to this change (decision record 0008). It makes the rebuilt program write what the 2016 program wrote for projects with a negative tension, instead of what today's rebuild writes. Recommended: yes.

---

## 1. What the 2016 binary contains

The 2016 `FVD.exe` has no symbol names, but its maths routines sit together near the end of the code (from address `0xEA8630`). They were identified by their instructions and by their callers, for example `subfunc::applyTension()` at `0x4103D0`, recognisable by its two `sinhf` calls, its `asinhf` call and the tension constants.

| 2016 function | Address | What it does |
|---|---|---|
| `asinhf` | `0xEAD7E0` | `x*x` in float (`mulss`); `z = (double)(x*x)`; `s = sqrt(z + 1)` with the x87 `fsqrt` stored to double; `w = |x| + z / (s + 1)` in double; if `w >= 0.29289321881345243` (that is 1 - sqrt(2)/2) `log(1 + w)` with `fyl2x`, else `log1p(w)` with `fyl2xp1`, stored to double; rounded to float; sign from `x > 0` (so `asinhf(+0)` is `-0`). Infinity and NaN returned unchanged. |
| `asinf` | `0xEAD7B0` | x87: `atan(x / sqrt(1 - x*x))` with `fpatan`, rounded to float. |
| `acosf` | `0xEAD780` | x87: `atan(sqrt(1 - x*x) / x)` with `fpatan`. |
| `atan2f` | `0xEAD8C0` | x87 `fpatan`, rounded to float. |
| `cosf`, `sinf` | `0xEADA10`, `0xEAE540` | `(float)cos((double)x)`, where `cos` and `sin` are mingw-w64's x87 `fcos`/`fsin` routines. |
| `tanf`, `sinhf` | `0xEAE5A0`, `0xEA8630` | `(float)tan((double)x)`, `(float)sinh((double)x)`, calling `msvcrt.dll` (the binary imports only `acos asin atan sinh tan log10` from it). |
| `sqrtf`, `sqrt` | `0xEA8750`, `0xEA8650` | x87 `fsqrt`. |
| `exp`, `pow`, `log` | `0xEADA60`, `0xEADE60`, `0xEADC70` | x87 (`f2xm1`, `fyl2x`). |

**Running the 2016 machine code.** A small test program (in a scratch folder, not in the repository) copies the 2016 `FVD.exe` image to its original addresses, fills in the six `msvcrt.dll` entries, and calls these routines directly. That makes the 2016 library an oracle that can be asked about any argument, on this PC, without opening a window. It also explains why the lead's experiment was mixed: the 2016 `asinhf` is neither the correctly rounded result (N1's guess) nor the Universal C Runtime's (UCRT) result, but agrees with each on most arguments.

**The x87 precision setting does not matter.** The x87 unit can be set to round intermediate results to 64 or 53 bits; the program's start-up code (mingw-w64's, in both programs) sets 64. The 2016 `asinhf` and `asinf`, and the reproductions, were compared under both settings: identical on every argument either way.

**mingw-w64 source.** MSYS2's old msvcrt-based `mingw64` environment is installed on this PC but empty (no libraries), and nothing was installed. The source of mingw-w64 v5.0.0 (`mingw-w64-crt/math/asinhf.c`, read on GitHub's mirror) has exactly the formula found in the machine code: `z = __fast_log1p (z + z * z / (__fast_sqrt (z * z + 1.0) + 1.0));` with `z` a `float`, so `z * z` is a float product; `__fast_log1p` and `__fast_sqrt` are the x87 helpers. That file says it "is placed in the Public Domain". `core/fvdmath.cpp` was written from the machine code and says where the formula comes from.

## 2. Proof that the reproductions are exact

Every `float` bit pattern (2^32 = 4 294 967 296 arguments, including infinities, NaNs and denormals) was passed to the 2016 machine code and to the function compiled from `core/fvdmath.cpp` with the project's compiler and flags:

| Function | x87 at 64 bits | x87 at 53 bits |
|---|---|---|
| `fvdmath::asinh2016` against 2016 `asinhf` | 0 differ | 0 differ |
| `fvdmath::asin2016` against 2016 `asinf` | 0 differ | 0 differ |

A version with `long double` and `logl`/`log1pl`/`sqrtl` was also exact, but needs the x87 unit; the plain double version was chosen. It calls `std::sqrt`, `std::log`, `std::log1p` and `std::asin` in double precision: the double results of today's UCRT agree with the 2016 ones closely enough that, after rounding to float, not one of the 2^32 results differs. A future runtime with a worse double `log1p` or `asin` could in principle break that; `tests/fvdmath` would show it (below).

`tests/fvdmath/` pins 201 `asinh` and 163 `asin` results taken from the 2016 machine code: special values, 90 arguments where the 2016 `asinhf` differs from the correctly rounded result, 60 where it differs from the UCRT's, 100 where the UCRT's `asinf` differs from the 2016 one, and plain ones. It runs with `tools\run-tests.cmd`.

## 3. Candidates on the three projects

The test that counts: a build with the candidate must reproduce the 2016 binary's files of all three projects, each of the four files exactly. "Own." is the owner's project (`samples/private/`, not in the repository), "loop" `n1-tension-loop`, "chain" `n1-tension-force-chain`; the 2016 files of the last two are now in `samples/baseline/`. NL2, Tan., Spl., Save are the NoLimits 2, Tangent and Spline exports and the re-saved project.

| # | `asinh` in `applyTension()` | `asin` in `calcDirFromLast()` | Own. | Loop | Chain | Source of the result |
|---|---|---|---|---|---|---|
| 0 | UCRT `asinhf` (today) | UCRT `asinf` | none (74 mm) | none (2795 mm) | NL2, Spl., Save match; Tan. 1 segment off | N1; the lead |
| 1 | `(float)asinh((double)x)`, divide in float (N1 "A") | UCRT `asinf` | NL2, Spl., Save; Tan. 2 segments off (0.002 mm) | | | N1 |
| 2 | as 1 | `(float)asin((double)x)` (N1 "A + asin") | **all four** | **all four** (the 2016 re-save holds the predicted bytes) | none: NL2 0.240 mm, Spl. 0.221 mm, Tan. 0.214 mm; Save `BF 80 03 71` at offset 695 where 2016 has `BF 80 03 6F` | N1; the lead |
| 3 | `(float)(asinh((double)x)/(double)tension)` | `(float)asin((double)x)` | none (16.6 mm) | none | none | the lead |
| 4 | **2016 formula, `fvdmath::asinh2016`** | **`fvdmath::asin2016`** | **all four MATCH** | **all four MATCH** | **all four MATCH** | this unit, build of this branch |

Candidate 4 is the change. With it, `py -3 tools/compare_exports.py` on the owner's project against `TheDays.2016.*`: NoLimits 2 "1120 numbers, largest difference 0.0000 mm", MATCH; Tangent "111 segments, 0 differ", MATCH; Spline "111 segments, 0 differ", MATCH; re-saved project "identical, 1149 bytes", MATCH.

How the three behaviours relate, over every float in [-3, 3]: the 2016 `asinhf` differs from the correctly rounded `asinhf` on 0.14 % of them (1 unit in the last place at most) and from the UCRT's on 5.6 % (2 units at most). The owner's project and the loop evaluate arguments where the 2016 result is the correctly rounded one and the UCRT's is not; the force chain evaluates at least one argument where the 2016 result is not correctly rounded and the UCRT's agrees with it. Only the exact formula covers all three.

## 4. Audit: the other maths functions on the model's path

Every maths call in `core/` that can change the computed track or an export, with how often today's UCRT gives a different result from the 2016 binary's. "Per float" counts every float value in the range once (exhaustive: all of them were tried); "uniform" draws 20 million values evenly over the range, which weights large values the way real arguments are spread (N1's figures were of this kind). "Calls" is from a private build that intercepted the float functions and counted, for each of the nine projects (eight baseline projects and the owner's), how many calls were made and on how many the UCRT result differed from the 2016 one; it counts every call the batch export makes, including the 3D view's.

| Function | Where (file:line) | UCRT differs from 2016: per float | uniform | Largest | 2016 reproducible as | Calls in the nine projects (differing) | Changed |
|---|---|---|---|---|---|---|---|
| `asinhf` | `subfunction.cpp:384` (`applyTension`, negative tension) | 32.8 % of all floats; 5.6 % in [-3, 3] | 24.7 % in [-2.5, 2.5] | 2 ulp | `fvdmath::asinh2016`, exact on all 2^32 | 4 of 9 projects call it (those with a negative tension) | **yes** |
| `asinf` | `section.cpp:293`, `295` (`calcDirFromLast`: Tangent export, flexion) | 0.11 % in [-1, 1] | 6.4 % | 1 ulp | `(float)asin((double)x)`, exact on all 2^32 | every project, every node | **yes** |
| `acosf` | through `glm::angle`: `mnode.cpp:185`, `186` (`exportNode`), `section.cpp:136`, `137` (`exportSection`), on segments flagged RelRoll | 0.033 % | 1.9 % | 1 ulp | `long double` `atan2l(sqrtl(1-x*x), x)` exact on all floats in [-1, 1] (x87); `(float)acos((double)x)` differs on 2 floats | 52 calls, 0 differ (chain only) | no |
| `atan2f` | `glm::atan(y, x)`: `mnode.h:69`, `70` (`getPitch`, `getDirection`, every node), `mnode.cpp:78`, `182`; `section.cpp:133`, `284`, `288`, `294`; `atan2` in `secnlcsv.cpp:53` | (two arguments: sampled) | 0.0001 % | 1 ulp | `(float)atan2((double)y, (double)x)`: 0 of 20 million samples differ | 0.28 to 0.63 million calls per project, 0 to 39 differ | no |
| `cosf` | `cos(float)`: `section.cpp:295`, `mnode.cpp:205`, `secforced.cpp:149`, `287`, `seccurved.cpp:160`, `secgeometric.cpp:191`, `353`, `secbezier.cpp:255`, `subfunction.cpp:223`; inside `glm::angleAxis` (`mnode.cpp:56`, `70`, `106`, `107`, `113`, `114`, `secforced.cpp:124`, `125`, `261`, `262`, `seccurved.cpp:119`, `120`), `glm::rotate` (`mnode.cpp:185`, `section.cpp:136`), `glm::cos` (`seccurved.cpp:119`, `120`) | 0.0090 % in [-64, 64] | 0.13 % in [-3.2, 3.2] | 1 ulp | `(float)cos((double)x)`, exact on all floats in [-64, 64] | 0.10 to 0.19 million per project, 0 to 52 differ | no |
| `sinf` | the same `glm` sites as `cosf`, `glm::sin` (`seccurved.cpp:119`, `120`) | 0.0085 % | 0.13 % | 1 ulp | `(float)sin((double)x)`, exact on all floats in [-64, 64] | 70 000 to 133 000 per project, 0 differ | no |
| `tanf` | `mnode.cpp:63` (`setRoll`), `mnode.cpp:163` (Tangent handle length) | 0.0014 % | 0.021 % in [-1.6, 1.6] | 1 ulp | `(float)tan((double)x)`, exact on all floats in [-64, 64] | 46 to 454 per project, 0 differ | no |
| `sinhf` | `subfunction.cpp:378`, `383` (tension) | 0 (all floats in [-89, 89]) | 0 | 0 | not needed | up to 8400, 0 differ | no |
| `sqrtf` | `sqrt(float)` in every section's force and speed code; `glm::length`, `glm::normalize`, `glm::distance` | 0 (all non-negative floats) | 0 | 0 | not needed | | no |
| `cos`, `sin` (double) | `cos(fRoll*F_PI/180.)` and `sin(...)` (the `180.` makes them double): `mnode.cpp:209`, `210`, `secforced.cpp:165`, `166`, `302`, `303`, `seccurved.cpp:168`, `169`, `secgeometric.cpp:199`, `200`, `361`, `362`, `secbezier.cpp:267`, `268` | double results differ on 3.2 % / 3.1 % | after rounding to float: 0 of 20 million | | 2016 used x87 `fcos`/`fsin` at 64 bits: not reproduced in plain double | every node (not counted) | no |
| `exp`, `pow` (double) | `subfunction.cpp:227` (quartic/quintic shape), `395`, `400` (`applyCenter`: `pow(x, pow(2, c/2))` is double) | 0.47 % (`exp`), 0.06 % (`pow`) of double results | after rounding to float: 0 | | x87, not reproduced | | no |
| `sqrt` (double) | `secgeometric.cpp:183`, `345`, `secbezier.cpp:115` (the `9.80665` literal makes them double), `subfunction.cpp:211`, `217` | 0.024 % of double results with the x87 at 64 bits (0 at 53) | after rounding to float: 0 | | x87, not reproduced | | no |
| `fabs`, `floor` | many | exact operations: cannot differ | | | | | no |

The double calls feed double arithmetic before the result is stored as a float, so a last-bit difference in them could in rare cases change a float; none was seen.

**A private build that returned the 2016 value for all six of these float functions** (`acosf`, `atan2f`, `cosf`, `sinf`, `tanf`, `sinhf`), on top of this branch, still matched all nine projects exactly: replacing them as well would be safe, but there is no file it would fix, so the change leaves them alone (decision record 0008: only with evidence). If a future project differs from its 2016 files, the first experiment is that build: `core/fvdmath.cpp` gets `cos2016` and friends in two minutes from the table above.

## 5. What this does to the owner's files

- Projects without a negative tension: nothing changes (all six earlier baseline projects were already exact and still are).
- Projects with a negative tension: the rebuilt program now writes what the 2016 program wrote. A file exported with today's rebuild before this change can differ from one exported after it (on the owner's project by up to 74 mm, on the synthetic loop by metres). Project files are safe either way: the values that differ are recomputed on every load (N1, section 6).
- The Tangent export: the last-bit `asin` difference no longer moves handles; `four-sections` now matches exactly, and its 1 mm tolerance has been removed from `samples/baseline/tolerances.json`.

## 6. Checks run on this branch

| Check | Result |
|---|---|
| `tools\build-windows.cmd` | built; `FVD.exe` no longer imports `asinhf` or `asinf` |
| `py -3 tools/run-baseline.py --undo-check --fixtures` | `PASS`: 8 projects, every file MATCH with no tolerance; `UNDO OK` for all 8; the 5 generated projects identical |
| Owner's project against `TheDays.2016.*` | NoLimits 2, Tangent, Spline, re-saved: MATCH, MATCH, MATCH, MATCH |
| `tools\run-tests.cmd` | `PASS` (`fvdmath`, `traintypes`) |
| Exhaustive comparison with the 2016 machine code | 0 of 2^32 differ, for each function, at both x87 settings |

## A limit the reproduction keeps

Because the square is rounded to single precision first, an input above about 1.8e19 makes the 2016 formula return an invalid number (NaN), where Windows' current library returned a finite one. In `subfunc::applyTension()` that needs a tension below about -44. The tension box of the transition editor is limited to -10 to 10, so it cannot be typed; a hand-edited or imported project with such a tension would get an invalid track from that transition on, exactly as in the 2016 program. If the tension range is ever widened, or loaded values are checked, this is where to look. (Noted by the GitHub review of the change.)

## Could not establish

- Which mingw-w64 release the 2016 binary used; the formula matches v5.0.0's `asinhf.c`, but nothing here depends on the release.
- Whether the other exposed functions (section 4) ever change a file. Nine projects say not yet; a longer layout with many RelRoll segments or force sections in a row is the most likely place.
