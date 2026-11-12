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
"""Trial A: aim one section (R5 C1's trial) and the camelback map (R6 E1).

A1  a force section of a baseline project, two transition values freed, solved for
    end height and heading; then a hundred random reachable goals and twenty random
    goals that may not be reachable.
A2  the camelback of R6 section 2.2 from R6's six starting guesses (entry 30 m/s,
    airtime 0 g, crest 38 m).
A3  the camelback map: entry 15 to 40 m/s, airtime +0.2 to -1.0 g, crest 25 m; per cell
    the scan for a valid start, Newton from the scan, from R6's fixed start and from
    the six guesses.
Writes results/trial_a.txt. Run: py -3 docs/research/e1-trials/trial_a.py [--quick]
"""
import random
import sys
import time

from common import Report, PROJECTS, REPO, program_ms, f
from e1solver import Model, newton, wrap180
from elements import Camelback, camelback_sections, T2_GRID
from fvdtext import write_project

QUICK = "--quick" in sys.argv
SIX_STARTS = [(0.5, 2.0), (0.2, 4.0), (1.0, 1.0), (0.1, 8.0), (2.0, 3.0), (0.6, 0.5)]


# ---- A1 -------------------------------------------------------------------------------------
def aim_section(rep):
    project = REPO / "samples" / "baseline" / "gen-normal-shapes.fvd"
    sec = 3
    m = Model(project)
    tn = {"section": sec, "function": "normal", "transition": 5}
    tl = {"section": sec, "function": "lateral", "transition": 1}
    x_orig = [-0.2, 0.0]
    m.recompute()
    e0 = m.end(sec)
    nodes = e0["node"]
    rep(f"A1. gen-normal-shapes.fvd, section {sec} (force, time, {e0['node'] - m.end(sec - 1)['node']} nodes; "
        f"track {m.end(6)['node']} nodes). Free: normal-force transition 5 change (now -0.2 g) and "
        f"lateral-force transition 1 change (now 0 g, a quartic bump). Goals: end height, end heading. "
        f"Tolerances 0.01 m and 0.05 deg. Start: the project as saved.")
    rep(f"As saved the section ends at height {e0['y']:.3f} m, heading {e0['heading_deg']:.3f} deg, "
        f"pitch {e0['pitch_deg']:.2f} deg, speed {e0['speed_mps']:.2f} m/s.")

    def setx(x):
        m.set("transition.setChange", tn, change=round(x[0], 6))
        m.set("transition.setChange", tl, change=round(x[1], 6))
        m.recompute()

    def make_eval(target):
        def ev(x):
            setx(x)
            e = m.end(sec)
            if e["non_finite"]:
                return None
            h = wrap180(e["heading_deg"] - e0["heading_deg"])
            return [e["y"] - target[0], h - target[1]], e
        return ev

    lo, hi, hstep, tol = [-2.0, -1.5], [2.0, 1.5], [0.01, 0.01], [0.01, 0.05]

    def solve(target):
        ms0, _ = program_ms(m)
        res = newton(make_eval(target), list(x_orig), lo, hi, hstep, tol, max_evals=60, max_step=[0.5, 0.5])
        ms1, _ = program_ms(m)
        return res, ms1 - ms0

    # one named goal
    target = (e0["y"] - 1.0, 10.0)
    res, pms = solve(target)
    rep(f"Goal: end 1.0 m lower and heading +10 deg: {res.status} in {res.evals} evaluations "
        f"({res.iters} Newton steps), {res.seconds * 1000:.0f} ms wall ({pms:.0f} ms in the program); "
        f"normal change {res.x[0]:.4f} g, lateral bump {res.x[1]:.4f} g; miss {res.residual[0] * 100:.2f} cm, "
        f"{res.residual[1]:.3f} deg; condition number at the start {res.cond:.1f}.")
    rep("")

    # a hundred reachable goals: random unknowns, the goals they give, solved from the saved values
    rng = random.Random(20261111)
    rows, stats = [], []
    n_goals = 30 if QUICK else 100
    for k in range(n_goals):
        xr = [rng.uniform(-1.2, 0.8), rng.uniform(-1.0, 1.0)]
        setx(xr)
        e = m.end(sec)
        if e["non_finite"]:
            stats.append(("stall at the drawn values", 0, 0.0, 0.0))
            continue
        goal = (e["y"], wrap180(e["heading_deg"] - e0["heading_deg"]))
        res, pms = solve(goal)
        stats.append((res.status, res.evals, res.seconds, pms))
    conv = [s for s in stats if s[0] == "converged"]
    evs = sorted(s[1] for s in conv)
    secs = sorted(s[2] for s in conv)
    rep(f"A1b. {n_goals} random reachable goals (unknowns drawn uniformly, normal -1.2..0.8 g, lateral -1..1 g, "
        f"seed 20261111; each solved from the saved values):")
    by = {}
    for s in stats:
        by[s[0]] = by.get(s[0], 0) + 1
    rep("  outcome counts: " + ", ".join(f"{k} {v}" for k, v in sorted(by.items())))
    if conv:
        rep(f"  converged: evaluations median {evs[len(evs) // 2]}, max {evs[-1]}; wall time median "
            f"{secs[len(secs) // 2] * 1000:.0f} ms, max {secs[-1] * 1000:.0f} ms; program time per evaluation "
            f"{sum(s[3] for s in conv) / sum(s[1] for s in conv):.1f} ms")
    # twenty goals drawn in a box that is partly out of reach
    out = {}
    for k in range(20):
        goal = (e0["y"] + rng.uniform(-30, 15), rng.uniform(-60, 60))
        res, pms = solve(goal)
        out.setdefault(res.status, []).append((goal[0] - e0["y"], goal[1], res.evals,
                                               abs(res.residual[0]) if res.residual else None,
                                               abs(res.residual[1]) if res.residual else None))
    rep("A1c. 20 goals drawn in a box (height -30..+15 m from the saved end, heading -60..+60 deg), partly out of reach:")
    for k, v in sorted(out.items()):
        worst = [r for r in v if r[3] is not None]
        rep(f"  {k}: {len(v)}; evaluations {sorted(r[2] for r in v)}" +
            (f"; closest misses (m, deg) " + ", ".join(f"({r[3]:.2f}, {r[4]:.2f})" for r in worst[:6])
             if k != "converged" else ""))
    rep("")
    m.close()


# ---- the camelback -----------------------------------------------------------------------------
class CamelbackProblem:
    def __init__(self, model, Ga, crest):
        self.m = model
        self.cb = Camelback(model, 0, Ga=Ga)
        self.cb.set_forces(Ga=Ga)
        self.crest = crest

    def read_at(self, t2, tair):
        self.cb.set_t2(t2)
        self.cb.set_tair(tair)
        self.m.recompute()
        return self.cb.read()

    def ev2(self, x):
        r = self.read_at(x[0], x[1])
        if r is None:
            return None
        return [r["exit_pitch"], r["crest"] - self.crest], r

    def solve2(self, x0, max_evals=60):
        return newton(self.ev2, list(x0), [0.05, 0.01], [6.0, 12.0], [0.01, 0.01], [0.05, 0.05],
                      max_evals=max_evals, max_step=[0.5, 2.0])


def six_starts(rep, project):
    m = Model(project)
    m.set("anchor.setSpeed", {}, speed=30.0)
    p = CamelbackProblem(m, 0.0, 38.0)
    rows = []
    for x0 in SIX_STARTS:
        res = p.solve2(x0)
        v = res.values or {}
        rows.append([f"{x0[0]}, {x0[1]}", res.status, res.evals, f(res.x[0], 3) if v else "",
                     f(res.x[1], 3) if v else "", f(v.get("exit_pitch")), f(v.get("crest")),
                     f(v.get("exit_height")), f(v.get("exit_speed")), f(res.seconds * 1000, 0)])
    rep("A2. Camelback, entry 30 m/s, airtime 0.0 g, crest 38 m above the entry, R6's six starting guesses "
        "(R6 Part 3, A1). Unknowns t2 (= t4) and t_air; goals exit level (unwrapped) and crest height; "
        "tolerances 0.05 deg and 0.05 m.")
    rep.table(["start t2, t_air (s)", "result", "evaluations", "t2 (s)", "t_air (s)", "exit pitch (deg)",
               "crest (m)", "exit height (m)", "exit speed (m/s)", "wall ms"], rows)
    m.close()


def camelback_map(rep, project):
    speeds = [15, 20, 25, 30, 35, 40]
    airs = [0.2, 0.0, -0.2, -0.4, -0.6, -0.8, -1.0]
    if QUICK:
        speeds, airs = [25, 30], [0.0, -0.6]
    t2_grid = T2_GRID
    if QUICK:
        t2_grid = t2_grid[::3]
    m = Model(project)
    cells = {}
    detail = []
    for v in speeds:
        m.set("anchor.setSpeed", {}, speed=float(v))
        for ga in airs:
            p = CamelbackProblem(m, ga, 25.0)
            t0 = time.perf_counter()
            ms0, _ = program_ms(m)
            # scan t2, solving t_air for a level exit at each (elements.Camelback.scan)
            scan, scan_evals = p.cb.scan(t2_grid)
            scan_s = time.perf_counter() - t0
            scan_ms = program_ms(m)[0] - ms0
            valid = [s for s in scan if s[2] is not None]
            # solutions: sign changes of crest - 25 between neighbouring valid points
            nsol = 0
            for a, b in zip(scan, scan[1:]):
                if a[2] is not None and b[2] is not None and (a[2] - 25.0) * (b[2] - 25.0) <= 0:
                    nsol += 1
            rng = (min(s[2] for s in valid), max(s[2] for s in valid)) if valid else None
            # Newton from R6's fixed start, from the scan's nearest point, and from the six guesses
            fixed = p.solve2((0.5, 2.0))
            from_scan = None
            if valid:
                best = min(valid, key=lambda s: abs(s[2] - 25.0))
                from_scan = p.solve2((best[0], best[1]))
            six = [p.solve2(x0) for x0 in SIX_STARTS]
            six_ok = sum(1 for r in six if r.converged)
            sol = from_scan if (from_scan and from_scan.converged) else next((r for r in [fixed] + six if r.converged), None)
            cells[(v, ga)] = (nsol, rng, fixed, from_scan, six_ok)
            detail.append([v, f"{ga:+.1f}", len(valid), f"{rng[0]:.1f} to {rng[1]:.1f}" if rng else "none", nsol,
                           scan_evals, f"{scan_s:.2f}", f"{scan_ms / max(scan_evals, 1):.1f}",
                           f"{fixed.status} ({fixed.evals})",
                           (f"{from_scan.status} ({from_scan.evals}, {from_scan.seconds * 1000:.0f} ms)" if from_scan else "no start"),
                           six_ok,
                           f(sol.x[0], 3) if sol else "", f(sol.x[1], 3) if sol else "",
                           f(sol.values["exit_height"]) if sol else "", f(sol.values["exit_speed"]) if sol else "",
                           f(sol.values["exit_force"] - 1.0, 4) if sol else "",
                           f(sol.values["crest_start_force"] - ga, 4) if sol else ""])
            print(detail[-1], flush=True)
    m.close()

    rep("A3. Camelback map on the real model: crest 25 m above the entry, exit level. Each cell: number of "
        "solutions found by the scan (t2 from 0.05 to 1.95 s in 0.1 s steps and 2.2 to 3.95 s in 0.25 s steps, t_air solved for a level exit at each, with a bracketing grid of t_air from 0.25 to 8 s when continuation fails) / "
        "the reachable crest range (m) / Newton from R6's fixed start (0.5 s, 2.0 s) / Newton from the scan's point "
        "nearest 25 m (evaluations, or the failure).")
    header = ["entry (m/s)"] + [f"{a:+.1f} g" for a in airs]
    rows = []
    for v in speeds:
        row = [v]
        for ga in airs:
            nsol, rng, fixed, from_scan, six_ok = cells[(v, ga)]
            if rng is None:
                row.append("no valid hill")
                continue
            fs = str(fixed.evals) if fixed.converged else "fail"
            sc = str(from_scan.evals) if (from_scan and from_scan.converged) else "fail"
            row.append(f"{nsol} / {rng[0]:.0f}-{rng[1]:.0f} / {fs} / {sc}")
        rows.append(row)
    rep.table(header, rows)
    rep("A3 in detail (scan evaluations, scan wall time, program ms per evaluation of the scan; the solution's "
        "t2, t_air, exit height and speed; exit force drift = exit normal force - 1 g; crest force drift = force "
        "measured at the start of the crest section - the airtime force asked for; 'six' = how many of R6's six "
        "starting guesses converged):")
    rep.table(["entry", "airtime", "valid scan points", "crest range (m)", "solutions", "scan evals", "scan s",
               "ms/eval", "fixed start", "from scan", "six", "t2", "t_air", "exit h (m)", "exit v (m/s)",
               "exit force drift (g)", "crest force drift (g)"], detail)


def main():
    rep = Report("trial_a" + ("_quick" if QUICK else ""))
    aim_section(rep)
    project = write_project(PROJECTS / "a-camelback.fvd", camelback_sections(Ga=0.0), speed=30.0)
    six_starts(rep, project)
    camelback_map(rep, project)
    rep.save()


if __name__ == "__main__":
    main()
