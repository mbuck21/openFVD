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
"""Shared parts of the X1 trial: the stand-in designer, the project writer,
the program's train ride, and the measures.

The stand-in. The program has no input for "design with this speed", so the
design is made outside it, by a copy of the program's force step in the
vertical plane (R7's fvd2d.py, extended here), and handed to the program as a
fixed shape: a NoLimits CSV section (a list of heartline points with their
direction and lateral vectors) written through the project text and
`fvdcore fromtext`. The program then rides that shape with `fvdcore ride`;
every force a car "feels" in this trial comes from that ride, never from the
designer's own arithmetic. validate.py checks the stand-in: the point design
made this way rides like the program's own force-section project.

The three shapes are R7's (docs/research/r7-prototype/projects/shapes.json):
the camelback, the vertical loop and the lift crest designed at 0.60 g, with
the program's defaults (heartline 1.1 m, friction 0.03, resistance 2e-5).
"""
import bisect
import json
import math
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
R7 = REPO / 'docs' / 'research' / 'r7-prototype'
PROJECTS = HERE / 'projects'
RESULTS = HERE / 'results'
WORK = HERE / 'work'           # throw-away files of a run (ignored by git)
EXE = REPO / 'build' / 'dist' / ('fvdcore.exe' if sys.platform == 'win32'
                                 else 'fvdcore')

sys.path.insert(0, str(R7))
import fvd2d  # noqa: E402  (R7's force step)
import make_shapes as ms  # noqa: E402  (R7's force programs)

G = fvd2d.G
HZ = fvd2d.HZ
META = json.loads((R7 / 'projects' / 'shapes.json').read_text())
SET = META['settings']
HEART, FRICTION, RESISTANCE = SET['heart'], SET['friction'], SET['resistance']

# R7's "long" train: 9 cars, 2.2 m apart (19.8 m), the B&M mini-hyper stand-in.
CARS, PITCH = 9, 2.2
# Car offsets from the train's centre, front car first (fvdcore ride's order).
OFFSETS = [(CARS / 2.0 - j - 0.5) * PITCH for j in range(CARS)]
CHOICES = (('centre', CARS // 2), ('front', 0), ('rear', CARS - 1))


# ---- the shapes --------------------------------------------------------------
class ShapeDef:
    """One test element: its force program (against time), its entry speed,
    and the fixed part in front of it (the lift), which is not redesigned."""

    def __init__(self, key, r7_project, steps, v0, lift=None):
        self.key = key
        self.r7_project = r7_project
        self.force = fvd2d.Force(1.0, steps)
        self.v0 = v0
        self.lift = lift            # None, or (seconds, speed): a level held section


def shapes():
    cb, lp, lc = META['camelback'], META['loop'], META['liftcrest-0.60']
    return [
        ShapeDef('camelback', R7 / 'projects' / 'camelback.fvd',
                 ms.camel_steps(cb['t2'], cb['tair'], cb['t4']), ms.CB['v0']),
        ShapeDef('loop', R7 / 'projects' / 'loop.fvd',
                 ms.loop_steps(lp['ta'], lp['td']), ms.LP['v0']),
        ShapeDef('liftcrest', R7 / 'projects' / 'liftcrest-0.60.fvd',
                 ms.layout_drop(lc['t1'], lc['th'], lc['crest']) + [(2.0, 0.0)],
                 ms.LAYOUT_LIFT_SPEED,
                 lift=(ms.LAYOUT_LIFT[0][0], ms.LAYOUT_LIFT_SPEED)),
    ]


def lift_length(sd):
    """Rail length of the level lift in front of the element (0 if none).
    The program advances a held section by speed / 1000 per node."""
    return 0.0 if sd.lift is None else sd.lift[0] * sd.lift[1]


# ---- the designer --------------------------------------------------------------
class TrainAhead:
    """'Solve together' for the FRONT car: the speed when the front car is at
    the head of the design, from the energy of a train whose other cars stand
    on the track already designed (or on the level track in front of it).
    The convention is fvdcore ride's: the train's centre is at the track's
    start (s = 0) at the start speed; the chain rule is R7's."""

    def __init__(self, s0, y_level, v_start, chain=None):
        self.s0 = s0                # global place of the design's first node
        self.y_level = y_level      # mass-point height of the level track before it
        self.v_start = v_start
        self.chain = chain          # (s_end, speed) of a level held section from -inf
        self.S, self.Y = [], []
        self.E = None
        self.v = v_start
        self.last_c = None

    def height(self, s):
        if s <= self.s0 or not self.S:
            return self.y_level
        i = bisect.bisect_right(self.S, s)
        if i >= len(self.S):
            return self.Y[-1]
        a = (s - self.S[i - 1]) / (self.S[i] - self.S[i - 1])
        return self.Y[i - 1] + a * (self.Y[i] - self.Y[i - 1])

    def speed(self, s_front, y09):
        """Add the node just designed (front car at s_front) and return the
        train's speed there."""
        self.S.append(s_front)
        self.Y.append(y09)
        sc = s_front - OFFSETS[0]
        if sc < 0.0:
            return self.v_start
        ybar = sum(self.height(sc + o) for o in OFFSETS) / CARS
        if self.E is None:
            self.E = 0.5 * self.v_start ** 2 + G * (ybar + FRICTION * sc)
            self.last_c = sc
            self.v = self.v_start
        ds = sc - self.last_c
        self.last_c = sc
        self.E -= RESISTANCE * self.v * self.v * ds
        e = self.E - G * (ybar + FRICTION * sc)
        vfree = math.sqrt(2.0 * e) if e > 0 else 0.0
        if self.chain and sc + OFFSETS[-1] <= self.chain[0] and vfree < self.chain[1]:
            self.v = self.chain[1]
            self.E = 0.5 * self.v ** 2 + G * (ybar + FRICTION * sc)
        else:
            self.v = max(vfree, 1e-3)
        return self.v


def design(sd, speed_of=None, ask_of_L=None, L_end=None, together=None):
    """The element designed step by step as the program's force section does
    it (time argument, 1 ms steps), in the vertical plane.

    speed_of: None for the program's point (the speed from the point's own
    energy), or a function of the rail length run in the element giving the
    speed to design with (reading T and D). ask_of_L: None to ask the force
    program against time (reading T), or a function of the rail length run
    giving the asked force (reading D, run until L_end). together: a
    TrainAhead (front car, solved with the shape).

    Returns a list of nodes: t, L (rail length run), x, y (heartline), pitch
    (rad), v (speed used), ask (the force asked there), y09 (mass point)."""
    heart = HEART
    th = 0.0
    hx, hy = 0.0, 0.0
    nx, ny = 0.0, -1.0
    v = sd.v0
    y09 = hy + 0.9 * heart * ny
    E = 0.5 * v * v + G * y09
    L = Lh = hdist = 0.0
    s0 = lift_length(sd)
    nodes = [dict(t=0.0, L=0.0, Lh=0.0, x=hx, y=hy, pitch=th, v=v, ask=1.0, y09=y09)]
    if together is not None:
        v = together.speed(s0, y09)
    elif speed_of is not None:
        v = speed_of(0.0)
    nodes[0]['v'] = v
    i = 0
    total = int(sd.force.total * HZ + 0.5)
    while True:
        if ask_of_L is None:
            if i >= total:
                break
            t = (i + 1) / HZ
            nF = sd.force(t)
        else:
            if L >= L_end:
                break
            t = (i + 1) / HZ
            nF = ask_of_L(L + v / HZ)
        nforce = (nF - math.cos(th)) * G
        est = hdist * HZ if hdist > 1e-12 else v
        th_new = th + nforce / HZ / est
        dxp, dyp = math.cos(th), math.sin(th)
        dxn, dyn = math.cos(th_new), math.sin(th_new)
        nxn, nyn = math.sin(th_new), -math.cos(th_new)
        hx_new = hx + v / (2 * HZ) * (dxn + dxp) + heart * (nx - nxn)
        hy_new = hy + v / (2 * HZ) * (dyn + dyp) + heart * (ny - nyn)
        rail = math.hypot(hx_new + heart * nxn - hx - heart * nx,
                          hy_new + heart * nyn - hy - heart * ny)
        hdist = math.hypot(hx_new - hx, hy_new - hy)
        L += rail
        Lh += hdist
        hx, hy, th, nx, ny = hx_new, hy_new, th_new, nxn, nyn
        y09 = hy + 0.9 * heart * ny
        if together is not None:
            v = together.speed(s0 + L, y09)
        elif speed_of is None:
            E -= v * v * v / HZ * RESISTANCE
            v = math.sqrt(max(2.0 * (E - G * (y09 + L * FRICTION)), 1e-9))
        else:
            v = speed_of(L)
        nodes.append(dict(t=t, L=L, Lh=Lh, x=hx, y=hy, pitch=th, v=v, ask=nF, y09=y09))
        i += 1
    return nodes


def interp(xs, ys, x):
    """Linear interpolation on increasing xs, held at the ends."""
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    i = bisect.bisect_right(xs, x)
    a = (x - xs[i - 1]) / (xs[i] - xs[i - 1])
    return ys[i - 1] + a * (ys[i] - ys[i - 1])


def at_L(nodes, key, L):
    return interp([n['L'] for n in nodes], [n[key] for n in nodes], L)


# ---- the project ------------------------------------------------------------------
_TEMPLATES = {}


def template(sd):
    """The project text of R7's own project for this shape (fvdcore totext)."""
    if sd.key not in _TEMPLATES:
        WORK.mkdir(exist_ok=True)
        out = WORK / (sd.key + '.r7.json')
        run_core('totext', str(sd.r7_project), str(out), '--force')
        _TEMPLATES[sd.key] = json.loads(out.read_text())
    return json.loads(json.dumps(_TEMPLATES[sd.key]))


def _point(x, y, pitch):
    """A CSV point in the program's own frame, where a level track with no
    yaw runs along -z with its lateral vector along +x."""
    return {'position': [0, round(y, 6), round(-x, 6)],
            'direction': [0, round(math.sin(pitch), 9), round(-math.cos(pitch), 9)],
            'lateral': [1, 0, 0]}


def csv_points(nodes, x0, y0, spacing):
    """Heartline points at EQUAL steps of heartline length (about `spacing`
    m), with direction and lateral vectors.

    Equal steps matter: the program's NoLimits CSV section finds the point
    before a distance with an interpolation search that, on unevenly spaced
    points, often stops one point too far (secnlcsv::getNodeAtDistance; see
    validate.py and the note). On evenly spaced points its guess is right
    to within a fraction of a millimetre."""
    Lh = [n['Lh'] for n in nodes]
    xs = [n['x'] for n in nodes]
    ys = [n['y'] for n in nodes]
    ps = [n['pitch'] for n in nodes]
    count = max(int(round(Lh[-1] / spacing)), 1)
    step = Lh[-1] / count
    pts = []
    for k in range(count + 1):
        h = k * step
        x, y, p = interp(Lh, xs, h), interp(Lh, ys, h), interp(Lh, ps, h)
        pts.append(_point(x0 + x, y0 + y, p))
    return pts


def csv_points_uneven(nodes, x0, y0, spacing):
    """The design's own nodes, about every `spacing` m (unevenly spaced): only
    to show the CSV section's search fault (validate.py)."""
    pts = []
    last = None
    for k, n in enumerate(nodes):
        if last is None or n['L'] - last >= spacing - 1e-9 or k == len(nodes) - 1:
            last = n['L']
            pts.append(_point(x0 + n['x'], y0 + n['y'], n['pitch']))
    return pts


def write_project(sd, nodes, path, spacing=0.1, name=None, even=True):
    """The design as a project: R7's lift (if any) as it is, then the element
    as one NoLimits CSV section. Written as text, made a .fvd by fromtext."""
    doc = template(sd)
    trk = doc['tracks'][0]
    keep = trk['sections'][:1] if sd.lift else []
    x0 = lift_length(sd) if sd.lift else 0.0
    sec = {'type': 'nolimitscsv',
           'points': (csv_points if even else csv_points_uneven)(nodes, x0, 0.0, spacing),
           'smoothing': {'active': False, 'iterations': 1, 'length': 400}}
    trk['sections'] = keep + [sec]
    # A CSV section spaces its nodes by the anchor speed / 1000. Behind the
    # lift (3.5 m/s) that is 3.5 mm: 45,000 nodes and 10 s a ride. The lift
    # holds its own speed from the first node, so a faster anchor changes
    # nothing the ride uses but the first sample (validate.py checks it).
    # (Only behind the lift: elsewhere the anchor speed is the entry speed.)
    if sd.lift:
        trk['anchor']['speed'] = 30.0
    trk['name'] = name or path.stem
    trk.get('smoothing', {}).get('wholeTrack', {}).pop('recalculatedOnLoad', None)
    for s in keep:
        s.get('smoothing', {}).pop('recalculatedOnLoad', None)
    path = Path(path)
    text = path.with_suffix('.json')
    text.write_text(json.dumps(doc, indent=1))
    run_core('fromtext', str(text), str(path), '--force')
    return path


# ---- the program -------------------------------------------------------------------
def run_core(*args):
    p = subprocess.run([str(EXE)] + list(args), capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError('fvdcore %s failed: %s' % (' '.join(args), p.stderr.strip()))
    return p.stdout


def ride(project, cars=CARS, pitch=PITCH):
    """fvdcore ride with the samples, as a dict (the JSON of track 1)."""
    args = ['ride', str(project), '--json', '--table']
    if cars > 1:
        args += ['--cars', str(cars), '--pitch', str(pitch)]
    return json.loads(run_core(*args))['tracks'][0]


def car_speed_fn(rd, car, s_shift):
    """The train's speed when `car` (0-based, front first) is at element place
    L (rail length into the element; the element starts at s_shift)."""
    off = rd['train_offsets'][car]
    pos = rd['samples']['position_m']
    sp = rd['samples']['speed_mps']
    # positions increase; keep the strictly increasing ones (a stall would not)
    xs, vs = [], []
    for p, v in zip(pos, sp):
        if not xs or p > xs[-1] + 1e-9:
            xs.append(p)
            vs.append(v)

    def f(L):
        return interp(xs, vs, s_shift + L - off)
    return f


def prepare(rd):
    """Adds the car offsets (from the JSON's per-car list) to a ride."""
    rd['train_offsets'] = [c['offset_m'] for c in rd['cars']]
    return rd


def felt_by_place(rd, car):
    """(place, gz) of a car while its centre is at or after the track's start
    (the callers keep only the element's places)."""
    off = rd['train_offsets'][car]
    pos = rd['samples']['position_m']
    gz = rd['samples']['cars'][car]['gz']
    return [(p + off, g) for p, g in zip(pos, gz) if p + off >= 0.0]


def ask_error(rd, car, nodes, s_shift, ask_key='ask', skip_m=0.0):
    """Largest |felt - asked| over the element for a car, and where (m into
    the element), and the RMS. Asked: the design's asked force at that place.
    skip_m: metres left out at the element's start (the lift crest's start,
    where the program's point and the train both begin at the held speed)."""
    Ls = [n['L'] for n in nodes]
    As = [n[ask_key] for n in nodes]
    worst, where, sq, k = 0.0, None, 0.0, 0
    for p, g in felt_by_place(rd, car):
        L = p - s_shift
        if L < skip_m or L > Ls[-1]:
            continue
        e = g - interp(Ls, As, L)
        sq += e * e
        k += 1
        if abs(e) > abs(worst):
            worst, where = e, L
    return worst, where, math.sqrt(sq / k) if k else float('nan')


# ---- comparing shapes ---------------------------------------------------------------
def crest(sd, nodes):
    """The feature the tables follow: the camelback's crest and the loop's top
    (highest point), the lift crest's point where the drop passes -30 deg."""
    if sd.key == 'liftcrest':
        for a, b in zip(nodes, nodes[1:]):
            if b['pitch'] <= math.radians(-30.0) < a['pitch']:
                return b
        return nodes[-1]
    return max(nodes, key=lambda n: n['y'])


def shape_gap(a, b, step=0.5):
    """Largest distance from a point of shape a (every `step` m) to the
    polyline of shape b: how far the shape moved, as a picture would show it."""
    bx = [n['x'] for n in b]
    by = [n['y'] for n in b]
    bL = [n['L'] for n in b]
    worst, where = 0.0, 0.0
    nextL = 0.0
    for n in a:
        if n['L'] < nextL:
            continue
        nextL = n['L'] + step
        # search b within 40 m of the same length along the track
        i0 = max(bisect.bisect_left(bL, n['L'] - 40.0) - 1, 0)
        i1 = min(bisect.bisect_right(bL, n['L'] + 40.0) + 1, len(b) - 1)
        best = 1e9
        for i in range(i0, i1):
            dx, dy = bx[i + 1] - bx[i], by[i + 1] - by[i]
            ll = dx * dx + dy * dy
            u = 0.0 if ll == 0 else max(0.0, min(1.0, ((n['x'] - bx[i]) * dx + (n['y'] - by[i]) * dy) / ll))
            d = math.hypot(n['x'] - bx[i] - u * dx, n['y'] - by[i] - u * dy)
            if d < best:
                best = d
        if best > worst:
            worst, where = best, n['L']
    return worst, where


def same_L_gap(a, b):
    """Largest distance between the two shapes' points at the same rail length
    (round-to-round change; includes any slide along the track)."""
    Lb = [n['L'] for n in b]
    xb = [n['x'] for n in b]
    yb = [n['y'] for n in b]
    worst = 0.0
    for n in a[::10]:
        if n['L'] > Lb[-1]:
            break
        worst = max(worst, math.hypot(n['x'] - interp(Lb, xb, n['L']),
                                      n['y'] - interp(Lb, yb, n['L'])))
    return worst


# ---- output --------------------------------------------------------------------------
class Report:
    """Prints lines and keeps them for results/<name>.txt."""

    def __init__(self, name):
        self.name = name
        self.lines = []

    def __call__(self, *parts):
        line = ' '.join(str(p) for p in parts)
        print(line, flush=True)
        self.lines.append(line)

    def table(self, header, rows):
        self('| ' + ' | '.join(header) + ' |')
        self('|' + '---|' * len(header))
        for r in rows:
            self('| ' + ' | '.join(str(c) for c in r) + ' |')
        self('')

    def save(self):
        RESULTS.mkdir(exist_ok=True)
        (RESULTS / ('%s.txt' % self.name)).write_text('\n'.join(self.lines) + '\n',
                                                     encoding='utf-8')


def need_exe():
    if not EXE.exists():
        sys.exit('needs %s: build the program first (tools\\build-windows.cmd)' % EXE)
