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
"""Make the three test tracks of R7 as FVD++ projects and dump them.

    py -3 make_shapes.py                  # tune, write projects/*.fvd
    py -3 make_shapes.py --dump <fvdcore> # also run fvdcore nodes on them and
                                          # write shapes/*.tsv.gz

The camelback and the loop are tuned with fvd2d.py (the program's own step,
in the vertical plane) so that they are symmetric and exit level; the layout
is written with fixed numbers. The projects are then computed by the real
program (fvdcore nodes, anchor frame, one point every 0.1 m of centre line),
and only those dumps are used by the analysis. The dumps are kept in the
repository so the analysis runs without a build.
"""
import gzip
import json
import math
import subprocess
import sys
from pathlib import Path

import fvd2d
import fvdwrite as w

HERE = Path(__file__).resolve().parent
PROJ = HERE / 'projects'
SHAPES = HERE / 'shapes'

HEART = 1.1
FRICTION = 0.03
RESISTANCE = 2e-5


def newton(fun, x, goals, h=0.01, tol=0.02, iters=30):
    """Damped Newton on len(x) unknowns with finite differences.

    The tolerance (0.02 degrees or metres) is about what one millisecond
    node moves; a section's length is a whole number of nodes."""
    x = list(x)
    for _ in range(iters):
        r = [a - b for a, b in zip(fun(x), goals)]
        if max(abs(e) for e in r) < tol:
            return x
        n = len(x)
        J = [[0.0] * n for _ in range(n)]
        for j in range(n):
            xp = list(x)
            xp[j] += h
            rp = [a - b for a, b in zip(fun(xp), goals)]
            for i in range(n):
                J[i][j] = (rp[i] - r[i]) / h
        if n == 1:
            dx = [-r[0] / J[0][0]]
        else:
            det = J[0][0] * J[1][1] - J[0][1] * J[1][0]
            dx = [-(J[1][1] * r[0] - J[0][1] * r[1]) / det,
                  -(-J[1][0] * r[0] + J[0][0] * r[1]) / det]
        lam = 1.0
        while max(abs(d) * lam for d in dx) > 0.3:
            lam *= 0.5
        x = [a + lam * d for a, d in zip(x, dx)]
    raise RuntimeError('no convergence')


def end(steps, v0, start=1.0):
    nodes = fvd2d.run(fvd2d.Force(start, steps), v0, HEART, FRICTION,
                      RESISTANCE, keep=10)
    return nodes


# ---- camelback ------------------------------------------------------------
CB = dict(v0=30.0, approach=1.5, pull=0.7, hold=0.4, valley=3.5, crest=-0.2,
          height=30.0, runout=1.5)


def camel_steps(t2, tair, t4):
    c = CB
    return [(c['approach'], 0.0), (c['pull'], c['valley'] - 1.0),
            (c['hold'], 0.0), (t2, c['crest'] - c['valley']), (tair, 0.0),
            (t4, c['valley'] - c['crest']), (c['hold'], 0.0),
            (c['pull'], 1.0 - c['valley']), (c['runout'], 0.0)]


def tune_camelback():
    c = CB

    def half(x):
        t2, th = x
        st = camel_steps(t2, th, 0.5)[:5]
        nodes = end(st, c['v0'])
        top = nodes[-1]
        return [math.degrees(top['pitch']), top['y']]

    # start from a scanned point near the answer (a naive start finds a loop)
    t2, th = newton(half, [0.5, 1.5], [0.0, c['height']])
    tair = 2 * th

    def exitp(x):
        nodes = end(camel_steps(t2, tair, x[0]), c['v0'])
        return [math.degrees(nodes[-1]['pitch'])]

    t4 = newton(exitp, [t2], [0.0])[0]
    return t2, tair, t4


# ---- loop -----------------------------------------------------------------
LP = dict(v0=25.0, approach=1.0, pull=0.5, hold=0.3, bottom=4.2, top=0.5,
          top_hold=0.4, runout=1.0)


def loop_steps(ta, td):
    c = LP
    return [(c['approach'], 0.0), (c['pull'], c['bottom'] - 1.0),
            (c['hold'], 0.0), (ta, c['top'] - c['bottom']),
            (c['top_hold'], 0.0), (td, c['bottom'] - c['top']),
            (c['hold'], 0.0), (c['pull'], 1.0 - c['bottom']),
            (c['runout'], 0.0)]


def tune_loop():
    c = LP

    def half(x):
        st = loop_steps(x[0], 0.5)[:4] + [(c['top_hold'] / 2, 0.0)]
        nodes = end(st, c['v0'])
        return [math.degrees(nodes[-1]['pitch'])]

    ta = newton(half, [1.0], [180.0])[0]

    def exitp(x):
        nodes = end(loop_steps(ta, x[0]), c['v0'])
        return [math.degrees(nodes[-1]['pitch'])]

    td = newton(exitp, [ta], [360.0])[0]
    return ta, td


# ---- layout ---------------------------------------------------------------
# A lift crest held at 3.5 m/s (a fixed-speed section, as a chain does), a
# 45 m first drop, a floater camelback, a smaller ejector hill, a banked turn
# and a run-out. The vertical part is tuned in stages with fvd2d (each stage
# exits level); the turn is a level banked turn at 2.6 g with the roll to
# about 67 degrees picked by hand.
LAYOUT_LIFT_SPEED = 3.5
LAYOUT_LIFT = [(4.0, 0.0)]


LAYOUT_CREST = 0.6       # the force the point feels over the lift crest
LIFT_CRESTS = (0.3, 0.6, 0.75)


def layout_drop(t1, tp, crest=LAYOUT_CREST):
    """Crest force held for t1, rise to 3.5 g over tp, hold 0.15 s, 1 g."""
    return [(0.8, crest - 1.0), (t1, 0.0), (tp, 3.5 - crest), (0.15, 0.0),
            (0.8, -2.5)]


def tune_drop(crest, depth=45.0):
    """Lift crest at `crest` g, drop `depth` m, pull out at 3.5 g, level."""
    def fa(x):
        nd = end(layout_drop(x[0], x[1], crest), LAYOUT_LIFT_SPEED)[-1]
        return [math.degrees(nd['pitch']), nd['y']]
    return newton(fa, [2.0 + 2.5 * (crest - 0.3), 1.0], [0.0, -depth])


def layout_hill(valley, crest, pull, hold, t2, tair, hold2):
    """A hill whose drop transition mirrors its climb (t2 both ways); the
    hold at the valley force after the drop (hold2) makes the exit level."""
    return [(pull, valley - 1.0), (hold, 0.0), (t2, crest - valley),
            (tair, 0.0), (t2, valley - crest), (hold2, 0.0),
            (pull, 1.0 - valley)]


def tune_layout():
    v0 = LAYOUT_LIFT_SPEED

    def endstate(steps):
        nd = end(steps, v0)[-1]
        return nd

    # stage A: crest at 0.6 g, drop to a valley 45 m down, exit level
    t1, th = tune_drop(LAYOUT_CREST)
    steps = layout_drop(t1, th) + [(0.5, 0.0)]
    y_valley = endstate(steps)['y']

    def hill(steps, valley, crest, pull, hold, height, guess):
        def fh(x):
            st = steps + layout_hill(valley, crest, pull, hold, x[0],
                                     x[1], 0.5)[:4]
            st[-1] = (x[1] / 2.0, 0.0)
            nd = endstate(st)
            return [math.degrees(nd['pitch']), nd['y']]
        y0 = endstate(steps)['y']
        t2, tair = newton(fh, guess, [0.0, y0 + height])

        def fe(x):
            nd = endstate(steps + layout_hill(valley, crest, pull, hold, t2,
                                              tair, x[0]))
            return [math.degrees(nd['pitch'])]
        t4 = newton(fe, [hold], [0.0])[0]
        return steps + layout_hill(valley, crest, pull, hold, t2, tair,
                                   t4), (t2, tair, t4)

    # stage B: floater camelback, 28 m above the valley, -0.2 g
    steps, hb = hill(steps, 3.5, -0.2, 0.7, 0.4, 28.0, [0.5, 0.9])
    steps = steps + [(0.4, 0.0)]
    # stage C: ejector hill, 12 m, -0.6 g
    steps, hc = hill(steps, 3.0, -0.6, 0.6, 0.3, 12.0, [0.5, 0.3])
    steps = steps + [(0.5, 0.0)]
    turn_start = sum(d for d, _ in steps)
    steps = steps + [(0.8, 1.6), (3.0, 0.0), (0.8, -1.6), (1.5, 0.0)]
    roll = [(turn_start, 0.0), (0.3, 150.0), (0.15, 0.0), (0.3, -150.0),
            (3.1, 0.0), (0.3, -150.0), (0.15, 0.0), (0.3, 150.0)]
    roll.append((sum(d for d, _ in steps) - sum(d for d, _ in roll), 0.0))
    return steps, roll, dict(drop=(t1, th), camel=hb, ejector=hc,
                             valley_y=y_valley)


def main():
    PROJ.mkdir(exist_ok=True)
    meta = {}
    t2, tair, t4 = tune_camelback()
    meta['camelback'] = dict(CB, t2=t2, tair=tair, t4=t4, fixed_sections=[])
    w.write_project(PROJ / 'camelback.fvd',
                    [w.forced_section('camelback', camel_steps(t2, tair, t4))],
                    speed=CB['v0'], heart=HEART, friction=FRICTION,
                    resistance=RESISTANCE, name='camelback')
    ta, td = tune_loop()
    meta['loop'] = dict(LP, ta=ta, td=td, fixed_sections=[])
    w.write_project(PROJ / 'loop.fvd',
                    [w.forced_section('loop', loop_steps(ta, td))],
                    speed=LP['v0'], heart=HEART, friction=FRICTION,
                    resistance=RESISTANCE, name='loop')
    ride_steps, roll, tuned = tune_layout()
    w.write_project(PROJ / 'layout.fvd',
                    [w.forced_section('lift crest', LAYOUT_LIFT,
                                      fixed_speed=LAYOUT_LIFT_SPEED),
                     w.forced_section('ride', ride_steps, roll_steps=roll)],
                    speed=LAYOUT_LIFT_SPEED, heart=HEART, friction=FRICTION,
                    resistance=RESISTANCE, name='layout')
    meta['layout'] = dict(lift_speed=LAYOUT_LIFT_SPEED, fixed_sections=[1],
                          ride=ride_steps, roll=roll, tuned=tuned)
    print('layout', tuned)
    # the lift crest alone, with three crest forces: lift, crest, drop,
    # pull-out, 2 s level
    for crest in LIFT_CRESTS:
        name = 'liftcrest-%.2f' % crest
        t1, th = tune_drop(crest)
        w.write_project(PROJ / (name + '.fvd'),
                        [w.forced_section('lift crest', LAYOUT_LIFT,
                                          fixed_speed=LAYOUT_LIFT_SPEED),
                         w.forced_section('drop', layout_drop(t1, th, crest)
                                          + [(2.0, 0.0)])],
                        speed=LAYOUT_LIFT_SPEED, heart=HEART,
                        friction=FRICTION, resistance=RESISTANCE, name=name)
        meta[name] = dict(crest=crest, t1=t1, th=th, fixed_sections=[1])
        print(name, t1, th)
    meta['settings'] = dict(heart=HEART, friction=FRICTION,
                            resistance=RESISTANCE)
    (PROJ / 'shapes.json').write_text(json.dumps(meta, indent=1))
    print('camelback t2=%.4f tair=%.4f t4=%.4f' % (t2, tair, t4))
    print('loop ta=%.4f td=%.4f' % (ta, td))

    if '--dump' in sys.argv:
        exe = sys.argv[sys.argv.index('--dump') + 1]
        SHAPES.mkdir(exist_ok=True)
        names = ['camelback', 'loop', 'layout'] + [
            'liftcrest-%.2f' % c for c in LIFT_CRESTS]
        for name in names:
            for spacing, suffix in (('0.1', ''),):
                out = subprocess.run(
                    [exe, 'nodes', str(PROJ / (name + '.fvd')), '--every-m',
                     spacing, '--frame', 'anchor'],
                    check=True, capture_output=True, text=True).stdout
                with gzip.open(SHAPES / (name + '.tsv.gz'), 'wt',
                               newline='\n') as fh:
                    fh.write(out)
            full = subprocess.run(
                [exe, 'nodes', str(PROJ / (name + '.fvd')), '--all',
                 '--frame', 'anchor'],
                check=True, capture_output=True, text=True).stdout
            # the full-resolution dump is only for the validation and is not
            # kept in the repository (several MB)
            (HERE / 'work').mkdir(exist_ok=True)
            (HERE / 'work' / (name + '.all.tsv')).write_text(full)
        print('dumped to', SHAPES)


if __name__ == '__main__':
    main()
