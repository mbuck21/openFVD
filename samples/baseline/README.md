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

## The generated projects (`gen-*.fvd`), added 2026-11-10

Five more projects, built by the program itself with `FVD.exe --batch-make-fixtures` (the scenarios in `ui/batchscenarios.cpp`, which drive the same windows a person uses; `docs/testing.md` has the table of what each covers). Between them: all four section types, every transition shape the editor offers with its variants, time and distance arguments, both orientations, fixed and physics speed, a hanging heartline, friction and resistance, a moved and turned anchor, smoothing on one section and on a whole track, and a project with two tracks.

Their references were made the same day with the 2016 binary, driven on screen: each project opened with File, Load; exported with File, Export As once per Exporter Type (segment length 2.00 m, RelRoll threshold 85, without heartlining off, from the first to the last section); then saved with File, Save As.

| File | What |
|---|---|
| `gen-<name>.fvd` | The project as the generator wrote it. `tools/run-baseline.py --fixtures` checks that the generator still writes exactly this. |
| `gen-<name>.2016.nl2elem`, `.2016.tangent.nlelem`, `.2016.spline.nlelem` | The 2016 binary's exports. |
| `gen-<name>.2016.resaved.fvd` | The project as the 2016 binary saved it again. |
| `gen-two-tracks.2016.track2.*` | The exports of that project's second track. |
| `four-sections.2016.resaved.fvd` | The first project as the 2016 binary saved it again (763 bytes). |

On 2026-11-10 the rebuilt program matched every one of these files exactly, with no tolerance.

Two things to know when making references by hand again:

- With more than one track in a project, the export dialog does not start on "all sections": "Export To Section" came up one short of the last section. Set it to the last section for each track. The batch mode always exports all sections.
- The dialog keeps the exporter type, track and sections from the last export while the program runs.

**2026-11-11: `gen-curves-speed.fvd` re-recorded** (Stage 2, anchor commands). The scenario sets the anchor's roll (8 degrees) and speed before adding sections. The running program used to keep a start point that a load does not build: the speed box took the train's energy from the rolled start point, while a load takes it from a level one. An anchor edit now builds the start point exactly as a load does. The header and every value typed in are unchanged. 22 bytes changed, all of them values the program stores but recalculates on every load: the start values of the first transition piece in six transitions, and the node numbers held by the smoothing entries, one node apart. The new file equals its own re-save byte for byte, which the old file never did (22 bytes differed). Loaded by the rebuilt program, the old and the new file give the same model: their re-saves and their three exports are byte for byte identical, and they match the 2016 references above. The `.2016.*` files are not touched. Before the merge, the lead opens the new file in the 2016 binary itself and compares its exports and re-save with those references.

**2026-11-11: `gen-geometric-distance.fvd`, `gen-normal-shapes.fvd` and `gen-two-tracks.fvd` re-recorded** (Stage 2, T1-U11: every command ends in the state that saving and reopening gives, decision record 0013). The scenarios save the running track; each edit used to recompute only from the edited point, and now ends with the whole track computed as a load computes it, with the smoothing entries' names and node ranges taken from the track as a load takes them. The header and every value typed in are unchanged. What changed are values the program stores but recalculates on every load: in all three, every smoothing entry's name (empty before; now the track's or section's name, as a load sets it, which makes the files 55, 55 and 67 bytes longer) and the node ranges of the whole-track and section entries; in `gen-geometric-distance.fvd` also the start values of eleven transition pieces in eight transitions (the running track had been stale there by centimetres, issue 63 item 14). Each new file equals its own re-save byte for byte, and is byte for byte the `.2016.resaved.fvd` the 2016 binary wrote from the old file. Loaded by the rebuilt program (`fvdcore export`), the old and the new file give byte-identical re-saves and exports, which match the 2016 references above. The `.2016.*` files are not touched. Before the merge, the lead opens the new files in the 2016 binary itself and compares their exports and re-save with those references.

## The negative-tension projects (`n1-*.fvd`), added 2026-11-10

Two synthetic projects from the N1 investigation (`docs/research/n1-difference-from-2016-on-an-owner-project.md`), copied unchanged from `samples/investigations/`: each is a baseline project with a few numbers changed so that a transition has a **negative tension**, which makes the program call `asinh` on a `float`. They test that the rebuilt program computes that function exactly as the 2016 binary did (`core/fvdmath.cpp`, `docs/research/n2-matching-the-2016-maths.md`). Neither contains anything from the owner's own projects.

| File | Made from | What it shows |
|---|---|---|
| `n1-tension-force-chain.fvd` | `gen-normal-shapes.fvd`, one transition of the first force section changed to -2.5 g with tension -1.4 | Two force sections in a row: the second starts from the force the first measured at its end, so a last-bit difference shows up as a fraction of a millimetre. |
| `n1-tension-loop.fvd` | `four-sections.fvd`, the force section's transition given tension -1.2 | The geometric section inherits the pitch and yaw rates at the top of a tight loop, so a last-bit difference shows up as metres. |

Their references were exported with the 2016 binary on 2026-11-10, the same way as the generated projects (File, Load; File, Export As once per Exporter Type with the dialog's defaults; File, Save As), and named like the others: `n1-<name>.2016.nl2elem`, `.2016.tangent.nlelem`, `.2016.spline.nlelem`, `.2016.resaved.fvd`. The rebuilt program matched them exactly from the change that added `core/fvdmath.cpp`; before it, the loop project differed in all three exports and the re-saved file, and the force-chain project in one segment of the Tangent export.
