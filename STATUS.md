# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-10, written by the session that set the repository up. The baseline (pull request #11) and the review change (#15) are in `master`; this revision goes with the roadmap pull request from `claude/roadmap`.

## Focus

- **The baseline is in place.** FVD++ builds on the owner's PC with one command, the result matches the 2016 release on a test track, and the defects a current compiler found in the original code are fixed. Nothing about what the program computes has changed.
- **The feature list is in and planned, and Stage 1 is under way.** The owner's nine requests are in `docs/feature-requests.md`; `docs/roadmap.md` orders them into five stages. The owner asked on 2026-11-10 for Stage 1 to be finished by the lead with its team. The lead session acts as architect and the owner as customer (decision record 0006).

## Milestone now

**Milestones 1 and 2 are reached:** a reproducible build (MSYS2 UCRT64: GCC 16.2, Qt 5.15.19, GLEW 2.3.1, glm 1.0.3; `tools\build-windows.cmd`; decision record 0003) and a clean baseline (#8 fixed, `samples/baseline/` as the fixed reference, `docs/testing.md` as the check).

**Milestone 3 is Stage 1 of the roadmap:** a wider baseline (B0), the core running without a window (F1), the research pack (R1 to R5), train types as data (D2) and heartline presets (D3). **In progress since 2026-11-10** on the integration branch `claude/stage-1`; its pull request into `master` lists the 18 pieces and their state. The cut and the order are in decision record 0007.

Open issues:
- #13 (`owner`): three checks deferred from the baseline: an export opened in NoLimits 2, the decision about the `.3ds` export, a NoLimits 2 CSV import.
- #5 and #14 (`owner`): both settled. The GitHub workflows run on the subscription (decision record 0005); the unused `ANTHROPIC_API_KEY` secret can be deleted.
- #3 (`claude`): assess the Qt path, stay on Qt 5.15 or port the renderer to Qt 6. Not urgent.

## Waiting on the owner

- Screen access for the FVD++ window when the lead asks (to export the test tracks with the 2016 program), and the owner decisions listed at the end of `docs/roadmap.md` as they come up.
- Optional tidy-up: delete the `ANTHROPIC_API_KEY` repository secret and the Console key behind it; nothing uses them now.
- #13, when convenient: the three deferred checks.

## Verified

- 2026-11-10: a clean build with `tools\build-windows.cmd` ends with no errors and none of the defect warnings #8 listed; `build/dist/FVD.exe` starts with only Windows on the `PATH` and opens the baseline project.
- 2026-11-10: the comparison in `docs/testing.md`, run by driving both programs' windows: the project re-saved identically by the 2016 binary and the new build (763 bytes); Spline export byte-identical; NoLimits 2 export equal in all 230 numbers; Tangent export equal in 21 of 22 segments, segment 18's handles within 0.975 mm, roll exact.
- 2026-11-10: the fixes for #8 change no output: the three exports and the re-saved project are byte-identical before and after. The C++ on `claude/baseline` is the same, file for file, as the commit that was compared.
- 2026-11-10: Claude Code Review ran on #9 (seven points, answered and fixed), on #10 (no issues found; it read three files closely and skimmed the rest) and on #11.
- Not verified: an export from the new build opened in NoLimits 2; the NoLimits 2 CSV import, whose crash risk #8 fixed; the other importers; the `.3ds` export (not compiled in); anything on Linux or macOS. The first two are in #13.

## Watch-outs

- **The GitHub workflows run on the subscription** (decision record 0005). The promotional API credits cannot pay for them: they do not cover Claude Code.
- **The review prompt is new and untried** (decision record 0005). The first pull request that changes code is its test: check that it posts inline comments and one summary listing the files it read. The stock review it replaces skipped drafts and skimmed.
- **Deleting a branch closes every open pull request based on it.** Retarget dependents first (`docs/development-workflow.md`, "Practical notes").
- **MSYS2 names the Qt tools `qmake-qt5` and `windeployqt-qt5`.** Plain `qmake` does not exist there.
- **Run the build through `tools\build-windows.cmd` or the UCRT64 shell.** The script refuses to run in any other MSYS2 environment. It never empties `build/`; delete the folder for a clean rebuild.
- **`build/` is ignored by git;** `build/dist/` is about 75 MB of DLLs. Never commit it.
- **The program writes `fvd.log` where it is started and `options.cfg` next to the executable.** Both are ignored; running `bin/win64/FVD.exe` in place still creates them, so run a copy.
- **A new compiler or glm can move results by the last bit.** After `pacman -Syu`, rebuild and rerun the baseline comparison.
- **`.fvd` files are raw binary written back to front** (`docs/codebase-map.md`, "Saved project file"). The format matched between the 2016 binary (Qt 5.6) and the new build (Qt 5.15) on the test project.
- **`ui/qcustomplot.*` is a vendored library,** half the lines in the repository. Never edit it. Its warnings in a build are not ours to fix.
- **Keep GitHub Actions light.** No build workflow (`docs/maybe-later.md`); the two Claude workflows run only when asked.
