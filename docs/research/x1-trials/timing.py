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
"""X1: what one round costs in the program, on rides of normal length.

    py -3 docs/research/x1-trials/timing.py

One round of "design for a car" inside the program would be: recompute the
track with the speed of the last ride (the cost of today's whole-track
recompute), then ride the train over it (the cost of `fvdcore ride`'s own
computation). Both are measured here on the program, not on the trial's
Python stand-in:
  - the ride: wall time of `fvdcore ride` with R7's 9-car train, less the wall
    time of `fvdcore nodes --every-m 1000` on the same project (which loads
    and computes the project and prints almost nothing); best of 15 runs each.
    This is crude: the difference is a few milliseconds against about 120 ms
    of starting the program, so it is good to about +-10 ms;
  - the recompute: `fvdcore session`, 20 whole-track `recompute` requests,
    the program's own milliseconds from `stats`.
Projects: samples/baseline/gen-normal-shapes.fvd (14,124 nodes, the size of
a normal ride's track in E1) and R7's camelback. (R7's layout was tried and
left out: `fvdcore nodes` alone takes about 2 s on it, so the ride's few
milliseconds were lost in the noise.) Run it alone (not beside another build or trial). Writes
results/timing.txt; the numbers vary from machine to machine and run to run.
"""
import subprocess
import sys
import time

import x1lib as X

sys.path.insert(0, str(X.REPO / 'tools'))
from fvdsession import Session  # noqa: E402


def wall(args, runs=15):
    best = 1e9
    for _ in range(runs):
        t0 = time.perf_counter()
        subprocess.run([str(X.EXE)] + args, check=True, capture_output=True)
        best = min(best, time.perf_counter() - t0)
    return best


def node_count(project):
    out = X.run_core('nodes', str(project), '--every-m', '100000')
    return int(out.splitlines()[-1].split('\t')[0]) + 1


def recompute_ms(project, n=20):
    with Session(str(project), exe=str(X.EXE)) as s:
        s.recompute()
        before = s.stats()['recompute_ms']
        for _ in range(n):
            s.recompute()
        return (s.stats()['recompute_ms'] - before) / n


def main():
    X.need_exe()
    rep = X.Report('timing')
    rep('# X1: the cost of one round in the program')
    rep('')
    rows = []
    for project in (X.REPO / 'samples' / 'baseline' / 'gen-normal-shapes.fvd',
                    X.R7 / 'projects' / 'camelback.fvd'):
        nodes = node_count(project)
        base = wall(['nodes', str(project), '--every-m', '1000'])
        r1 = wall(['ride', str(project)])
        r9 = wall(['ride', str(project), '--cars', str(X.CARS), '--pitch', str(X.PITCH)])
        rc = recompute_ms(project)
        ride_ms = 1000.0 * (r9 - base)
        rows.append([project.name, '{:,}'.format(nodes), '%.0f' % (1000 * base),
                     '%.0f' % (1000.0 * (r1 - base)), '%.0f' % ride_ms, '%.1f' % rc,
                     '%.0f' % (rc + ride_ms)])
    rep('Milliseconds. "load": `fvdcore nodes --every-m 1000` (start, load, compute, '
        'print almost nothing). "ride": `fvdcore ride` less "load", with the point and '
        'with %d cars %.1f m apart. "recompute": the program\'s own time for a whole-track '
        'recompute in `fvdcore session`. One round = recompute + 9-car ride.'
        % (X.CARS, X.PITCH))
    rep('')
    rep.table(['project', 'nodes', 'load', 'ride, point', 'ride, 9 cars',
               'recompute', 'one round'], rows)
    rep.save()


if __name__ == '__main__':
    main()
