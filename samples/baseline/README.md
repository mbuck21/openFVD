# Baseline from the 2016 release binary

Made on 2026-11-10 with `bin/win64/FVD.exe` (the binary upstream committed, built 2016-10-19). These files are the fixed reference for `docs/testing.md`. Do not regenerate or overwrite them.

## How `four-sections.fvd` was made

Started the program, opened the default "unnamed Track", and added four sections in this order, leaving every value at its default except one:

1. **Straight Section**: 10 m, fixed speed 10 m/s.
2. **Curved Section**: radius 15 m, total angle 90, direction 90, lead in 10, lead out 10.
3. **Force Section**: 1 s. The normal-force transition was changed from 0 g to **+2 g** (cubic, 1 s), so the track pulls up from 1 g to 3 g.
4. **Geometric Section**: 1 s, defaults (it continues the pitch rate the force section ends with, which makes a tight loop).

Anchor, heartline, friction and resistance are the program's defaults. Saved with File, Save As.

## How the exports were made

File, Export As, with the dialog's defaults: track 1, from section 1 to section 4, segment length 2.00 m, threshold for RelRoll 85 degrees, "Export without heartlining" off. One export per Exporter Type:

| File | Exporter Type |
|---|---|
| `four-sections.2016.nl2elem` | NoLimits2 Exporter |
| `four-sections.2016.tangent.nlelem` | Tangent Exporter |
| `four-sections.2016.spline.nlelem` | Spline Exporter |
