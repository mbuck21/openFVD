# 0002: The GitHub workflows pay with API credits first, the subscription second

- Status: superseded by 0005 (2026-11-10): the promotional credits do not cover Claude Code
- Date: 2026-11-10
- Needs owner approval: yes (the owner's request of 2026-11-10; given by merging the pull request that carries this record)

## Context

- Both Claude workflows (`@claude` mentions and the label-triggered review) were set up to run on the owner's Claude subscription through the `CLAUDE_CODE_OAUTH_TOKEN` secret (record 0001). Every run draws on the same allowance the owner's own Claude Code sessions use.
- The owner also has Claude Console API credits that renew monthly and would otherwise go unused, and asked that the workflows spend those first and return to the subscription when they run out.
- The first run on the fork failed because the Claude GitHub App was not installed on the repository. The app is needed whichever credential pays.

## Decision

- Each workflow runs the Claude Code action in two steps. The first uses the `ANTHROPIC_API_KEY` secret (Console API credits) and is allowed to fail. The second runs only when the first failed, and uses `CLAUDE_CODE_OAUTH_TOKEN` (the subscription).
- The fallback fires on any failure of the first step, not only on exhausted credits: a missing or revoked key, an API outage or a run that broke halfway all fall back too. A request that fails halfway and is then repeated on the subscription can post twice. That is accepted for a hobby repository; the Actions log shows which step did the work.
- Auto-reload stays off in the Console, so the credits can actually run out instead of charging a card.
- The Claude GitHub App must be installed on the repository (https://github.com/apps/claude). Neither secret works without it.
- Unchanged from 0001: what triggers each workflow, and that sessions never add the `review` label themselves.

## Where enforced

- `.github/workflows/claude.yml` and `.github/workflows/claude-code-review.yml` (the two steps and the `outcome == 'failure'` condition).
- `CLAUDE.md`, "Working with the owner".
