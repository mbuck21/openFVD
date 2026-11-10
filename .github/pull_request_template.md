<!-- Plain words; the owner is a coaster enthusiast, not a developer. These comments do not show on the page. Rules: CLAUDE.md, "Working with the owner". Open as a draft. -->

## What this does

<!-- What the pull request changes and why, in plain words. If it changes C++, .ui, .pro or shader files, start with "Compiled and run with <toolchain>" or "Not compiled". -->

## Tests

<!-- What was run by hand: the project opened, the change made, the export checked in NoLimits, the diff against the 2016 binary's export. "None" is an honest answer; say why. -->

## New dependency

<!-- "None", or each one with its one-line justification. -->

None.

## For the owner

<!--
- Say who merges: the lead session for a piece going into an integration branch, the owner for anything going into master. Name the files and what to look at.
- For a pull request into master: list what the owner must do before merging, or say "nothing".
- The GitHub review: add the `review` label yourself to a pull request that changes code, and to an integration pull request at the end (CLAUDE.md, "Working with the owner"). Say here that it ran and how its comments were answered.
- Anything the owner must do on the PC (install, check in NoLimits) goes in an `owner` issue, linked here.
-->

<!--
Checklist before marking ready:
- [ ] Branch claude/<slug>; one task; issue number in the description
- [ ] Based on the right branch: the integration branch for a piece, master for a single small task
- [ ] No change under bin/ unless the task was to rebuild the binaries
- [ ] No fvd.log, options.cfg or *.bak committed
- [ ] ui/qcustomplot.* untouched
- [ ] An area label: core, renderer, ui, build, export or docs
-->
