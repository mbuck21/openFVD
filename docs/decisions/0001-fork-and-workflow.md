# 0001: Fork altlenny/openFVD and run it with the HSA_Shoebox workflow

- Status: accepted
- Date: 2026-11-10
- Needs owner approval: yes (given by merging the pull request that carries this record)

## Context

- FVD++ (openFVD) is the force-vector-design tool the owner wants to extend. Upstream altlenny/openFVD is unmaintained: last commit 2023-02-06 (v0.79), five open issues, no response since. Its dependencies are from 2014 to 2016 (Qt 5.2 to 5.4, glm 0.9.5, GLEW 1.12, lib3ds 2008, QCustomPlot 1.2.1) and current versions of at least glm no longer compile it (upstream issue 5).
- The owner's PC has no C++ toolchain, Qt or CMake. The committed 2016 Windows binary runs.
- The owner already runs one repository with Claude sessions (HSA_Shoebox) and wants the same shape here: a root `CLAUDE.md` read first, `STATUS.md` rewritten in place, a `docs/` reading order, decision records, issue and pull request templates, the `@claude` workflow and a label-triggered review, branches `claude/<slug>`, small draft pull requests merged by the owner.

## Decision

- Work happens on the fork mbuck21/openFVD, default branch `master` (kept from upstream so upstream commits can still be pulled if any appear). Upstream is not contacted for now; the GPL-3.0 licence and headers stay.
- The workflow is copied from HSA_Shoebox in shape, not content: `CLAUDE.md`, `STATUS.md`, `docs/README.md`, `docs/development-workflow.md`, `docs/maybe-later.md`, `docs/decisions/`, the pull request template, the two issue forms, `dependabot.yml` for GitHub Actions only, `workflows/claude.yml` on `@claude`, `workflows/claude-code-review.yml` on the `review` label only. Labels: `claude`, `owner`, `review`, and the areas `core`, `renderer`, `ui`, `build`, `export`, `docs`.
- No build or test workflow on GitHub yet (`docs/maybe-later.md`): the toolchain is undecided and Actions minutes are scarce.
- Three rules specific to this code (in `CLAUDE.md`): a C++ change is not done until compiled and run, and says "not compiled" until then; the committed binaries stay out of pull requests unless rebuilt on purpose; `ui/qcustomplot.*` is never edited.
- The first milestone is a reproducible build on the owner's PC. The toolchain choice is the owner's (an `owner` issue with the options from `docs/building.md`).
- Issues were enabled on the fork (GitHub disables them on forks by default); the workflow depends on them.

## Where enforced

- `CLAUDE.md` ("Rules that do not bend", "Working with the owner").
- `.github/` (templates and workflows), the repository labels.
- `docs/building.md` (the toolchain options), `STATUS.md` (the milestone).
