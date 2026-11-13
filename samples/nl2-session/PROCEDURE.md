# Your hour in NoLimits 2

What this settles: how NoLimits 2 slows a train down (friction, air drag), whether a valley costs more than a level, and whether it runs a train as one body or as a point. Why: `docs/research/r12-nolimits2-train-physics.md` section 7. About an hour for E0 to E4; E5 only with the Professional licence.

"**check on screen**" marks a step that NoLimits 2's published help does not settle (menu names, where a button is, what a number means). Do what looks right and write what you did in `notes.txt` (copy `notes-template.txt`). Anything else in a step comes from the help.

Open a terminal in the repository folder. Recordings go in `samples/private/` (git ignores that folder; nothing there is ever committed).

## A. Once, before the first run (10 minutes)

1. Check the tools: `py -3 tools/nl2log.py --selftest` and `py -3 tools/nl2fit.py --selftest` both end `PASS`.
2. In NoLimits 2: Main Menu, Setup, Others: switch the telemetry server on and the units to Metric. The help names this place; **check on screen** the exact words of the telemetry switch (it can also be switched on by starting NoLimits 2 with `--telemetry`). The port is 15151 unless you changed it.
3. Make a new park and one coaster in a steel style, a plain station or none (**check on screen**: how a coaster and its first train are made, and whether a train needs a station to start). Write the style in the notes. Open Coaster Properties, Mode tab: write the Physics Model it shows (**check on screen**: NL 2.0 or NL 2.2, R12 could not tell which is the default). Leave it as it is. Trains tab: set 2 cars (the number is in the Trains tab; **check on screen** whether 2 is allowed for your style, else use the smallest).
4. Bring in the track: `samples/nl2-session/e1-level-800-v10.nl2elem`. **check on screen**: the help names the format but not where an element file is imported. Look for an import or element entry under the Editor's File or Element menu. Note where it was.
5. The track is level for its whole 800 m, and the train (up to 9 cars, about 20 m) stands at its start. Make the first 50 m a Transport section, so the train starts on it: Editor, Track tab, Select/Modify, select the section, Section Type, Transport (these names are from the help; **check on screen** how to select just the first 50 m, perhaps by splitting the track, and that the rest is plain Track). Click Type Settings.

## B. E0: the units (10 minutes)

6. Set the Transport Speed to 10. Start the ride: **check on screen** how to enter play mode (the help does not say; it says Escape leaves it and P pauses). Press F2. When the train has settled after the transport, write the speed F2 shows. Do it again with 20 and 40 (**check on screen** whether the train needs a reset: the Ride Control Panel, F4, has Reset). This tells you whether the setting is m/s, km/h or mph. Write it down.
7. While the train runs, in a terminal run `py -3 tools/nl2log.py --probe`. Write the speed it prints next to the one on F2: that is the unit of the telemetry speed (**check on screen** which unit; the help gives none). It also prints the position and g: write position `x` before and after the train has moved 100 m by the F2 distance or the track, if you can tell (**check on screen** whether position is in metres).

## C. E1: a coast-down on the level (15 minutes)

8. With the Transport speed that makes F2 show 10 m/s (use step 6) run: `py -3 tools/nl2log.py --out samples/private/e1-v10-2cars.csv --seat 0 0`. Start the ride. The recorder waits for the train to move and stops by itself after the train has been stopped for about 3 seconds, or press Ctrl+C. (`--seat CAR SEAT` takes numbers counted from 0; **check on screen** that they match C and V in the ride by running `--probe` after pressing C or V.) Use the seat in the middle of the first car.
9. Same with F2 showing 40 m/s: `--out samples/private/e1-v40-2cars.csv`.
10. Trains tab: 9 cars. Repeat steps 8 and 9 with `--seat` set to the middle car (`--seat 4 0`; **check on screen**) and names `e1-v10-9cars.csv`, `e1-v40-9cars.csv`.
11. Fit each pair: `py -3 tools/nl2fit.py --speed-unit <unit from step 7> --skip-start 60 samples/private/e1-v10-2cars.csv samples/private/e1-v40-2cars.csv` (and the 9 cars pair). `--skip-start 60` leaves out the transport; change it if your transport is longer. Write the friction and drag it prints. The predicted numbers are in `predictions.md`.

## D. E2: a valley against a level (10 minutes)

12. Replace the track with `e2-level-600.nl2elem`, transport speed so F2 shows 30 m/s, 2 cars. Make the first 50 m a Transport section (the track is level for its first 120 m, before the valley, so the train stands on level track; **check on screen** where the train starts). Record: `py -3 tools/nl2log.py --out samples/private/e2-level.csv --seat 0 0`.
13. Same with `e2-valley-1.5g.nl2elem` (`e2-valley-1.5g.csv`) and `e2-valley-4g.nl2elem` (`e2-valley-4g.csv`). Same transport, same seat.

## E. E3: the camelback with a short and a long train (10 minutes)

14. Replace the track with `e3-camelback.nl2elem`, the track starts with 60 m of level run-in before the camelback: make the first 40 m a Transport section (the train, up to 9 cars of about 20 m, stands on it; **check on screen** where it starts), transport speed so F2 shows 30 m/s. Record with 2 cars and with 9 cars, one run for each seat: the first car (`--seat 0 0`), the middle car (9 cars: `--seat 4 0`; 2 cars: use the first car again and then car 1, `--seat 1 0`) and the last car (9 cars: `--seat 8 0`). Names `e3-2cars-front.csv`, `e3-9cars-mid.csv` and so on. **check on screen** the car count and numbers of your style.

## F. E4: lift crest and brake (10 minutes)

15. Replace the track with `e4-liftcrest-0.60.nl2elem`. The first 42 m are level; make them a Lift section (Section Type, Lift), so that the lift ends at the crest, 42 m from the start, where the drop begins. The 9-car train (about 20 m) stands on the lift, with about 8 m to spare; **check on screen** where the train starts and that the lift can be set to end exactly there, Lift Speed 3.5 (**check on screen** the unit; it is in the units of step 6). 9 cars. Record one run for each of the front, middle and last car: `e4-front.csv`, `e4-mid.csv`, `e4-rear.csv`.
16. Brake trigger: take `e1-level-800-v40.nl2elem` back, transport at F2 40 m/s, put a Trim Brake (Section Type, Brake; **check on screen** its names) across the middle of the track with a Speed Limit below the train's speed. Choose First Car as the trigger car (the help names First Car and Middle Car and a position slider; **check on screen** the exact words) with the slider at 0; record `e4b-first.csv` (`--seat 0 0`). Same with Middle Car as trigger: `e4b-middle.csv`.

## G. E5: custom friction (Professional licence only; skip if you have none)

17. Back to `e1-level-800-v10.nl2elem`. Coaster Properties, Mode tab, Physics Model: choose the custom friction model (**check on screen** its name and the two boxes). Friction 0.03, air resistance 0. F2 at 10 m/s, record `e5-fric0.03.csv`. Then friction 0, air resistance 1e-4, F2 at 40 m/s: `e5-drag1e-4.csv`. Fit each alone: `py -3 tools/nl2fit.py --speed-unit <unit> --skip-start 60 --fit friction samples/private/e5-fric0.03.csv` and `--fit drag` for the other. Write what comes back.

## When you are done

18. Fill in `notes.txt`. Put it with the CSV files in `samples/private/`. Tell the session that did this kit that the recordings are there; it reads them (the CSV columns and what each experiment is compared with are in `README.md` and `predictions.md`). If something did not work, write down exactly what you saw; that is also a result.
