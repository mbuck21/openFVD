# The kit for a session in NoLimits 2

Built for Stage 3 (piece N3a). It makes the owner's session in NoLimits 2 short: the six experiments of `docs/research/r12-nolimits2-train-physics.md` section 7 (E0 to E5), each with a test track, the numbers this program predicts for it, a recorder for NoLimits 2's telemetry and a fit for friction and drag. What the owner does, step by step: **[PROCEDURE.md](PROCEDURE.md)**.

## Files

| File | What | Experiment |
|---|---|---|
| `e1-level-800-v10.fvd`, `.nl2elem` | a dead level straight, 800 m, held at 10 m/s in the program | E0, E1, E4b, E5 |
| `e1-level-800-v40.fvd`, `.nl2elem` | the same, held at 40 m/s (the `.nl2elem` files are the same track; the speed is set by the transport section in NoLimits 2) | E1, E5 |
| `e2-level-600.fvd`, `.nl2elem` | level, 600 m, entry 30 m/s | E2 |
| `e2-valley-1.5g.fvd`, `.nl2elem` | the same 600 m with a 1.5 g valley about 72 m long and 8 m deep, back at the same height | E2 |
| `e2-valley-4g.fvd`, `.nl2elem` | the same with a 4 g valley about 31 m long and 30 m deep, back at the same height | E2 |
| `e3-camelback.fvd`, `.nl2elem` | R7's camelback (`docs/research/r7-prototype/projects/camelback.fvd`, copied byte for byte), entry 30 m/s | E3 |
| `e4-liftcrest-0.60.fvd`, `.nl2elem` | R7's lift crest at 0.60 g, a 3.5 m/s lift and the drop after it (copied byte for byte) | E4a |
| `predictions.md`, `predictions.json` | the predicted numbers, readable and for a script | all |
| `notes-template.txt` | the answers the owner writes down during the session | all |
| `make_kit.py` | regenerates every file above | |

The tools are `tools/nl2log.py` (the recorder, `--selftest`) and `tools/nl2fit.py` (the fit, `--selftest`). Recordings go to `samples/private/`, which git ignores (see below).

## Where the predicted numbers come from

- The projects are computed by the real program (`fvdcore nodes`, `fvdcore summary`, `fvdcore export`, `build/dist/fvdcore.exe`); the straights are written through the text form (`fvdcore fromtext`), the valley tracks by R7's `fvdwrite.py` and shaped with R7's `fvd2d.py` so they enter and leave at the same height and pitch. All use the program's defaults: heartline 1.1 m, friction 0.03, resistance 2e-5.
- E1 and E5 (the level coast-downs): the program's energy rule `d(v^2/2)/ds = -(g f + k v^2)` integrated in Python for three sets of numbers: the program's defaults and the two fits one NoLimits 2 user reported (R12 section 2.3, low confidence).
- E2 (the valleys): `fvdcore nodes` on each project. The program's rule loses the same energy per metre in a valley as on the level, so the three tracks must agree to within the longer path of the valleys. If NoLimits 2 loses more in the valleys, friction depends on wheel load.
- E3 and E4 (the train): R7's Python reference `docs/research/r7-prototype/ride.py` on R7's committed dumps, a train of cars sharing one speed. Car lengths are R7's assumptions (2.5 m for 2 or 4 cars, 2.2 m for 9); NoLimits 2's real car length is not published. **The C++ ride (`fvdcore ride`, piece P1 of Stage 3) is being built in parallel and was not available; nothing here uses it.** Once it exists, a check that it agrees with these numbers belongs in P1's tests.
- None of these numbers is a prediction of NoLimits 2. They are what this program's model says, so that a recording can be compared at once.

## Regenerate

```
py -3 samples/nl2-session/make_kit.py                 # writes into this folder
py -3 samples/nl2-session/make_kit.py --out <folder>  # or somewhere else
py -3 samples/nl2-session/make_kit.py --check         # PASS when the committed files equal a fresh run
```

It needs `build/dist/fvdcore.exe` (`tools\build-windows.cmd`, or `--fvdcore <path>`). It takes about six minutes (the valleys are shaped in Python and the straights have 80,000 nodes). Running it twice gives the same bytes. Only the standard library is used.

## Recordings are private

`tools/nl2log.py --out samples/private/<name>.csv`. The folder `samples/private/` is ignored by git (`.gitignore`), so recordings are not committed; send them to the session that analyses them. If a clone has no such folder, `nl2log.py` makes it.
