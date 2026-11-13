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
"""Trial B: the exit contract (R6 E2) on the real model.

Layout (projects/b-baseline.fvd): the top of the lift (anchor, 46 m, 5 m/s), the first
drop (solved to a level valley at height 0), camelback E1 (0.0 g airtime), camelback E2
(-0.6 g), a banked turn, a 30 m run-out. Each element's contract: exit level at height
0 with its crest at a stated height (unknowns t2, t_air, t4). Changes: lift +1 m and
-1 m, the first drop 1 m deeper (same lift), the start speed +3 %, and a start speed that
makes the speed at the bottom of the first drop 3 % higher (R6's energy change). In every
change but "1 m deeper" the first drop is re-solved to keep its valley on the ground, as
a designer would. Cases: nothing re-solves; E1
re-solves; E1 and E2 re-solve; both re-solve keeping exit speed instead of exit height.
The knock-on is `fvdcore compare` of the baseline and the changed project.
Writes results/trial_b.txt. Run: py -3 docs/research/e1-trials/trial_b.py
"""
import subprocess
import time

from common import Report, PROJECTS, WORK, f
from e1solver import Model
from elements import Camelback, camelback_sections, solve_with_scan
from fvdtext import write_project, straight, FVDCORE
from layouts import drop_section, turn_section, Drop, raise_start

TOP = 46.0
E1_CREST, E2_CREST = 36.0, 26.0
SECTIONS = ["drop", "E1 up", "E1 crest", "E1 down", "E2 up", "E2 crest", "E2 down", "turn", "run-out"]


def build_baseline(rep):
    secs = ([drop_section()] + camelback_sections(Ga=0.0, prefix="E1") +
            camelback_sections(Ga=-0.6, prefix="E2") + [turn_section(), straight("run-out", 30)])
    path = write_project(WORK / "b-start.fvd", secs, speed=5.0, y=TOP)
    m = Model(path)
    t0 = time.perf_counter()
    d = Drop(m, 1)
    rd = d.problem(0.0).solve([1.2, 0.6])
    e1 = Camelback(m, 1, Ga=0.0)
    g1 = [("exit_pitch", 0.0, 0.05), ("crest_abs", E1_CREST, 0.05), ("exit_abs", 0.0, 0.05)]
    r1, n1, _ = solve_with_scan(e1, g1, ("t2", "tair", "t4"), key="crest_abs", target=E1_CREST)
    e2 = Camelback(m, 4, Ga=-0.6)
    e2.set_forces(Ga=-0.6)
    g2 = [("exit_pitch", 0.0, 0.05), ("crest_abs", E2_CREST, 0.05), ("exit_abs", 0.0, 0.05)]
    r2, n2, _ = solve_with_scan(e2, g2, ("t2", "tair", "t4"), key="crest_abs", target=E2_CREST)
    secs_ = time.perf_counter() - t0
    rep(f"Baseline: first drop {rd.status} in {rd.evals} evaluations (0 g hold {rd.x[0]:.3f} s, valley hold "
        f"{rd.x[1]:.3f} s, valley speed {rd.values['exit_speed']:.2f} m/s); E1 {r1.status} in {n1} evaluations "
        f"including the scan (t2 {r1.x[0]:.3f}, t_air {r1.x[1]:.3f}, t4 {r1.x[2]:.3f} s); E2 {r2.status} in {n2} "
        f"(t2 {r2.x[0]:.3f}, t_air {r2.x[1]:.3f}, t4 {r2.x[2]:.3f} s); {secs_:.1f} s in all.")
    out = PROJECTS / "b-baseline.fvd"
    m.session.save(out, force=True)
    base = {"drop": rd.x, "e1": r1.x, "e2": r2.x, "v1": r1.values["exit_speed"], "v2": r2.values["exit_speed"]}
    m.close()
    return out, base


def compare(a, b):
    out = subprocess.run([str(FVDCORE), "compare", str(a), str(b), "--tsv"], capture_output=True, text=True)
    rows = {}
    for line in out.stdout.splitlines()[1:]:
        p = line.split("\t")
        rows[p[0]] = [float(x) if x else None for x in p[1:]]
    return rows


def main():
    rep = Report("trial_b")
    base_path, base = build_baseline(rep)
    m0 = Model(base_path)
    m0.recompute()
    ref = {s: m0.end(s) for s in (1, 4, 7, 9)}
    m0.close()
    rep(f"Baseline ends: valley {ref[1]['speed_mps']:.2f} m/s; E1 exit {ref[4]['speed_mps']:.2f} m/s at x "
        f"{ref[4]['x']:.1f} m; E2 exit {ref[7]['speed_mps']:.2f} m/s at x {ref[7]['x']:.1f} m; track end x "
        f"{ref[9]['x']:.1f}, z {ref[9]['z']:.1f} m, heading {ref[9]['heading_deg']:.1f} deg.")
    rep("")

    v_bottom = ref[1]["speed_mps"]
    # the start speed that gives 3 % more at the bottom of the first drop (same energy line otherwise)
    v_launch = (5.0 ** 2 + (1.03 ** 2 - 1.0) * v_bottom ** 2) ** 0.5
    # every change keeps the valley of the first drop on the ground except "1 m deeper"
    changes = [
        ("lift +1 m", lambda m: (raise_start(m, 1.0, TOP), 0.0)),
        ("lift -1 m", lambda m: (raise_start(m, -1.0, TOP), 0.0)),
        ("first drop 1 m deeper", lambda m: (None, -1.0)),
        ("start speed +3 %", lambda m: (m.set("anchor.setSpeed", {}, speed=5.15), 0.0)),
        (f"start speed {v_launch:.2f} m/s (+3 % at the valley)", lambda m: (m.set("anchor.setSpeed", {}, speed=round(v_launch, 4)), 0.0)),
    ]
    cases = ["nothing re-solves", "E1 re-solves", "E1 and E2 re-solve", "both keep exit speed"]
    rows = []
    full = []
    for cname, change in changes:
        for case in cases:
            m = Model(base_path)
            _, valley = change(m)
            drop_evals = 0
            if valley is not None:
                rd = Drop(m, 1).problem(valley).solve(base["drop"])
                drop_evals = rd.evals
            m.recompute()
            evals, ok = 0, []
            t0 = time.perf_counter()
            if case != "nothing re-solves":
                e1 = Camelback(m, 1, Ga=0.0)
                keep = ("exit_speed", base["v1"], 0.01) if case == "both keep exit speed" else ("exit_abs", 0.0, 0.05)
                g1 = [("exit_pitch", 0.0, 0.05), ("crest_abs", E1_CREST, 0.05), keep]
                r1, n1, sc1 = solve_with_scan(e1, g1, ("t2", "tair", "t4"), x0=base["e1"], key="crest_abs", target=E1_CREST)
                evals += n1
                ok.append(r1 is not None and r1.converged)
                if case != "E1 re-solves":
                    e2 = Camelback(m, 4, Ga=-0.6)
                    keep = ("exit_speed", base["v2"], 0.01) if case == "both keep exit speed" else ("exit_abs", 0.0, 0.05)
                    g2 = [("exit_pitch", 0.0, 0.05), ("crest_abs", E2_CREST, 0.05), keep]
                    r2, n2, sc2 = solve_with_scan(e2, g2, ("t2", "tair", "t4"), x0=base["e2"], key="crest_abs", target=E2_CREST)
                    evals += n2
                    ok.append(r2 is not None and r2.converged)
            secs = time.perf_counter() - t0
            m.recompute()
            ends = {s: m.end(s) for s in (4, 7, 9)}
            out = WORK / f"b-{len(rows)}.fvd"
            m.session.save(out, force=True)
            m.close()
            c = compare(base_path, out)
            e1d, e2d, endd = c["section 4"], c["section 7"], c["track end"]
            rows.append([cname if case == cases[0] else "", case,
                         f(ends[4]["x"] - ref[4]["x"]), f(ends[4]["y"] - ref[4]["y"]), f(e1d[2]), f(e1d[4], 3),
                         f(ends[7]["x"] - ref[7]["x"]), f(ends[7]["y"] - ref[7]["y"]), f(e2d[2]), f(e2d[4], 3),
                         f(endd[0]), f(ends[9]["y"] - ref[9]["y"]), f(endd[1]), f(endd[2]), f(endd[4], 3),
                         f"{evals}" + ("" if all(ok) else " (not all converged)"), f"{secs:.2f}", drop_evals])
            full.append((cname, case, c))
            print(rows[-1], flush=True)
    rep("Knock-on against the baseline (changed minus baseline). E1, E2: the element's exit (end of its drop "
        "section); dx along the first heading, dy height, d pitch in degrees, dv speed in m/s. End: the end of the "
        "track, distance between the two end points (m), dy, d heading and d pitch (deg), dv. 'evaluations' are "
        "the elements' re-solves, each started from the baseline solution; 'drop evals' the first drop's re-solve "
        "that is part of the change.")
    rep.table(["change", "case", "E1 dx", "E1 dy", "E1 dpitch", "E1 dv", "E2 dx", "E2 dy", "E2 dpitch", "E2 dv",
               "end distance", "end dy", "end dheading", "end dpitch", "end dv", "evaluations", "solve s",
               "drop evals"], rows)
    rep("`fvdcore compare --tsv` for every case (distance, d heading, d pitch, d bank, d speed per section end):")
    for cname, case, c in full:
        rep(f"- {cname}, {case}: " + "; ".join(
            f"{k} {v[0]:.2f} m {v[1]:+.2f}/{v[2]:+.2f}/{v[3]:+.2f} deg {v[4]:+.3f} m/s"
            for k, v in c.items() if k.startswith("section") or k == "track end"))
    rep.save()


if __name__ == "__main__":
    main()
