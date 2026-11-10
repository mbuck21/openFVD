# Documents index

One row per document. Keep this table current when a document is added, frozen or retired.

## Start here

Pick the kind of task and read only what is listed. Root `CLAUDE.md` comes first in every session.

- **Any change to the code:** `docs/codebase-map.md` (layout, data model, flows, where each kind of change lands), then the files it names for your change. `docs/glossary.md` when a term is unclear.
- **Building or the toolchain:** `docs/building.md`.
- **GitHub process (pull requests, issues, labels, the Claude workflows):** "Working with the owner" in `CLAUDE.md`, then `docs/development-workflow.md`.
- **What is going on right now:** `STATUS.md`.
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
| `docs/codebase-map.md` | Build system, layout, data model, `.fvd` format, flows, where changes land. | current (written 2026-11-10, unverified by a build) |
| `docs/glossary.md` | FVD terms as the code uses them. | current |
| `docs/building.md` | Toolchain expectations, what is installed, the options, whether the binaries run. | current; the "exact commands" section waits for the toolchain choice |
| `docs/development-workflow.md` | How a session runs a task here. | current |
| `docs/maybe-later.md` | Ideas deliberately left out, each with its trigger. | current |
| `docs/decisions/0001-fork-and-workflow.md` | The fork, the workflow and the first rules. | accepted |
| `docs/decisions/0002-api-credits-first-subscription-fallback.md` | The GitHub workflows spend API credits first, the subscription second. | accepted |
| `docs/testing.md` | How to test a change by hand and, later, automatically. | planned, after the first build |
| `.github/pull_request_template.md` | The pull request layout. | current |
| `.github/ISSUE_TEMPLATE/claude.yml`, `owner.yml` | The two issue forms. | current |
| `.github/workflows/claude.yml` | `@claude` mentions. Secrets `ANTHROPIC_API_KEY` (first) and `CLAUDE_CODE_OAUTH_TOKEN` (fallback), plus the Claude GitHub App. | current |
| `.github/workflows/claude-code-review.yml` | The review, on the `review` label only. | current |
| `.github/dependabot.yml` | Monthly updates of the GitHub Actions used by the workflows. | current |
