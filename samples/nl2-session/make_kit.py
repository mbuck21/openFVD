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
"""Make the kit for the owner's NoLimits 2 session (research note R12, section 7).

    py -3 make_kit.py [--fvdcore <fvdcore.exe>] [--out <folder>] [--check]

Writes into the output folder (default: the folder of this script):

  the test projects (.fvd) and their NoLimits 2 exports (.nl2elem),
  predictions.json and predictions.md: what this program's model says the train
  does on each, for comparison with a recording.

It does not write README.md, PROCEDURE.md or notes-template.txt (those are
written by hand).

Running it twice gives the same bytes. With --check it writes into a temporary
folder instead and compares with the files committed here (exit 1 if they
differ).

Where the numbers come from (also said in README.md):
  * every project is computed by the real program (`fvdcore nodes`,
    `fvdcore export`);
  * the level coast-downs (E1, E5) are the program's energy rule integrated
    here (the same two numbers, friction and resistance);
  * the valley tracks (E2) are shaped with R7's fvd2d.py, then computed by
    `fvdcore nodes`;
  * the camelback and the lift crest (E3, E4) are R7's shapes (tuning numbers
    from R7's shapes.json) behind a level run-in each, so that a 9-car train
    can stand on level track before the first curvature; the shape after the
    run-in is checked against R7's own dump, and the train numbers come from
    R7's ride.py (the whole train sharing one speed) on that dump, with the
    distances shifted by the run-in. The C++ ride (`fvdcore ride`, piece P1 of
    Stage 3) is built in parallel and is not used here.
"""
import argparse
import csv
import filecmp
import io
import json
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
R7 = REPO / 'docs' / 'research' / 'r7-prototype'
sys.path.insert(0, str(R7))

import fvd2d            # noqa: E402
import fvdwrite as w    # noqa: E402
import make_shapes as ms  # noqa: E402  (only its newton() and settings)
import ride as r7ride   # noqa: E402

G = 9.80665
HEART, FRICTION, RESISTANCE = ms.HEART, ms.FRICTION, ms.RESISTANCE

# Friction and drag sets the predictions are given for (R12 section 2).
PARAM_SETS = [
    ('program defaults', 0.03, 2e-5),
    ('Aeneas NL 2.0 fit', 0.026, 2e-5),
    ('Aeneas NL 2.2 fit', 0.026, 1e-4),
]
# Train shapes the predictions assume (R7's; NoLimits 2's real car length is
# not published, R12 section 6).
TRAINS = [('2 cars', 2, 2.5), ('4 cars', 4, 2.5), ('9 cars', 9, 2.2)]

E1_LENGTH = 800.0
E1_TRANSPORT = 50.0                 # metres of Transport at the start
E1_AFTER = E1_LENGTH - E1_TRANSPORT  # metres left to coast after it
# Level run-ins. A train must stand on level track, on the Transport or the
# Lift section, before the first curvature, and the longest train used on a
# track (9 cars, about 20 m) must fit with room to spare for wherever
# NoLimits 2 puts it.
E2_TRANSPORT = 50.0                 # E2: 120 m of level before the valley
E3_RUNIN_S = 2.0                    # E3: 60 m (2 s at 30 m/s) in front of R7's
E3_TRANSPORT = 40.0                 #     camelback; Transport on the first 40 m
E4_LIFT_S = 12.0                    # E4: lift of 42 m, 28 m longer than R7's
E2_LENGTH = 600.0
E2_MARK_BEFORE = 100.0
E2_MARK_AFTER = 450.0
E2_ENTRY = 30.0


def rnd(x, n=3):
    return None if x is None else round(float(x) + 0.0, n)


# ---------------------------------------------------------------- fvdcore --
def run_core(exe, *args):
    res = subprocess.run([str(exe)] + [str(a) for a in args], check=True,
                         capture_output=True, text=True)
    return res.stdout


def nodes(exe, project, every_m):
    out = run_core(exe, 'nodes', project, '--every-m', every_m,
                   '--frame', 'anchor')
    rows = list(csv.DictReader(io.StringIO(out), delimiter='\t'))
    return [{k: float(v) for k, v in row.items()} for row in rows]


def at_dist(rows, d, key):
    """Linear interpolation of a column at centre-line distance d."""
    if d <= rows[0]['dist_m']:
        return rows[0][key]
    prev = rows[0]
    for row in rows[1:]:
        if row['dist_m'] >= d > prev['dist_m'] or row is rows[-1]:
            span = row['dist_m'] - prev['dist_m']
            f = 0.0 if span <= 0 else (d - prev['dist_m']) / span
            return prev[key] + f * (row[key] - prev[key])
        prev = row
    return rows[-1][key]


def export_nl2elem(exe, project, dest):
    with tempfile.TemporaryDirectory() as tmp:
        run_core(exe, 'export', project, tmp)
        found = sorted(Path(tmp).glob('*.nl2elem'))
        if len(found) != 1:
            raise RuntimeError('expected one .nl2elem for %s, got %s'
                               % (project, found))
        shutil.copyfile(found[0], dest)


# ---------------------------------------------------------- level straight --
def straight_project(exe, path, speed, name):
    """An 800 m dead level straight at a held speed, made through the text form."""
    base = json.loads(run_core(exe, 'totext',
                               REPO / 'samples' / 'baseline' / 'four-sections.fvd',
                               '-'))
    trk = base['tracks'][0]
    trk['name'] = name
    trk['anchor']['speed'] = speed
    trk['anchor']['position'] = [0, 5, 0]
    trk['sections'] = [{
        'name': 'level straight',
        'roll': [{'center': 0, 'change': 0, 'from': 0, 'locked': False,
                  'shape': 'quartic', 'shapeParameter': -10, 'tension': 0,
                  'to': E1_LENGTH}],
        'smoothing': {'active': False, 'iterations': 1, 'length': 400},
        'speed': speed, 'speedMode': 'fixed', 'type': 'straight'}]
    base['tracks'] = [trk]
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'in.json'
        src.write_text(json.dumps(base, indent=1))
        run_core(exe, 'fromtext', src, path, '--force')


def coast(v0, friction, drag, length, step=100.0):
    """Speeds along a level straight (the program's energy rule).

    Per kilogram: d(v^2/2)/ds = -(g f + k v^2). Integrated in 1 cm steps.
    Returns [(s, v)] every `step` metres, and the last metre the train still
    moves (None when it does not stop before `length`)."""
    ds = 0.01
    u = v0 * v0 / 2.0
    out = [(0.0, v0)]
    s = 0.0
    stop = None
    nxt = step
    while s < length - 1e-9:
        u -= (G * friction + drag * 2.0 * u) * ds
        s += ds
        if u <= 0:
            stop = s
            u = 0.0
        if s + 1e-9 >= nxt:
            out.append((nxt, math.sqrt(2 * max(u, 0.0))))
            nxt += step
        if stop is not None:
            break
    while nxt <= length + 1e-9 and stop is not None:
        out.append((nxt, 0.0))
        nxt += step
    return out, stop


def e1_predictions():
    res = {}
    for v0 in (10.0, 40.0):
        key = 'v%d' % v0
        res[key] = {}
        for label, f, k in PARAM_SETS:
            table, stop = coast(v0, f, k, E1_AFTER, step=150.0)
            res[key][label] = dict(
                friction=f, drag=k,
                speed_mps=[[rnd(s, 0), rnd(v, 3)] for s, v in table],
                stops_at_m=rnd(stop, 1),
                initial_slope_of_half_v2=rnd(G * f + k * v0 * v0, 4))
    return res


def e5_predictions():
    """R12 E5: the slope of v^2/2 against distance, in m/s^2."""
    return {
        'friction_0.03_no_drag_slope': rnd(G * 0.03, 4),
        'drag_1e-4_at_40mps_slope': rnd(1e-4 * 40 * 40, 4),
        'drag_2e-5_at_40mps_slope': rnd(2e-5 * 40 * 40, 4),
        'friction_0.026_slope': rnd(G * 0.026, 4),
        'note': 'slope of v^2/2 against distance (m/s^2); R12 section 7, E5',
    }


# ------------------------------------------------------------- the valleys --
def valley_steps(n, a, depth, v0, ease=0.5, approach=4.0):
    """Force steps of a level run with a valley, tuned to enter and leave at
    the same height and the same (level) pitch, with the bottom `depth` m
    below the start. Returns the step list up to the end of the last ease."""
    def end(steps):
        return ms.end(steps, v0)

    def half(x):
        td, th1 = x
        st = [(approach, 0.0), (ease, a - 1.0), (td, 0.0), (ease, n - a),
              (th1, 0.0)]
        nd = end(st)[-1]
        return [math.degrees(nd['pitch']), nd['y'] + depth]

    td, th1 = None, None
    for guess in ([1.0, 0.5], [0.5, 0.3], [1.5, 0.5], [2.0, 1.0],
                  [0.8, 0.8], [1.0, 2.0], [2.0, 2.0]):
        try:
            x = ms.newton(half, guess, [0.0, 0.0], iters=40)
        except Exception:
            continue
        if min(x) > 0.2:
            td, th1 = x
            break
    if td is None:
        raise RuntimeError('valley half did not converge')

    def full(x):
        th2, tu = x
        st = [(approach, 0.0), (ease, a - 1.0), (td, 0.0), (ease, n - a),
              (th1 + th2, 0.0), (ease, a - n), (tu, 0.0), (ease, 1.0 - a)]
        nd = end(st)[-1]
        return [math.degrees(nd['pitch']), nd['y']]

    th2, tu = ms.newton(full, [th1, td], [0.0, 0.0], iters=40)
    return [(approach, 0.0), (ease, a - 1.0), (td, 0.0), (ease, n - a),
            (th1 + th2, 0.0), (ease, a - n), (tu, 0.0), (ease, 1.0 - a)]


def level_to_length(steps, v0, length):
    """Append a level 1 g run-out so the rail length is `length` m."""
    base = list(steps)
    extra = 1.0
    for _ in range(8):
        nodes_ = ms.end(base + [(extra, 0.0)], v0)
        L = nodes_[-1]['L']
        v = nodes_[-1]['v']
        if abs(L - length) < 0.02:
            break
        extra += (length - L) / max(v, 1.0)
    return base + [(round(extra, 4), 0.0)]


def e2_projects(exe, outdir):
    cases = [('e2-level-600', None), ('e2-valley-1.5g', (1.5, 0.3, 8.0)),
             ('e2-valley-4g', (4.0, 0.3, 30.0))]
    info = {}
    for name, par in cases:
        if par is None:
            steps = level_to_length([(0.5, 0.0)], E2_ENTRY, E2_LENGTH)
        else:
            n, a, depth = par
            steps = level_to_length(valley_steps(n, a, depth, E2_ENTRY),
                                    E2_ENTRY, E2_LENGTH)
        w.write_project(outdir / (name + '.fvd'),
                        [w.forced_section(name, steps)], speed=E2_ENTRY,
                        heart=HEART, friction=FRICTION, resistance=RESISTANCE,
                        name=name)
        info[name] = (par, steps)
    return info


def e2_predictions(exe, outdir, info):
    res = {}
    for name, (par, _steps) in info.items():
        rows = nodes(exe, outdir / (name + '.fvd'), 1)
        sp = lambda d: at_dist(rows, d, 'speed_mps')  # noqa: E731
        ys = [row['heart_y'] for row in rows]
        entry = {
            'length_m': rnd(rows[-1]['dist_m'], 1),
            'speed_at_%dm_mps' % E2_MARK_BEFORE: rnd(sp(E2_MARK_BEFORE)),
            'speed_at_%dm_mps' % E2_MARK_AFTER: rnd(sp(E2_MARK_AFTER)),
            'half_v2_loss_between_marks_m2s2': rnd(
                (sp(E2_MARK_BEFORE) ** 2 - sp(E2_MARK_AFTER) ** 2) / 2.0
                + G * (at_dist(rows, E2_MARK_BEFORE, 'heart_y')
                       - at_dist(rows, E2_MARK_AFTER, 'heart_y')), 2),
            'drag_part_of_that_m2s2': rnd(sum(
                RESISTANCE * sp(d + 0.5) ** 2 for d in range(
                    int(E2_MARK_BEFORE), int(E2_MARK_AFTER))), 2),
            'lowest_heartline_y_m': rnd(min(ys) - ys[0], 2),
            'heartline_height_change_between_marks_m': rnd(
                at_dist(rows, E2_MARK_AFTER, 'heart_y')
                - at_dist(rows, E2_MARK_BEFORE, 'heart_y'), 3),
            'max_normal_g': rnd(max(r['normal_g'] for r in rows), 2),
        }
        if par is not None:
            hold = [r['dist_m'] for r in rows
                    if r['normal_g'] >= par[0] - 0.05]
            entry['valley_g_set'] = par[0]
            metres = (hold[-1] - hold[0]) if hold else 0.0
            entry['metres_within_0.05g_of_the_valley_g'] = rnd(metres, 1)
            # what friction growing in step with the wheel load would add:
            # g * friction * (valley g - 1) * metres held, at the program's
            # friction
            entry['extra_loss_if_friction_follows_load_m2s2'] = rnd(
                G * FRICTION * (par[0] - 1.0) * metres, 1)
        res[name] = entry
    return res


# --------------------------------------------------------- R7's own tracks --
def r7_meta(key):
    return json.loads((R7 / 'projects' / 'shapes.json').read_text())[key]


def e3_project(path):
    """R7's camelback behind a level run-in held at its entry speed."""
    meta = r7_meta('camelback')
    steps = ms.camel_steps(meta['t2'], meta['tair'], meta['t4'])
    w.write_project(path, [
        w.forced_section('run-in', [(E3_RUNIN_S, 0.0)],
                         fixed_speed=ms.CB['v0']),
        w.forced_section('camelback', steps)],
        speed=ms.CB['v0'], heart=HEART, friction=FRICTION,
        resistance=RESISTANCE, name='camelback')


def e4_project(path):
    """R7's 0.60 g lift crest behind a longer level lift."""
    meta = r7_meta('liftcrest-0.60')
    drop = ms.layout_drop(meta['t1'], meta['th'], meta['crest']) + [(2.0, 0.0)]
    w.write_project(path, [
        w.forced_section('lift crest', [(E4_LIFT_S, 0.0)],
                         fixed_speed=ms.LAYOUT_LIFT_SPEED),
        w.forced_section('drop', drop)],
        speed=ms.LAYOUT_LIFT_SPEED, heart=HEART, friction=FRICTION,
        resistance=RESISTANCE, name='liftcrest-0.60')


def same_shape_as_r7(exe, project, dump, sec_new, sec_old):
    """Check that the new project, from the start of section sec_new, is R7's
    track from the start of its section sec_old (same height and speed along
    the way). Returns (shift in metres, worst difference)."""
    import gzip
    new = nodes(exe, project, 0.1)
    with gzip.open(R7 / 'shapes' / dump, 'rt') as fh:
        old = [{k: float(v) for k, v in row.items()}
               for row in csv.DictReader(fh, delimiter='	')]
    b_new = next(r['dist_m'] for r in new if r['section'] == sec_new)
    b_old = next(r['dist_m'] for r in old if r['section'] == sec_old)
    worst = 0.0
    for row in old:
        if row['section'] < sec_old:
            continue
        d = b_new + row['dist_m'] - b_old
        if d > new[-1]['dist_m']:
            break
        worst = max(worst,
                    abs(at_dist(new, d, 'heart_y') - row['heart_y']),
                    abs(at_dist(new, d, 'speed_mps') - row['speed_mps']))
    if worst > 0.15:
        raise RuntimeError('%s does not follow the R7 shape (%.3f)'
                           % (project, worst))
    return b_new - b_old, worst


def ride_dump(exe, project, path, section_from=None):
    """Write `fvdcore nodes` of a project in the form ride.py reads. With
    section_from, only the rows from that section on, and distances counted
    from its start. Returns the metres cut off in front."""
    rows = nodes(exe, project, 0.1)
    cut = 0.0
    if section_from:
        rows = [r for r in rows if r['section'] >= section_from]
        cut = rows[0]['dist_m']
        hcut = rows[0]['heart_dist_m']
        for r in rows:
            r['dist_m'] -= cut
            r['heart_dist_m'] -= hcut
    with open(path, 'w', newline='') as fh:
        fh.write('\t'.join(rows[0]) + '\n')
        for r in rows:
            fh.write('\t'.join(repr(v) for v in r.values()) + '\n')
    return cut


def r7_prediction(project, dump, fixed, crest_seat_note, trains=TRAINS,
                  offset=0.0):
    sh = r7ride.Shape.from_dump(dump, HEART, FRICTION, RESISTANCE,
                                fixed_sections=fixed, name=project)
    if fixed:
        # a lift crest: the end of the chain section is the crest
        s_c = sh.fixed[0][1]
        i_top = min(range(sh.n), key=lambda i: abs(sh.s[i] - s_c))
    else:
        i_top = max(range(sh.n), key=lambda i: sh.hy[i])
        s_c = sh.s[i_top]
    pt = r7ride.point_as_train(sh)
    point = dict(crest_dist_m=rnd(s_c + offset, 1), crest_height_m=rnd(sh.hy[i_top], 2),
                 speed_mps=rnd(pt['v'][i_top]))
    f = r7ride.force_when_car_at(pt, 0, s_c)
    point['normal_g'] = rnd(f[0], 2)
    out = {'point': point, 'note': crest_seat_note, 'trains': {}}
    for label, n, L in trains:
        res = r7ride.ride_train(sh, r7ride.Train(n, L))
        offs = r7ride.Train(n, L).offsets()
        mid = n // 2

        def seat(j):
            fo = r7ride.force_when_car_at(res, j, s_c)
            v = r7ride.speed_at(res, s_c - offs[j])
            return {'train_speed_mps': rnd(v),
                    'normal_g': rnd(fo[0], 2) if fo else None}
        cars = {'front': seat(0), 'rear': seat(n - 1)}
        if n % 2:
            cars['middle'] = seat(mid)
        else:
            a, b = seat(mid - 1), seat(mid)
            cars['middle'] = {
                k: (None if a[k] is None or b[k] is None else
                    rnd((a[k] + b[k]) / 2.0, 3 if 'speed' in k else 2))
                for k in a}
        # the lowest vertical g any seat of the car feels near the crest
        lo = {}
        for j, key in ((0, 'front'), (n - 1, 'rear')):
            lo[key] = rnd(r7ride.extreme(res, j, s_c - 25.0, s_c + 25.0,
                                         'min'), 2)
        cars['lowest_normal_g_within_25m_of_crest'] = lo
        out['trains'][label] = dict(car_length_m=L, cars=cars)
    return out


# ------------------------------------------------------------------ report --
def write_markdown(path, pred):
    L = []
    a = L.append
    a('# Predicted numbers')
    a('')
    a('What this program\'s model says, for comparison with a recording. '
      'Generated by `make_kit.py` from `predictions.json`; do not edit by '
      'hand. Per kilogram, metric. NoLimits 2 will differ in the ways the '
      'experiments are meant to find out.')
    a('')
    a('## E1 and E5: coast-down on the level 800 m straight')
    a('')
    a('The track is %g m long and its first %g m are the Transport section. '
      'Speed (m/s) after the stated distance **counted from the end of the '
      'Transport** (so the last column, %g m, is the end of the track), for '
      'three friction and drag sets (friction in m/m, drag in 1/m), for a '
      'train that leaves the Transport at the start speed. `0` means the '
      'train has stopped.' % (E1_LENGTH, E1_TRANSPORT, E1_AFTER))
    a('')
    for key in ('v10', 'v40'):
        a('Start %s m/s:' % key[1:])
        a('')
        dists = [p[0] for p in next(iter(pred['e1'][key].values()))['speed_mps']]
        a('| set | friction | drag | ' + ' | '.join('%d m' % d for d in dists)
          + ' |')
        a('|---|---|---|' + '---|' * len(dists))
        for label, d in pred['e1'][key].items():
            a('| %s | %g | %g | ' % (label, d['friction'], d['drag'])
              + ' | '.join('%.2f' % v for _, v in d['speed_mps']) + ' |')
        a('')
    a('The fit (`tools/nl2fit.py`) should return the friction and drag of '
      'whichever row matches NoLimits 2. E5 (custom friction, Professional '
      'licence): slope of v^2/2 against distance %(friction_0.03_no_drag_slope)s '
      'm/s^2 for friction 0.03 and no drag; %(drag_1e-4_at_40mps_slope)s m/s^2 '
      'for drag 1e-4 at 40 m/s (if the number is a loss in metres per metre '
      'and a 1/m number).' % pred['e5'])
    a('')
    a('## E2: a valley against a level (entry speed %g m/s)' % E2_ENTRY)
    a('')
    a('Marks are metres of centre line from the start of the track; the '
      'first %g m are the Transport section and the valley starts after '
      'about 120 m of level. In the program\'s own model the loss is friction '
      '(the same on all three tracks: it is charged per metre of track) plus '
      'air drag, and the drag is larger in a valley because the train is '
      'faster there. So the "loss" column is NOT the same on the three '
      'tracks: it differs by %s m^2/s^2 (1.5 g valley) and %s m^2/s^2 (4 g '
      'valley) from the level; the "drag part" column differs by %s and %s '
      '(the small rest is the rounding of the node steps), with the '
      'program\'s drag of %g. **Scale those two numbers by '
      'the drag you fitted in E1 divided by %g before reading an excess.** '
      'An excess over the scaled number means friction grows with wheel load. '
      'The last column shows how big that would be: friction that follows '
      'the vertical force (g x friction x (valley g - 1) x metres held at '
      'the valley g, with the program\'s friction %g) would add the number '
      'in "extra if friction follows load". It is much larger than the drag '
      'differences, so the two effects can be told apart.' % (
          E2_TRANSPORT,
          rnd(pred['e2']['e2-valley-1.5g']['half_v2_loss_between_marks_m2s2']
              - pred['e2']['e2-level-600']['half_v2_loss_between_marks_m2s2'],
              2),
          rnd(pred['e2']['e2-valley-4g']['half_v2_loss_between_marks_m2s2']
              - pred['e2']['e2-level-600']['half_v2_loss_between_marks_m2s2'],
              2),
          rnd(pred['e2']['e2-valley-1.5g']['drag_part_of_that_m2s2']
              - pred['e2']['e2-level-600']['drag_part_of_that_m2s2'], 2),
          rnd(pred['e2']['e2-valley-4g']['drag_part_of_that_m2s2']
              - pred['e2']['e2-level-600']['drag_part_of_that_m2s2'], 2),
          RESISTANCE, RESISTANCE, FRICTION))
    a('')
    a('| track | speed at %d m | speed at %d m | loss, v^2/2 + g y '
      'between the marks (m^2/s^2) | drag part | peak vertical g | metres at '
      'the valley g | extra if friction follows load (m^2/s^2) |'
      % (E2_MARK_BEFORE, E2_MARK_AFTER))
    a('|---|---|---|---|---|---|---|---|')
    for name, d in pred['e2'].items():
        a('| %s | %s | %s | %s | %s | %s | %s | %s |' % (
            name, d['speed_at_%dm_mps' % E2_MARK_BEFORE],
            d['speed_at_%dm_mps' % E2_MARK_AFTER],
            d['half_v2_loss_between_marks_m2s2'],
            d['drag_part_of_that_m2s2'], d['max_normal_g'],
            d.get('metres_within_0.05g_of_the_valley_g', '-'),
            d.get('extra_loss_if_friction_follows_load_m2s2', '-')))
    a('')
    a('## E3: the camelback, one train speed against the point')
    a('')
    a(pred['e3']['note'])
    a('')
    p = pred['e3']['point']
    a('The point (what the program\'s ride sheet uses): crest at %s m, %s m '
      'above the start, speed %s m/s, vertical force %s g.' % (
          p['crest_dist_m'], p['crest_height_m'], p['speed_mps'],
          p['normal_g']))
    a('')
    a('| train | car length (assumed) | speed with front car at crest | with '
      'middle | with rear | vertical g at the crest, front / middle / rear | '
      'lowest vertical g within 25 m of the crest, front / rear |')
    a('|---|---|---|---|---|---|---|')
    for label, t in pred['e3']['trains'].items():
        c = t['cars']
        lo = c['lowest_normal_g_within_25m_of_crest']
        a('| %s | %g m | %s | %s | %s | %s / %s / %s | %s / %s |' % (
            label, t['car_length_m'], c['front']['train_speed_mps'],
            c['middle']['train_speed_mps'], c['rear']['train_speed_mps'],
            c['front']['normal_g'], c['middle']['normal_g'],
            c['rear']['normal_g'], lo['front'], lo['rear']))
    a('')
    a('## E4a: lift crest (crest force 0.60 g, chain 3.5 m/s), 9 cars')
    a('')
    a(pred['e4']['note'])
    a('')
    p = pred['e4']['point']
    a('The point: the lift ends at %s m (the crest), the chain holds %s m/s '
      'to there.' % (p['crest_dist_m'], p['speed_mps']))
    a('')
    a('| train | train speed with front car at the end of the lift | middle '
      '| rear | lowest vertical g within 25 m of it, front / rear |')
    a('|---|---|---|---|---|')
    for label, t in pred['e4']['trains'].items():
        c = t['cars']
        lo = c['lowest_normal_g_within_25m_of_crest']
        a('| %s | %s | %s | %s | %s / %s |' % (
            label, c['front']['train_speed_mps'],
            c['middle']['train_speed_mps'], c['rear']['train_speed_mps'],
            lo['front'], lo['rear']))
    a('')
    a('## E4b: brake trigger car')
    a('')
    a('On the level straight, a trim brake with First Car and then Middle '
      'Car as trigger should start braking about (n - 1) / 2 car lengths '
      'apart: about %.1f m for 9 cars of 2.2 m, about %.1f m for 4 cars of '
      '2.5 m. The gap depends on where NoLimits 2 measures a car from, and '
      'the car length is assumed (NoLimits 2 publishes none). The program '
      'has no model of the brake yet, so there is no predicted speed.' % (
          8 * 2.2 / 2.0, 3 * 2.5 / 2.0))
    a('')
    path.write_text('\n'.join(L), encoding='utf-8', newline='\n')


# -------------------------------------------------------------------- main --
def build(exe, outdir):
    outdir.mkdir(parents=True, exist_ok=True)
    projects = []
    for v in (10, 40):
        name = 'e1-level-800-v%d' % v
        straight_project(exe, outdir / (name + '.fvd'), float(v), name)
        projects.append(name)
    info = e2_projects(exe, outdir)
    projects += list(info)
    e3_project(outdir / 'e3-camelback.fvd')
    e4_project(outdir / 'e4-liftcrest-0.60.fvd')
    projects += ['e3-camelback', 'e4-liftcrest-0.60']
    for name in projects:
        run_core(exe, 'summary', outdir / (name + '.fvd'))
        export_nl2elem(exe, outdir / (name + '.fvd'), outdir / (name + '.nl2elem'))

    shift3, worst3 = same_shape_as_r7(
        exe, outdir / 'e3-camelback.fvd', 'camelback.tsv.gz', 2, 1)
    shift4, worst4 = same_shape_as_r7(
        exe, outdir / 'e4-liftcrest-0.60.fvd', 'liftcrest-0.60.tsv.gz', 2, 2)
    with tempfile.TemporaryDirectory() as tmp:
        d3 = Path(tmp) / 'e3.tsv'
        cut3 = ride_dump(exe, outdir / 'e3-camelback.fvd', d3, section_from=2)
        d4 = Path(tmp) / 'e4.tsv'
        ride_dump(exe, outdir / 'e4-liftcrest-0.60.fvd', d4)
        e3 = r7_prediction(
            'e3-camelback', d3, (),
            'Speed of the whole train (one speed for all cars, R7) at the '
            'moment the named car is at the crest, and the vertical force '
            'that car feels there. Car lengths are assumed (R7). The track '
            'is the R7 camelback behind a %.0f m level run-in, and the numbers '
            'are computed on this project from the start of the camelback '
            '(cars behind that point stand on the straight run-in, as in '
            'R7). Distances are from the start of the track. The shape '
            'equals the R7 one to within %.2f m or m/s.' % (cut3, worst3),
            offset=cut3)
        e4 = r7_prediction(
            'e4-liftcrest-0.60', d4, (1,),
            'Same reading as E3; the chain holds 3.5 m/s only while the rear '
            'car is on the lift and gravity alone would be slower (R7). The '
            'lift is the R7 one with %.0f m of level run-in in front, so the '
            'train stands on it; the numbers are computed on this project. '
            'The drop equals the R7 one to within %.2f m or m/s.'
            % (shift4, worst4),
            trains=[t for t in TRAINS if t[1] == 9])
    pred = {
        'about': 'Predicted by this program\'s model; see README.md.',
        'settings': dict(heartline_m=HEART, friction=FRICTION,
                         resistance=RESISTANCE),
        'e1': e1_predictions(),
        'e2': e2_predictions(exe, outdir, info),
        'e3': e3,
        'e4': e4,
        'e5': e5_predictions(),
    }
    (outdir / 'predictions.json').write_text(
        json.dumps(pred, indent=1, sort_keys=True) + '\n', encoding='utf-8',
        newline='\n')
    write_markdown(outdir / 'predictions.md', pred)
    return projects


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--fvdcore', default=None)
    ap.add_argument('--out', default=None)
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    exe = Path(args.fvdcore) if args.fvdcore else (
        REPO / 'build' / 'dist' / 'fvdcore.exe')
    if not exe.exists():
        alt = exe.with_suffix('')
        if alt.exists():
            exe = alt
        else:
            sys.exit('fvdcore not found: %s (build with tools/build-windows.cmd'
                     ' or pass --fvdcore)' % exe)
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            build(exe, Path(tmp))
            bad = []
            for f in sorted(Path(tmp).iterdir()):
                mine = HERE / f.name
                if not mine.exists() or not filecmp.cmp(f, mine, shallow=False):
                    bad.append(f.name)
            if bad:
                print('DIFFERENT: ' + ', '.join(bad))
                sys.exit(1)
            print('PASS: the committed kit equals a fresh regeneration')
        return
    out = Path(args.out) if args.out else HERE
    names = build(exe, out)
    print('wrote %d projects, exports, predictions to %s' % (len(names), out))


if __name__ == '__main__':
    main()
