# X1 trials: the design following a chosen car

The scripts behind `docs/research/x1-design-for-a-car.md`. Nothing in the program is changed: the design for a car is made by a stand-in outside the program (a copy of the program's force step, R7's `fvd2d.py`, extended in `x1lib.py`), written as a project with one NoLimits CSV section, and every force a car feels is then measured by the program itself with `fvdcore ride`. Python 3, standard library only. The scripts import R7's `fvd2d.py` and `make_shapes.py` (and, for the re-solved camelback, `redesign.py`) from `docs/research/r7-prototype/`.

## Before you run

1. Build the program: `tools\build-windows.cmd` (it leaves `build\dist\fvdcore.exe`, which every script uses).
2. Run each script with `py -3 docs/research/x1-trials/<script>.py` (from anywhere; paths are built from the script's own place). Each prints its tables as it goes and writes `results/<name>.txt`; the tables in the note are copied from those files.

| Script | What | Run time here |
|---|---|---|
| `validate.py` | step 0: the stand-in against the program's own design (the point design made both ways and ridden by the program with the point and the train), the measuring floor, and the CSV section's search fault on a plain straight | under a minute |
| `iterate.py` | the trial: three shapes x three chosen cars x two readings, each repeated until the shape settles, both plain and with the rule the note recommends (the new speed blended half-and-half with the last one used from the second redesign on); for the front car the one-pass "solve together"; for the camelback R7's version B (durations re-solved each round). Writes `results/iterate.txt`, `results/iterate-rounds.txt` (every round), `results/iterate-times.txt` (wall times) and `projects/<shape>-<car>.fvd` | about 2 to 3 minutes |
| `timing.py` | what one round costs in the program: `fvdcore ride` with the 9-car train and a whole-track recompute, on a 14,000-node project and R7's camelback. Run it alone | under a minute |

`iterate.py --quick` runs the camelback for the centre and front cars only (about 20 seconds; it writes `results/iterate-quick.txt`, `iterate-rounds-quick.txt` and `iterate-times-quick.txt`, which are not kept (git ignores them). It also rewrites two committed projects, `projects/camelback-centre.fvd` and `camelback-front.fvd`; they come out byte for byte the same as the full run's.

Running a script twice gives the same `results/<name>.txt`, except `iterate-times.txt` and `timing.txt`, which are wall times.

## The files

| File | What |
|---|---|
| `x1lib.py` | the shapes (R7's camelback, loop and 0.60 g lift crest), the stand-in designer (`design()`: the program's force step in the vertical plane, with the speed from the point, from a given speed profile, or solved with the train for the front car, `TrainAhead`), the project writer (the design as a NoLimits CSV section, written as project text and made a `.fvd` by `fvdcore fromtext`), the ride (`fvdcore ride --json --table`) and the measures (felt minus asked, how far a shape moved) |
| `validate.py`, `iterate.py`, `timing.py` | the three scripts above |
| `projects/` | the settled designs of reading T (`<shape>-<car>.fvd`; seven of the nine: the loop's and the lift crest's front car have none); they open in FVD++ as a track of one NoLimits CSV section (the lift crest's lift is R7's held section in front of it, and its anchor speed is 30 m/s, see `x1lib.write_project`). Rewritten by `iterate.py` |
| `results/` | the outputs |
| `work/` | throw-away files of a run (every round's project and its text); ignored by git |
