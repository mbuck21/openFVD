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
"""A force section in the vertical plane, step for step as the program does it.

A copy, in double precision and in two dimensions (no roll, no lateral force),
of secforced::updateSection() in core/secforced.cpp, time argument: one step
per millisecond, the direction turned by (n - cos(pitch)) * g / heart speed,
the rail point advanced by the speed, the speed taken from the energy at the
point 0.9 of the way from the heartline to the rail, friction charged on the
rail length run, drag as v^3/1000 * resistance per step.

Used two ways: to shape the test elements before the real program computes
them (make_shapes.py), and to redesign an element with a speed taken from a
train instead of from the point (redesign.py), which the program cannot do.
"""
import math

G = 9.80665
HZ = 1000


def cubic(x):
    return x * x * (3.0 - 2.0 * x)


class Force:
    """A normal-force curve made of cubic pieces: [(duration, change), ...]."""

    def __init__(self, start, steps):
        self.pieces = []
        t, v = 0.0, start
        for dur, ch in steps:
            self.pieces.append((t, t + dur, v, ch))
            t += dur
            v += ch
        self.total = t

    def __call__(self, t):
        for t0, t1, v, ch in self.pieces:
            if t <= t1:
                x = min(max((t - t0) / (t1 - t0), 0.0), 1.0)
                return v + ch * cubic(x)
        t0, t1, v, ch = self.pieces[-1]
        return v + ch


def run(force, v0, heart=1.1, friction=0.03, resistance=2e-5, pitch0=0.0,
        speed_of=None, keep=1):
    """Integrate one force section from a level (or pitched) start at (0, 0).

    force: a Force. speed_of: None for the program's energy rule, or a
    function of the rail distance run that gives the speed to use instead
    (a train's speed). Returns a list of nodes (dicts) every `keep` steps,
    always including the last."""
    th = math.radians(pitch0)
    hx, hy = 0.0, 0.0                       # heartline point
    nx, ny = math.sin(th), -math.cos(th)    # norm: rider toward rail
    v = v0
    y09 = hy + 0.9 * heart * ny
    E = 0.5 * v * v + G * y09
    L = 0.0          # rail length run (fTotalLength)
    Lh = 0.0         # heartline length run
    hdist = 0.0      # heart distance of the last step
    n_out = 1.0
    nodes = [dict(t=0.0, x=hx, y=hy, pitch=th, v=v, n=n_out, L=0.0, Lh=0.0,
                  E=E, rx=hx + heart * nx, ry=hy + heart * ny)]
    steps = int(force.total * HZ + 0.5)
    for i in range(steps):
        t = (i + 1) / HZ
        nF = force(t)
        nforce = (nF - math.cos(th)) * G
        est = hdist * HZ if hdist > 1e-12 else v
        th_new = th + nforce / HZ / est
        dxp, dyp = math.cos(th), math.sin(th)
        dxn, dyn = math.cos(th_new), math.sin(th_new)
        nxn, nyn = math.sin(th_new), -math.cos(th_new)
        # rail point advances by v/HZ along the mean direction
        hx_new = hx + v / (2 * HZ) * (dxn + dxp) + heart * (nx - nxn)
        hy_new = hy + v / (2 * HZ) * (dyn + dyp) + heart * (ny - nyn)
        rail_step = math.hypot(hx_new + heart * nxn - hx - heart * nx,
                               hy_new + heart * nyn - hy - heart * ny)
        hdist = math.hypot(hx_new - hx, hy_new - hy)
        L += rail_step
        Lh += hdist
        dth = th_new - th
        hx, hy, th, nx, ny = hx_new, hy_new, th_new, nxn, nyn
        if speed_of is None:
            E -= v * v * v / HZ * resistance
            y09 = hy + 0.9 * heart * ny
            v = math.sqrt(max(2.0 * (E - G * (y09 + L * friction)), 1e-9))
        else:
            v = speed_of(L)
        n_out = -ny + dth * hdist * HZ * HZ / G
        if (i + 1) % keep == 0 or i == steps - 1:
            nodes.append(dict(t=t, x=hx, y=hy, pitch=th, v=v, n=n_out, L=L,
                              Lh=Lh, E=E, rx=hx + heart * nx,
                              ry=hy + heart * ny))
    return nodes
