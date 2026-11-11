# R7 prototype: the train against the point

The experiment behind `docs/research/r7-train-against-point.md`. Python 3, standard library only. Nothing here is part of the program.

## Reproduce every table

From this folder:

```
py -3 run_r7.py --redesign
```

About 30 seconds. It reads the track dumps in `shapes/`, prints every table of the note and writes them to `results.txt` (the note's tables are copied from it) and the plots to `plots/`. Without `--redesign` it skips the last section (the camelback designed with the train's speed) and takes a few seconds.

The full-resolution rows of the validation table ("every node") need the dumps in `work/`, which are not kept in the repository (about 9 MB). To make them, and to remake everything in `projects/` and `shapes/` from scratch, build the program first (`tools\build-windows.cmd` from the repository root) and then:

```
py -3 make_shapes.py --dump ../../../build/dist/fvdcore.exe
py -3 run_r7.py --redesign
```

`make_shapes.py` tunes the shapes (a few seconds), writes the projects and runs `fvdcore nodes` on each. Remade dumps are identical as long as the program computes as it does today.

## Files

| File | What |
|---|---|
| `fvdwrite.py` | A small writer for `.fvd` projects (force sections only), following `track::saveTrack()` and the section, function and transition save code in `core/`. |
| `fvd2d.py` | The program's force-section step (`secforced::updateSection()`, time argument) in the vertical plane, in double precision. Used to tune the shapes and, with a train's speed in place of the point's, for the redesign. For the same force program it reproduces the program's camelback to 0.1 mm in height and the loop to 2 mm (`results.txt`). |
| `make_shapes.py` | Tunes the camelback, the loop, the layout and three lift crests, writes them to `projects/*.fvd`, and with `--dump` has the real program compute them and keeps its output in `shapes/`. |
| `ride.py` | The two models on a fixed shape: the point (the program's energy rule) and the train of N cars sharing one speed. Forces at any car from the shared speed and the shape where that car is. |
| `run_r7.py` | The validation, the closed-form check, the comparison tables, the lift crests and the scaling with train length. |
| `redesign.py` | Question (b): the camelback designed with the train's speed, found by repetition. |
| `projects/*.fvd` | The test tracks as FVD++ projects; they open in the program. `shapes.json` holds the tuned numbers. |
| `shapes/*.tsv.gz` | `fvdcore nodes <project> --every-m 0.1 --frame anchor` for each project, gzipped. The analysis uses only these. |
| `results.txt` | The output of the last full run. |
| `plots/*.svg` | Speed along the camelback, the loop and the layout for the point and the three trains; the camelback's normal force for the long train's front, middle and rear car. |

Settings of every test track: heartline 1.1 m, friction 0.03, resistance 2e-5 (the program's defaults), start in the anchor frame at height 0.
