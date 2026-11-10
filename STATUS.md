# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-10, written by the session that set the repository up and made the first build, on the branch `claude/fix-compiler-found-defects` (stacked on `claude/build-msys2`).

## Focus

- **A sound baseline before any feature.** The owner asked on 2026-11-10 for the bugs fixed and the repository brought to a state worth building on. A list of features (friendlier editing, heartline defaults, a minimum track radius check and more) will follow from the owner.
- **FVD++ builds on the owner's PC.** MSYS2 UCRT64 with GCC 16.2, Qt 5.15.19, GLEW 2.3.1 and glm 1.0.3; one command, `tools\build-windows.cmd` (decision record 0003, `docs/building.md`).
- **The new build matches the 2016 release** on a four-section test track: same project file, same Spline and NoLimits 2 exports, Tangent export within 1 mm in one segment (`docs/testing.md`). The reference files are in `samples/baseline/`.

## Milestone now

**Milestone 1, a reproducible build on the owner's PC, is reached** with the pull request from `claude/build-msys2` (it answers #1, #2 and #4).

**Milestone 2: a clean baseline.** Done when the defects the compiler found are fixed (#8, in the pull request from `claude/fix-compiler-found-defects`), the baseline comparison still gives the same result (it does), and the owner has opened an export from the new build in NoLimits 2.

Open issues:
- #3: assess the Qt path, stay on Qt 5.15 or port the renderer to Qt 6 (`claude`, `renderer`). Not urgent: Qt 5.15 builds the code as it is.

## Waiting on the owner

- The merge of two pull requests, in this order: the one from `claude/build-msys2` (the build; decision record 0003 covers making the `.3ds` model export optional on Windows), then the one from `claude/fix-compiler-found-defects` (the fixes for #8).
- A look at the new build: start `build\dist\FVD.exe`, open `samples\baseline\four-sections.fvd`, and import one of its exports into NoLimits 2.
- The feature list.
- #5 can be closed: both secrets and the Claude GitHub App are in place.

## Verified

- 2026-11-10: a clean build with `tools\build-windows.cmd` ends with no errors; `build/dist/FVD.exe` starts with only Windows on the `PATH` and opens the baseline project.
- 2026-11-10: the comparison in `docs/testing.md`, run by driving both programs' windows: project re-saved identically by the 2016 binary and the new build (763 bytes); Spline export byte-identical; NoLimits 2 export equal in all 230 numbers; Tangent export equal in 21 of 22 segments, segment 18's handles within 0.975 mm.
- 2026-11-10: `@claude` answered a test comment on #5 (on the subscription token; the API key added afterwards has not been seen in a run yet).
- 2026-11-10, after the fixes for #8: a clean build shows none of the defect warnings; the fixed build's three exports and its re-saved project are byte-identical to the build before the fixes; the start-up log is free of the "framebuffer incomplete" lines.
- Not verified: the NoLimits 2 CSV import, whose crash risk #8 fixed (no CSV file to test with); an export from the new build opened in NoLimits; the importers; the `.3ds` export (not compiled in); anything on Linux or macOS.

## Watch-outs

- **MSYS2 names the Qt tools `qmake-qt5` and `windeployqt-qt5`.** Plain `qmake` does not exist there.
- **Run the build through `tools\build-windows.cmd` or the UCRT64 shell.** The script refuses to run in any other MSYS2 environment.
- **`build/` is ignored by git;** `build/dist/` is about 75 MB of DLLs. Never commit it.
- **The program writes `fvd.log` where it is started and `options.cfg` next to the executable.** Both are ignored; running `bin/win64/FVD.exe` in place still creates them, so run a copy.
- **A new compiler or glm can move results by the last bit.** After `pacman -Syu`, rebuild and rerun the baseline comparison.
- **`.fvd` files are raw binary written back to front** (`docs/codebase-map.md`, "Saved project file"). The format matched between the 2016 binary (Qt 5.6) and the new build (Qt 5.15) on the test project.
- **`ui/qcustomplot.*` is a vendored library,** half the lines in the repository. Never edit it. Its 37 warnings in a build are not ours to fix.
- **Keep GitHub Actions light.** No build workflow (`docs/maybe-later.md`); the two Claude workflows run only when asked.
