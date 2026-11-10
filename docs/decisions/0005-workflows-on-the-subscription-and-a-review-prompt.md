# 0005: The GitHub workflows run on the subscription; the review gets its own prompt

- Status: accepted; supersedes 0002
- Date: 2026-11-10
- Needs owner approval: yes (the owner's instruction of 2026-11-10; given by merging the pull request that carries this record)

## Context

- Record 0002 had both workflows try Console API credits first and fall back to the subscription. In every run that day the API step failed within seconds with `billing_error`, "Credit balance is too low", and the subscription did the work.
- The owner's Billing page explains it: the purchased balance is $0.00, and the $399.99 of promotional credits apply to "Agent SDK, API, Batch API, Playground, Managed Agents". Claude Code, which is what `anthropics/claude-code-action` runs, is not on that list. No key or scope changes that.
- The failed step cost each run 20 to 40 seconds and made a green job say nothing about who paid.
- The stock review (the `code-review` plugin's command) did poorly here: it skipped a pull request because it was a draft and posted nothing; on #10 it read three files closely, skimmed the rest and skipped its own multi-agent pass, finishing in about 30 seconds; on #11 it posted its result twice. It ran on Sonnet 5.5, the action's default for a subscription.

## Decision

- Both workflows use `CLAUDE_CODE_OAUTH_TOKEN` only. The API-credits step is removed. The `ANTHROPIC_API_KEY` secret is no longer read and can be deleted.
- `Claude Code Review` no longer uses the plugin. Its instructions are a prompt in `.github/workflows/claude-code-review.yml`, written for this codebase: read `CLAUDE.md` and the map first; read the whole diff and the code around each change; look, in order, at what the program computes, the saved-file format, memory and lifetime, Qt wiring, the repository's rules, and documentation the change makes wrong; verify a finding before reporting it; post findings as inline comments and exactly one summary that lists which files were read in full and which were not; review drafts too.
- The review runs on Opus (`--model claude-opus-5-5`), because it is invoked only for pull requests that change code and a mistake there is silent and costly (record 0004). `@claude` mentions keep the action's default model.
- Unchanged: the review is invoked by the `review` label and never runs by itself; the Claude GitHub App must be installed on the repository.
- Using the promotional credits would need a workflow that calls the API directly instead of running Claude Code. Not built; see `docs/maybe-later.md`.

## Where enforced

- `.github/workflows/claude.yml` and `.github/workflows/claude-code-review.yml`.
- `CLAUDE.md`, "Working with the owner"; `docs/development-workflow.md`, section 4.
