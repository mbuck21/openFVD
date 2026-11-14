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
"""X1: the design following a chosen car, by repetition.

    py -3 docs/research/x1-trials/iterate.py [--quick]

For each shape (camelback, loop, lift crest), each chosen car (the train's
centre, the front car, the rear car of R7's 9-car, 19.8 m train) and each
reading of "design with the train's speed at the chosen car":

  T  the asked forces stay a function of TIME (the force program as it is,
     on the chosen car's own clock); R7's redesign.py reading.
  D  the asked forces stay a function of DISTANCE along the track (the
     force the point design asks at each metre).

it repeats: design (round 0 with the program's point); write the design as a
project; ride it with `fvdcore ride` and the train; take the train's speed
when the chosen car is at each place; design again with it. It stops when
the shape moves less than 2 cm between two rounds (at the same rail length),
or after 20 rounds; if plain repetition does not settle, it is run again
with each round's speed halfway between the old and the new (damped).

Then, for the front car only: one pass that solves the shape and the
train's energy together (x1lib.TrainAhead). And for the camelback: R7's
version B, the three durations re-solved each round for the same goals
(crest 30 m, symmetric crest, level exit).

Every force reported is from `fvdcore ride` on the written project.
Writes results/iterate.txt (the tables), results/iterate-rounds.txt (every
round), results/iterate-times.txt (wall times; these vary from run to run)
and the settled designs of reading T as projects/<shape>-<car>.fvd.
"""
import math
import sys
import time

import x1lib as X

MAX_ROUNDS = 20
SETTLED_M = 0.02


def speed_grid(fn, L_max, step=0.05):
    n = int(L_max / step) + 2
    Ls = [k * step for k in range(n)]
    return Ls, [fn(L) for L in Ls]


def grid_fn(grid):
    Ls, vs = grid

    def f(L):
        return X.interp(Ls, vs, L)
    return f


class Case:
    def __init__(self, sd, choice, car, reading, mode='plain'):
        self.sd, self.choice, self.car, self.reading, self.mode = sd, choice, car, reading, mode
        self.tag = '%s-%s-%s-%s' % (sd.key, choice, reading, mode)
        self.rounds = []
        self.result = None
        self.times = {'design': 0.0, 'write': 0.0, 'ride': 0.0}


def design_for(case, base, speed_of, steps=None):
    sd = case.sd if steps is None else with_steps(case.sd, steps)
    if case.reading == 'D':
        Ls = [n['L'] for n in base]
        As = [n['ask'] for n in base]
        return X.design(sd, speed_of=speed_of, ask_of_L=lambda L: X.interp(Ls, As, L),
                        L_end=base[-1]['L'])
    return X.design(sd, speed_of=speed_of)


def with_steps(sd, steps):
    return X.ShapeDef(sd.key, sd.r7_project, steps, sd.v0, sd.lift)


def ride_design(case, nodes, k):
    t0 = time.perf_counter()
    proj = X.write_project(case.sd, nodes, X.WORK / ('%s-r%d.fvd' % (case.tag, k)))
    t1 = time.perf_counter()
    rd = X.prepare(X.ride(proj))
    t2 = time.perf_counter()
    case.times['write'] += t1 - t0
    case.times['ride'] += t2 - t1
    return proj, rd


def iterate(case, base, weight, resolve=None):
    """Repeat design -> ride -> speed at the chosen car. weight: share of the
    new speed in the speed used next (1 = plain repetition)."""
    s0 = X.lift_length(case.sd)
    L_max = base[-1]['L'] * 2.0 + 60.0
    nodes, steps = base, None
    prev, used = None, None
    for k in range(MAX_ROUNDS + 1):
        proj, rd = ride_design(case, nodes, k)
        err = X.ask_error(rd, case.car, nodes, s0)
        gap = X.same_L_gap(nodes, prev) if prev is not None else None
        top = X.crest(case.sd, nodes)
        case.rounds.append(dict(k=k, gap=gap, err=err, crest=(top['x'], top['y']),
                                stalled=rd['stalled'], v_crest=top['v']))
        if rd['stalled']:
            case.result = ('stall', k, nodes, rd, proj, steps)
            return
        if gap is not None and gap < SETTLED_M:
            case.result = ('settled', k, nodes, rd, proj, steps)
            return
        if k == MAX_ROUNDS:
            case.result = ('not settled', k, nodes, rd, proj, steps)
            return
        new = speed_grid(X.car_speed_fn(rd, case.car, s0), L_max)
        if used is None or weight >= 1.0:
            used = new
        else:
            used = (new[0], [weight * a + (1.0 - weight) * b for a, b in zip(new[1], used[1])])
        prev = nodes
        t0 = time.perf_counter()
        if resolve is not None:
            steps = resolve(grid_fn(used))
        nodes = design_for(case, base, grid_fn(used), steps)
        case.times['design'] += time.perf_counter() - t0


def summary_cars(rd, nodes, s0):
    """Front, centre, rear: lowest / highest gz, below 0 g and ejector seconds,
    largest felt-minus-asked over the element."""
    out = []
    for name, car in X.CHOICES:
        c = rd['cars'][car]
        err = X.ask_error(rd, car, nodes, s0)
        out.append((name, c['gz_min']['value'], c['gz_max']['value'], c['below_zero_s'],
                    c['ejector_s'], err))
    return out


def body(sd, nodes, base, reading):
    """The element without its final level run-out (the last piece of the
    force program): T keeps the nodes up to that time, D up to the point
    design's rail length there."""
    t0, t1, _, _ = sd.force.pieces[-1]
    if reading == 'D':
        L_cut = X.interp([n['t'] for n in base], [n['L'] for n in base], t0)
        return [n for n in nodes if n['L'] <= L_cut]
    return [n for n in nodes if n['t'] <= t0]


def exit_state(sd, nodes):
    pitch = math.degrees(nodes[-1]['pitch'])
    if sd.key == 'loop':
        pitch -= 360.0
    return pitch, nodes[-1]['y'], nodes[-1]['x'], nodes[-1]['L']


def fmt_err(e):
    w, where, rms = e
    if where is None:
        return '-'
    return '%+.2f at %.0f m' % (w, where)


def main():
    X.need_exe()
    X.WORK.mkdir(exist_ok=True)
    X.PROJECTS.mkdir(exist_ok=True)
    quick = '--quick' in sys.argv
    rep = X.Report('iterate' + ('-quick' if quick else ''))
    rnd = X.Report('iterate-rounds' + ('-quick' if quick else ''))
    tim = X.Report('iterate-times' + ('-quick' if quick else ''))
    rep('# X1: the design following a chosen car, by repetition')
    rep('')
    rep('Train: %d cars %.1f m apart (%.1f m; R7\'s "long" train). Car 1 is the front '
        'car, car %d the centre, car %d the rear. Every force here is from '
        '`fvdcore ride` on the written project (CSV points every 0.1 m).'
        % (X.CARS, X.PITCH, X.CARS * X.PITCH, X.CARS // 2 + 1, X.CARS))
    rep('')
    shapes = X.shapes()
    if quick:
        shapes = shapes[:1]
    rows_main, rows_feel, rows_shape, rows_extra = [], [], [], []
    rows_time = []
    for sd in shapes:
        s0 = X.lift_length(sd)
        base = X.design(sd)
        bproj = X.write_project(sd, base, X.WORK / ('%s-point.fvd' % sd.key))
        brd = X.prepare(X.ride(bproj))
        btop = X.crest(sd, base)
        bexit = exit_state(sd, base)
        for name, mn, mx, bz, ej, err in summary_cars(brd, base, s0):
            rows_feel.append([sd.key, 'point design', name, '%.2f' % mn, '%.2f' % mx,
                              '%.2f' % bz, '%.2f' % ej, fmt_err(err)])
        choices = X.CHOICES if not quick else X.CHOICES[:2]
        for choice, car in choices:
            for reading in ('T', 'D'):
                case = Case(sd, choice, car, reading)
                iterate(case, base, 1.0)
                tried = [case]
                if case.result[0] != 'settled':
                    damped = Case(sd, choice, car, reading, mode='damped')
                    iterate(damped, base, 0.5)
                    tried.append(damped)
                for c in tried:
                    status, k, nodes, rd, proj, _ = c.result
                    for r in c.rounds:
                        rnd_row(rnd, c, r)
                    top = X.crest(sd, nodes)
                    ex = exit_state(sd, nodes)
                    gap, where = X.shape_gap(nodes, base)
                    gap_b, where_b = X.shape_gap(body(sd, nodes, base, reading), base)
                    first = c.rounds[0]['err']
                    last = c.rounds[-1]['err']
                    rows_main.append([sd.key, choice, reading, c.mode, status,
                                      str(k), fmt_err(first), fmt_err(last),
                                      '%.2f / %.2f' % (btop['v'], top['v'])])
                    rows_shape.append([sd.key, choice, reading, c.mode, status,
                                       '%+.2f / %+.2f' % (top['x'] - btop['x'], top['y'] - btop['y']),
                                       '%.2f at %.0f m' % (gap_b, where_b),
                                       '%.2f at %.0f m' % (gap, where),
                                       '%+.1f' % (ex[0] - bexit[0]),
                                       '%+.2f / %+.1f' % (ex[1] - bexit[1], ex[2] - bexit[2]),
                                       '%+.1f' % (ex[3] - bexit[3])])
                    if status != 'stall':
                        for name, mn, mx, bz, ej, err in summary_cars(rd, nodes, s0):
                            rows_feel.append([sd.key, 'for the %s, %s%s' % (
                                choice, reading, '' if c.mode == 'plain' else ', damped'),
                                name, '%.2f' % mn, '%.2f' % mx, '%.2f' % bz, '%.2f' % ej,
                                fmt_err(err)])
                    n_r = len(c.rounds)
                    rows_time.append([c.tag, str(n_r), '%.2f' % (c.times['design'] / max(n_r - 1, 1)),
                                      '%.2f' % (c.times['write'] / n_r), '%.2f' % (c.times['ride'] / n_r)])
                    if reading == 'T' and status == 'settled':
                        X.write_project(sd, nodes, X.PROJECTS / ('%s-%s.fvd' % (sd.key, choice)),
                                        name='%s designed for the %s car' % (sd.key, choice))
                        (X.PROJECTS / ('%s-%s.json' % (sd.key, choice))).unlink()
                        if choice == 'front':
                            _REPEATED[sd.key] = nodes
                if choice == 'front':
                    extra_together(rows_extra, sd, base, s0, car, reading)
        if sd.key == 'camelback' and not quick:
            extra_resolved(rows_extra, rnd, sd, base, s0)
    rep('## Does it settle')
    rep('')
    rep('Status after the rounds (round 0 is the point design; "settled": the '
        'shape moved less than 2 cm between two rounds at the same rail length). '
        'Felt minus asked: the chosen car\'s largest difference over the element '
        '(g, and where: metres into the element), on the point design and on the '
        'last round. Speed at the crest: the point\'s on the point design / the '
        'speed used for the design at the crest on the last round.')
    rep('')
    rep.table(['shape', 'chosen car', 'reading', 'repetition', 'status', 'rounds',
               'felt - asked, point design', 'felt - asked, last round',
               'speed at the crest (m/s)'], rows_main)
    rep('## How far the shape moves (last round against the point design)')
    rep('')
    rep('Crest (camelback crest, loop top, lift crest: where the drop passes '
        '-30 deg) moved along / up (m); the largest distance from the new shape to '
        'the old (m, and where: metres into the element), before the final level '
        'run-out and over the whole element; exit pitch change (deg); exit height / horizontal '
        'change (m); element length change (m of rail). A "stall" row describes the '
        'last design, which the train cannot ride to its end; its numbers say how far '
        'the repetition went astray, not a usable shape.')
    rep('')
    rep.table(['shape', 'chosen car', 'reading', 'repetition', 'status', 'crest moved',
               'largest gap before the run-out', 'largest gap', 'exit pitch',
               'exit height / along', 'length'], rows_shape)
    rep('## What each car feels (fvdcore ride)')
    rep('')
    rep('Lowest and highest vertical force (g), seconds below 0 g and below -0.5 g '
        '(ejector), and the car\'s largest felt-minus-asked over the element, where '
        'asked is what that design asks.')
    rep('')
    rep.table(['shape', 'design', 'car', 'lowest gz', 'highest gz', 'below 0 g (s)',
               'ejector (s)', 'felt - asked'], rows_feel)
    rep('## Other ways to compute')
    rep('')
    rep.table(['shape', 'method', 'chosen car', 'reading', 'result',
               'felt - asked (chosen car)', 'gap to the repeated design', 'note'], rows_extra)
    rep.save()
    rnd.save()
    tim('# Wall time per round (s), on the machine that ran it; varies between runs')
    tim('')
    tim.table(['case', 'rounds', 'design (Python stand-in)', 'write project + fromtext',
               'fvdcore ride'], rows_time)
    tim.save()


def rnd_row(rnd, c, r):
    if r['k'] == 0:
        rnd('')
        rnd('## %s' % c.tag)
    rnd('round %d: change from last round %s m; chosen car felt - asked %s (RMS %.3f); '
        'crest at %.2f, %.2f m; design speed at the crest %.2f m/s%s' % (
            r['k'], '-' if r['gap'] is None else '%.3f' % r['gap'], fmt_err(r['err']),
            r['err'][2], r['crest'][0], r['crest'][1], r['v_crest'],
            '; STALL' if r['stalled'] else ''))


_REPEATED = {}


def extra_together(rows, sd, base, s0, car, reading):
    """Front car: shape and the train's energy solved in one pass."""
    if reading != 'T':
        return
    lift = sd.lift
    ta = X.TrainAhead(s0, base[0]['y09'], sd.v0,
                      chain=(s0, lift[1]) if lift else None)
    nodes = X.design(sd, together=ta)
    proj = X.write_project(sd, nodes, X.WORK / ('%s-front-together.fvd' % sd.key))
    rd = X.prepare(X.ride(proj))
    err = X.ask_error(rd, car, nodes, s0)
    # compare with the repeated design of the same case
    gap = '-'
    ref = _REPEATED.get(sd.key)
    if ref is not None:
        g, where = X.shape_gap(nodes, ref)
        gap = '%.3f m at %.0f m' % (g, where)
    turned = math.degrees(nodes[-1]['pitch'])
    note = 'lowest design speed %.2f m/s; pitch turned %.0f deg in all' % (
        min(n['v'] for n in nodes), turned)
    rows.append([sd.key, 'shape and train solved together, one pass', 'front', 'T',
                 'stalled' if rd['stalled'] else 'ridden', fmt_err(err), gap, note])


def extra_resolved(rows, rnd, sd, base, s0):
    """Camelback: R7's version B, durations re-solved each round."""
    sys.path.insert(0, str(X.R7))
    import redesign  # R7's re-solve of the three durations

    def resolve(speed_of):
        return redesign.resolve_steps(speed_of)
    for choice, car in X.CHOICES:
        case = Case(sd, choice, car, 'T', mode='re-solved')
        try:
            iterate(case, base, 1.0, resolve=resolve)
        except RuntimeError as e:
            rows.append([sd.key, 'durations re-solved each round (R7 B)', choice, 'T',
                         'no solution: %s' % e, '', '', ''])
            continue
        for r in case.rounds:
            rnd_row(rnd, case, r)
        status, k, nodes, rd, proj, steps = case.result
        tt = [d for d, _ in steps] if steps else []
        top = X.crest(sd, nodes)
        btop = X.crest(sd, base)
        ex = exit_state(sd, nodes)
        rows.append([sd.key, 'durations re-solved each round (R7 B)', choice, 'T',
                     '%s after %d rounds' % (status, k),
                     fmt_err(case.rounds[-1]['err']), '-',
                     'durations %s s; crest %+.2f m along, %.2f m high; exit pitch %+.2f deg'
                     % (' / '.join('%.3f' % t for t in (tt[3], tt[4], tt[6])) if tt else '-',
                        top['x'] - btop['x'], top['y'], ex[0])])


if __name__ == '__main__':
    main()
