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
"""R7 question (b): the camelback designed with the train's speed.

A force-vector design turns the track by (asked force - gravity) / speed. If
the speed used is the train's (when the chosen car is at that place) instead
of the point's, the shape changes. Found by repetition: design with the
point, ride the train over it, design again with that train speed, ride
again, until the crest moves by less than 1 cm.

Two versions:
  A. Same force program (same forces and durations) as the point design;
     the shape is whatever comes out.
  B. The force program re-solved, as make_shapes.py does for the point, to
     the same goals: crest 30 m above the entry, symmetric crest, level exit.

Run from run_r7.py --redesign (prints into the results), or alone.
"""
import bisect
import json
import math
from pathlib import Path

import fvd2d
import make_shapes as ms
import ride as r

HERE = Path(__file__).resolve().parent
META = json.loads((HERE / 'projects' / 'shapes.json').read_text())
SET = META['settings']
CB = ms.CB


def shape_from_nodes(nodes, name='design'):
    cols = {k: [] for k in ('dist_m', 'heart_dist_m', 'heart_y', 'pos_y',
                            'heart_x', 'dir_x', 'dir_y', 'dir_z', 'norm_x',
                            'norm_y', 'norm_z', 'lat_x', 'lat_y', 'lat_z',
                            'speed_mps', 'pitch', 'time_s', 'roll')}
    last = -1.0
    for nd in nodes:
        if nd['L'] <= last + 1e-9:
            continue
        last = nd['L']
        th = nd['pitch']
        cols['dist_m'].append(nd['L'])
        cols['heart_dist_m'].append(nd['Lh'])
        cols['heart_x'].append(nd['x'])
        cols['heart_y'].append(nd['y'])
        cols['pos_y'].append(nd['ry'])
        for k, v in zip('xyz', (math.cos(th), math.sin(th), 0.0)):
            cols['dir_' + k].append(v)
        for k, v in zip('xyz', (math.sin(th), -math.cos(th), 0.0)):
            cols['norm_' + k].append(v)
        for k, v in zip('xyz', (0.0, 0.0, 1.0)):
            cols['lat_' + k].append(v)
        cols['speed_mps'].append(nd['v'])
        cols['pitch'].append(math.degrees(th))
        cols['time_s'].append(nd['t'])
        cols['roll'].append(0.0)
    return r.Shape(cols, SET['heart'], SET['friction'], SET['resistance'],
                   [], name)


def speed_fn(res, offset):
    """Speed when the car at `offset` from the train's centre is at L."""
    ss, vs = res['s'], res['v']

    def f(L):
        x = L - offset
        if x <= ss[0]:
            return vs[0]
        if x >= ss[-1]:
            return vs[-1]
        i = bisect.bisect_right(ss, x)
        a = (x - ss[i - 1]) / (ss[i] - ss[i - 1])
        return vs[i - 1] + a * (vs[i] - vs[i - 1])
    return f


def design(steps, speed_of=None, keep=3):
    return fvd2d.run(fvd2d.Force(1.0, steps), CB['v0'], SET['heart'],
                     SET['friction'], SET['resistance'], speed_of=speed_of,
                     keep=keep)


def summary(nodes):
    # the crest: the first place where the climb turns into a descent
    top = next(b for a, b in zip(nodes, nodes[1:])
               if b['y'] > 5.0 and a['pitch'] > 0.0 >= b['pitch'])
    t_pull = CB['approach']
    x0 = min(nodes, key=lambda n: abs(n['t'] - t_pull))['x']
    # airtime span: horizontal length where the design force is below 0.1 g
    xs = [n['x'] for n in nodes if n['n'] < 0.1]
    return dict(crest_y=top['y'], crest_x=top['x'] - x0,
                span=(xs[-1] - xs[0]) if xs else 0.0,
                exit_pitch=math.degrees(nodes[-1]['pitch']),
                exit_y=nodes[-1]['y'], crest_v=top['v'],
                length=nodes[-1]['L'])


def fixed_point(train, offset, steps_of, iters=40):
    """Repeat: design with the current speed, ride the train, until the crest
    height, the crest's place and the airtime span each move less than 2 cm.
    steps_of(speed_of) gives the force program (fixed, or re-solved for the
    goals). From the third round on, the speed used is the mean of the last
    two rounds' (damping; the rear-car design otherwise swings slowly)."""
    speed_of = None
    prev = None
    for k in range(iters):
        steps = steps_of(speed_of)
        nodes = design(steps, speed_of)
        sm = summary(nodes)
        key = (sm['crest_y'], sm['crest_x'], sm['span'])
        if prev is not None and max(abs(a - b)
                                    for a, b in zip(key, prev)) < 0.02:
            return steps, nodes, sm, k
        prev = key
        res = r.ride_train(shape_from_nodes(nodes), train)
        new = speed_fn(res, offset)
        if speed_of is None or k < 2:
            speed_of = new
        else:
            old = speed_of
            speed_of = (lambda f, g: (lambda L: 0.5 * (f(L) + g(L))))(old,
                                                                    new)
    raise RuntimeError('no convergence')


def camel_steps_b(t2, tair, hold2):
    """The point design's force program with the hold at the valley force
    after the drop (0.4 s in the point design) free."""
    st = ms.camel_steps(t2, tair, META['camelback']['t4'])
    st[6] = (hold2, 0.0)
    return st


def resolve_steps(speed_of):
    """t2, the airtime hold and the exit-side hold solved for the goals with
    this speed function (None = the point's own speed)."""
    def endn(st):
        return design(st, speed_of, keep=50)[-1]

    def half(x):
        nd = endn(ms.camel_steps(x[0], x[1], 0.5)[:5])
        return [math.degrees(nd['pitch']), nd['y']]
    t2, th = ms.newton(half, [META['camelback']['t2'],
                              META['camelback']['tair'] / 2],
                       [0.0, CB['height']])

    def exitp(x):
        return [math.degrees(endn(camel_steps_b(t2, 2 * th, x[0]))['pitch'])]
    hold2 = ms.newton(exitp, [CB['hold']], [0.0])[0]
    if hold2 < 0:
        raise RuntimeError('no level exit with this force program')
    return camel_steps_b(t2, 2 * th, hold2)


def felt(nodes, trains):
    """Front, middle, rear: lowest g and seconds below 0 g on the design."""
    sh = shape_from_nodes(nodes)
    out = {}
    for key, n, L in trains:
        res = r.ride_train(sh, r.Train(n, L))
        row = []
        for idx in (0, n // 2, n - 1):
            row.append((r.extreme(res, idx, 0, sh.s[-1], 'min'),
                        r.airtime(res, idx, 0, sh.s[-1])[0]))
        out[key] = row
    return out


def main(say=print, table=None):
    cb = META['camelback']
    point_steps = ms.camel_steps(cb['t2'], cb['tair'], cb['t4'])
    base = design(point_steps)
    sb = summary(base)
    trains = [('long', 9, 2.2), ('extreme', 10, 3.0)]
    say('## Redesign: the camelback designed with the train\'s speed')
    say()
    say('Point design: crest %.2f m above the entry, %.1f m (horizontally) '
        'after the start of the pull-up, airtime span %.1f m, crest speed '
        '%.2f m/s, exit pitch %.2f deg.' % (sb['crest_y'], sb['crest_x'],
                                            sb['span'], sb['crest_v'],
                                            sb['exit_pitch']))
    say()
    rows_a, rows_b, felt_rows = [], [], []
    for key, n, L in trains:
        train = r.Train(n, L)
        offs = train.offsets()
        for aim, off in (('middle', 0.0), ('front', offs[0]),
                         ('rear', offs[-1])):
            # A: same force program
            steps, nodes, sm, k = fixed_point(train, off,
                                              lambda so: point_steps)
            rows_a.append([key, aim, str(k),
                           '%.2f (%+.2f)' % (sm['crest_y'],
                                             sm['crest_y'] - sb['crest_y']),
                           '%.1f (%+.1f)' % (sm['crest_x'],
                                             sm['crest_x'] - sb['crest_x']),
                           '%.1f (%+.1f)' % (sm['span'],
                                             sm['span'] - sb['span']),
                           '%+.2f' % sm['exit_pitch'],
                           '%+.2f' % (sm['exit_y'] - base[-1]['y'])])
            if aim == 'rear':
                continue
            # B: re-solved to the same goals
            try:
                steps, nodes, sm, k = fixed_point(train, off, resolve_steps)
            except RuntimeError as e:
                rows_b.append([key, aim, '-', 'no solution: %s' % e, '', '',
                               '', ''])
                continue
            tt = [d for d, _ in steps]
            rows_b.append([key, aim, str(k),
                           '%.3f / %.3f / %.3f' % (tt[3], tt[4], tt[6]),
                           '%.2f' % sm['crest_y'],
                           '%.1f (%+.1f)' % (sm['crest_x'],
                                             sm['crest_x'] - sb['crest_x']),
                           '%.1f (%+.1f)' % (sm['span'],
                                             sm['span'] - sb['span']),
                           '%.2f (%+.2f)' % (sm['crest_v'],
                                             sm['crest_v'] - sb['crest_v'])])
            f = felt(nodes, [(key, n, L)])[key]
            felt_rows.append([key, 'aimed at the ' + aim] + [
                '%.2f g, %.2f s' % p for p in f])
        f = felt(base, [(key, n, L)])[key]
        felt_rows.append([key, 'point design'] + ['%.2f g, %.2f s' % p
                                                  for p in f])
    say('A. Same forces and durations as the point design; the shape '
        'follows. Numbers in brackets: change from the point design.')
    say()
    table(['train', 'design aims at', 'repetitions', 'crest height (m)',
           'crest distance from pull-up (m)', 'airtime span (m)',
           'exit pitch (deg)', 'exit height change (m)'], rows_a)
    say('B. Durations re-solved for the same goals (crest 30 m, symmetric '
        'crest, level exit): the fall to the crest force, the crest hold '
        'and the hold at the valley force after the drop. Point design: '
        '%.3f / %.3f / %.3f s.' % (cb['t2'], cb['tair'], CB['hold']))
    say()
    table(['train', 'design aims at', 'repetitions',
           'durations (s)', 'crest height (m)',
           'crest distance from pull-up (m)', 'airtime span (m)',
           'crest speed of the design car (m/s)'], rows_b)
    say('What each car feels on the re-solved designs (lowest g, seconds '
        'below 0 g):')
    say()
    table(['train', 'design', 'front car', 'middle car', 'rear car'],
          felt_rows)


if __name__ == '__main__':
    def _table(head, rows):
        print('| ' + ' | '.join(head) + ' |')
        for row in rows:
            print('| ' + ' | '.join(row) + ' |')
        print()
    main(print, _table)
