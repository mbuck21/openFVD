#!/usr/bin/env python3
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
"""Trial D: two harder elements (R6 E4) on the real model.

D1 zero-g roll: the camelback (valley 2.5 g, 1 s ramps, 0.4 s holds) with a roll on its
   crest (roll rate 0 -> r -> 0 over t_air). Unknowns t2 (= t4), t_air, r; goals exit
   level, crest height (the middle of the reachable range at that speed, from a scan),
   roll 360 degrees. Airtime 0.0 g and -0.2 g (where R6 expects the track to yaw).
D2 overbanked turn: pull-up at 2.5 g, roll-in, a hold at 1.0 g, roll-out with a pull-out
   (elements.overbank_sections). Unknowns roll-in rate r, hold, roll-out rate q; goals
   heading change 180 deg, exit pitch 0, exit bank 0 (unwrapped).
Both at entry 20, 25, 30, 35 m/s, entry bank 0 and 10 deg, quaternion and euler orientation.
Writes results/trial_d.txt. Run: py -3 docs/research/e1-trials/trial_d.py
"""
import time

from common import Report, PROJECTS, f
from e1solver import Model
from elements import ZeroGRoll, zero_g_roll_sections, Overbank, overbank_sections, T2_GRID

SPEEDS = [20.0, 25.0, 30.0, 35.0]
BANKS = [0.0, 10.0]
ORIENTATIONS = ["quaternion", "euler"]


def set_entry(m, speed, bank):
    m.set("anchor.setSpeed", {}, speed=speed)
    m.set("anchor.setRoll", {}, roll=bank, holdX=0.0, holdY=0.0, holdZ=0.0)


def zero_g(rep):
    rows = []
    for ga in (0.0, -0.2):
        targets = {}
        for orient in ORIENTATIONS:
            path = PROJECTS / f"d-zero-g-roll-{orient}.fvd"
            from fvdtext import write_project
            write_project(path, zero_g_roll_sections(Ga=0.0, orientation=orient), speed=25.0)
            for v in SPEEDS:
                for bank in BANKS:
                    m = Model(path)
                    set_entry(m, v, bank)
                    el = ZeroGRoll(m, Ga=ga, entry_bank=bank)
                    el.set_forces(Ga=ga, Gp=2.5)
                    t0 = time.perf_counter()
                    evals = 0
                    # scan for a valid start (roll fixed at 360 deg over 2 s; t_air changes the roll, not
                    # the path, at 0 g)
                    scan, n = el.scan(T2_GRID[::2])
                    evals += n
                    valid = [s for s in scan if s[2] is not None]
                    if not valid:
                        rows.append([f"{ga:+.1f}", orient, f(v, 0), f(bank, 0), "no valid hill", evals] + [""] * 9)
                        m.close()
                        continue
                    if v not in targets:
                        c = sorted(s[2] for s in valid)
                        targets[v] = round((c[0] + c[-1]) / 2 * 2) / 2
                    crest = targets[v]
                    best = min(valid, key=lambda s: abs(s[2] - crest))
                    goals = [("exit_pitch", 0.0, 0.05), ("crest", crest, 0.05), ("roll", 360.0, 0.1)]
                    res = el.problem(goals).solve([best[0], best[1], 720.0 / max(best[1], 0.2)], max_evals=80)
                    evals += res.evals
                    secs = time.perf_counter() - t0
                    val = res.values or {}
                    rows.append([f"{ga:+.1f}", orient, f(v, 0), f(bank, 0), res.status, evals, res.evals, f(secs, 1),
                                 f(crest, 1), f(res.x[0], 3), f(res.x[1], 3), f(res.x[2], 1),
                                 f(val.get("heading_drift"), 2), f(val.get("exit_bank"), 2),
                                 f(val.get("exit_pitch"), 2)])
                    print(rows[-1], flush=True)
                    m.close()
    rep("D1. Zero-g roll. 'evals' all evaluations including the scan; 'newton' the final solve's; the "
        "heading drift is the exit heading minus the entry heading and the exit bank is the bank at the exit "
        "(neither is a goal); entry bank is the anchor's roll.")
    rep.table(["airtime g", "orientation", "entry m/s", "entry bank", "result", "evals", "newton", "s",
               "crest goal (m)", "t2 (s)", "t_air (s)", "roll rate peak (deg/s)", "heading drift (deg)",
               "exit bank (deg)", "exit pitch (deg)"], rows)


def overbank(rep):
    rows = []
    for orient in ORIENTATIONS:
        from fvdtext import write_project
        path = write_project(PROJECTS / f"d-overbank-{orient}.fvd",
                             overbank_sections(climb=2.5, n=1.0, orientation=orient), speed=25.0)
        for bank in BANKS:
            prev = None
            for v in SPEEDS:
                m = Model(path)
                set_entry(m, v, bank)
                ob = Overbank(m, n=1.0, climb=2.5, entry_bank=bank)
                goals = [("heading", 180.0, 0.1), ("exit_pitch", 0.0, 0.1), ("exit_bank", 0.0, 0.1)]
                t0 = time.perf_counter()
                tries = []
                for start, label in (([140.0, 1.5, 140.0], "fixed"), (prev, "previous speed")):
                    if start is None:
                        continue
                    res = ob.problem(goals).solve(start, max_evals=80)
                    tries.append((label, res))
                    if res.converged:
                        break
                secs = time.perf_counter() - t0
                label, res = tries[-1]
                if res.converged:
                    prev = res.x
                val = res.values or {}
                p = val.get("pitches") or [None] * 3
                rows.append([orient, f(bank, 0), f(v, 0), "; ".join(f"{l}: {r.status} ({r.evals})" for l, r in tries),
                             f(secs, 2), f(res.x[0], 1), f(res.x[1], 3), f(res.x[2], 1),
                             f(0.8 * res.x[0] + bank, 0), f(val.get("max_bank"), 1),
                             f(val.get("heading", 0) - 180.0 if val else None, 2), f(val.get("exit_bank"), 2),
                             f(p[0], 0), f(p[1], 0), f(val.get("exit_height"), 1), f(val.get("exit_speed"), 1)])
                print(rows[-1], flush=True)
                m.close()
    rep("D2. Overbanked turn. Starts: R6-style fixed start (r 140 deg/s, hold 1.5 s, q 140 deg/s); if it fails, "
        "the solution at the previous entry speed (continuation). 'commanded bank' is the anchor roll plus the "
        "roll-in (0.8 r); 'world bank' the bank the program reports at the end of the roll-in. Errors are the "
        "solved end minus the goal.")
    rep.table(["orientation", "entry bank", "entry m/s", "solves (evaluations)", "s", "r (deg/s)", "hold (s)",
               "q (deg/s)", "commanded bank", "world bank at roll-in end", "heading error", "exit bank",
               "pitch end of S1", "pitch end of S2", "exit height (m)", "exit speed"], rows)


def main():
    rep = Report("trial_d")
    zero_g(rep)
    overbank(rep)
    rep.save()


if __name__ == "__main__":
    main()
