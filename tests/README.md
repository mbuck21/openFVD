# Automated tests

Small console programs that check one part of FVD++ without opening the program. They are separate from the baseline comparison in `docs/testing.md`, which compares exported files with the 2016 binary's.

## Run them

From a normal terminal in the repository root:

```
tools\run-tests.cmd
```

(`tools/run-tests.sh` does the same inside the MSYS2 UCRT64 shell.) Every folder under `tests/` is built into `build/tests/<name>/`, run, and the last line is `PASS` or `FAIL` (exit code 0 or 1). Each test prints one line per check, so a failure says which one.

## The tests

| Folder | Checks |
|---|---|
| `fvdmath/` | `core/fvdmath.*`: `asinh2016` and `asin2016` give the 2016 binary's exact result on 364 pinned arguments, most of them chosen where that result differs from the correctly rounded one or from the C runtime's. |
| `traintypes/` | `data/traintypes.json` and its loader `core/traintype.*`: the 38 seeded heartlines match the owner's table, ids are unique, values are in range, and three broken files are rejected with a message. |
| `coreonly/` | `core/` alone: the console program `fvdcore` (sources in `cli/`) builds and links with QtCore and QtGui only, its export of `four-sections.fvd` matches the 2016 binary's Spline export and re-saved project byte for byte, `nodes` writes the expected lines, `compare` of a project with itself is zero, and an unknown command exits 1. |
| `projecttext/` | `core/projecttext.*`: twelve saved projects go binary to text to binary byte for byte and text to text unchanged; awkward floats come back exactly; wrong types, missing keys, out-of-range values, freeform transitions and cut-off texts are refused with the place named. |
| `measures/` | `core/measure.*` and `core/finding.*`: every measure and check is described; values are one per node and nothing in the model is written; the along-track force agrees with the program's energy model; a stalled train (not-a-number speed) gives a finding; one-node and empty tracks. |
| `trackstate/` | `core/trackstate.*`, the snapshot undo is built on: for every baseline project, the CSV import samples and the saved CSV project, a state put back gives its saved bytes exactly and the same exports as one more full recompute, before and after edits made with the calls the windows make; unchanged sections share their bytes; names with accented letters survive; a listener sees the two notices and nothing between them. Run by `tools\run-tests.cmd`; a second argument, a folder of further `.fvd` projects, adds them. |
| `appendix/` | the project file's appendix after `EOP` (`core/projectfile.*`, the additions in `core/trackstate.*` and `core/projecttext.*`): no re-saved baseline project has one, and each saves the same bytes again; with additions the bytes up to `EOP` are those without, the layout and the JSON are as specified and the same for the same additions, save, load, save gives the same bytes and the exports do not change; 24 kinds of damage dropped with a warning or (an unknown `needs` name) refused; unknown keys survive deleting other tracks; undo and redo of an addition, and of an edit beside one, with unchanged bytes shared; the text form round trips. |
| `ridesummary/` | `core/ridesummary.*`, the numbers of the ride sheet: known answers on every baseline project (length, top speed, peak forces, duration, sections, nothing written), airtime, inversions, the biggest drop, the band edges and the threshold on built or synthetic tracks, and an empty and a stalled track. Run by `tools\run-tests.cmd`. |
| `commands/` | `core/command.*` and `core/trackhistory.*`: every registered command is complete and described; every command runs with undo and redo on every baseline project (the exports come back exactly); the history (scopes, rollback, merging with a fake clock, the caps, selections); validation errors; the two dialogs' steps; and "running equals reopened" (decision record 0013). Run by `tools\run-tests.cmd` (about 820 command runs, three to four minutes). By hand, with the repository folder first: `--reopen` runs only the running-equals-reopened and dialog checks and the command times, at every place (about 1100 runs); `--imported` runs only the checks on the imported tracks of `samples/import/`; a folder of further projects as the last argument also runs the running-equals-reopened check on every `*.fvd` in it (`docs/testing.md`). |

`coreonly/` is different from the others: it holds only a `.pro`, which builds the console program `fvdcore` from `cli/` (through `cli/cli.pri`, the same sources `tools/build-windows.sh` deploys) and all of `core/` (through `core/core.pri`) with only QtCore and QtGui, so a tie from `core/` back to the windows stops it from building. With the repository folder as its argument (that is the `selftest` command) it exports `samples/baseline/four-sections.fvd` and compares the Spline export and the re-saved project with the 2016 binary's files, byte for byte; `fvdcore export <project> <outdir>` writes everything `FVD.exe --batch-export` writes (the other commands are in `cli/README.md`), and `tools/run-baseline.py --core` runs the whole baseline comparison with it (`docs/testing.md`). `run-tests` also starts `tools/check-core.py`, which names the file and line of any `core/` include of a window header, before anything is built.

## Add one

1. Make `tests/<name>/` with a `<name>.pro` (copy `traintypes/traintypes.pro`) and a `main.cpp`. The program name is the folder name, or the `TARGET` of the `.pro` when it sets one (`coreonly` builds `fvdcore`).
2. Keep it to QtCore and the few source files under test (`SOURCES += ../../core/<file>.cpp`), so it builds on its own and stays fast. Every test is run with the repository folder as its first argument; find data files from that, never from an absolute path typed by hand or a `DEFINES` path (qmake splits those at a space in the folder name).
3. Print one line per check, then `PASS` or `FAIL` as the last line, and return 0 or 1.
4. Add a row to the table above. `tools/run-tests.*` picks the folder up by itself.
