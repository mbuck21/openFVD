# How a Claude session runs the work

Guidance, not rules. The rules that do not bend are in `CLAUDE.md`. Within them, the session leading a task uses its judgment.

## 1. Taking a task

The owner gives the goal: an issue, a fix, a feature. The session that takes it:

- reads `CLAUDE.md`, `STATUS.md` and the part of `docs/codebase-map.md` that covers the change; the map says which files each kind of change touches;
- decides whether to plan first (a few lines in the thread for a fix, a short plan in the issue for a feature);
- branches from `master` as `claude/<slug>` and opens a draft pull request after the first push, so another session can pick the work up;
- rewrites `STATUS.md` when the state of the project changes, not for every commit.

One task per pull request. A finding that does not belong to the task becomes an issue (`claude` or `owner`, with an area label) instead of widening the pull request.

## 2. Where there is no build

The owner's PC builds the program (`tools\build-windows.cmd`, `docs/building.md`). A GitHub `@claude` run and a cloud session do not: their machines have no Qt toolchain. A C++ change written there starts its pull request with "not compiled" and stays a draft until a session on the PC has built and run it. Reading the code carefully is no substitute: it is C++03 with raw pointers and a 25,000-line vendored plot library.

## 3. Building and checking a change

- Build before and after the change. Run the baseline comparison in `docs/testing.md` after it, and paste the last lines of `tools/compare_exports.py` in the "Tests" part of the pull request. Check the result in NoLimits if the change touches geometry or export.
- Driving the program's window from a session needs screen access, which the owner approves per session. Ask for the FVD++ window only, work on a copy of the program outside the repository, and close it when done.
- A change to the physics (`core/sec*.cpp`, `core/mnode.cpp`, `core/subfunction.cpp`) or the exporters (`core/track.cpp` export functions, `core/exportfuncs.cpp`) deserves a before/after export of the same project and a diff of the files. Say so and ask for the `review` label.
- A change to the `.fvd` format (`track::saveTrack()`, any `saveSection()`) needs a version bump and a load test of an old file.

## 4. Sub-agents

Worth it for reading widely (the map, a search across `ui/` for every use of a field) and for independent review of a physics change. The lead keeps the judgment: what to change, the exact wording in the pull request, everything on GitHub. Sub-agents do not commit, push or post on GitHub.

## 5. The pull request

`.github/pull_request_template.md` holds the layout: what this does, tests, for the owner. Plain words: the owner is a coaster enthusiast, not a developer. Say whether the code was compiled and run. Name the files to look at. Say that the owner merges.

## 6. Practical notes

- `bin/win64/FVD.exe` writes `fvd.log` and `options.cfg` where it runs. Both are ignored by git; delete them anyway if you ran it in the repository.
- `git status` must show no `bin/` change unless the task was to rebuild the binaries.
- Issues and pull requests are sent to Anthropic when a Claude workflow runs on them; there is nothing personal in this repository, but keep it that way (no local paths, no account details).
- The GitHub tools take a label name as given; the area labels are `core`, `renderer`, `ui`, `build`, `export`, `docs`.
- Upstream (altlenny/openFVD) is unmaintained; its issues 2, 5 and 6 (macOS build, glm maths errors, macOS use) are the only outside signal about building this code today.
