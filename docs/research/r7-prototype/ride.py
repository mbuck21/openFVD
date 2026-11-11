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
"""Ride a fixed track shape with one point or with a train of cars.

The shape is a table of points along the centre line (from `fvdcore nodes
--frame anchor`, or made from fvd2d.py nodes). Everything is per kilogram, as
in the program.

Point model (the program's rule on a fixed shape): energy E per kg; each step
of rail length ds takes resistance * v^2 * ds out of E (the program's v^3 per
millisecond); then v = sqrt(2 (E - g (y09 + friction * s))), with y09 the
height of the point 0.9 of the way from the heartline to the rail and s the
rail length run. On a fixed-speed section v is held and E reset.

Train model: N cars, each a point mass at its own place on the track, centres
L apart along the centre line, rigidly coupled along the track so that all
share one rail speed. The energy of the whole train is the average of its
cars': E = v^2/2 + g (mean of the cars' y09 + friction * s), with s the rail
length the train has run. Same friction and drag per kg. On a chain (fixed-
speed section) the train is held at the chain speed while its rear car is on
the chain and gravity alone would make it slower; if gravity makes it faster
it runs free. Off the ends of the dumped track the track is extended along
its end direction (a straight line), so cars can stand on it.

Felt forces at a car (in g), from the shared speed and the shape where that
car is: normal = -norm_y + kN * vh^2 / g, lateral = -lat_y + kL * vh * v / g,
with kN, kL the heartline's turning per metre of heartline toward the rider's
head and sideways (from the frame's direction vectors), vh the heartline speed
(the rail speed times the heartline's length per metre of rail) and v the
rail speed, as in secforced::updateSection(). Along the track: a/g + dir_y,
with a the train's acceleration (positive: pressed into the seat back).
"""
import bisect
import gzip
import math
from pathlib import Path

G = 9.80665
HERE = Path(__file__).resolve().parent


class Shape:
    """A track shape on a grid of centre-line distances."""

    def __init__(self, cols, heart, friction, resistance, fixed=None,
                 name=''):
        self.name = name
        self.heart = heart
        self.friction = friction
        self.resistance = resistance
        self.c = cols
        self.s = cols['dist_m']
        self.n = len(self.s)
        # fixed-speed stretches: list of (s_from, s_to, speed)
        self.fixed = fixed or []
        self._derive()

    @classmethod
    def from_dump(cls, path, heart, friction, resistance, fixed_sections=(),
                  name=''):
        opener = gzip.open if str(path).endswith('.gz') else open
        with opener(path, 'rt') as fh:
            head = fh.readline().rstrip('\n').split('\t')
            rows = [line.rstrip('\n').split('\t') for line in fh if line.strip()]
        cols = {h: [float(r[i]) for r in rows] for i, h in enumerate(head)}
        fixed = []
        for sec in fixed_sections:
            idx = [i for i, v in enumerate(cols['section']) if int(v) == sec]
            s0 = cols['dist_m'][idx[0]]
            s1 = cols['dist_m'][idx[-1]]
            fixed.append((s0, s1, cols['speed_mps'][idx[0]]))
        # drop repeated distances (the first node of a section repeats the
        # previous section's last node)
        keep = [0]
        for i in range(1, len(cols['dist_m'])):
            if cols['dist_m'][i] > cols['dist_m'][keep[-1]] + 1e-6:
                keep.append(i)
        cols = {h: [v[i] for i in keep] for h, v in cols.items()}
        return cls(cols, heart, friction, resistance, fixed, name)

    def _derive(self):
        c, n = self.c, self.n
        s, sh = c['dist_m'], c['heart_dist_m']
        self.y09 = [hy + 0.9 * (py - hy)
                    for hy, py in zip(c['heart_y'], c['pos_y'])]
        self.kN = [0.0] * n
        self.kL = [0.0] * n
        self.rh = [1.0] * n
        for i in range(n):
            a, b = max(i - 1, 0), min(i + 1, n - 1)
            dsh = sh[b] - sh[a]
            ds = s[b] - s[a]
            dd = [c['dir_' + k][b] - c['dir_' + k][a] for k in 'xyz']
            nrm = [c['norm_' + k][i] for k in 'xyz']
            lat = [c['lat_' + k][i] for k in 'xyz']
            self.kN[i] = -sum(p * q for p, q in zip(dd, nrm)) / dsh
            self.kL[i] = sum(p * q for p, q in zip(dd, lat)) / dsh
            self.rh[i] = dsh / ds
        self.normy = c['norm_y']
        self.laty = c['lat_y']
        self.diry = c['dir_y']
        self.hy = c['heart_y']

    # -- lookups at any centre-line distance --------------------------------
    def _locate(self, x):
        s = self.s
        if x <= s[0]:
            return 0, 0.0, x - s[0]
        if x >= s[-1]:
            return self.n - 2, 1.0, x - s[-1]
        i = bisect.bisect_right(s, x) - 1
        i = min(i, self.n - 2)
        return i, (x - s[i]) / (s[i + 1] - s[i]), 0.0

    def at(self, arr, x):
        i, f, _ = self._locate(x)
        return arr[i] + f * (arr[i + 1] - arr[i])

    def height(self, x):
        """y09 at x, extended in a straight line beyond both ends."""
        i, f, out = self._locate(x)
        if out < 0:
            return self.y09[0] + self.diry[0] * out
        if out > 0:
            return self.y09[-1] + self.diry[-1] * out
        return self.y09[i] + f * (self.y09[i + 1] - self.y09[i])

    def on_track(self, x):
        return self.s[0] <= x <= self.s[-1]

    def forces(self, x, v):
        """normal and lateral g at x for rail speed v."""
        kN = self.at(self.kN, x)
        kL = self.at(self.kL, x)
        rh = self.at(self.rh, x)
        vh = v * rh
        n = -self.at(self.normy, x) + kN * vh * vh / G
        lat = -self.at(self.laty, x) - kL * vh * v / G
        return n, lat

    def chain_speed(self, x):
        """The fixed speed at x, extending a chain at the start backwards."""
        for s0, s1, sp in self.fixed:
            if (x <= s1 and (x >= s0 or s0 <= self.s[0] + 1e-9)):
                return sp
        return None


def ride_point(shape, v0=None, E0=None):
    """The program's point on the fixed shape. Returns per-grid lists."""
    s = shape.s
    v = shape.c['speed_mps'][0] if v0 is None else v0
    E = (0.5 * v * v + G * (shape.y09[0] + shape.friction * s[0])
         if E0 is None else E0)
    out_v = [v]
    for i in range(1, shape.n):
        ds = s[i] - s[i - 1]
        cs = shape.chain_speed(s[i])
        if cs is not None:
            v = cs
            E = 0.5 * v * v + G * (shape.y09[i] + shape.friction * s[i])
        else:
            E -= shape.resistance * v * v * ds
            e = E - G * (shape.y09[i] + shape.friction * s[i])
            if e <= 0:
                raise RuntimeError('point stalls at s=%.1f' % s[i])
            v = math.sqrt(2 * e)
        out_v.append(v)
    return out_v


class Train:
    def __init__(self, ncars, car_len):
        self.N = ncars
        self.L = car_len

    @property
    def length(self):
        return self.N * self.L

    def offsets(self):
        """Car centres relative to the train's centre, front car first."""
        return [(self.N / 2.0 - j - 0.5) * self.L for j in range(self.N)]


def ride_train(shape, train, v0=None, step=0.1, s_start=None, s_end=None):
    """Ride the train with its centre from s_start to s_end.

    Returns a dict of lists along the centre position: s (centre), v, acc,
    and per car (front first) the normal, lateral and along-track g and the
    car's position."""
    offs = train.offsets()
    s0 = shape.s[0] if s_start is None else s_start
    s1 = shape.s[-1] if s_end is None else s_end
    v = shape.c['speed_mps'][0] if v0 is None else v0
    fr, k = shape.friction, shape.resistance

    def ybar(x):
        return sum(shape.height(x + o) for o in offs) / len(offs)

    rear = offs[-1]
    E = 0.5 * v * v + G * (ybar(s0) + fr * s0)
    res = dict(s=[], v=[], cars=[[] for _ in offs])
    x = s0
    prev_v = v
    while x <= s1 + 1e-9:
        if x > s0:
            E -= k * v * v * step
            e = E - G * (ybar(x) + fr * x)
            vfree = math.sqrt(2 * e) if e > 0 else 0.0
            cs = shape.chain_speed(x + rear)
            if cs is not None and vfree < cs:
                v = cs
                E = 0.5 * v * v + G * (ybar(x) + fr * x)
            else:
                if e <= 0:
                    raise RuntimeError('train stalls at s=%.1f' % x)
                v = vfree
        res['s'].append(x)
        res['v'].append(v)
        x += step
    # accelerations from the speed profile: a = v dv/ds
    vs, ss = res['v'], res['s']
    acc = []
    for i in range(len(vs)):
        a, b = max(i - 1, 0), min(i + 1, len(vs) - 1)
        acc.append(vs[i] * (vs[b] - vs[a]) / (ss[b] - ss[a]) if b > a else 0.0)
    res['acc'] = acc
    for j, o in enumerate(offs):
        rows = res['cars'][j]
        for x, v, a in zip(ss, vs, acc):
            p = x + o
            if not shape.on_track(p):
                rows.append(None)
                continue
            n, lat = shape.forces(p, v)
            along = a / G + shape.at(shape.diry, p)
            rows.append((p, n, lat, along))
    return res


def point_as_train(shape):
    """The point model expressed like ride_train's result (one car)."""
    v = ride_point(shape)
    res = dict(s=list(shape.s), v=v, cars=[[]])
    for i, (x, vv) in enumerate(zip(shape.s, v)):
        a_, b_ = max(i - 1, 0), min(i + 1, shape.n - 1)
        acc = vv * (v[b_] - v[a_]) / (shape.s[b_] - shape.s[a_])
        n, lat = shape.forces(x, vv)
        res['cars'][0].append((x, n, lat, acc / G + shape.diry[i]))
    res['acc'] = None
    return res


def airtime(res, car, s_from=-1e9, s_to=1e9):
    """Seconds below 0 g, floater (+0.1 to -0.5 g) and ejector (< -0.5 g).

    Each step's time is its length over the shared speed; only while the
    car is between s_from and s_to."""
    ss, vs, rows = res['s'], res['v'], res['cars'][car]
    t_neg = t_flo = t_ej = 0.0
    for i in range(1, len(ss)):
        r = rows[i]
        if r is None or not (s_from <= r[0] <= s_to):
            continue
        dt = (ss[i] - ss[i - 1]) / (0.5 * (vs[i] + vs[i - 1]))
        n = r[1]
        if n < 0.0:
            t_neg += dt
        if -0.5 <= n < 0.1:
            t_flo += dt
        if n < -0.5:
            t_ej += dt
    return t_neg, t_flo, t_ej


def speed_at(res, x):
    """The train's (or point's) speed when its centre is at x."""
    ss = res['s']
    i = min(max(bisect.bisect_left(ss, x), 1), len(ss) - 1)
    f = (x - ss[i - 1]) / (ss[i] - ss[i - 1])
    return res['v'][i - 1] + f * (res['v'][i] - res['v'][i - 1])


def force_when_car_at(res, car, x):
    """The car's (normal, lateral, along) when the car passes x."""
    rows = res['cars'][car]
    best = None
    for i in range(1, len(rows)):
        a, b = rows[i - 1], rows[i]
        if a is None or b is None:
            continue
        if a[0] <= x <= b[0]:
            f = (x - a[0]) / (b[0] - a[0]) if b[0] > a[0] else 0.0
            best = tuple(a[k] + f * (b[k] - a[k]) for k in (1, 2, 3))
            break
    return best


def extreme(res, car, s_from, s_to, which):
    """Min or max normal g felt by a car while it is between s_from, s_to."""
    vals = [r[1] for r in res['cars'][car]
            if r is not None and s_from <= r[0] <= s_to]
    return (min if which == 'min' else max)(vals)
