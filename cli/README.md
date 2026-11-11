# fvdcore: the command-line program

A console program built from `core/` and `cli/` alone (QtCore and QtGui; no windows, no OpenGL). `tools\build-windows.cmd` builds it and leaves `build/dist/fvdcore.exe` beside `FVD.exe`. Metric units only; errors go to stderr with exit code 1. `--log <file>` anywhere keeps Qt's debug output, which is otherwise dropped.

| File | What |
|---|---|
| `main.cpp` | argument handling and the table of commands |
| `commands.h` | the `command` struct and one `extern` line per command |
| `project.{h,cpp}` | loading a project, picking a track, option parsing (the world placement of a node, `placement`, is in `core/ridesummary.h`) |
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
fvdcore summary <project.fvd> [--track N] [--sections] [--json]
fvdcore summary samples\baseline\four-sections.fvd --sections
```
The ride sheet (`core/ridesummary.h`; definitions in `docs/decisions/0012`): length (centre line and heartline), duration, highest and lowest point, biggest drop and climb (bumps under 1 m on the way do not split them), top and lowest speed, floater airtime (vertical force from +0.1 g down to -0.5 g; the labels are built from the thresholds in `rideSummaryOptions`), ejector airtime (below -0.5 g), total airtime, the highest and lowest vertical force, the lateral force to each side, the along-track force each way, inversions (the up direction passes 135 degrees from upright) and the number of findings. Every extreme says where it happens (metres along the centre line, seconds, section). The lateral force is positive when the rider is pressed toward his right (what a left turn does) and negative toward his left. `--sections` adds a table of the sections: type, name, length, duration and the end state (world position of the centre line, heading, pitch, bank, speed). `--json` prints one JSON object instead, always `{"tracks": [...]}` with one entry per track (keys of an entry such as `length_m`, `duration_s`, `highest`, `biggest_drop`, `floater_airtime_s`, `normal_max_g`, `lateral_right_g`, `inversions`, `finding_counts`, `findings`, `section_list`; an extreme is `{value, node, section, time_s, dist_m}`); `note` and `labels` carry the words the text sheet prints; seconds and metres are rounded to 4 decimals. Without `--track` every track is printed; `--track` must be 1 or more, as in `check` and `measures`. The forces and airtime are for the single point the program follows along the heartline; the front and rear cars of a train feel more (`docs/research/r7-train-against-point.md`); the text sheet and the JSON say so. A time here is the sum of the node steps: 1/1000 s per node in force, geometric, straight and curved sections, distance over mean speed in Bezier and NoLimits CSV sections, so it can differ slightly from the `time_s` of `nodes` on those.

```
fvdcore measures <project.fvd> [--track N] [--ids a,b,c] [--every-m X | --every-s X | --all] [--out <file>]
fvdcore measures samples\baseline\four-sections.fvd --ids speed,forceNormal --every-m 5
fvdcore measures --list
```
The measure registry as tab-separated text with one header line: `index`, `time_s`, `section`, `dist_m`, then one column per measure id (all of them without `--ids`). Rows are chosen as `nodes` chooses them; numbers carry 9 significant digits. `--list` prints id, unit, name and description of every measure.

```
fvdcore check <project.fvd> [--track N] [--check ID] [--json] [--fail-on problem|warning|information]
fvdcore check samples\baseline\four-sections.fvd --fail-on problem
fvdcore check --list
```
The findings as sentences, one per line, with the severity and the stretch (metres along the track; the sentence has the times), and a last line with the counts. `--json` prints them as an object. Exit code 0 when it ran, 1 on an error, 3 when `--fail-on` is given and a finding of that severity or worse exists. `--list` prints the checks.

```
fvdcore totext <project.fvd> <out.json|-> [--force]
fvdcore fromtext <in.json|-> <out.fvd> [--force]
fvdcore totext samples\baseline\four-sections.fvd four.json
fvdcore fromtext four.json four.fvd
```
The project as text (`docs/project-text-format.md`) and back. `-` means standard output (`totext`) or standard input (`fromtext`). Warnings go to standard error; a text that is refused prints the message that names the place and exits 1. An existing output file is not replaced unless `--force` is given. A project written out and read back saves the same bytes as a re-save.

```
fvdcore selftest <repository folder>
fvdcore <repository folder>
```
The test `tools\run-tests.cmd` runs (`docs/testing.md`).
