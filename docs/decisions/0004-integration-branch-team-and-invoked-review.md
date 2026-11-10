# 0004: An integration branch, a team on Opus, Sonnet and Haiku, and a review the session invokes

- Status: accepted; amends 0001 (who adds the `review` label, who merges what)
- Date: 2026-11-10
- Needs owner approval: no (the owner's instruction of 2026-11-10, written down here)

## Context

- Record 0001 had every pull request go straight into `master`, merged by the owner, with the GitHub review run only when the owner added the `review` label. The first day's work produced stacked pull requests the owner had to merge in the right order.
- On 2026-11-10 the owner asked for one branch per body of work that lays out everything being done, with the pieces merged into it by the sessions, the GitHub review used for reviewing instead of reviewer sub-agents, and a single hand-over at the end: the owner merges the branch or tells the session to, after being told what, if anything, they must do first.
- The owner also asked that sub-agents and branch work default to Opus, Sonnet and Haiku as needed.

## Decision

- **Integration branch.** Work with more than one piece runs on `claude/<topic>`, created from `master`, with a draft pull request into `master` whose description lists every piece and its state. Each piece is its own branch and its own pull request into the integration branch. The lead session merges a piece when it builds, passes the baseline comparison and has its review answered.
- **Hand-over.** When every piece is in, the lead builds the integration branch, runs the baseline comparison, rewrites `STATUS.md`, invokes a final review, answers it, marks the pull request ready and tells the owner, listing the owner's actions before merging. The owner merges into `master`, or tells the session to. A session never merges into `master` on its own decision.
- **Single small tasks** may still go as one draft pull request into `master`.
- **Reviews.** `Claude Code Review` on GitHub does the reviewing; sessions do not use reviewer sub-agents for it. It stays invoked, never automatic: it runs only when the `review` label is added. The lead session now adds the label itself, to each piece that changes code and to the integration pull request at the end. Every comment is answered or fixed before the merge.
- **Models.** Sub-agents and side sessions run on Opus, Sonnet or Haiku, the cheapest that does the piece well: Haiku for mechanical work from an exact brief, Sonnet by default, Opus where a mistake would be silent and costly.
- Unchanged from 0001 and 0002: branch names, plain-word commits and pull requests, the issue templates and labels, how the workflows are paid for, and that the committed binaries and the vendored plot library are left alone.

## Where enforced

- `CLAUDE.md`, "Working with the owner".
- `docs/development-workflow.md`, sections 1 and 4.
- `.github/pull_request_template.md`.
- `.github/workflows/claude-code-review.yml` (the label trigger; its opening comment still names the owner as the one who adds the label, see `docs/maybe-later.md`).
