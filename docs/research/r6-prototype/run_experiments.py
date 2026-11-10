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
"""Research R6 experiments A, B and C on the 2D stand-in (fvd2d.py).

    py -3 docs/research/r6-prototype/run_experiments.py

Prints every table and writes the same text to results.txt next to this file.
Takes a few minutes. Standard library only. Nothing here touches the program.
"""

import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fvd2d import (Track, anchor, run, damped_newton, bracketed_root, F_G,  # noqa: E402
                   PEAK_SLOPE)

OUT = []


def say(line=""):
    print(line)
    OUT.append(line)


TRACK = Track()          # fHeart 1.1 m, friction 0.03, resistance 2e-5 (program defaults)
PITCH_GUARD = 80.0       # a "camelback" never steeper than this (shape guard), degrees

# ------------------------------------------------------------ the element


class Camelback:
    """A valley-to-valley airtime hill made of three force sections.

    S1 pull-up and climb: 1 -> Gp (cubic, t1), hold Gp (th), Gp -> Ga (cubic, t2)
    S2 crest:             hold Ga (t_air)
    S3 drop and pull-out: Ga -> Gp (cubic, t4), hold Gp (th), Gp -> 1 (cubic, t5)
    Every transition is stored as a change, as in the program.
    """

    def __init__(self, Ga, Gp=3.5, t1=1.0, th=0.4, t2=0.5, t_air=2.0, t4=None, t5=None):
        self.Ga, self.Gp, self.t1, self.th = Ga, Gp, t1, th
        self.t2, self.t_air = t2, t_air
        self.t4 = t2 if t4 is None else t4
        self.t5 = t1 if t5 is None else t5

    def sections(self):
        Ga, Gp = self.Ga, self.Gp
        return [
            [("cubic", self.t1, Gp - 1.0), ("linear", self.th, 0.0), ("cubic", self.t2, Ga - Gp)],
            [("linear", self.t_air, 0.0)],
            [("cubic", self.t4, Gp - Ga), ("linear", self.th, 0.0), ("cubic", self.t5, 1.0 - Gp)],
        ]

    def copy(self):
        c = Camelback(self.Ga, self.Gp, self.t1, self.th, self.t2, self.t_air, self.t4, self.t5)
        return c


def run_el(el, start):
    return run(TRACK, start, el.sections())


def valid(res):
    return (not res.stall) and res.xmax_pitch < PITCH_GUARD


def onset_of(el):
    """Largest prescribed onset rate in the element (g/s), from the shapes."""
    s = PEAK_SLOPE["cubic"]
    return max(s * abs(el.Gp - 1.0) / el.t1, s * abs(el.Gp - el.Ga) / el.t2,
               s * abs(el.Gp - el.Ga) / el.t4, s * abs(el.Gp - 1.0) / el.t5)


# Unknowns are attribute names of Camelback; goals are functions of the result.
def g_exit_pitch(res, start, target):
    return math.degrees(res.nodes[-1].th) - target   # unwrapped angle


def g_crest_y(res, start, target):
    return res.ymax - target


def g_exit_y(res, start, target):
    return res.nodes[-1].y - target


GOAL_SCALE = {g_exit_pitch: 1.0, g_crest_y: 1.0, g_exit_y: 1.0}
GOAL_TOL = {g_exit_pitch: 0.05, g_crest_y: 0.05, g_exit_y: 0.05}
BOUNDS = {"t2": (0.05, 6.0), "t_air": (0.0, 12.0), "t4": (0.05, 6.0), "Gp": (1.5, 6.0)}
FD = {"t2": 0.01, "t_air": 0.01, "t4": 0.01, "Gp": 0.01}


def solve(el, start, unknowns, goals, x0=None, max_iter=30):
    """goals: list of (goal_function, target). Returns (report, solved element)."""
    work = el.copy()

    def fun(x):
        for name, val in zip(unknowns, x):
            setattr(work, name, val)
            if name == "t2" and "t4" not in unknowns and el.t4 == el.t2:
                work.t4 = val
        res = run_el(work, start)
        if not valid(res):
            return None
        return [g(res, start, tgt) for (g, tgt) in goals]

    if x0 is None:
        x0 = [getattr(el, u) for u in unknowns]
    rep = damped_newton(fun, x0,
                        [BOUNDS[u][0] for u in unknowns], [BOUNDS[u][1] for u in unknowns],
                        [GOAL_SCALE[g] for (g, _) in goals], [FD[u] for u in unknowns],
                        [GOAL_TOL[g] for (g, _) in goals], max_iter=max_iter)
    fun(rep.x)
    return rep, work


def solve_tair_level(el, start):
    """Inner 1D solve: t_air so the exit is level (unwrapped pitch 0)."""
    work = el.copy()

    def f(ta):
        work.t_air = ta
        res = run_el(work, start)
        if res.stall:
            return None
        return math.degrees(res.nodes[-1].th)

    lo, flo = 0.0, f(0.0)
    hi, fhi = 0.5, f(0.5)
    ev = 2
    while flo is not None and fhi is not None and fhi > 0 and hi < 12.0:
        lo, flo = hi, fhi
        hi *= 2.0
        fhi = f(hi)
        ev += 1
    if flo is None or fhi is None or flo * fhi > 0:
        return None, ev
    root, e2 = bracketed_root(f, lo, hi, flo, fhi, tol=0.01)
    return root, ev + e2


def start_by_inner_solve(el, start):
    """A valid starting point: keep el.t2, solve t_air for a level exit."""
    ta, ev = solve_tair_level(el, start)
    if ta is not None:
        el.t_air = ta
    return ev


def fmt(v, nd=2):
    return ("%." + str(nd) + "f") % v


# ------------------------------------------------------------ check 0

def check0():
    say("## Check 0: the integrator against the closed form for a constant-g crest")
    say("")
    say("Heart offset 0, no friction, no drag. For a hold at constant normal force n the")
    say("exact solution keeps v * |n - cos(pitch)| constant. Entry 20 m/s at +30 deg, 1.5 s hold.")
    say("")
    say("| n (g) | v|n-cos| at entry | at exit | relative error | exit pitch (deg) |")
    say("|---|---|---|---|---|")
    tr = Track(heart=0.0, friction=0.0, resistance=0.0)
    for n in (0.2, 0.0, -0.5, -1.0):
        a = anchor(tr, 20.0, pitch_deg=30.0, n=n)
        r = run(tr, a, [[("linear", 1.5, 0.0)]])
        e = r.nodes[-1]
        i0 = 20.0 * abs(n - math.cos(math.radians(30.0)))
        i1 = e.v * abs(n - math.cos(e.th))
        say("| %s | %s | %s | %.1e | %s |" % (fmt(n, 1), fmt(i0, 4), fmt(i1, 4), abs(i1 - i0) / i0,
                                             fmt(math.degrees(e.th), 2)))
    say("")


# ------------------------------------------------------------ experiment A

def experiment_a():
    say("## Experiment A: one camelback, two unknowns, two goals")
    say("")
    say("Element: valley-to-valley camelback, Gp = 3.5 g valley force, t1 = 1.0 s ramps,")
    say("0.4 s holds at Gp, cubic transitions; the drop mirrors the climb (t4 = t2).")
    say("Goals: exit level (unwrapped pitch 0) and crest (heartline) at a stated height.")
    say("Unknowns: t2 (the Gp -> airtime transition) and t_air (airtime hold).")
    say("Solver: damped Newton, forward-difference Jacobian (step 0.01 s), backtracking")
    say("by halving; tolerance 0.05 deg and 0.05 m; shape guard: |pitch| < 80 deg, no stall.")
    say("An 'evaluation' is one integration of the whole element.")
    say("")

    # A1: one case, several starts
    v0, Ga, H = 30.0, 0.0, 38.0
    start = anchor(TRACK, v0)
    say("### A1: entry 30 m/s, airtime 0.0 g, crest 38 m above the entry; different starting guesses")
    say("")
    say("| start t2, t_air (s) | result | evaluations | t2 (s) | t_air (s) | exit pitch (deg) | crest (m) | exit y (m) | exit speed (m/s) |")
    say("|---|---|---|---|---|---|---|---|---|")
    for (a, b) in [(0.5, 2.0), (0.2, 4.0), (1.0, 1.0), (0.1, 8.0), (2.0, 3.0), (0.6, 0.5)]:
        el = Camelback(Ga, t2=a, t_air=b)
        rep, w = solve(el, start, ["t2", "t_air"], [(g_exit_pitch, 0.0), (g_crest_y, H)])
        res = run_el(w, start)
        e = res.nodes[-1]
        say("| %s, %s | %s | %d | %s | %s | %s | %s | %s | %s |" % (
            fmt(a, 1), fmt(b, 1), rep.reason, rep.evals, fmt(w.t2, 3), fmt(w.t_air, 3),
            fmt(math.degrees(e.th), 3), fmt(res.ymax, 3), fmt(e.y, 3), fmt(e.v, 3)))
    say("")

    # A2: the degenerate goal pair
    say("### A2: the goal pair 'exit level at the entry height' with a mirrored element")
    say("")
    say("Same element; goals exit pitch 0 and exit height = entry height (0 m).")
    say("")
    say("| case | unknowns | result | evaluations | exit pitch (deg) | exit y (m) | crest (m) |")
    say("|---|---|---|---|---|---|---|")
    for label, tr in [("friction on (program defaults)", TRACK),
                      ("no friction, no drag", Track(friction=0.0, resistance=0.0))]:
        st = anchor(tr, v0)
        el = Camelback(Ga, t2=0.5, t_air=3.5)

        def fun(x, el=el, st=st, tr=tr):
            el.t2 = el.t4 = x[0]
            el.t_air = x[1]
            r = run(tr, st, el.sections())
            if r.stall or r.xmax_pitch >= PITCH_GUARD:
                return None
            return [math.degrees(r.nodes[-1].th), r.nodes[-1].y]
        rep = damped_newton(fun, [0.5, 3.5], [0.05, 0.0], [6.0, 12.0], [1.0, 1.0], [0.01, 0.01],
                            [0.05, 0.05])
        fun(rep.x)
        r = run(tr, st, el.sections())
        say("| %s | t2 (= t4), t_air | %s | %d | %s | %s | %s |" % (
            label, rep.reason, rep.evals, fmt(math.degrees(r.nodes[-1].th), 3),
            fmt(r.nodes[-1].y, 3), fmt(r.ymax, 2)))
    say("")
    say("Why: with t_air solved for a level exit, the exit height of the mirrored hill")
    say("for several t2 (entry 30 m/s, airtime 0.0 g):")
    say("")
    say("| t2 (s) | exit y, friction on (m) | exit y, no friction or drag (m) |")
    say("|---|---|---|")
    for t2 in (0.2, 0.4, 0.6, 0.8):
        row = []
        for tr in (TRACK, Track(friction=0.0, resistance=0.0)):
            st = anchor(tr, v0)
            el = Camelback(Ga, t2=t2)
            w = el.copy()

            def f(ta, w=w, st=st, tr=tr):
                w.t_air = ta
                r = run(tr, st, w.sections())
                return None if r.stall else math.degrees(r.nodes[-1].th)
            ta, _ = bracketed_root(f, 0.0, 12.0, tol=0.01)
            if ta is None:
                lo = 0.5
                while f(lo) is not None and f(lo) > 0 and lo < 12:
                    lo *= 2
                ta, _ = bracketed_root(f, lo / 2, lo, tol=0.01)
            if ta is None:
                row.append("no level exit")
                continue
            w.t_air = ta
            r = run(tr, st, w.sections())
            row.append(fmt(r.nodes[-1].y, 3) if r.xmax_pitch < PITCH_GUARD else "fails guard")
        say("| %s | %s | %s |" % (fmt(t2, 1), row[0], row[1]))
    say("")
    say("| case | unknowns | result | evaluations | exit pitch (deg) | exit y (m) | crest (m) |")
    say("|---|---|---|---|---|---|---|")
    # the 3-unknown version that breaks the mirror
    el = Camelback(Ga, t2=0.5, t4=0.5)
    start_by_inner_solve(el, start)
    rep, w = solve(el, start, ["t2", "t_air", "t4"],
                   [(g_exit_pitch, 0.0), (g_crest_y, H), (g_exit_y, 0.0)])
    r = run_el(w, start)
    say("| friction on; third goal crest 38 m | t2, t_air, t4 (drop no longer mirrors) | %s | %d | %s | %s | %s |" % (
        rep.reason, rep.evals, fmt(math.degrees(r.nodes[-1].th), 3), fmt(r.nodes[-1].y, 3),
        fmt(r.ymax, 2)))
    say("")
    say("Solved t2 = %s s, t4 = %s s, t_air = %s s." % (fmt(w.t2, 3), fmt(w.t4, 3), fmt(w.t_air, 3)))
    say("")

    # A3: the map
    say("### A3: where a solution exists (crest 25 m above the entry, exit level)")
    say("")
    say("For each entry speed and airtime value: t2 is scanned from 0.05 s to 4.00 s in")
    say("0.05 s steps; for each t2, t_air is solved for a level exit (bracketed secant);")
    say("the crest height is then a function of t2 alone. Each cell: number of t2 values")
    say("that reach 25 m within the shape guard / the reachable crest range in m /")
    say("damped Newton from (t2, t_air) = (0.5 s, 2.0 s): evaluations if it converged,")
    say("'fail' otherwise / damped Newton started from the scan's valid point whose")
    say("crest is nearest 25 m (evaluations of Newton only; '-' = no valid point).")
    say("'none valid' = no t2 gives a valid hill.")
    say("")
    H = 25.0
    speeds = [15.0, 20.0, 25.0, 30.0, 35.0, 40.0]
    airs = [0.2, 0.0, -0.2, -0.4, -0.6, -0.8, -1.0]
    say("| entry speed (m/s) | " + " | ".join("%+.1f g" % a for a in airs) + " |")
    say("|---|" + "---|" * len(airs))
    total_scan = 0
    for v0 in speeds:
        st = anchor(TRACK, v0)
        cells = []
        for Ga in airs:
            hs = []
            tas = []
            for k in range(1, 81):
                t2 = 0.05 * k
                el = Camelback(Ga, t2=t2)
                ta, ev = solve_tair_level(el, st)
                total_scan += ev
                tas.append(ta)
                if ta is None:
                    hs.append(None)
                    continue
                el.t_air = ta
                r = run_el(el, st)
                total_scan += 1
                hs.append(r.ymax if valid(r) else None)
            vals = [h for h in hs if h is not None]
            roots = 0
            for i in range(1, len(hs)):
                if hs[i] is not None and hs[i - 1] is not None and (hs[i] - H) * (hs[i - 1] - H) <= 0:
                    roots += 1
            el = Camelback(Ga, t2=0.5, t_air=2.0)
            rep, w = solve(el, st, ["t2", "t_air"], [(g_exit_pitch, 0.0), (g_crest_y, H)])
            nwt = ("%d" % rep.evals) if rep.ok else "fail"
            best = None
            for k_, h_ in enumerate(hs):
                if h_ is not None and (best is None or abs(h_ - H) < best[0]):
                    best = (abs(h_ - H), 0.05 * (k_ + 1), tas[k_])
            if best is None:
                nwt2 = "-"
            else:
                el = Camelback(Ga, t2=best[1], t_air=best[2])
                rep2, w2 = solve(el, st, ["t2", "t_air"], [(g_exit_pitch, 0.0), (g_crest_y, H)])
                nwt2 = ("%d" % rep2.evals) if rep2.ok else "fail"
            if vals:
                cells.append("%d / %s-%s / %s / %s" % (roots, fmt(min(vals), 0), fmt(max(vals), 0),
                                                       nwt, nwt2))
            else:
                cells.append("none valid / %s / %s" % (nwt, nwt2))
        say("| %s | " % fmt(v0, 0) + " | ".join(cells) + " |")
    say("")
    say("(%d element evaluations for the scan.)" % total_scan)
    say("")


# ------------------------------------------------------------ experiment B

def layout_run(els, start, runout=2.0):
    """Run elements back to back, then a level 1 g run-out; returns exit nodes."""
    secs = []
    for el in els:
        secs.extend(el.sections())
    secs.append([("linear", runout, 0.0)])
    res = run(TRACK, start, secs)
    return res


def experiment_b():
    say("## Experiment B: the exit contract in a two-element layout")
    say("")
    say("Layout: camelback E1 (airtime 0.0 g, crest at y = 38 m), camelback E2 (airtime")
    say("-0.6 g, crest at y = 28 m), then 2 s of level 1 g run-out. Entry at y = 0.")
    say("Each element's contract: exit level at y = 0 m with its crest at the stated")
    say("height; unknowns t2, t_air, t4 (three goals, three unknowns).")
    say("Baseline entry 30 m/s. Changes: entry speed +3 % and -3 % (the energy changes, as")
    say("with a taller lift), and 'same energy, entry 1 m lower' (as with a deeper drop")
    say("before the element). Cases: nothing re-solves (the solved numbers are kept);")
    say("E1 re-solves its contract; E1 and E2 both re-solve.")
    say("")
    v0 = 30.0
    goals1 = [(g_exit_pitch, 0.0), (g_crest_y, 38.0), (g_exit_y, 0.0)]
    goals2 = [(g_exit_pitch, 0.0), (g_crest_y, 28.0), (g_exit_y, 0.0)]
    unk = ["t2", "t_air", "t4"]

    def solve_layout(start, e1, e2, re1, re2, fresh=False):
        e1, e2 = e1.copy(), e2.copy()
        ev = 0
        if re1 and fresh:
            ev += start_by_inner_solve(e1, start)
        if re1:
            rep, e1 = solve(e1, start, unk, goals1)
            ev += rep.evals
            if not rep.ok:
                return None, rep.reason, ev
        r1 = run_el(e1, start)
        mid = r1.nodes[-1]
        if re2 and fresh:
            ev += start_by_inner_solve(e2, mid)
        if re2:
            rep, e2 = solve(e2, mid, unk, goals2)
            ev += rep.evals
            if not rep.ok:
                return None, rep.reason, ev
        return layout_run([e1, e2], start), (e1, e2), ev

    base_start = anchor(TRACK, v0)
    e1 = Camelback(0.0, t2=0.5, t_air=3.5)
    e2 = Camelback(-0.6, t2=0.5, t_air=2.5)
    base, els, ev0 = solve_layout(base_start, e1, e2, True, True, fresh=True)
    if base is None:
        say("Baseline could not be solved (%d evaluations); experiment B skipped." % ev0)
        say("")
        return
    e1, e2 = els
    bn = base.nodes
    b1, bend = bn[2], bn[-1]
    say("Baseline solved in %d evaluations. E1: t2 %s, t_air %s, t4 %s s. E2: t2 %s, t_air %s, t4 %s s." % (
        ev0, fmt(e1.t2, 3), fmt(e1.t_air, 3), fmt(e1.t4, 3), fmt(e2.t2, 3), fmt(e2.t_air, 3), fmt(e2.t4, 3)))
    say("Baseline end of layout: x %s m, y %s m, pitch %s deg, speed %s m/s." % (
        fmt(bend.x), fmt(bend.y), fmt(math.degrees(bend.th), 2), fmt(bend.v, 3)))
    say("")
    say("Differences from the baseline, at the exit of E1 and at the end of the layout:")
    say("")
    say("| change | case | E1 exit: dx (m) | dy (m) | dpitch (deg) | dv (m/s) | end: dx (m) | dy (m) | dpitch (deg) | dv (m/s) | evaluations |")
    say("|---|---|---|---|---|---|---|---|---|---|---|")
    g = F_G
    h9 = 0.9 * TRACK.heart
    changes = [
        ("entry speed +3 %", anchor(TRACK, v0 * 1.03)),
        ("entry speed -3 %", anchor(TRACK, v0 * 0.97)),
        ("same energy, entry 1 m lower", anchor(TRACK, math.sqrt(v0 * v0 + 2 * g * 1.0), y=-1.0)),
    ]
    _ = h9
    for label, st in changes:
        for case, r1, r2 in [("nothing re-solves", False, False),
                             ("E1 re-solves", True, False),
                             ("E1 and E2 re-solve", True, True)]:
            res, els, ev = solve_layout(st, e1, e2, r1, r2)
            if res is None or res.stall:
                say("| %s | %s | not solved: %s | | | | | | | | %d |" % (label, case, els, ev))
                continue
            n1, ne = res.nodes[2], res.nodes[-1]
            say("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %d |" % (
                label, case,
                fmt(n1.x - b1.x), fmt(n1.y - b1.y), fmt(math.degrees(n1.th - b1.th), 2), fmt(n1.v - b1.v, 3),
                fmt(ne.x - bend.x), fmt(ne.y - bend.y), fmt(math.degrees(ne.th - bend.th), 2),
                fmt(ne.v - bend.v, 3), ev))
    say("")


# ------------------------------------------------------------ experiment C

def experiment_c():
    say("## Experiment C: transition lengths from an onset-rate limit")
    say("")
    say("Every transition is as short as the limit J (g/s) allows: a cubic of change dG")
    say("lasts 1.5 |dG| / J seconds (its peak slope is 1.5 dG per unit time). So t1 = t5")
    say("= 1.5 (Gp - 1) / J and t2 = t4 = 1.5 (Gp - Ga) / J, holds at Gp 0.4 s. With the")
    say("transition lengths no longer free, the unknowns become Gp (valley force) and")
    say("t_air; goals: exit level, crest 25 m above the entry. Airtime 0.0 g and -0.6 g.")
    say("Starting point: Gp scanned from 1.5 to 5.0 g in 0.25 g steps with t_air solved")
    say("for a level exit at each; the valid one with the crest nearest 25 m is the start")
    say("(those evaluations included). Then damped Newton.")
    say("")
    say("| airtime (g) | entry (m/s) | J (g/s) | result | evals | Gp (g) | t_air (s) | t2 (s) | track length (m) | horizontal span (m) | peak onset (g/s) |")
    say("|---|---|---|---|---|---|---|---|---|---|---|")
    H = 25.0
    for Ga in (0.0, -0.6):
        for v0 in (25.0, 30.0, 35.0, 40.0):
            st = anchor(TRACK, v0)
            for J in (2.0, 3.0, 5.0, 10.0):
                work = Camelback(Ga)

                def fun(x, work=work, J=J, st=st, Ga=Ga):
                    Gp, ta = x
                    work.Gp = Gp
                    work.t1 = work.t5 = 1.5 * (Gp - 1.0) / J
                    work.t2 = work.t4 = 1.5 * (Gp - Ga) / J
                    work.t_air = ta
                    r = run_el(work, st)
                    if not valid(r):
                        return None
                    return [math.degrees(r.nodes[-1].th), r.ymax - H]
                # starting point: scan Gp, solve t_air for a level exit at each,
                # keep the valid one whose crest is nearest the goal
                ev_in = 0
                best = None
                for k in range(15):
                    Gp = 1.5 + 0.25 * k
                    work.Gp = Gp
                    work.t1 = work.t5 = 1.5 * (Gp - 1.0) / J
                    work.t2 = work.t4 = 1.5 * (Gp - Ga) / J
                    ev_in += start_by_inner_solve(work, st)
                    r0 = run_el(work, st)
                    ev_in += 1
                    if valid(r0) and abs(math.degrees(r0.nodes[-1].th)) < 1.0:
                        if best is None or abs(r0.ymax - H) < best[0]:
                            best = (abs(r0.ymax - H), Gp, work.t_air)
                if best is None:
                    say("| %+.1f | %s | %s | no valid start | %d | | | | | | |" % (
                        Ga, fmt(v0, 0), fmt(J, 0), ev_in))
                    continue
                rep = damped_newton(fun, [best[1], best[2]], [1.5, 0.0], [6.0, 12.0], [1.0, 1.0],
                                    [0.01, 0.01], [0.05, 0.05])
                rep.evals += ev_in
                fun(rep.x)
                r = run_el(work, st)
                e = r.nodes[-1]
                if rep.ok:
                    say("| %+.1f | %s | %s | ok | %d | %s | %s | %s | %s | %s | %s |" % (
                        Ga, fmt(v0, 0), fmt(J, 0), rep.evals, fmt(work.Gp, 2), fmt(work.t_air, 2),
                        fmt(work.t2, 2), fmt(e.L, 1), fmt(e.x, 1), fmt(r.peak_onset, 2)))
                else:
                    say("| %+.1f | %s | %s | %s | %d | (%s) | (%s) | | | | |" % (
                        Ga, fmt(v0, 0), fmt(J, 0), rep.reason, rep.evals, fmt(work.Gp, 2),
                        fmt(work.t_air, 2)))
    say("")


def main():
    t0 = time.time()
    say("# R6 prototype results (2D stand-in for a force section; see fvd2d.py)")
    say("")
    say("Track inputs: heartline 1.1 m, friction 0.03, resistance 2e-5 (program defaults), 1000 steps/s.")
    say("")
    check0()
    experiment_a()
    experiment_b()
    experiment_c()
    say("Run time %.0f s." % (time.time() - t0))
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "results.txt"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write("\n".join(OUT) + "\n")


if __name__ == "__main__":
    main()
