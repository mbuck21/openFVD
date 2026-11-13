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
"""Fit friction and drag from recorded coast-downs on a level straight (R12, E1).

    py -3 tools/nl2fit.py --speed-unit kmh --skip-start 60 run10.csv run40.csv
    py -3 tools/nl2fit.py --selftest

Input: CSV files written by tools/nl2log.py (columns frame, speed, x, y, z). The
program's rule, per kilogram on a level track, is

    d(v^2/2)/ds = -(g * friction + drag * v^2)        (s = metres run)

so with u = v^2 the recording obeys, between its first sample and sample i,

    u_i - u_1 = -2 g friction (s_i - s_1) - 2 drag * integral(u ds).

That is linear in friction and drag. Several files (for example 10 m/s and
40 m/s starts) are fitted together, each with its own starting value, which
is what separates the two numbers: one slow run is nearly all friction, a
fast one adds drag. The distance s is the position projected on the straight
line from the first to the last sample, so jitter in the position does not
add length.

The units of NoLimits 2's telemetry are not published (experiment E0 finds
them out): say them with --speed-unit (mps, kmh, mph) and --pos-scale (metres
per position unit, default 1). A wrong unit gives wrong numbers silently.

The reported errors are one standard error from the scatter of the points. They
assume independent errors and are therefore optimistic for samples taken 60
times a second; trust the difference between your own repeated runs more.
"""
import argparse
import csv
import math
import random
import sys

G = 9.80665
# noise of the self-test: speed (m/s) and position (m), per sample
SIGMA_V, SIGMA_P = 0.1, 0.05
SPEED_TO_MPS = {'mps': 1.0, 'kmh': 1.0 / 3.6, 'mph': 0.44704}


def load(path, speed_unit, pos_scale):
    """Returns lists (s, v) in metres and m/s, one entry per new frame."""
    k = SPEED_TO_MPS[speed_unit]
    with open(path, newline='') as fh:
        lines = [ln for ln in fh if not ln.startswith('#')]
    rows = list(csv.DictReader(lines))
    seen = set()
    pts = []
    for r in rows:
        frame = r['frame']
        if frame in seen:
            continue
        seen.add(frame)
        pts.append((float(r['x']) * pos_scale, float(r['y']) * pos_scale,
                    float(r['z']) * pos_scale, float(r['speed']) * k))
    if len(pts) < 10:
        raise ValueError('%s: fewer than 10 rows' % path)
    p0, p1 = pts[0][:3], pts[-1][:3]
    d = [b - a for a, b in zip(p0, p1)]
    n = math.sqrt(sum(c * c for c in d))
    if n < 1.0:
        raise ValueError('%s: the train moved less than 1 m' % path)
    d = [c / n for c in d]
    s = [sum((p[i] - p0[i]) * d[i] for i in range(3)) for p in pts]
    v = [p[3] for p in pts]
    return s, v


def select(s, v, skip_start, end, min_speed):
    out = [(a, b) for a, b in zip(s, v)
           if a >= skip_start and (end is None or a <= end) and b >= min_speed]
    # keep the first unbroken run: stop at the first sample that turns round
    keep = []
    for a, b in out:
        if keep and a < keep[-1][0] - 0.5:
            break
        keep.append((a, b))
    return [a for a, _ in keep], [b for _, b in keep]


def design(s, v):
    """y, x_fric, x_drag for one file, each with its mean removed."""
    u = [x * x for x in v]
    integ = [0.0]
    for i in range(1, len(s)):
        integ.append(integ[-1] + 0.5 * (u[i] + u[i - 1]) * (s[i] - s[i - 1]))
    y = u
    x1 = [-2.0 * G * x for x in s]            # coefficient: friction
    x2 = [-2.0 * x for x in integ]            # coefficient: drag
    m = len(s)

    def centred(a):
        mu = sum(a) / m
        return [x - mu for x in a]
    return centred(y), centred(x1), centred(x2)


def solve(files, fit, fixed_friction, fixed_drag):
    """files: list of (s, v). Returns dict with friction, drag, errors, rms."""
    Y, X1, X2 = [], [], []
    for s, v in files:
        y, x1, x2 = design(s, v)
        Y += y
        X1 += x1
        X2 += x2
    n = len(Y)
    if fit == 'both':
        a11 = sum(x * x for x in X1)
        a12 = sum(x * z for x, z in zip(X1, X2))
        a22 = sum(z * z for z in X2)
        b1 = sum(x * y for x, y in zip(X1, Y))
        b2 = sum(z * y for z, y in zip(X2, Y))
        det = a11 * a22 - a12 * a12
        if det <= 1e-12 * a11 * a22:
            raise ValueError('friction and drag cannot be told apart in this '
                             'data (use a fast and a slow run, or --fit '
                             'friction)')
        f = (a22 * b1 - a12 * b2) / det
        k = (a11 * b2 - a12 * b1) / det
        inv = [[a22 / det, -a12 / det], [-a12 / det, a11 / det]]
        dof = n - 2 - len(files)
    elif fit == 'friction':
        k = fixed_drag
        yy = [y - k * z for y, z in zip(Y, X2)]
        a11 = sum(x * x for x in X1)
        f = sum(x * y for x, y in zip(X1, yy)) / a11
        inv = [[1.0 / a11, 0.0], [0.0, 0.0]]
        dof = n - 1 - len(files)
    else:
        f = fixed_friction
        yy = [y - f * x for y, x in zip(Y, X1)]
        a22 = sum(z * z for z in X2)
        k = sum(z * y for z, y in zip(X2, yy)) / a22
        inv = [[0.0, 0.0], [0.0, 1.0 / a22]]
        dof = n - 1 - len(files)
    res = [y - f * x - k * z for y, x, z in zip(Y, X1, X2)]
    sse = sum(r * r for r in res)
    var = sse / max(dof, 1)
    return dict(friction=f, drag=k, friction_err=math.sqrt(var * inv[0][0]),
                drag_err=math.sqrt(var * inv[1][1]),
                rms_residual=math.sqrt(sse / n), points=n)


def report(paths, args):
    files = []
    for p in paths:
        s, v = load(p, args.speed_unit, args.pos_scale)
        s, v = select(s, v, args.skip_start, args.end, args.min_speed)
        if len(s) < 10:
            raise ValueError('%s: fewer than 10 points left after trimming'
                             % p)
        print('%s: %d points, %.0f m to %.0f m, speed %.2f to %.2f m/s'
              % (p, len(s), s[0], s[-1], v[0], v[-1]))
        files.append((s, v))
    out = solve(files, args.fit, args.friction, args.drag)
    print('friction  %.4f  (+/- %.4f)   m per m' % (out['friction'],
                                                   out['friction_err']))
    print('drag      %.3g  (+/- %.2g)   1/m' % (out['drag'], out['drag_err']))
    print('rms of the fit in v^2: %.3f m^2/s^2 over %d points'
          % (out['rms_residual'], out['points']))
    return out


# ------------------------------------------------------------ self-test --
def synthetic(v0, friction, drag, length, rng, sigma_v, sigma_p, hz=60.0,
              unit='mps'):
    """A noisy coast-down as the logger would write it, as a list of rows."""
    dt = 1.0 / hz
    s, v = 0.0, v0
    rows = []
    frame = 0
    while s < length and v > 0.5:
        k = 1.0 / SPEED_TO_MPS[unit]
        rows.append({'frame': frame,
                     'speed': (v + rng.gauss(0, sigma_v)) * k,
                     'x': 12.0 + s + rng.gauss(0, sigma_p),
                     'y': 5.0 + rng.gauss(0, sigma_p),
                     'z': -3.0 + rng.gauss(0, sigma_p)})
        frame += 1
        # a = -(g friction + drag v^2); two half steps for accuracy
        a1 = G * friction + drag * v * v
        vm = max(v - 0.5 * dt * a1, 0.0)
        a2 = G * friction + drag * vm * vm
        s += 0.5 * (v + max(v - dt * a2, 0.0)) * dt
        v = max(v - dt * a2, 0.0)
    return rows


def selftest():
    import tempfile
    from pathlib import Path
    tol_f, tol_k = 0.001, 1e-5          # the stated tolerance
    cases = [(0.026, 1e-4, 'mps', 1.0), (0.017, 2e-5, 'kmh', 1.0),
             (0.030, 0.0, 'mps', 1.0), (0.026, 2e-5, 'mph', 0.01)]
    failures = 0
    print('tolerance: friction +/-%.4f, drag +/-%.0e' % (tol_f, tol_k))
    for n, (f, k, unit, pscale) in enumerate(cases):
        rng = random.Random(100 + n)
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for v0, length in ((10.0, 800.0), (40.0, 800.0)):
                rows = synthetic(v0, f, k, length, rng, SIGMA_V, SIGMA_P,
                                 unit=unit)
                for r in rows:
                    for c in 'xyz':
                        r[c] = r[c] / pscale
                p = Path(tmp) / ('run%d.csv' % int(v0))
                with open(p, 'w', newline='') as fh:
                    fh.write('# synthetic\n')
                    w = csv.DictWriter(fh, fieldnames=['frame', 'speed', 'x',
                                                       'y', 'z'])
                    w.writeheader()
                    w.writerows(rows)
                paths.append(str(p))
            ns = argparse.Namespace(speed_unit=unit, pos_scale=pscale,
                                    skip_start=50.0, end=None, min_speed=1.0,
                                    fit='both', friction=0.0, drag=0.0)
            print('case %d: friction %.3f drag %.0e, speed unit %s, '
                  'position scale %g' % (n + 1, f, k, unit, pscale))
            out = report(paths, ns)
        ef, ek = out['friction'] - f, out['drag'] - k
        print('  recovered friction %.5f (error %+.5f), drag %.3g '
              '(error %+.2g)' % (out['friction'], ef, out['drag'], ek))
        if abs(ef) > tol_f or abs(ek) > tol_k:
            failures += 1
            print('  FAIL: outside the tolerance')
    # friction only, from one slow run, with drag known
    rng = random.Random(7)
    rows = synthetic(10.0, 0.026, 2e-5, 800.0, rng, SIGMA_V, SIGMA_P)
    s = [r['x'] - rows[0]['x'] for r in rows]
    v = [r['speed'] for r in rows]
    s, v = select(s, v, 20.0, None, 1.0)
    out = solve([(s, v)], 'friction', 0.0, 2e-5)
    print('friction-only fit of one slow run: %.4f (true 0.026)'
          % out['friction'])
    if abs(out['friction'] - 0.026) > tol_f:
        failures += 1
        print('  FAIL: outside the tolerance')
    print('PASS' if not failures else 'FAIL (%d)' % failures)
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('files', nargs='*', help='CSV files from tools/nl2log.py')
    ap.add_argument('--speed-unit', choices=sorted(SPEED_TO_MPS),
                    default='mps', help='unit of the recorded speed (find it '
                    'out with experiment E0; default mps)')
    ap.add_argument('--pos-scale', type=float, default=1.0,
                    help='metres per unit of the recorded position')
    ap.add_argument('--skip-start', type=float, default=0.0,
                    help='metres to leave out after the first sample '
                         '(the transport section)')
    ap.add_argument('--end', type=float, default=None,
                    help='last metre to use, from the first sample')
    ap.add_argument('--min-speed', type=float, default=1.0,
                    help='leave out samples slower than this, m/s')
    ap.add_argument('--fit', choices=['both', 'friction', 'drag'],
                    default='both')
    ap.add_argument('--friction', type=float, default=0.0,
                    help='friction to hold when fitting drag only')
    ap.add_argument('--drag', type=float, default=0.0,
                    help='drag to hold when fitting friction only')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args()
    if args.selftest:
        sys.exit(selftest())
    if not args.files:
        ap.error('give one or more CSV files')
    try:
        report(args.files, args)
    except (ValueError, OSError, KeyError) as e:
        sys.exit('error: %s' % e)


if __name__ == '__main__':
    main()
