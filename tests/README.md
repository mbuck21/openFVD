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
| `traintypes/` | `data/traintypes.json` and its loader `core/traintype.*`: the 38 seeded heartlines match the owner's table, ids are unique, values are in range, and three broken files are rejected with a message. |

## Add one

1. Make `tests/<name>/` with a `<name>.pro` (copy `traintypes/traintypes.pro`) and a `main.cpp`. The program name in the `.pro` must equal the folder name.
2. Keep it to QtCore and the few source files under test (`SOURCES += ../../core/<file>.cpp`), so it builds on its own and stays fast. Find data files through a `DEFINES` path built from `$$PWD`, never an absolute path.
3. Print one line per check, then `PASS` or `FAIL` as the last line, and return 0 or 1.
4. Add a row to the table above. `tools/run-tests.*` picks the folder up by itself.
