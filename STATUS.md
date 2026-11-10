# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-10, written by the session that set the repository up, as the closing step of the integration branch `claude/baseline` (pull request #11, merged into `master` at the owner's word).

## Focus

- **The baseline is in place.** FVD++ builds on the owner's PC with one command, the result matches the 2016 release on a test track, and the defects a current compiler found in the original code are fixed. Nothing about what the program computes has changed.
- **Features come next.** The owner is putting together a list (friendlier editing, heartline defaults, a minimum track radius check and more). Each body of work runs on its own integration branch (decision record 0004).

## Milestone now

**Milestones 1 and 2 are reached:** a reproducible build (MSYS2 UCRT64: GCC 16.2, Qt 5.15.19, GLEW 2.3.1, glm 1.0.3; `tools\build-windows.cmd`; decision record 0003) and a clean baseline (#8 fixed, `samples/baseline/` as the fixed reference, `docs/testing.md` as the check).

**Milestone 3 is the owner's feature list.** Not started; it waits for the list.

Open issues:
- #13 (`owner`): three checks deferred from the baseline: an export opened in NoLimits 2, the decision about the `.3ds` export, a NoLimits 2 CSV import.
- #14 (`owner`): the API credits are refused ("Credit balance is too low"), so the GitHub workflows run on the subscription.
- #3 (`claude`): assess the Qt path, stay on Qt 5.15 or port the renderer to Qt 6. Not urgent.
- #5 (`owner`): can be closed; both secrets and the Claude GitHub App are in place.

## Waiting on the owner

- The feature list.
- #14, when convenient: find out why the Console refuses the API key's requests.
- #13, when convenient: the three deferred checks.

## Verified

- 2026-11-10: a clean build with `tools\build-windows.cmd` ends with no errors and none of the defect warnings #8 listed; `build/dist/FVD.exe` starts with only Windows on the `PATH` and opens the baseline project.
- 2026-11-10: the comparison in `docs/testing.md`, run by driving both programs' windows: the project re-saved identically by the 2016 binary and the new build (763 bytes); Spline export byte-identical; NoLimits 2 export equal in all 230 numbers; Tangent export equal in 21 of 22 segments, segment 18's handles within 0.975 mm, roll exact.
- 2026-11-10: the fixes for #8 change no output: the three exports and the re-saved project are byte-identical before and after. The C++ on `claude/baseline` is the same, file for file, as the commit that was compared.
- 2026-11-10: Claude Code Review ran on #9 (seven points, answered and fixed), on #10 (no issues found; it read three files closely and skimmed the rest) and on #11.
- Not verified: an export from the new build opened in NoLimits 2; the NoLimits 2 CSV import, whose crash risk #8 fixed; the other importers; the `.3ds` export (not compiled in); anything on Linux or macOS. The first two are in #13.

## Watch-outs

- **The GitHub workflows are running on the subscription, not the API credits** (#14). The job is green either way; open a run and read the two "Run Claude Code" steps to see which one worked.
- **The GitHub review skips a draft pull request** and says nothing on it. Mark the pull request ready before adding the `review` label.
- **Deleting a branch closes every open pull request based on it.** Retarget dependents first (`docs/development-workflow.md`, "Practical notes").
- **MSYS2 names the Qt tools `qmake-qt5` and `windeployqt-qt5`.** Plain `qmake` does not exist there.
- **Run the build through `tools\build-windows.cmd` or the UCRT64 shell.** The script refuses to run in any other MSYS2 environment. It never empties `build/`; delete the folder for a clean rebuild.
- **`build/` is ignored by git;** `build/dist/` is about 75 MB of DLLs. Never commit it.
- **The program writes `fvd.log` where it is started and `options.cfg` next to the executable.** Both are ignored; running `bin/win64/FVD.exe` in place still creates them, so run a copy.
- **A new compiler or glm can move results by the last bit.** After `pacman -Syu`, rebuild and rerun the baseline comparison.
- **`.fvd` files are raw binary written back to front** (`docs/codebase-map.md`, "Saved project file"). The format matched between the 2016 binary (Qt 5.6) and the new build (Qt 5.15) on the test project.
- **`ui/qcustomplot.*` is a vendored library,** half the lines in the repository. Never edit it. Its warnings in a build are not ours to fix.
- **Keep GitHub Actions light.** No build workflow (`docs/maybe-later.md`); the two Claude workflows run only when asked.
