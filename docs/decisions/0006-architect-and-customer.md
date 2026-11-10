# 0006: The lead session is the architect; the owner is the customer

- Status: accepted
- Date: 2026-11-10
- Needs owner approval: no (the owner's instruction of 2026-11-10, written down here)

## Context

- The owner gave a list of nine feature requests on 2026-11-10 and asked for them to be documented and ordered into a roadmap, not built.
- With the list came an instruction for future sessions: the session is the architecture expert in control of a team of sub-agents; the owner is, in his words, basically the customer, and a lot of decisions can be made by the expert.
- Until now sessions brought most choices to the owner, including technical ones he had no reason to weigh in on.

## Decision

- **The owner is the customer.** He says what the program should do for him, sets priorities, tries the results, and decides anything that changes how his coasters come out, his saved project files, his money or his PC (installs, purchases, accounts).
- **The lead session is the architect.** It decides how things are built: the structure of the code, the technology, the order of work, the tooling, what to refactor and what to leave. It makes those decisions without asking and writes the significant ones down as decision records, so the owner can read why and overrule.
- **It runs the team.** Sub-agents and side sessions work under the lead on an integration branch (record 0004), on Opus, Sonnet or Haiku.
- **What still goes to the owner:** product choices (what a feature does, what it is called on screen, which train types come first), priorities between requests, anything that needs his hands, and anything that would change results or saved files. The lead brings a recommendation with the question, not a list of options to research.
- **The requests and the plan are documents in the repository:** `docs/feature-requests.md` keeps what was asked, with an id per request; `docs/roadmap.md` holds the stages and is rewritten as the plan changes. Issues are opened for a stage's pieces when the stage starts, not before.
- The first architectural decisions taken under this record are in the roadmap: the core is separated from the widgets before any analysis feature is built; train types become data; the radius and force checks share one analysis framework; macOS is a non-goal; refactoring is targeted, not a rewrite.

## Where enforced

- `CLAUDE.md`, "Working with the owner" (the first bullet).
- `docs/roadmap.md`, "Decisions that are the owner's".
- `docs/feature-requests.md`.
