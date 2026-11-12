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
"""Trial C: limits as inputs (R6 E3) on the real model.

Every transition of the camelback is as short as an onset limit J allows: k * dG / J,
with k = 1.5 for a cubic and 1.875 for a quintic (their steepest slopes). The durations
are then not free, so the valley force Gp and the crest hold t_air are the unknowns;
goals: exit level, crest 25 m above the entry. Start: a scan of Gp (1.5 to 5.0 g in
0.25 g steps, t_air solved for a level exit at each), then Newton. For each solved
element the project is saved and `fvdcore measures --all` gives the force at every node:
the program's own onset (`jerkNormal`, over 20 ms) and the onset over a 0.1 s window
(the F2291-style window) are compared with J.
Writes results/trial_c.txt. Run: py -3 docs/research/e1-trials/trial_c.py
"""
import subprocess
import time

from common import Report, PROJECTS, WORK, f
from e1solver import Model, Problem, Unknown
from elements import Camelback, camelback_sections
from fvdtext import write_project, FVDCORE

K = {"cubic": 1.5, "quintic": 1.875}
TH = 0.4
CREST = 25.0


def ramps(gp, ga, J, shape):
    k = K[shape]
    t1 = max(k * (gp - 1.0) / J, 0.01)
    t2 = max(k * (gp - ga) / J, 0.01)
    return t1, t2, t2, t1


def onsets(path, n0, n1):
    out = subprocess.run([str(FVDCORE), "measures", str(path), "--all", "--ids", "forceNormal,jerkNormal"],
                         capture_output=True, text=True).stdout.splitlines()
    head = out[0].split("\t")
    i_f, i_j = head.index("forceNormal"), head.index("jerkNormal")
    fn, jn = [], []
    for line in out[1:]:
        p = line.split("\t")
        fn.append(float(p[i_f]))
        jn.append(float(p[i_j]))
    jmax = max(abs(v) for v in jn[n0 + 1:n1])
    w = 50  # 0.1 s window, centred: (F(i+50) - F(i-50)) / 0.1 s
    j01 = max(abs(fn[i + w] - fn[i - w]) / 0.1 for i in range(n0 + w, n1 - w))
    return jmax, j01


def main():
    rep = Report("trial_c")
    rows = []
    for shape in ("cubic", "quintic"):
        path = write_project(PROJECTS / f"c-camelback-{shape}.fvd",
                             camelback_sections(Ga=0.0, shape=shape), speed=30.0)
        for ga in (0.0, -0.6):
            for v in (30.0, 40.0):
                for J in (2.0, 3.0, 5.0, 10.0):
                    m = Model(path)
                    m.set("anchor.setSpeed", {}, speed=v)
                    cb = Camelback(m, 0, Ga=ga)
                    t0 = time.perf_counter()

                    def set_gp(gp):
                        ok = cb.set_forces(Ga=ga, Gp=gp)
                        return ok and cb.set_ramp_lengths(*ramps(gp, ga, J, shape))

                    u_gp = Unknown("Gp", set_gp, 1.2, 6.0, 0.01, 0.5)
                    u_air = Unknown("tair", cb.set_tair, 0.01, 12.0, 0.01, 2.0)
                    # scan Gp with t_air solved for a level exit
                    scan, evals, prev = [], 0, None
                    for i in range(15):
                        gp = 1.5 + 0.25 * i
                        set_gp(gp)
                        p1 = Problem(m, [u_air], [("exit_pitch", 0.0, 0.05)], cb.read)
                        res = None
                        for s in ([prev] if prev is not None else []) + [2.0, 0.5, 4.0, 8.0]:
                            r = p1.solve([s], max_evals=25)
                            evals += r.evals
                            if r.converged:
                                res = r
                                break
                        if res is not None:
                            prev = res.x[0]
                            scan.append((gp, res.x[0], res.values["crest"]))
                        else:
                            prev = None
                    status, sol = "no valid hill", None
                    if scan:
                        best = min(scan, key=lambda s: abs(s[2] - CREST))
                        p2 = Problem(m, [u_gp, u_air], [("exit_pitch", 0.0, 0.05), ("crest", CREST, 0.05)], cb.read)
                        sol = p2.solve([best[0], best[1]], max_evals=60)
                        evals += sol.evals
                        status = sol.status
                        lo_c, hi_c = min(s[2] for s in scan), max(s[2] for s in scan)
                    secs = time.perf_counter() - t0
                    row = [shape, f"{ga:+.1f}", f(v, 0), f(J, 0), status, evals, f(secs, 1),
                           f"{lo_c:.0f}-{hi_c:.0f}" if scan else ""]
                    if sol is not None and sol.converged:
                        val = sol.values
                        gp, tair = sol.x
                        t1, t2, _, _ = ramps(gp, ga, J, shape)
                        m.recompute()
                        out = WORK / "c-solved.fvd"
                        m.session.save(out, force=True)
                        jmax, j01 = onsets(out, 0, val["ends"][2]["node"])
                        row += [f(gp), f(tair), f(t1), f(t2), f(val["length"], 1),
                                f(val["exit_x"] - 0.0, 1), f(val["exit_height"]), f(jmax, 2), f(j01, 2),
                                f"{(jmax / J - 1) * 100:+.0f} %", f"{(j01 / J - 1) * 100:+.0f} %"]
                    else:
                        row += [""] * 11
                    rows.append(row)
                    print(row, flush=True)
                    m.close()
    rep("Trial C (R6 E3 on the real model). Valley hold 0.4 s; crest 25 m above the entry; exit level. "
        "'evals' includes the scan. Onset: the program's jerkNormal (largest over the element, a 20 ms "
        "difference) and the largest change of the normal force over a 0.1 s window, against J = k dG / T. "
        "Span is the horizontal distance from entry to exit; length is along the centre line.")
    rep.table(["shape", "airtime g", "entry m/s", "J g/s", "result", "evals", "s", "crest range of scan (m)",
               "Gp (g)", "t_air (s)", "t1 (s)", "t2 (s)", "length (m)", "span (m)", "exit height (m)",
               "jerkNormal max (g/s)", "0.1 s onset (g/s)", "jerkNormal vs J", "0.1 s vs J"], rows)
    rep.save()


if __name__ == "__main__":
    main()
