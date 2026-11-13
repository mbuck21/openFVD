# FVD++ (openFVD fork)

A force-vector-design tool for roller-coaster track, inspired by newton2: C++ with Qt 5 and OpenGL, exporting to NoLimits 1 and 2. This is mbuck21/openFVD, a fork of altlenny/openFVD (v0.79, last upstream commit 2023-02-06, GPL-3.0). The owner is a coaster enthusiast and mechanical engineer, not a professional developer: explain changes in plain words.

Read this file first in every session. Then `docs/README.md` for what to read for the task. Why the rules exist: `docs/decisions/`.

## Where the knowledge is

- **The map:** `docs/codebase-map.md` (layout, data model, file format, the flows and their entry functions, where each kind of change lands). Terms: `docs/glossary.md`.
- **One guide per folder:** `core/CLAUDE.md` (the model), `ui/CLAUDE.md` (the windows), `renderer/CLAUDE.md` (the 3D view): what the folder may depend on, its main classes, where a change lands, its traps, how to check a change. Read the one for the folder you are about to change.
- **Building:** `docs/building.md` (MSYS2 UCRT64; one command, `tools\build-windows.cmd`). **Testing:** `docs/testing.md`, against the 2016 baseline in `samples/baseline/`.
- **Research behind the plan:** `docs/research/` (R1 heartlines and trains, R2 minimum radii, R3 ride-force limits, R4 NoLimits 2 blocks, R5 the force-vector-design workflow study, R7 a train against the point, R8 friction, R12 how NoLimits 2 simulates a train, R13 second sources for the force limits; `docs/README.md` lists them all). Read the one for your feature before designing it.
- **The plan:** `docs/roadmap.md` (stages, what is serial, what runs together) and `docs/feature-requests.md` (the owner's requests, `FR-1` to `FR-19`).
- **Status:** `STATUS.md`, rewritten in place, never appended. Deferred ideas: `docs/maybe-later.md`. How a session runs the work: `docs/development-workflow.md`.
- **Upstream:** altlenny/openFVD, its five issues and the `readme.txt` changelog. The official FVD++ documentation and the NoLimits Exchange forum it pointed to are gone.

## Rules that do not bend

1. **A C++ change is not done until it compiles, the program was run, and the baseline comparison in `docs/testing.md` gives the expected result.** A pull request that changes `.cpp`, `.h`, `.ui`, `.pro` or shader files says at the top what was built and run. One written where no build was possible (a GitHub `@claude` run has no toolchain) says "not compiled" and stays a draft until someone builds it.
2. **The committed binaries stay out of pull requests** (`bin/win64/FVD.exe`, `bin/win64/glew32.dll`, `bin/osx64.zip`) unless the pull request rebuilt them, says from which commit and with which toolchain, and the owner asked for new binaries. The 2016 Windows binary is the baseline every rebuild is compared against.
3. **Never edit `ui/qcustomplot.cpp` or `ui/qcustomplot.h`.** They are the vendored QCustomPlot 1.2.1 library. Upgrading it is its own issue.
4. **The `.fvd` file format changes in one of two ways only, and projects the owner saved must keep opening** (decision record 0014, points 14 and 15; the owner agreed on 2026-11-13). A new saved value that does not change the computed track goes in the appendix after the end marker `EOP` (roadmap piece F4), which has its own version number; the main version stays as it is, so the project still opens in the 2016 program. A change to anything before `EOP`, and the first saved value that changes the computed track, needs a bump of the main version and a load path for old files (`writeProject()` / `readProject()` in `core/projectfile.cpp`, `track::saveTrack()` / `loadTrack()`, `docs/codebase-map.md` "Saved project file"). Until F4 is built and a project with an appendix has been seen to open in the 2016 binary, nothing is added to the file.
5. **Keep the GPL header** on every source file and the license on the repository. New source files and scripts (`tools/`) get the same header with the fork's author line added below the original. Documentation, fixtures and configuration files carry no header.
6. **Nothing the program writes gets committed:** `fvd.log`, `options.cfg`, `*.bak`, build directories. `.gitignore` lists them; add to it rather than committing around it.
7. **No new dependency without a one-line justification** in the pull request. The three external libraries (GLEW, glm, lib3ds) and Qt 5 are already a burden; see `docs/building.md`.

## Conventions found in the code (follow the file you are in)

- Classes are lowerCamel (`track`, `secforced`, `trackWidget`, `glViewWidget`), except `MainWindow` and `TrackProperties`. One class per `.h`/`.cpp` pair, named after the class in lower case.
- Member prefixes: `f` float, `b` bool, `i` int, `v` glm vector, `l` list, `m` owned object or handler, `s` string (`fHeart`, `bSpeed`, `iTime`, `vPos`, `lSections`, `mMesh`, `sName`).
- Qt slots are auto-connected by name: `on_<objectName>_<signal>()` matches a widget in the `.ui` form. Renaming a widget in a form silently disconnects its slot.
- Maths is glm with `GLM_FORCE_RADIANS`; angles are degrees in the UI and the node fields (`fRoll`, `getPitch()`), converted with `TO_RAD`/`TO_DEG` from `lenassert.h`. Everything is `float`. Time steps are `1/F_HZ` seconds (`core/mnode.h`).
- Checks are `lenAssert(cond)` (an `assert` in debug, a `qCritical` in release). Logging is `qDebug`/`qWarning`, which land in `fvd.log` next to the working directory.
- Indentation is four spaces, with tabs in some files; keep each file's own style and do not reformat files you did not otherwise change.
- Every edit of a track is an undo step: the slot runs a named command (`commandRegistry::run()`, `core/command.h`), which changes the model and records its own step. Undo and redo put back a snapshot of the whole track (`core/trackstate.h`), so a new model value must be saved, kept in `trackState`, or derived. The Track Properties and smoothing dialogs run their commands inside one step that OK commits and Cancel rolls back. Only a change that is deliberately not a step calls `trackHandler::takeOutsideChange()` (`ui/trackhandler.h`): the track list's show tick, and switching on `bAutoSmooth` when a track's smoothing dialog is first made. In the windows, values typed into one box merge into one step (a second's pause, leaving the box or another selection ends the run); batch mode and `fvdcore` keep one step per edit.
- Binary file I/O uses the helpers in `core/exportfuncs.cpp`, never raw `fwrite` of a struct in new code.

## Build and run

- Build: `tools\build-windows.cmd` from a normal terminal (or `tools/build-windows.sh` in the MSYS2 UCRT64 shell). It leaves `build/release/FVD.exe` and a copy that runs on its own in `build/dist/`. Details, versions and the lib3ds switch: `docs/building.md`.
- Run: `build\dist\FVD.exe`. It writes `fvd.log` where it is started and `options.cfg` beside itself; both are ignored by git.
- Test: `py -3 tools/run-baseline.py` exports every project in `samples/baseline/` through the program's batch mode (no window) and compares with the 2016 binary's files; it must end `PASS`. `tools\run-tests.cmd` builds and runs the small automated tests under `tests/`. A change in `core/` also runs `py -3 tools/check-core.py` (`core/CLAUDE.md`). `docs/testing.md` has the details and the by-hand procedure. Write the last lines of both in the pull request's "Tests" part.
- The committed `bin/win64/FVD.exe` is the 2016 release and the reference. Run it from a copy outside the repository.

## Working with the owner

- **Who decides what** (decision record 0006). The owner is the customer: he says what the program should do for him, sets the priorities, tries the results, and decides anything that changes how his coasters come out, his saved files, his money or his PC. The lead session is the architect and runs the team of sub-agents: it decides how things are built (structure, technology, order of work, tooling, what to refactor), makes those calls without asking, and records the significant ones as decision records. It goes to the owner only for product choices and priorities, and then with a recommendation. `docs/roadmap.md` is the plan; `docs/feature-requests.md` is what the owner asked for.
- **Nothing reaches `master` except through a pull request the owner merges**, or one the owner tells a session to merge. Branches are named `claude/<slug>`; commit subjects are in plain words.
- **Work with more than one piece runs on an integration branch** (decision record 0004). The lead session creates `claude/<topic>` from `master` and opens a draft pull request from it into `master` whose description lists every piece of the work and its state. Each piece is a branch of its own, opened as a pull request **into the integration branch**. The lead merges a piece once it is built, checked and reviewed. When every piece is in, the lead runs the baseline comparison on the integration branch, rewrites `STATUS.md`, invokes a final review, and tells the owner, **listing anything the owner must do before merging**.
- **A single small task** (one fix, a doc correction) may go as one draft pull request straight into `master`.
- **Reviews are done by `Claude Code Review` on GitHub, not by reviewer sub-agents.** It runs only when the `review` label is added to a pull request, never by itself on a commit. The lead adds the label to each piece that changes code and to the integration pull request at the end; taking the label off and adding it again reviews a later push. Documentation-only pieces need none. Every review comment is answered or fixed before the merge. When the GitHub review's run is queued or slow, the owner has allowed a reviewer sub-agent in the lead's session to stand in for it, given the diff and not the lead's conclusions; the pull request then says that this was done.
- **Sub-agents and side sessions run on Opus, Sonnet or Haiku, the cheapest that does the piece well:** Haiku for mechanical edits from an exact brief, Sonnet by default, Opus where a mistake would be silent and costly (the physics, the exporters, the file format). `docs/development-workflow.md` has the details.
- The pull request follows `.github/pull_request_template.md`: what it does, tests, and a "For the owner" part naming the files and what to look at. Say whether the code was compiled and run.
- Anything that needs the owner's hands (a choice, an install, a check in NoLimits) is an issue labelled `owner`, with exact steps. A follow-up for Claude is an issue labelled `claude` (the "Task for Claude" template: What, Where, Why, Done when). Each gets one area label: `core`, `renderer`, `ui`, `build`, `export` or `docs`.
- `@claude` in an issue or pull request comment starts a separate GitHub run (`.github/workflows/claude.yml`). Sessions on the PC do not use it.
- Both Claude workflows run on the owner's Claude subscription (secret `CLAUDE_CODE_OAUTH_TOKEN`; decision record 0005) and need the Claude GitHub App installed on the repository. The review runs on Opus with a prompt written for this codebase, kept in `.github/workflows/claude-code-review.yml`; `@claude` mentions use the action's default model.
- Keep GitHub Actions use light: there is no build or test workflow, and the two Claude workflows run only when asked.
