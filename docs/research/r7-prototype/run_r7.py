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
"""R7: the train against the point. Writes every table of the note.

    py -3 run_r7.py            # validation, comparison, scaling, closed form
    py -3 run_r7.py --redesign # also the redesign of section 5 (slower)

Reads the dumps in shapes/ (and work/*.all.tsv for the full-resolution
validation, when make_shapes.py --dump has been run). Prints the tables and
writes results.txt and plots/*.svg.
"""
import json
import math
import sys
from pathlib import Path

import ride as r

HERE = Path(__file__).resolve().parent
META = json.loads((HERE / 'projects' / 'shapes.json').read_text())
SET = META['settings']
OUT = []

TRAINS = [('short', 4, 2.5), ('long', 9, 2.2), ('extreme', 10, 3.0)]


def say(text=''):
    OUT.append(text)
    print(text)


def load(name, path=None):
    path = path or HERE / 'shapes' / (name + '.tsv.gz')
    return r.Shape.from_dump(path, SET['heart'], SET['friction'],
                             SET['resistance'],
                             META[name]['fixed_sections'], name)


def table(head, rows):
    say('| ' + ' | '.join(head) + ' |')
    say('|' + '---|' * len(head))
    for row in rows:
        say('| ' + ' | '.join(row) + ' |')
    say()


# ---------------------------------------------------------------------------
def validation():
    say('## Validation: the point model against the program')
    say()
    rows = []
    for name in ('camelback', 'loop', 'layout'):
        for label, path in (('0.1 m', None),
                            ('every node',
                             HERE / 'work' / (name + '.all.tsv'))):
            if path is not None and not path.exists():
                rows.append([name, label, 'not run (no work/ dump)', '', '',
                             ''])
                continue
            sh = load(name, path)
            v = r.ride_point(sh)
            dv = max(abs(a - b) for a, b in zip(v, sh.c['speed_mps']))
            dn = dl = dn_own = 0.0
            skipped = 0
            for i in range(1, sh.n - 1):
                if abs(sh.c['pitch'][i]) > 89.5:
                    skipped += 1
                    continue
                n, l = sh.forces(sh.s[i], sh.c['speed_mps'][i])
                dn = max(dn, abs(n - sh.c['normal_g'][i]))
                dl = max(dl, abs(l - sh.c['lateral_g'][i]))
                n2, _ = sh.forces(sh.s[i], v[i])
                dn_own = max(dn_own, abs(n2 - sh.c['normal_g'][i]))
            rows.append([name, label, '%.4f' % dv, '%.3f' % dn,
                         '%.3f' % dn_own, '%.3f' % dl, str(skipped)])
    table(['shape', 'points', 'worst speed diff (m/s)',
           'worst normal diff, program speed (g)',
           'worst normal diff, own speed (g)', 'worst lateral diff (g)',
           'points at vertical left out'], rows)
    # fvd2d (the design step used for the redesign) against the program
    import fvd2d
    import make_shapes as ms
    rows = []
    for name in ('camelback', 'loop'):
        m = META[name]
        steps = (ms.camel_steps(m['t2'], m['tair'], m['t4'])
                 if name == 'camelback' else ms.loop_steps(m['ta'], m['td']))
        nodes = fvd2d.run(fvd2d.Force(1.0, steps), m['v0'], SET['heart'],
                          SET['friction'], SET['resistance'])
        sh = load(name)
        dy = dv = dp = 0.0
        for i in range(sh.n):
            k = int(round(sh.c['time_s'][i] * 1000))
            nd = nodes[k]
            dy = max(dy, abs(nd['y'] - sh.c['heart_y'][i]))
            dv = max(dv, abs(nd['v'] - sh.c['speed_mps'][i]))
            p = math.degrees(math.atan2(math.sin(nd['pitch']),
                                        abs(math.cos(nd['pitch']))))
            dp = max(dp, abs(p - sh.c['pitch'][i]))
        rows.append([name, '%.4f' % dy, '%.5f' % dv, '%.4f' % dp])
    say('The design step of fvd2d.py (used for the redesign) against the '
        'program, same force program, compared at the dumped nodes:')
    say()
    table(['shape', 'worst height diff (m)', 'worst speed diff (m/s)',
           'worst pitch diff (deg)'], rows)


# ---------------------------------------------------------------------------
def ride_all(sh):
    pt = r.point_as_train(sh)
    trains = {}
    for key, n, L in TRAINS:
        trains[key] = r.ride_train(sh, r.Train(n, L))
    return pt, trains


def cars_of(train_key):
    n = [t for t in TRAINS if t[0] == train_key][0][1]
    return [('front', 0), ('middle', n // 2 if n % 2 else None),
            ('rear', n - 1)]


def car_force_at(res, car, x, centre_if_none=None):
    if car is None:
        return None
    f = r.force_when_car_at(res, car, x)
    return f


def mid_value(res, key, x):
    """Middle car: the real middle car when N is odd; for even N the mean
    of the two cars either side of the centre."""
    n = [t for t in TRAINS if t[0] == key][0][1]
    if n % 2:
        return r.force_when_car_at(res, n // 2, x)
    a = r.force_when_car_at(res, n // 2 - 1, x)
    b = r.force_when_car_at(res, n // 2, x)
    return tuple(0.5 * (p + q) for p, q in zip(a, b))


def mid_car_index(key):
    n = [t for t in TRAINS if t[0] == key][0][1]
    return n // 2


def argext(sh, arr, lo, hi, which):
    idx = [i for i in range(sh.n) if lo <= sh.s[i] <= hi]
    f = max if which == 'max' else min
    return f(idx, key=lambda i: arr[i])


def describe(name, sh, pt):
    v = pt['v']
    i_top = max(range(sh.n), key=lambda i: sh.hy[i])
    say('Shape `%s`: %.1f m of centre line, entry %.2f m/s, highest point '
        '%.2f m above the start at s = %.1f m, speed there %.2f m/s, exit '
        '%.2f m/s.' % (name, sh.s[-1], v[0], sh.hy[i_top], sh.s[i_top],
                       v[i_top], v[-1]))
    say()


def compare_element(sh, pt, trains, points, airtime_range):
    """points: list of (label, x, kind) with kind 'crest'/'valley'."""
    # speeds
    rows = []
    for key, n, L in TRAINS:
        res = trains[key]
        dvs = [(r.speed_at(res, x) - vp, x) for x, vp in zip(pt['s'], pt['v'])
               if res['s'][0] <= x <= res['s'][-1]]
        worst = max(dvs, key=lambda d: abs(d[0]))
        row = ['%s, %d x %.1f m = %.1f m' % (key, n, L, n * L),
               '%+.2f at s=%.0f' % worst]
        for label, x, kind in points:
            row.append('%+.2f' % (r.speed_at(res, x) - r.speed_at(pt, x)))
        rows.append(row)
    say('Speed of the train (when its centre is at the place) minus the '
        'point\'s speed there, m/s:')
    say()
    table(['train', 'largest difference (m/s, where)'] +
          ['at %s (s=%.0f m, point %.2f m/s)' % (l, x, r.speed_at(pt, x))
           for l, x, k in points], rows)
    # forces
    say('Normal force (g) felt when each car passes the place:')
    say()
    head = ['train', 'car'] + ['%s (point %.2f)' % (
        l, r.force_when_car_at(pt, 0, x)[0]) for l, x, k in points]
    rows = []
    for key, n, L in TRAINS:
        res = trains[key]
        for cname, idx in (('front', 0), ('middle', None), ('rear', n - 1)):
            row = [key, cname + ('' if cname != 'middle' or n % 2 else
                                 ' (mean of the two centre cars)')]
            for l, x, k in points:
                f = mid_value(res, key, x) if idx is None else \
                    r.force_when_car_at(res, idx, x)
                row.append('%.2f' % f[0])
            rows.append(row)
    table(head, rows)
    # extremes and airtime
    lo, hi = airtime_range
    say('Extremes and airtime for each car while it is between s = %.0f and '
        '%.0f m (seconds; floater = +0.1 to -0.5 g, ejector = below -0.5 g):'
        % (lo, hi))
    say()
    rows = []
    a = r.airtime(pt, 0, lo, hi)
    rows.append(['point', '-', '%.2f' % r.extreme(pt, 0, lo, hi, 'min'),
                 '%.2f' % r.extreme(pt, 0, lo, hi, 'max'),
                 '%.2f' % a[0], '%.2f' % a[1], '%.2f' % a[2]])
    for key, n, L in TRAINS:
        res = trains[key]
        for cname, idx in (('front', 0), ('middle', mid_car_index(key)),
                           ('rear', n - 1)):
            a = r.airtime(res, idx, lo, hi)
            label = cname
            if cname == 'middle' and n % 2 == 0:
                label = 'car %d of %d' % (idx + 1, n)
            rows.append([key, label,
                         '%.2f' % r.extreme(res, idx, lo, hi, 'min'),
                         '%.2f' % r.extreme(res, idx, lo, hi, 'max'),
                         '%.2f' % a[0], '%.2f' % a[1], '%.2f' % a[2]])
    table(['train', 'car', 'lowest g', 'highest g', 'below 0 g (s)',
           'floater (s)', 'ejector (s)'], rows)
    return rows


def along_table(sh, pt, trains, points):
    say('Along-track force (g) when each car passes the place '
        '(+ pressed into the seat back, - pressed forward into the '
        'restraint; the point feels only friction and drag):')
    say()
    rows = []
    for key, n, L in TRAINS:
        res = trains[key]
        for cname, idx in (('front', 0), ('rear', n - 1)):
            row = [key, cname]
            for l, x, k in points:
                row.append('%+.2f' % r.force_when_car_at(res, idx, x)[2])
            rows.append(row)
    table(['train', 'car'] + [l for l, x, k in points], rows)


def camelback():
    say('## Camelback')
    say()
    sh = load('camelback')
    pt, trains = ride_all(sh)
    describe('camelback', sh, pt)
    i_top = max(range(sh.n), key=lambda i: sh.hy[i])
    n_pt = [c[1] for c in pt['cars'][0]]
    i_v1 = argext(sh, n_pt, 0, sh.s[i_top], 'max')
    i_v2 = argext(sh, n_pt, sh.s[i_top], sh.s[-1], 'max')
    pts = [('pull-up', sh.s[i_v1], 'valley'), ('crest', sh.s[i_top], 'crest'),
           ('pull-out', sh.s[i_v2], 'valley')]
    compare_element(sh, pt, trains, pts, (0, sh.s[-1]))
    along_table(sh, pt, trains, pts)
    plot_speed('camelback', sh, pt, trains)
    plot_force('camelback', pt, trains, 'long')
    return sh, pt, trains


def loop():
    say('## Vertical loop')
    say()
    sh = load('loop')
    pt, trains = ride_all(sh)
    describe('loop', sh, pt)
    i_top = max(range(sh.n), key=lambda i: sh.hy[i])
    n_pt = [c[1] for c in pt['cars'][0]]
    i_v1 = argext(sh, n_pt, 0, sh.s[i_top], 'max')
    i_v2 = argext(sh, n_pt, sh.s[i_top], sh.s[-1], 'max')
    pts = [('entry', sh.s[i_v1], 'valley'), ('top', sh.s[i_top], 'crest'),
           ('exit', sh.s[i_v2], 'valley')]
    compare_element(sh, pt, trains, pts, (0, sh.s[-1]))
    along_table(sh, pt, trains, pts)
    plot_speed('loop', sh, pt, trains)
    return sh, pt, trains


def layout():
    say('## Layout: lift crest, first drop, camelback, ejector hill, turn')
    say()
    sh = load('layout')
    pt, trains = ride_all(sh)
    describe('layout', sh, pt)
    n_pt = [c[1] for c in pt['cars'][0]]
    s = sh.s
    i_turn0 = min(i for i in range(sh.n) if abs(sh.c['roll'][i]) > 5)
    i_low = min(i for i in range(sh.n) if sh.hy[i] < -44.0)
    i_v1 = argext(sh, n_pt, 0, s[i_low], 'max')
    i_c1 = argext(sh, sh.hy, s[i_low], s[i_low] + 150, 'max')
    i_tr = argext(sh, sh.hy, s[i_c1], s[i_c1] + 80, 'min')
    i_c2 = argext(sh, sh.hy, s[i_tr], s[i_turn0], 'max')
    i_turn = argext(sh, n_pt, s[i_turn0], s[-1], 'max')
    i_drop = min(i for i in range(sh.n) if sh.c['pitch'][i] < -30)
    pts = [('drop, pitch -30 deg', s[i_drop], 'crest'),
           ('first valley', s[i_v1], 'valley'),
           ('camelback crest', s[i_c1], 'crest'),
           ('ejector crest', s[i_c2], 'crest'),
           ('turn', s[i_turn], 'valley')]
    compare_element(sh, pt, trains, pts, (0, s[-1]))
    along_table(sh, pt, trains, pts)
    say('Per element, seconds below 0 g for each car (first drop: s = 0 to '
        '%.0f m; camelback: to %.0f m; ejector hill: to %.0f m):'
        % (s[i_v1], s[i_tr], s[i_turn0]))
    say()
    rows = []
    for (lab, lo, hi) in (('first drop', 0, s[i_v1]),
                          ('camelback', s[i_v1], s[i_tr]),
                          ('ejector hill', s[i_tr], s[i_turn0])):
        row = [lab, '%.2f' % r.airtime(pt, 0, lo, hi)[0]]
        for key, n, L in TRAINS:
            res = trains[key]
            row.append(' / '.join('%.2f' % r.airtime(res, i, lo, hi)[0]
                                  for i in (0, mid_car_index(key), n - 1)))
        rows.append(row)
    table(['element', 'point'] + ['%s front / middle / rear' % k
                                  for k, n, L in TRAINS], rows)
    plot_speed('layout', sh, pt, trains)
    return sh, pt, trains


def liftcrest():
    say('## Lift crest and first drop, three crest designs')
    say()
    say('Lift at 3.5 m/s (a fixed-speed section; the train is held at that '
        'speed while its rear car is on it, unless gravity pulls it faster), '
        'then a crest designed for the point at the force given, a 45 m drop '
        'and a 3.5 g pull-out. Values while the car is between the start '
        'and the bottom of the drop.')
    say()
    rows = []
    for crest in (0.3, 0.6, 0.75):
        name = 'liftcrest-%.2f' % crest
        sh = load(name)
        pt, trains = ride_all(sh)
        n_pt = [c[1] for c in pt['cars'][0]]
        i_low = min(i for i in range(sh.n) if sh.hy[i] < -44.0)
        hi = sh.s[i_low]
        # tightest crest radius of the heartline before the drop gets steep
        i_d = min(i for i in range(sh.n) if sh.c['pitch'][i] < -30)
        i_t = min(range(i_d), key=lambda i: sh.kN[i])
        rad = -1.0 / sh.kN[i_t]
        x_t = sh.s[i_t]
        x30 = sh.s[i_d]
        rows.append(['%.2f' % crest, '%.1f' % rad, 'point', '-',
                     '%.2f' % r.speed_at(pt, x30),
                     '%.2f' % r.speed_at(pt, x_t),
                     '%.2f' % r.extreme(pt, 0, 0, hi, 'min'),
                     '%.2f' % r.airtime(pt, 0, 0, hi)[0],
                     '%.2f' % r.airtime(pt, 0, 0, hi)[2]])
        for key, n, L in TRAINS:
            res = trains[key]
            for cname, idx in (('front', 0), ('middle', mid_car_index(key)),
                               ('rear', n - 1)):
                a = r.airtime(res, idx, 0, hi)
                off = r.Train(n, L).offsets()[idx]
                rows.append(['', '', key, cname,
                             '%.2f' % r.speed_at(res, x30) if idx == 0
                             else '',
                             '%.2f' % r.speed_at(res, x_t - off),
                             '%.2f' % r.extreme(res, idx, 0, hi, 'min'),
                             '%.2f' % a[0], '%.2f' % a[2]])
    table(['crest force for the point (g)', 'tightest crest radius (m)',
           'train', 'car',
           'train speed when its centre passes pitch -30 deg (m/s)',
           'speed when this car passes the tightest point (m/s)', 'lowest g',
           'below 0 g (s)', 'ejector (s)'], rows)


# ---------------------------------------------------------------------------
def element_length(sh, pt):
    """Length of track where the point feels less than +0.1 g (the airtime
    zone) or, for a loop, more than... returns the crest zone length."""
    n_pt = [c[1] for c in pt['cars'][0]]
    xs = [sh.s[i] for i in range(sh.n) if n_pt[i] < 0.1]
    return xs[-1] - xs[0] if xs else float('nan')


def crest_radius(sh, pt, x):
    """Radius of the heartline at x."""
    k = sh.at(sh.kN, x)
    return 1.0 / abs(k)


def scaling():
    say('## How the difference grows with train length')
    say()
    for name in ('camelback', 'loop'):
        sh = load(name)
        pt = r.point_as_train(sh)
        i_top = max(range(sh.n), key=lambda i: sh.hy[i])
        x = sh.s[i_top]
        R = crest_radius(sh, pt, x)
        n_point = r.force_when_car_at(pt, 0, x)[0]
        if name == 'camelback':
            zone = element_length(sh, pt)
            say('Camelback: the point feels under +0.1 g over %.1f m of '
                'track; heartline radius at the crest %.1f m; point %.3f g '
                'at the crest.' % (zone, R, n_point))
        else:
            # the loop's "element": the length of the top half (pitch beyond
            # +-90 degrees, i.e. upside down)
            xs = [sh.s[i] for i in range(sh.n) if abs(sh.c['roll'][i]) > 90]
            zone = xs[-1] - xs[0]
            say('Loop: the top half (upside down) is %.1f m of track; '
                'heartline radius at the top %.1f m; point %.3f g at the '
                'top.' % (zone, R, n_point))
        say()
        rows = []
        for length in (5, 10, 15, 20, 25, 30, 40):
            ncar = max(1, int(round(length / 2.5)))
            res = r.ride_train(sh, r.Train(ncar, length / ncar))
            dv = r.speed_at(res, x) - r.speed_at(pt, x)
            f = r.force_when_car_at(res, 0, x)[0]
            b = r.force_when_car_at(res, ncar - 1, x)[0]
            m = (r.force_when_car_at(res, ncar // 2, x)[0] if ncar % 2 else
                 0.5 * (r.force_when_car_at(res, ncar // 2 - 1, x)[0] +
                        r.force_when_car_at(res, ncar // 2, x)[0]))
            # closed form for the middle: chain on a circle of radius R
            offs = r.Train(ncar, length / ncar).offsets()
            dh = R * (1 - sum(math.cos(o / R) for o in offs) / ncar)
            sign = 1 if name == 'camelback' else -1
            dn_cf = -sign * 2 * dh / R
            rows.append(['%d (%d cars)' % (length, ncar),
                         '%.2f' % (length / zone), '%+.2f' % dv,
                         '%+.3f' % (f - n_point), '%+.3f' % (m - n_point),
                         '%+.3f' % (b - n_point), '%+.3f' % dn_cf])
        table(['train length (m)', 'train / element length',
               'speed diff at the top (m/s)', 'front car minus point (g)',
               'middle minus point (g)', 'rear car minus point (g)',
               'circle formula, middle (g)'], rows)


# ---------------------------------------------------------------------------
def synthetic_circle(R=20.0, a0=1.0, ds=0.05, heart=0.0):
    """A crest made of a straight climb at a0 radians, a circular arc of
    radius R over the top and a straight descent; heartline = rail."""
    cols = {k: [] for k in ('dist_m', 'heart_dist_m', 'heart_y', 'pos_y',
                            'dir_x', 'dir_y', 'dir_z', 'norm_x', 'norm_y',
                            'norm_z', 'lat_x', 'lat_y', 'lat_z', 'speed_mps',
                            'pitch')}
    straight = 20.0   # longer than half the longest train, so every train
                      # starts with all its cars on the straight
    arc = 2 * a0 * R
    total = 2 * straight + arc
    x = y = 0.0
    n = int(total / ds)
    for i in range(n + 1):
        s = i * ds
        if s < straight:
            th = a0
        elif s < straight + arc:
            th = a0 - (s - straight) / R
        else:
            th = -a0
        if i:
            x += ds * math.cos(th)
            y += ds * math.sin(th)
        cols['dist_m'].append(s)
        cols['heart_dist_m'].append(s)
        cols['heart_y'].append(y)
        cols['pos_y'].append(y)
        for k, v in zip('xyz', (math.cos(th), math.sin(th), 0.0)):
            cols['dir_' + k].append(v)
        for k, v in zip('xyz', (math.sin(th), -math.cos(th), 0.0)):
            cols['norm_' + k].append(v)
        for k, v in zip('xyz', (0.0, 0.0, 1.0)):
            cols['lat_' + k].append(v)
        cols['speed_mps'].append(0.0)
        cols['pitch'].append(math.degrees(th))
    return r.Shape(cols, 0.0, 0.0, 0.0, [], 'circle'), straight + arc / 2


def closed_form_check():
    say('## Check against the closed form: a train on a circular crest')
    say()
    say('No friction, no drag, heartline on the rail, crest radius 20 m, '
        'arc from +57 to -57 degrees (40 m long, so every car of a 30 m '
        'train is on it when the train is centred), entry 25 m/s on a 20 m '
        'straight climb before the arc. Train centred on the top: its centre '
        'of mass sits '
        'R (1 - mean cos(offset/R)) below the top, so it has that much more '
        'speed head, and every car feels n = cos(phi) - v^2/(g R).')
    say()
    rows = []
    for length, ncar in ((10, 4), (20, 8), (30, 10)):
        sh, x = synthetic_circle()
        sh.c['speed_mps'][0] = 25.0
        tr = r.Train(ncar, length / ncar)
        res = r.ride_train(sh, tr, v0=25.0)
        pt = r.point_as_train(sh)
        R = 20.0
        offs = tr.offsets()
        v2p = 25.0 ** 2 - 2 * r.G * sh.at(sh.y09, x)
        dh = R * (1 - sum(math.cos(o / R) for o in offs) / ncar)
        v2t = v2p + 2 * r.G * dh
        num_v = r.speed_at(res, x)
        # the car just behind the centre (or at it), when the train is centred
        idx = ncar // 2
        phi = offs[idx] / R
        n_cf = math.cos(phi) - v2t / (r.G * R)
        k = min(range(len(res['s'])), key=lambda i: abs(res['s'][i] - x))
        num_f = res['cars'][idx][k][1]
        rows.append(['%d m, %d cars' % (length, ncar),
                     '%.4f' % math.sqrt(v2p), '%.4f' % r.speed_at(pt, x),
                     '%.4f' % math.sqrt(v2t), '%.4f' % num_v,
                     '%.4f' % n_cf, '%.4f' % num_f])
    table(['train', 'point speed, formula', 'point speed, prototype',
           'train speed (centred), formula', 'train speed, prototype',
           'centre car force (g), formula', 'centre car force, prototype'],
          rows)
    say('(Centre car: the car at the centre, or just behind it for an even '
        'number of cars, when the train is centred on the top.)')
    say()


# ---------------------------------------------------------------------------
def plot_speed(name, sh, pt, trains):
    series = [('point', pt['s'], pt['v'])]
    for key, n, L in TRAINS:
        series.append((key, trains[key]['s'], trains[key]['v']))
    svg_lines(HERE / 'plots' / ('%s-speed.svg' % name),
              '%s: speed when the train centre (or the point) is at s' % name,
              'centre-line distance s (m)', 'speed (m/s)', series)


def plot_force(name, pt, trains, key):
    n = [t for t in TRAINS if t[0] == key][0][1]
    res = trains[key]
    series = [('point', [c[0] for c in pt['cars'][0]],
               [c[1] for c in pt['cars'][0]])]
    for cname, idx in (('front', 0), ('middle', n // 2), ('rear', n - 1)):
        rows = [c for c in res['cars'][idx] if c is not None]
        series.append(('%s %s car' % (key, cname), [c[0] for c in rows],
                       [c[1] for c in rows]))
    svg_lines(HERE / 'plots' / ('%s-normal-%s.svg' % (name, key)),
              '%s: normal force felt at each place, %s train' % (name, key),
              'centre-line distance s (m)', 'normal force (g)', series)


def svg_lines(path, title, xl, yl, series):
    path.parent.mkdir(exist_ok=True)
    W, H, ml, mr, mt, mb = 760, 360, 60, 150, 30, 45
    xs = [x for _, sx, _ in series for x in sx]
    ys = [y for _, _, sy in series for y in sy]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    pad = 0.05 * (y1 - y0 or 1)
    y0, y1 = y0 - pad, y1 + pad

    def X(x):
        return ml + (x - x0) / (x1 - x0) * (W - ml - mr)

    def Y(y):
        return H - mb - (y - y0) / (y1 - y0) * (H - mt - mb)
    cols = ['#222222', '#1f77b4', '#d62728', '#2ca02c', '#9467bd']
    dash = ['', '', '6,3', '2,2', '8,3,2,3']
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'font-family="sans-serif" font-size="11">' % (W, H),
           '<rect width="100%" height="100%" fill="white"/>',
           '<text x="%d" y="18" font-size="13">%s</text>' % (ml, title)]
    for k in range(6):
        yv = y0 + k * (y1 - y0) / 5
        out.append('<line x1="%d" x2="%d" y1="%.1f" y2="%.1f" '
                   'stroke="#ddd"/>' % (ml, W - mr, Y(yv), Y(yv)))
        out.append('<text x="%d" y="%.1f" text-anchor="end">%.1f</text>'
                   % (ml - 4, Y(yv) + 4, yv))
    for k in range(6):
        xv = x0 + k * (x1 - x0) / 5
        out.append('<text x="%.1f" y="%d" text-anchor="middle">%.0f</text>'
                   % (X(xv), H - mb + 15, xv))
    out.append('<text x="%d" y="%d" text-anchor="middle">%s</text>'
               % ((ml + W - mr) / 2, H - 8, xl))
    out.append('<text x="14" y="%d" transform="rotate(-90 14 %d)" '
               'text-anchor="middle">%s</text>' % (H / 2, H / 2, yl))
    for j, (lab, sx, sy) in enumerate(series):
        step = max(1, len(sx) // 1500)
        pts = ' '.join('%.1f,%.1f' % (X(a), Y(b))
                       for a, b in list(zip(sx, sy))[::step])
        out.append('<polyline fill="none" stroke="%s" stroke-width="1.4" '
                   'stroke-dasharray="%s" points="%s"/>'
                   % (cols[j % 5], dash[j % 5], pts))
        out.append('<line x1="%d" x2="%d" y1="%d" y2="%d" stroke="%s" '
                   'stroke-width="2" stroke-dasharray="%s"/>'
                   % (W - mr + 10, W - mr + 35, mt + 15 * j, mt + 15 * j,
                      cols[j % 5], dash[j % 5]))
        out.append('<text x="%d" y="%d">%s</text>'
                   % (W - mr + 40, mt + 15 * j + 4, lab))
    out.append('</svg>')
    path.write_text('\n'.join(out) + '\n')


def main():
    say('# R7 results (written by run_r7.py; do not edit by hand)')
    say()
    say('Settings of every track: heartline %.2f m, friction %.3f, '
        'resistance %.0e (the program defaults). Trains: %s.' % (
            SET['heart'], SET['friction'], SET['resistance'],
            '; '.join('%s %d cars x %.1f m = %.1f m' % (k, n, L, n * L)
                      for k, n, L in TRAINS)))
    say()
    validation()
    closed_form_check()
    camelback()
    loop()
    layout()
    liftcrest()
    scaling()
    if '--redesign' in sys.argv:
        import redesign
        redesign.main(say, table)
    (HERE / 'results.txt').write_text('\n'.join(OUT) + '\n')


if __name__ == '__main__':
    main()
