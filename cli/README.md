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
The ride sheet (`core/ridesummary.h`; definitions in `docs/decisions/0012`): length (centre line and heartline), duration, highest and lowest point, biggest drop and climb (bumps under 1 m on the way do not split them), top and lowest speed, floater airtime (vertical force from +0.1 g down to -0.5 g; the labels are built from the thresholds in `rideSummaryOptions`), ejector airtime (below -0.5 g), total airtime, the highest and lowest vertical force, the lateral force to each side, the along-track force each way, inversions (the up direction passes 135 degrees from upright) and the number of findings. Every extreme says where it happens (metres along the centre line, seconds, section). The lateral force is positive when the rider is pressed toward his right (what a left turn does) and negative toward his left. `--sections` adds a table of the sections: type, name, length, duration and the end state (world position of the centre line, heading, pitch, bank, speed). `--json` prints one JSON object instead, always `{"tracks": [...]}` with one entry per track (keys of an entry such as `length_m`, `duration_s`, `highest`, `biggest_drop`, `floater_airtime_s`, `normal_max_g`, `lateral_right_g`, `inversions`, `finding_counts`, `findings`, `section_list`; an extreme is `{value, node, section, time_s, dist_m}`); `note` and `labels` carry the words the text sheet prints; seconds and metres are rounded to 4 decimals. Without `--track` every track is printed; `--track` must be 1 or more, as in `check` and `measures`. The forces and airtime are for the single point the program follows along the heartline; the front and rear cars of a train feel more (`docs/research/r7-train-against-point.md`); the text sheet and the JSON say so. A time here is the sum of the node steps: 1/1000 s per node in force, geometric, straight and curved sections, distance over mean speed in Bezier and NoLimits CSV sections, so it can differ slightly from the `time_s` of `nodes` on those. On a track whose train stalls (nodes that are not numbers) the length is that of the last node whose length is a number; the sheet says so and the JSON has `length_stops_at_stall`.

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
The findings as sentences, one per line, with the severity and the stretch (metres along the track; the sentence has the times), and a last line with the counts. `--json` prints them as an object. Exit code 0 when it ran, 1 on an error, 3 when `--fail-on` is given and a finding of that severity or worse exists. `--list` prints the checks: `basic`, and `forceLimits`, the force limits of ASTM F2291 (public numbers, not a certification) at the point the program follows, as warnings with the node stretch; on `four-sections.fvd` it gives the same six findings as `limits --trace` on its measures. `summary` prints it as the line "Force limits (ASTM F2291, public numbers): within / n findings" (`force_limits` in the JSON).

```
fvdcore totext <project.fvd> <out.json|-> [--force]
fvdcore fromtext <in.json|-> <out.fvd> [--force]
fvdcore totext samples\baseline\four-sections.fvd four.json
fvdcore fromtext four.json four.fvd
```
The project as text (`docs/project-text-format.md`) and back. `-` means standard output (`totext`) or standard input (`fromtext`). Warnings go to standard error; a text that is refused prints the message that names the place and exits 1. An existing output file is not replaced unless `--force` is given. A project written out and read back saves the same bytes as a re-save.

```
fvdcore additions <project.fvd> --show
fvdcore additions <project.fvd> (--project | --track N) (--set key=<json> | --remove key) [--out <file>] [--force]
fvdcore additions samples\baseline\four-sections.fvd --track 1 --set "note=\"made by hand\"" --out four-with-appendix.fvd
```
The additions a project keeps in the appendix after the end of its file (`docs/codebase-map.md`, "Saved project file"). Nothing in the program uses one yet; this makes a test file by hand. `--show` prints the document the appendix holds (`{"needs": [], "project": {...}, "tracks": [{...}, ...]}`, empty objects when there are none; with none at all the file has no appendix). With `--set` or `--remove` the key belongs to the project (`--project`) or to track N (`--track N`, 1-based); exactly one of the two is given, and keys are plain names (`train`, `note`), the same for both. `--set` takes any JSON value after the first `=` (an object, a list, a number, `true`, `null`, or a text in its double quotes) and replaces a key that is there; `--remove` of a key that is not there is an error. The project is re-saved with the change to `--out`, or over itself without `--out`; an existing file is replaced only with `--force`, as `totext` and `import` do. A project whose appendix cannot be read (damaged, or saved by a newer version) loads with a warning on stderr ("... The original file is not written over, so that it keeps its additions: save the project under another name."), without those additions; the file itself still holds them, so `additions`, `totext` and the session's `save` and `totext` refuse to write over that file, even with `--force` (give another output file, which is then written without them).

```
fvdcore import <nltrack|txt|csv> <file> <out.fvd> [--force] [--window-project] [--export-dir <dir>]
fvdcore import csv samples\import\nl2.csv nl2.fvd
```
The three importers of the project panel on the window-less program: a NoLimits 1 track (`nltrack`), a point list (`txt`, one `z x y` per line) or a NoLimits 2 CSV (`csv`, tab-separated with a header line) becomes track 1 of a new project, saved as `<out.fvd>` (an existing file is not replaced without `--force`). `--window-project` first adds the empty track the windowed program starts with, so the import is track 2 and the saved project is what the window's import entry makes; `--export-dir <dir>` also writes, from memory and before anything is reloaded, what `FVD.exe --batch-import` writes: the three exports of each track with sections and `<input name>.resaved.fvd`; `tools/run-baseline.py --core --imports` uses both options to compare with `samples/import/*.ref.*`. A saved import that is opened again does not come back the same (the re-saved project is 33 bytes longer, and a NoLimits 1 import's spline export moves by up to 12 mm), which is why `fvdcore export` of the saved file is not used for that comparison. The reading is `core/nolimitsimporter.*`. Input that cannot be read is refused with a message and exit code 1, nothing is written: a point list with fewer than two points or a word where a number belongs, a CSV line with fewer than ten columns or a CSV with fewer than two lines of data, a NoLimits 1 file whose counts are larger than the file or whose tubes name a point that is not there. (The point-list importer still leaves some values of the first and last point unset, issue 41 item 1; two imports of one list can differ in the exports.)

```
fvdcore commands [--json]
```
The model's named commands (`core/command.h`) with their parameters, units and ranges, as text or as the JSON array a script reads; no project needed. The session's `describe` returns the same list.

```
fvdcore limits [--file <limits.json>] [--json]
fvdcore limits --trace <trace.tsv> [--dt S] [--restraint base|otsr|prone] [--extended] [--no-filter] [--samples <out.tsv>] [--json]
```
The force-limit engine (`core/forcelimits.h`, decision record 0014 points 8 to 11). Without `--trace` it shows the limit file as the program reads it (the embedded `data/limits/astm-f2291-public.json`, or `--file`): the edition, the sources, and every number by its name with its value, confidence, the owner's "checked" note and its source; a file it refuses is named with the place of the problem. With `--trace` it checks one rider's forces: a tab-separated file with a header line naming `gz` (or `forceNormal`/`normal_g`), optionally `gx` (`forceAlong`), `gy` (`forceLateral`/`lateral_g`) and `time_s` (evenly spaced; else `--dt`), so `fvdcore measures <project> --ids forceAlong,forceLateral,forceNormal --every-s 0.01` is a valid input. It prints the result, the largest share of a limit per rule family, the restraint area and one sentence per finding; `--samples` writes the share per sample and family. Exit code 0 when it ran, with or without findings; 1 on an error (a refused file or trace, an unknown restraint). Where nothing applies, the text says "none" or "not found" and the JSON has null. The restraint kind (default `base`; the train catalogue's `lapbar`, `vest`, `other` are the base case) selects the eyes-front line and the egg's radius, `--extended` allows the extended airtime line. The numbers are transcribed from public secondary sources; a design aid, not a certification.

```
fvdcore session <project.fvd> [--track N]
```
Holds a project open and drives it with the model's named commands. It reads one request per line on standard input, as a JSON object with an `"op"`, and writes one reply per line on standard output, flushed, until end of input or `quit`. Every reply has `"ok": true` or `"ok": false` with `"error": "<plain words>"`. A bad line (not JSON, an unknown op, a missing key) gets an error reply and the session goes on. Nothing but the replies goes to standard output (`--log <file>` keeps Qt's messages). Messages from the model, such as the `lenAssert` lines a train that stalls writes, go to standard error; a caller must read or redirect it, or a long session can fill the pipe and stop. `tools/fvdsession.py` drains it in a thread (`stderr_lines()` gives the last 200 lines; `stderr_log=` keeps all of them in a file). Each track has its own undo history (`core/trackhistory.h`); a `"track": N` key (1-based, as the program shows tracks) picks the track for a request, `--track N` sets the default (1). `tools/fvdsession.py` is the Python side (`Session(...).run(...)`, `.undo()`, `.section_end(n)` ...; run as a script it demonstrates itself).

```
{"op":"run","name":"transition.setChange","target":{"section":3,"function":"normal","transition":1},"args":{"change":1.5}}
{"ok":true}
{"op":"sectionEnd","section":3}
{"bank_deg":0.17,"heading_deg":90.2,"ok":true,"pitch_deg":-40.7,"speed_mps":8.1,"x":27.6,...}
{"op":"undo"}
{"done":true,"label":"Change transition value","ok":true,"position":0}
```

| Request | Reply (besides `ok`) |
|---|---|
| `{"op":"describe"}` | `commands`: the list `fvdcore commands --json` prints |
| `{"op":"run","name":"...","target":{...},"args":{...}}` | nothing, or the registry's error text. `target` is written as the registry writes it (1-based `track`, `section`, `transition`, and `function` by name: `roll`, `normal`, `lateral`, `pitch`, `yaw`); `track` defaults to the request's track. `args` are the command's parameters in model units. One command is one undo step |
| `{"op":"undo"}`, `{"op":"redo"}` | `done` (whether it happened), `label` of the step, `position`, and `reason` when it did not (also while a step is open: undo is refused then) |
| `{"op":"history"}` | `steps` (labels, oldest first), `position` (steps below it can be undone), `can_undo`, `can_redo`, `open_scopes` |
| `{"op":"begin","label":"..."}` | `depth`. Opens a step; commands run until `end` join it |
| `{"op":"end","commit":true}` | `recorded` (whether a step was recorded). `"commit":false` rolls back everything since `begin` (a rollback rebuilds and recomputes the whole track, about 0.1 s on a large one) |
| `{"op":"recompute"}` | a whole-track recompute, `updateTrack(0, 0)`. On some section types the recompute from an edited point is not exact, and only a whole-track recompute gives what saving and reopening gives (issue 63, item 14): a solver that commits a trial should recompute first |
| `{"op":"summary","sections":false}` | `summary`: the object `summary --json` prints for one track; `section_list` (with each section's end state) only when `sections` is true |
| `{"op":"sectionEnd","section":N}` | one section's end state, without the ride sheet: `x`, `y`, `z` (centre line, world frame, m), `heading_deg`, `pitch_deg`, `bank_deg`, `speed_mps`, `norm_x/y/z`, `normal_g`, `lateral_g`, `along_g` (roll smoothing included), `dist_m`, `node` (its number), and `non_finite`: whether any node up to and including it is not a number |
| `{"op":"measures","ids":["speed",...],"node":N}` (or `"at_m":X`, `"at_s":X`) | `values` (id to number; `null` where not a number), `node` (0-based, as `nodes` prints it), `time_s`, `section`, `dist_m` |
| `{"op":"check"}` (optional `"check":"id"`) | `findings`, `problems`, `warnings`, `information`, as `check --json` |
| `{"op":"save","path":"...","force":false}` | writes the project as `.fvd` |
| `{"op":"totext","path":"...","force":false}` | writes the project as text (JSON) |
| `{"op":"export","dir":"...","force":false}` | the three exports of each track and the re-saved project, named from the project file as `fvdcore export` names them; `report` lists the files. Each of the three refuses to write over an existing file unless `"force":true` |
| `{"op":"stats"}` | `requests`, `run_count`, `run_ms`, `recompute_count`, `recompute_ms`: the number of requests handled and the time spent in `run` and in `recompute` |
| `{"op":"quit"}` | ends the session |

The cost of a `run` is the model's: it keeps the track's whole state before and after as an undo step (about 12 ms on `gen-normal-shapes.fvd`, 14,000 nodes); `sectionEnd` reads the forces of the whole track once and takes about 0.5 ms there.

```
fvdcore selftest <repository folder>
fvdcore <repository folder>
```
The test `tools\run-tests.cmd` runs (`docs/testing.md`).
