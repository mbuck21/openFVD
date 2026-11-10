# How a Claude session runs the work

Guidance, not rules. The rules that do not bend are in `CLAUDE.md`. Within them, the session leading a task uses its judgment.

## 1. Taking a task

The owner gives the goal: an issue, a fix, a feature, or a list of them. The session that takes it (the lead):

- reads `CLAUDE.md`, `STATUS.md` and the part of `docs/codebase-map.md` that covers the change; the map says which files each kind of change touches;
- decides whether to plan first (a few lines in the thread for a fix, a short plan in the issue for a feature);
- for a single small task, branches from `master` as `claude/<slug>` and opens a draft pull request into `master`;
- for anything with more than one piece, sets up an integration branch (section 4);
- rewrites `STATUS.md` when the state of the project changes, not for every commit.

One task per pull request. A finding that does not belong to the task becomes an issue (`claude` or `owner`, with an area label) instead of widening the pull request.

## 2. Where there is no build

The owner's PC builds the program (`tools\build-windows.cmd`, `docs/building.md`). A GitHub `@claude` run and a cloud session do not: their machines have no Qt toolchain. A C++ change written there starts its pull request with "not compiled" and stays a draft until a session on the PC has built and run it. Reading the code carefully is no substitute: it is C++03 with raw pointers and a 25,000-line vendored plot library.

## 3. Building and checking a change

- Build before and after the change. Run the baseline comparison in `docs/testing.md` after it, and paste the last lines of `tools/compare_exports.py` in the "Tests" part of the pull request. Check the result in NoLimits if the change touches geometry or export.
- Driving the program's window from a session needs screen access, which the owner approves per session. Ask for the FVD++ window only, work on a copy of the program outside the repository, and close it when done.
- A change to the physics (`core/sec*.cpp`, `core/mnode.cpp`, `core/subfunction.cpp`) or the exporters (`core/track.cpp` export functions, `core/exportfuncs.cpp`) deserves a before/after export of the same project and a diff of the files. Say so and ask for the `review` label.
- A change to the `.fvd` format (`track::saveTrack()`, any `saveSection()`) needs a version bump and a load test of an old file.

## 4. An integration branch and a team

The owner's instruction of 2026-11-10 (decision record 0004).

**The integration branch.**

1. The lead creates `claude/<topic>` from `master`, pushes it, and opens a **draft pull request into `master`**. Its description is the plan: one checklist line per piece of work, with the piece's pull request number once it has one. Keep it current; it is where the owner looks.
2. Each piece gets its own branch from the integration branch, `claude/<slug>`, and its own pull request **into the integration branch** (`gh pr create --base claude/<topic>`). Git cannot hold a branch named `claude/<topic>/<piece>` beside `claude/<topic>`, so pieces are named side by side.
3. A piece is ready when it builds, the baseline comparison in `docs/testing.md` gives the expected result, and its review is answered. The lead then merges it into the integration branch with a merge commit.
4. When every piece is in: build the integration branch, run the baseline comparison once more (or show that the code is file for file the commit that was compared), rewrite `STATUS.md` as a commit on the integration branch, fill in the "Tests" and "For the owner" parts of the integration pull request, add the `review` label for a final review, answer or fix its comments, and mark the pull request ready.
5. Tell the owner it is ready and **list what the owner must do before merging** (a check in NoLimits, a decision, a secret), or say there is nothing. The owner merges into `master`, or tells the session to.

**Reviews.** `Claude Code Review` on GitHub does them; do not spend a sub-agent on reviewing. It runs when the `review` label is added and at no other time:

```
gh pr edit <number> --add-label review
```

The review's instructions are the prompt in `.github/workflows/claude-code-review.yml`, written for this codebase: it reads `CLAUDE.md` and the map first, reads every changed file, posts findings as inline comments and one summary that says which files it read in full. It runs on Opus and reviews a draft like any other pull request. If adding the label starts no run within a minute (seen once, on a pull request whose merge check was still pending), take the label off and add it again. To review a later push, do the same.

**The team.** Sub-agents and side sessions run on Opus, Sonnet or Haiku. Pick the cheapest that does the piece well:

- **Haiku** for mechanical work from an exact brief: apply this list of edits, rename this field everywhere, run these commands and report what each printed.
- **Sonnet** by default: building a piece from a clear description, writing a doc, searching `ui/` for every use of a field.
- **Opus** where a mistake would be silent and costly: the physics (`core/sec*.cpp`, `core/mnode.cpp`, `core/subfunction.cpp`), the exporters, the `.fvd` format, or a piece Sonnet has missed twice.

Split a piece until a cheaper model can do it. Run independent pieces in parallel, each in its own git worktree so no two share files or a build folder. The lead keeps what needs judgment across the work: what each piece is and its exact wording, checking a finding before anyone fixes it, merging, the baseline comparison, and everything on GitHub (sub-agents do not push or post).

## 5. The pull request

`.github/pull_request_template.md` holds the layout: what this does, tests, for the owner. Plain words: the owner is a coaster enthusiast, not a developer. Say whether the code was compiled and run. Name the files to look at. Say who merges: the lead for a piece going into an integration branch, the owner for anything going into `master`.

## 6. Practical notes

- `bin/win64/FVD.exe` writes `fvd.log` and `options.cfg` where it runs. Both are ignored by git; delete them anyway if you ran it in the repository.
- `git status` must show no `bin/` change unless the task was to rebuild the binaries.
- **A pull request that changes `.github/workflows/claude-code-review.yml` may not be reviewable by it:** the action checks that the workflow matches the copy on `master`. Change the review prompt in a pull request of its own and try it on the next one.
- **Do not write a literal at-sign followed by `claude` in an issue or pull request you open** unless you mean to start the GitHub run; the workflow triggers on that text anywhere in the body.
- **Deleting a branch closes every open pull request that uses it as its base,** and a closed pull request cannot be retargeted. Before merging a piece that another piece is stacked on, point the dependent pull request at the integration branch (`gh pr edit <number> --base claude/<topic>`).
- Issues and pull requests are sent to Anthropic when a Claude workflow runs on them; there is nothing personal in this repository, but keep it that way (no local paths, no account details).
- The GitHub tools take a label name as given; the area labels are `core`, `renderer`, `ui`, `build`, `export`, `docs`.
- Upstream (altlenny/openFVD) is unmaintained; its issues 2, 5 and 6 (macOS build, glm maths errors, macOS use) are the only outside signal about building this code today.
