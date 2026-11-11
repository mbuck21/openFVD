# fvdcore: the command-line program

A console program built from `core/` and `cli/` alone (QtCore and QtGui; no windows, no OpenGL). `tools\build-windows.cmd` builds it and leaves `build/dist/fvdcore.exe` beside `FVD.exe`. Metric units only; errors go to stderr with exit code 1. `--log <file>` anywhere keeps Qt's debug output, which is otherwise dropped.

| File | What |
|---|---|
| `main.cpp` | argument handling and the table of commands |
| `commands.h` | the `command` struct and one `extern` line per command |
| `project.{h,cpp}` | loading a project, picking a track, world placement of a node, option parsing |
| `cmd_<name>.cpp` | one command each |
| `cli.pri`, `fvdcore.pro` | the source list (shared with `tests/coreonly/`) and the deployed build |

A new command is a new `cmd_<name>.cpp` defining `const command cmd<Name>`, one `extern` line in `commands.h`, one line in the table in `main.cpp` and one in `cli.pri`.

## Commands

```
fvdcore help
```
Lists the commands (also what no arguments prints).

```
fvdcore export samples\baseline\four-sections.fvd out
```
Writes the NoLimits exports, `<project>.resaved.fvd` and `<project>.batch.txt`, the same files `FVD.exe --batch-export` writes.

```
fvdcore nodes <project.fvd> [--track N] [--every-m X | --every-s X | --all] [--frame world|anchor] [--out <file>]
fvdcore nodes samples\baseline\four-sections.fvd --every-m 5 --out four.tsv
```
The track as tab-separated text with one header line, for Python (`pandas.read_csv(path, sep="\t")`). Track 1 and one point per metre along the centre line by default; the first and the last node are always there. `--every-s X` takes a point every X seconds, `--all` every node (1000 per second). `--frame world` (default) puts the track where the program shows it; `--frame anchor` gives the raw node values.

Columns: `index` (node number), `time_s`, `section` (1-based), `dist_m` and `heart_dist_m` (distance along the centre line and the heartline), `heart_x/y/z` (the heartline position), `pos_x/y/z` (the centre line, heartline plus the heart offset along `norm`), `dir_*`, `lat_*`, `norm_*` (unit vectors; `norm` points from the rider toward the rails), `roll`, `pitch`, `heading` (degrees; the heading includes the track's start yaw in the world frame), `speed_mps`, `normal_g`, `lateral_g` (roll smoothing included), `energy` (J/kg). Numbers carry 9 significant digits.

```
fvdcore compare <a.fvd> <b.fvd> [--track N] [--tsv]
fvdcore compare before.fvd after.fvd
```
How far project b moved from project a: for each section present in both (matched by position in the list) and for the track end, the distance between the two end positions (world frame, centre line) and the change in heading, pitch, bank and speed (b minus a; heading and bank wrapped to -180..180), then the total length and duration. If the section counts differ it says so and compares the common ones. `--tsv` prints a tab-separated table; its last two rows (`total_length_m`, `total_duration_s`) carry b minus a in the `distance_m` column.

```
fvdcore selftest <repository folder>
fvdcore <repository folder>
```
The test `tools\run-tests.cmd` runs (`docs/testing.md`).
