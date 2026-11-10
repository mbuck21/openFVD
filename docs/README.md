# Documents index

One row per document. Keep this table current when a document is added, frozen or retired.

## Start here

Pick the kind of task and read only what is listed. Root `CLAUDE.md` comes first in every session.

- **Any change to the code:** `docs/codebase-map.md` (layout, data model, flows, where each kind of change lands), then the files it names for your change. `docs/glossary.md` when a term is unclear.
- **Building or the toolchain:** `docs/building.md`. **Checking a change:** `docs/testing.md`.
- **GitHub process (pull requests, issues, labels, the Claude workflows):** "Working with the owner" in `CLAUDE.md`, then `docs/development-workflow.md`.
- **What is going on right now:** `STATUS.md`. **What comes next and in what order:** `docs/roadmap.md`. **What the owner asked for:** `docs/feature-requests.md`.
- **Why things are the way they are:** `docs/decisions/`.
- **Skip, or grep only:** `ui/qcustomplot.cpp` and `.h` (a vendored library), `readme.txt` (upstream changelog; the same text is `README.md` and `bin/win64/readme.txt`).

## Documents

| File | What it is | Status |
|---|---|---|
| `CLAUDE.md` | The rules, the conventions, how to work with the owner. | current |
| `STATUS.md` | Where the project stands; rewritten in place. | current |
| `README.md` | Upstream's readme: intro, changelog, licence. Left as upstream wrote it. | frozen (upstream) |
| `readme.txt` | The same text as `README.md`, shipped beside the binaries. | frozen (upstream) |
| `docs/README.md` | This index. | current |
| `docs/codebase-map.md` | Build system, layout, data model, `.fvd` format, flows, where changes land. | current |
| `docs/glossary.md` | FVD terms as the code uses them. | current |
| `docs/building.md` | How to build on Windows with MSYS2, the versions, the lib3ds switch, what was verified. | current |
| `docs/testing.md` | How to compare a build with the 2016 baseline; what the comparison does not cover. | current |
| `samples/baseline/` | A four-section project and its three exports from the 2016 binary; its `README.md` says how they were made. | frozen (the reference) |
| `tools/` | `build-windows.cmd` and `.sh` (the build), `compare_exports.py` (the comparison). | current |
| `docs/development-workflow.md` | How a session runs a task here. | current |
| `docs/feature-requests.md` | The owner's feature list (`FR-1` to `FR-9`) with the architect's notes on what each needs. | current |
| `docs/roadmap.md` | The stages, the dependencies, what is serial and what runs together. | current |
| `docs/maybe-later.md` | Ideas deliberately left out, each with its trigger. | current |
| `docs/decisions/0001-fork-and-workflow.md` | The fork, the workflow and the first rules. | accepted |
| `docs/decisions/0002-api-credits-first-subscription-fallback.md` | The GitHub workflows spend API credits first, the subscription second. | superseded by 0005 |
| `docs/decisions/0003-msys2-toolchain-and-optional-lib3ds.md` | MSYS2 UCRT64 is the Windows build; lib3ds is optional there; the 2016 binary is the reference. | accepted |
| `docs/decisions/0004-integration-branch-team-and-invoked-review.md` | Multi-piece work runs on an integration branch; sub-agents use Opus, Sonnet or Haiku; sessions invoke the GitHub review by label. | accepted |
| `docs/decisions/0005-workflows-on-the-subscription-and-a-review-prompt.md` | The GitHub workflows run on the subscription only; the review gets its own prompt and runs on Opus. | accepted |
| `docs/decisions/0006-architect-and-customer.md` | The lead session is the architect and decides how things are built; the owner is the customer and decides what and in what order. | accepted |
| `docs/decisions/0007-stage-1-batch-mode-first-and-the-nine-seams.md` | Stage 1 starts with a no-click batch mode; the core is separated from the windows through nine seams; train data is JSON. | accepted |
| `.github/pull_request_template.md` | The pull request layout. | current |
| `.github/ISSUE_TEMPLATE/claude.yml`, `owner.yml` | The two issue forms. | current |
| `.github/workflows/claude.yml` | `@claude` mentions. Secret `CLAUDE_CODE_OAUTH_TOKEN`, plus the Claude GitHub App. | current |
| `.github/workflows/claude-code-review.yml` | The review, on the `review` label only, on Opus, with the review prompt for this codebase. | current |
| `.github/dependabot.yml` | Monthly updates of the GitHub Actions used by the workflows. | current |
