# 0008: Where a C runtime maths function changes the track, compute it as the 2016 binary did

- Status: proposed; needs the owner's yes
- Date: 2026-11-10
- Needs owner approval: yes (it changes what the rebuilt program writes for projects with a negative tension; record 0006 makes that the owner's call)

## Context

- The 2016 release binary linked its maths functions statically from the mingw-w64 runtime of the time. The rebuilt program gets them from the C runtime it runs on, today Windows' Universal C Runtime (UCRT), which rounds the last bit differently on some arguments.
- N1 (`docs/research/n1-difference-from-2016-on-an-owner-project.md`) traced the owner's project's difference from 2016 (up to 74 mm) to `asinhf` in `subfunc::applyTension()`, and the `four-sections` Tangent tolerance (0.975 mm) to `asinf` in `section::calcDirFromLast()`. A last-bit difference survives because a force section starts from the force the previous one measured, and because the Tangent handle length is near a pole in tight loops.
- N2 (`docs/research/n2-matching-the-2016-maths.md`) read both functions out of the 2016 machine code and reproduced them in plain double arithmetic. The reproductions equal the 2016 machine code on all 2^32 float arguments; a build with them reproduces the 2016 files of the owner's project and of every baseline project exactly, where "double, then round" (N1's guess) failed on one of three projects.
- The other float functions the core uses (`acosf`, `atan2f`, `cosf`, `sinf`, `tanf`) also differ from 2016 on a few calls per project, but no difference reached a file in nine projects.

## Decision

1. The two functions are computed by `core/fvdmath.cpp` (`fvdmath::asinh2016`, `fvdmath::asin2016`), which reproduces the 2016 binary bit for bit using double precision only: no x87 instructions, no assembly, no dependence on the processor's precision setting, no compiler flag for the whole project.
2. A maths call is moved to `core/fvdmath.*` only with evidence: a project whose files differ from the 2016 binary's because of it, and a reproduction shown equal to the 2016 machine code on every argument (or, for two-argument functions, on a large sample and on the projects). Functions that differ but change no file stay as they are; the research note lists them.
3. The 2016 binary stays the reference: a difference from it is a fault to explain, not a tolerance to add. `samples/baseline/tolerances.json` is now empty.
4. The two negative-tension projects of N1 join the baseline with their 2016 files, so a return to the runtime's `asinhf` or `asinf` fails `tools/run-baseline.py`; `tests/fvdmath` pins 364 results taken from the 2016 machine code.

## Where enforced

- `core/fvdmath.h`, `core/fvdmath.cpp`; the calls in `core/subfunction.cpp` (`applyTension`) and `core/section.cpp` (`calcDirFromLast`).
- `samples/baseline/n1-tension-*.fvd` and their `.2016.*` files; `samples/baseline/tolerances.json`.
- `tests/fvdmath/`; `docs/testing.md`.
