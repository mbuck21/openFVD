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
"""The cost of one solver evaluation against the length of the ride.

A track is a camelback (about 5,800 nodes) followed by level force sections held at
20 m/s, 5 s each, up to 6,000 / 15,000 / 50,000 / 150,000 nodes. One evaluation is:
one command (a transition length), a whole-track recompute, one sectionEnd. It is
timed for an edit in the first section (everything after it is recomputed) and in
the last one. Writes results/timing.txt. Run: py -3 docs/research/e1-trials/timing.py
"""
import time

from common import Report, WORK, f
from e1solver import Model
from elements import camelback_sections
from fvdtext import write_project, force

SIZES = [6000, 15000, 50000, 150000]
REPEATS = 20


def build(nodes):
    sections = camelback_sections(Ga=0.0, t2=0.438, tair=4.092)
    total = 5800
    while total + 2500 <= nodes:
        sections.append(force(f"run {len(sections)}", [("quartic", 5.0, 0.0)], fixed_speed=20.0))
        total += 5000
    return write_project(WORK / f"timing-{nodes}.fvd", sections, speed=30.0), len(sections)


def main():
    rep = Report("timing")
    rows = []
    for size in SIZES:
        path, nsec = build(size)
        m = Model(path)
        last = m.end(nsec)["node"]
        for where, sec, tr in (("first section", 1, 3), ("last section", nsec, 1)):
            base = 0.438 if sec == 1 else (0.4 if nsec <= 3 else 5.0)
            if sec == nsec and nsec == 3:
                tr = 3
                base = 1.0
            s0 = m.session.stats()
            t0 = time.perf_counter()
            for k in range(REPEATS):
                m.set("transition.setLength", {"section": sec, "function": "normal", "transition": tr},
                      length=base + 0.001 * (k + 1))
                m.recompute()
                m.end(nsec)
            wall = (time.perf_counter() - t0) / REPEATS * 1000
            s1 = m.session.stats()
            run_ms = (s1["run_ms"] - s0["run_ms"]) / REPEATS
            rec_ms = (s1["recompute_ms"] - s0["recompute_ms"]) / REPEATS
            t0 = time.perf_counter()
            for k in range(REPEATS):
                m.end(nsec)
            se = (time.perf_counter() - t0) / REPEATS * 1000
            t0 = time.perf_counter()
            for k in range(REPEATS):
                m.measure(["height"], 100 + k)
            me = (time.perf_counter() - t0) / REPEATS * 1000
            rows.append([last, nsec, where, f(run_ms, 1), f(rec_ms, 1), f(wall, 1), f(se, 2), f(me, 2)])
            print(rows[-1], flush=True)
        m.close()
    rep("Cost of one evaluation (one command, a whole-track recompute, one sectionEnd), mean of "
        f"{REPEATS}. 'run' is the program's time in the command (its own recompute from the edited point "
        "and the undo step's two snapshots of the track); 'recompute' the whole-track recompute; 'wall' "
        "everything as Python sees it, including the pipe. The last two columns: one sectionEnd and one "
        "measures request alone (wall ms), the reads a goal costs on top.")
    rep.table(["nodes", "sections", "edit in", "run ms", "recompute ms", "wall ms per evaluation",
               "sectionEnd ms", "measures ms"], rows)
    rep.save()


if __name__ == "__main__":
    main()
