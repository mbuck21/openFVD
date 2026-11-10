# FVD++ (openFVD fork)

A force-vector-design tool for roller-coaster track, inspired by newton2: C++ with Qt 5 and OpenGL, exporting to NoLimits 1 and 2. This is mbuck21/openFVD, a fork of altlenny/openFVD (v0.79, last upstream commit 2023-02-06, GPL-3.0). The owner is a coaster enthusiast and mechanical engineer, not a professional developer: explain changes in plain words.

Read this file first in every session. Then `docs/README.md` for what to read for the task. Why the rules exist: `docs/decisions/`.

## Where the knowledge is

- **The map:** `docs/codebase-map.md` (layout, data model, file format, the flows and their entry functions, where each kind of change lands). Terms: `docs/glossary.md`.
- **Building:** `docs/building.md`. No toolchain is installed on the owner's PC yet; the choice is an open `owner` issue.
- **Status:** `STATUS.md`, rewritten in place, never appended. Deferred ideas: `docs/maybe-later.md`. How a session runs the work: `docs/development-workflow.md`.
- **Upstream:** altlenny/openFVD, its five issues and the `readme.txt` changelog. The official FVD++ documentation and the NoLimits Exchange forum it pointed to are gone.

## Rules that do not bend

1. **A C++ change is not done until it compiles and the program was run.** Until a toolchain exists on the owner's PC, a pull request that changes `.cpp`, `.h`, `.ui`, `.pro` or shader files says so at the top of its description, in plain words: "not compiled". Prefer documentation, build-system and GitHub work until then.
2. **The committed binaries stay out of pull requests** (`bin/win64/FVD.exe`, `bin/win64/glew32.dll`, `bin/osx64.zip`) unless the pull request rebuilt them, says from which commit and with which toolchain, and the owner asked for new binaries. The 2016 Windows binary is the baseline every rebuild is compared against.
3. **Never edit `ui/qcustomplot.cpp` or `ui/qcustomplot.h`.** They are the vendored QCustomPlot 1.2.1 library. Upgrading it is its own issue.
4. **The `.fvd` file format changes only with a version bump and a load path for old files** (`projectWidget::saveProject()` / `loadProject()`, `docs/codebase-map.md` "Saved project file"). Projects the owner saved must keep opening.
5. **Keep the GPL header** on every source file and the license on the repository. New files get the same header with the fork's author line added below the original.
6. **Nothing the program writes gets committed:** `fvd.log`, `options.cfg`, `*.bak`, build directories. `.gitignore` lists them; add to it rather than committing around it.
7. **No new dependency without a one-line justification** in the pull request. The three external libraries (GLEW, glm, lib3ds) and Qt 5 are already a burden; see `docs/building.md`.

## Conventions found in the code (follow the file you are in)

- Classes are lowerCamel (`track`, `secforced`, `trackWidget`, `glViewWidget`), except `MainWindow` and `TrackProperties`. One class per `.h`/`.cpp` pair, named after the class in lower case.
- Member prefixes: `f` float, `b` bool, `i` int, `v` glm vector, `l` list, `m` owned object or handler, `s` string (`fHeart`, `bSpeed`, `iTime`, `vPos`, `lSections`, `mMesh`, `sName`).
- Qt slots are auto-connected by name: `on_<objectName>_<signal>()` matches a widget in the `.ui` form. Renaming a widget in a form silently disconnects its slot.
- Maths is glm with `GLM_FORCE_RADIANS`; angles are degrees in the UI and the node fields (`fRoll`, `getPitch()`), converted with `TO_RAD`/`TO_DEG` from `lenassert.h`. Everything is `float`. Time steps are `1/F_HZ` seconds (`core/mnode.h`).
- Checks are `lenAssert(cond)` (an `assert` in debug, a `qCritical` in release). Logging is `qDebug`/`qWarning`, which land in `fvd.log` next to the working directory.
- Indentation is four spaces, with tabs in some files; keep each file's own style and do not reformat files you did not otherwise change.
- Every editable value goes through `undoAction` (`core/undoaction.h`): a new spin box means a new `eActionType`, an "old value" field on `undoHandler`, and a case in `undoAction::doUndo()`.
- Binary file I/O uses the helpers in `core/exportfuncs.cpp`, never raw `fwrite` of a struct in new code.

## Build and run

- There is no build on this PC yet. `docs/building.md` holds the toolchain options and will hold the exact commands once one is chosen.
- The committed `bin/win64/FVD.exe` runs on Windows 11 as-is. Run it from a copy outside the repository, or delete the `fvd.log` and `options.cfg` it writes.
- There is no test suite. Testing is by hand: open a project, change something, export, and compare with the 2016 binary. Write down what you did in the pull request's "Tests" part. `docs/testing.md` is planned once a build exists.

## Working with the owner

- Every change reaches `master` by pull request, opened as a **draft** from a branch named `claude/<slug>` (`claude/0001-workflow-and-map`, `claude/fix-export-roll`). Commit subjects in plain words. The **owner merges**; a session never merges its own pull request here.
- The pull request follows `.github/pull_request_template.md`: what it does, tests, and a "For the owner" part naming the files and what to look at. Say whether the code was compiled and run.
- Anything that needs the owner's hands (a choice, an install, a check in NoLimits) is an issue labelled `owner`, with exact steps. A follow-up for Claude is an issue labelled `claude` (the "Task for Claude" template: What, Where, Why, Done when). Each gets one area label: `core`, `renderer`, `ui`, `build`, `export` or `docs`.
- `Claude Code Review` runs only when the owner adds the `review` label to a pull request. Ask for it in the description, with the reason, when the change touches the physics, the exporters or the file format. Never add the label yourself.
- `@claude` in an issue or pull request comment starts a separate GitHub run (`.github/workflows/claude.yml`). Sessions on the PC do not use it.
- Both Claude workflows pay with Console API credits first (secret `ANTHROPIC_API_KEY`) and fall back to the owner's subscription (secret `CLAUDE_CODE_OAUTH_TOKEN`) when that step fails (decision record 0002). They also need the Claude GitHub App installed on the repository.
- Keep GitHub Actions use light: there is no build or test workflow, and the two Claude workflows run only when asked.
- Small pull requests, one task each. Open the draft after the first push so another session can pick the work up.
