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
- The owner merges every pull request here. Name the files and what to look at.
- Whether the GitHub review is asked for: only when the change touches the physics (core/sec*.cpp, mnode, subfunction), the exporters or the .fvd format, with the reason. Never add the `review` label yourself.
- Anything the owner must do on the PC (install, check in NoLimits) goes in an `owner` issue, linked here.
-->

<!--
Checklist before marking ready:
- [ ] Branch claude/<slug>; one task; issue number in the description
- [ ] No change under bin/ unless the task was to rebuild the binaries
- [ ] No fvd.log, options.cfg or *.bak committed
- [ ] ui/qcustomplot.* untouched
- [ ] An area label: core, renderer, ui, build, export or docs
-->
