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
| `coreonly/` | `core/` alone: the console program `fvdcore` builds and links with QtCore and QtGui only, and its export of `four-sections.fvd` matches the 2016 binary's Spline export and re-saved project byte for byte. |

`coreonly/` is different from the others: it builds the console program `fvdcore` from all of `core/` (through `core/core.pri`) with only QtCore and QtGui, so a tie from `core/` back to the windows stops it from building. With the repository folder as its argument it exports `samples/baseline/four-sections.fvd` and compares the Spline export and the re-saved project with the 2016 binary's files, byte for byte; `fvdcore export <project> <outdir>` writes everything `FVD.exe --batch-export` writes, and `tools/run-baseline.py --core` runs the whole baseline comparison with it (`docs/testing.md`). `run-tests` also starts `tools/check-core.py`, which names the file and line of any `core/` include of a window header, before anything is built.

## Add one

1. Make `tests/<name>/` with a `<name>.pro` (copy `traintypes/traintypes.pro`) and a `main.cpp`. The program name is the folder name, or the `TARGET` of the `.pro` when it sets one (`coreonly` builds `fvdcore`).
2. Keep it to QtCore and the few source files under test (`SOURCES += ../../core/<file>.cpp`), so it builds on its own and stays fast. Every test is run with the repository folder as its first argument; find data files from that, never from an absolute path typed by hand or a `DEFINES` path (qmake splits those at a space in the folder name).
3. Print one line per check, then `PASS` or `FAIL` as the last line, and return 0 or 1.
4. Add a row to the table above. `tools/run-tests.*` picks the folder up by itself.
