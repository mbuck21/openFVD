# How a Claude session runs the work

Guidance, not rules. The rules that do not bend are in `CLAUDE.md`. Within them, the session leading a task uses its judgment.

## 1. Taking a task

The owner gives the goal: an issue, a fix, a feature. The session that takes it:

- reads `CLAUDE.md`, `STATUS.md` and the part of `docs/codebase-map.md` that covers the change; the map says which files each kind of change touches;
- decides whether to plan first (a few lines in the thread for a fix, a short plan in the issue for a feature);
- branches from `master` as `claude/<slug>` and opens a draft pull request after the first push, so another session can pick the work up;
- rewrites `STATUS.md` when the state of the project changes, not for every commit.

One task per pull request. A finding that does not belong to the task becomes an issue (`claude` or `owner`, with an area label) instead of widening the pull request.

## 2. Until there is a build

No toolchain is installed on the owner's PC (`docs/building.md`). Until the owner chooses one:

- Documentation, `fvd.pro`, `.gitignore` and `.github/` changes are the normal work.
- A C++ change can be written, but its pull request starts with "not compiled" and stays a draft until someone has built and run it. Reading the code carefully is no substitute: the code is C++03 with raw pointers and a 25,000-line vendored plot library, and a 2026 compiler will have opinions.
- Nothing is verified against the program except by running `bin/win64/FVD.exe`, the 2016 binary.

## 3. Once there is a build

- Build before and after the change. Open a project saved by the 2016 binary, change something, export, and check the result in NoLimits if the change touches geometry or export. Write what you did in the "Tests" part of the pull request.
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
