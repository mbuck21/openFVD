# Status

Rewrite this file in place each time. Do not append to it. The long story is in `docs/decisions/` and in the Issues tab.

## As of

2026-11-10, written by the session that set the repository up, on the branch `claude/0001-workflow-and-map` (its pull request is the first one).

## Focus

- **Getting the fork workable.** The fork was made on 2026-11-10 from altlenny/openFVD at its last commit (2023-02-06, v0.79). Nothing in the C++ has been changed. The first pull request adds the map, the build notes, the rules and the GitHub workflow (decision record 0001).
- **No build yet.** The PC has no C++ toolchain, Qt, qmake or CMake. The options with sizes are in `docs/building.md`; the choice is the owner's.
- **The 2016 Windows binary runs** on this PC (OpenGL 4.6 context, GLEW initialised). It is the baseline for any rebuild.

## Milestone now

**Milestone 1: a reproducible build on the owner's PC.** Done when a session can run `qmake` and `make` here, the result opens a project saved by the 2016 binary and exports the same element, and `docs/building.md` holds the exact commands.

Steps, as issues:
- #1: choose the toolchain (`owner`, `build`).
- #2: make `fvd.pro` build with a 2026 Qt 5.15 and glm: fix the hard-coded library paths and whatever the compiler and upstream issue altlenny/openFVD#5 report (`claude`, `build`).
- #3: assess the Qt path: stay on Qt 5.15 (the last Qt with `QGLWidget`) or port the renderer to `QOpenGLWidget` for Qt 6 (`claude`, `renderer`).
- #4: add a sample project and export fixtures so a rebuild can be checked against the 2016 binary (`claude`, `export`).

## Waiting on the owner

- The merge of the first pull request.
- #1: the toolchain choice (`docs/building.md` lists the three options).
- #5: the `CLAUDE_CODE_OAUTH_TOKEN` secret in the fork's settings, before `@claude` or the `review` label can do anything.
- What to change in FVD++ first, once it builds.

## Verified

- 2026-11-10: `bin/win64/FVD.exe` starts on the owner's PC and logs an OpenGL 4.6 context (`docs/building.md`).
- 2026-11-10: the map was written from reading every header in `core/`, `renderer/` and `ui/`, `main.cpp`, `fvd.pro` and the save, load, update and export functions. Nothing was compiled.

## Watch-outs

- **Issues were disabled on the fork** (GitHub's default for forks) and were turned on by the first session so the workflow can use them.
- **The program writes `fvd.log` and `options.cfg` next to itself.** Both are in `.gitignore`; running `bin/win64/FVD.exe` in place creates them.
- **`.fvd` files are raw binary** tied to the compiler's struct layout. A rebuilt binary must be checked against files saved by the old one before anyone trusts it.
- **The GitHub Actions minutes on this account were exhausted until 2026-11-01.** They should be back, but keep the workflows light: no build CI yet (`docs/maybe-later.md`).
- **`ui/qcustomplot.*` is a vendored library,** half the lines in the repository. Never edit it; grep it only when a plot question needs it.
