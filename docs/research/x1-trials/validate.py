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
"""X1, step 0: is the stand-in faithful?

    py -3 docs/research/x1-trials/validate.py

For each of the three shapes, the POINT design is made twice: by the real
program from R7's force-section project, and by the trial's stand-in (the
designer in x1lib.py, written as a NoLimits CSV section). Both are ridden by
the program (`fvdcore ride`) with its own point and with R7's 9-car train,
and the results are compared: the speed of the train at every sample, and
per car the lowest and highest vertical force and the airtime. It also says
how closely the program's ride of the stand-in reproduces the asked force
(the measuring floor of the trial), and how the CSV point spacing matters.
Writes results/validate.txt.
"""
import x1lib as X


def summary_row(label, rd, car):
    c = rd['cars'][car]
    return [label, 'car %d' % (car + 1), '%.3f' % c['gz_min']['value'],
            '%.3f' % c['gz_max']['value'], '%.2f' % c['below_zero_s'],
            '%.2f' % c['floater_s'], '%.2f' % c['ejector_s']]


def speed_diff(a, b):
    """Largest speed difference at the same time of the two rides, from the
    second sample on (the first is the anchor speed, which the lift crest's
    stand-in sets to 30 m/s; see x1lib.write_project)."""
    va, vb = a['samples']['speed_mps'], b['samples']['speed_mps']
    n = min(len(va), len(vb))
    return max(abs(va[i] - vb[i]) for i in range(1, n))


def uneven_straight():
    """A 200 m level straight given as 2,001 points whose spacing shrinks
    slowly from 0.12 m to 0.08 m, imported as a NoLimits 2 CSV: the program's
    length of it, against the 200 m it is."""
    csv = X.WORK / 'uneven-straight.csv'
    tab = '\t'
    lines = [tab.join('"%s"' % h for h in ('No.', 'PosX', 'PosY', 'PosZ', 'FrontX', 'FrontY',
                                           'FrontZ', 'LeftX', 'LeftY', 'LeftZ', 'UpX', 'UpY',
                                           'UpZ'))]
    x = 0.0
    for i in range(2001):
        lines.append(tab.join(['%d' % (i + 1), '%.6f' % x, '0.000000', '0.000000',
                               '1', '0', '0', '0', '0', '-1', '0', '1', '0']))
        x += 0.12 - 0.00002 * i
    csv.write_text('\n'.join(lines) + '\n')
    fvd = X.WORK / 'uneven-straight.fvd'
    X.run_core('import', 'csv', str(csv), str(fvd), '--force')
    rows = X.run_core('nodes', str(fvd), '--every-m', '20', '--frame', 'anchor').splitlines()
    head = rows[0].split(tab)
    last = rows[-1].split(tab)
    return (float(last[head.index('dist_m')]), float(last[head.index('heart_x')]))


def main():
    X.need_exe()
    X.WORK.mkdir(exist_ok=True)
    rep = X.Report('validate')
    rep('# X1 step 0: the stand-in against the program\'s own design')
    rep('')
    rep('Program: %s. Train: %d cars %.1f m apart (R7\'s "long" train). '
        'All forces below are from `fvdcore ride`.' % (X.EXE.name, X.CARS, X.PITCH))
    rep('')
    rows, crows, floor = [], [], []
    for sd in X.shapes():
        nodes = X.design(sd)
        s0 = X.lift_length(sd)
        variants = [(0.1, True), (0.25, True)]
        if sd.key == 'camelback':
            variants.append((0.1, False))
        for spacing, even in variants:
            label = '%g m, %s' % (spacing, 'even' if even else 'uneven')
            proj = X.write_project(sd, nodes, X.WORK / ('%s-point-%g-%s.fvd' % (
                sd.key, spacing, 'even' if even else 'uneven')), spacing=spacing, even=even)
            for cars in (1, X.CARS):
                own = X.prepare(X.ride(sd.r7_project, cars=cars))
                ours = X.prepare(X.ride(proj, cars=cars))
                worst = 0.0
                for c in range(cars):
                    for key in ('gz_min', 'gz_max'):
                        worst = max(worst, abs(own['cars'][c][key]['value']
                                               - ours['cars'][c][key]['value']))
                rows.append([sd.key, label, str(cars),
                             '%.4f' % speed_diff(own, ours), '%.4f' % worst,
                             '%.2f / %.2f' % (own['duration_s'], ours['duration_s'])])
                if spacing == 0.1 and even:
                    picks = [0] if cars == 1 else [0, X.CARS // 2, X.CARS - 1]
                    for car in picks:
                        crows.append(summary_row('%s, program' % sd.key, own, car))
                        crows.append(summary_row('%s, stand-in' % sd.key, ours, car))
                if cars == 1 and even:
                    w, where, rms = X.ask_error(ours, 0, nodes, s0)
                    w2, where2, rms2 = X.ask_error(own, 0, nodes, s0)
                    floor.append([sd.key, '%g m' % spacing,
                                  '%+.3f at %.1f m, RMS %.4f' % (w, where, rms),
                                  '%+.3f at %.1f m, RMS %.4f' % (w2, where2, rms2)])
    rep('## The same design, two ways: the program\'s force section and the stand-in\'s CSV section')
    rep('')
    rep('Largest difference of the train\'s speed at the same sample time '
        '(every 0.01 s, after the first sample: the lift crest\'s stand-in starts at '
        'an anchor speed of 30 m/s that its lift cuts to 3.5 m/s at once); largest '
        'difference of any car\'s lowest or highest '
        'vertical force; the ride\'s duration (s). "even": CSV points at equal '
        'steps of heartline length (what the trial uses); "uneven": the '
        'design\'s own nodes about every 0.1 m.')
    rep('')
    rep.table(['shape', 'CSV points', 'cars', 'speed difference (m/s)',
               'force difference (g)', 'duration program / stand-in'], rows)
    rep('## What each car feels (CSV points every 0.1 m, even)')
    rep('')
    rep.table(['ride', 'car', 'lowest gz', 'highest gz', 'below 0 g (s)',
               'floater (s)', 'ejector (s)'], crows)
    rep('## The measuring floor: the point\'s felt force against the asked force')
    rep('')
    rep('The program\'s point ridden on the design; "asked" is the force program '
        'at the place (the design\'s own record). Largest felt minus asked over the '
        'element (m into the element) and its RMS, in g.')
    rep('')
    rep.table(['shape', 'CSV spacing', 'stand-in shape', 'program\'s own shape'], floor)
    length, x_end = uneven_straight()
    rep('## The CSV section\'s search fault, on a plain straight')
    rep('')
    rep('`work/uneven-straight.csv` (made by this script): a level straight of '
        '2,001 points from x = 0 to 200.02 m, spacing shrinking from 0.12 to 0.08 m. '
        '`fvdcore import csv work/uneven-straight.csv work/uneven-straight.fvd`, then '
        '`fvdcore nodes work/uneven-straight.fvd --every-m 20 --frame anchor`: the last '
        'node has dist_m %.2f m at heart_x %.2f m. The centre-line length of a straight '
        'is overstated by %.1f %%.' % (length, x_end, 100.0 * (length / x_end - 1.0)))
    rep('')
    rep.save()


if __name__ == '__main__':
    main()
