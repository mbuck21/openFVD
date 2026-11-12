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
"""The E1 goal-seeking solver: damped Newton on the real model, through `fvdcore session`.

The rules (R5 C1, R6 section 4.1):
- the unknowns are command arguments (a transition's length or change, a straight's
  length, a curve's angle or radius), each with bounds and a finite-difference step;
- the goals are numbers read after a whole-track recompute (section ends, crest height,
  unwrapped angles), each with a target and a tolerance in its own unit;
- a forward-difference Jacobian, a Newton step solved exactly (least squares when there
  are more goals than unknowns), the step cut to the bounds and to a largest step,
  then backtracking by halving until the scaled miss falls;
- an evaluation that is not a number (the train stalled) or breaks a shape guard returns
  None and counts as "worse", never as an error.

One *evaluation* is: set the unknowns by commands, `recompute`, read the goals. It is
the machine-independent unit every timing in the note is backed by.
Standard library only.
"""
import math
import sys
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(REPO / "tools"))
from fvdsession import Session, SessionError  # noqa: E402


# ---- small linear algebra -----------------------------------------------------------------
def _solve_square(a, b):
    """Gaussian elimination with partial pivoting; None if singular."""
    n = len(a)
    m = [list(row) + [b[i]] for i, row in enumerate(a)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        if abs(m[p][c]) < 1e-300:
            return None
        m[c], m[p] = m[p], m[c]
        for r in range(c + 1, n):
            f = m[r][c] / m[c][c]
            for k in range(c, n + 1):
                m[r][k] -= f * m[c][k]
    x = [0.0] * n
    for r in range(n - 1, -1, -1):
        x[r] = (m[r][n] - sum(m[r][k] * x[k] for k in range(r + 1, n))) / m[r][r]
    return x


def _transpose(a):
    return [list(col) for col in zip(*a)]


def _matmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(len(b))) for j in range(len(b[0]))] for i in range(len(a))]


def lstsq(j, r):
    """Solves J dx = r (square) or the normal equations (more rows than columns)."""
    if len(j) == len(j[0]):
        return _solve_square(j, r)
    jt = _transpose(j)
    return _solve_square(_matmul(jt, j), [sum(jt[i][k] * r[k] for k in range(len(r))) for i in range(len(jt))])


def condition(j):
    """1-norm condition number of a square matrix (inf when singular)."""
    n = len(j)
    if n != len(j[0]):
        j = _matmul(_transpose(j), j)
        n = len(j)
    inv = []
    for c in range(n):
        e = [1.0 if i == c else 0.0 for i in range(n)]
        col = _solve_square(j, e)
        if col is None:
            return math.inf
        inv.append(col)
    inv = _transpose(inv)
    norm = lambda a: max(sum(abs(a[i][c]) for i in range(n)) for c in range(n))
    return norm(j) * norm(inv)


# ---- angles -----------------------------------------------------------------------------------
def wrap180(a):
    return (a + 180.0) % 360.0 - 180.0


def nearest_branch(angle, reference):
    """angle + k*360 closest to reference."""
    return angle + 360.0 * round((reference - angle) / 360.0)


def vertical_angle(end, heading0, reference):
    """The pitch turned through in the vertical plane of heading0, unwrapped: a section end
    whose heading has flipped by 180 degrees has gone over the top (180 - pitch) or under
    the bottom (-180 - pitch). The branch closest to `reference` is taken."""
    pitch = end["pitch_deg"]
    if abs(wrap180(end["heading_deg"] - heading0)) <= 90.0:
        base = pitch
    else:
        base = 180.0 - pitch
    return nearest_branch(base, reference)


# ---- the solver -----------------------------------------------------------------------------
class Result:
    def __init__(self, status, x, values, residual, evals, iters, seconds, cond, history):
        self.status = status          # "converged", "start invalid", "stalled", "no valid step", "budget"
        self.x = x
        self.values = values          # the readings at x (dict) or None
        self.residual = residual      # value - target per goal
        self.evals = evals
        self.iters = iters
        self.seconds = seconds
        self.cond = cond              # condition number of the scaled Jacobian at the start
        self.history = history

    @property
    def converged(self):
        return self.status == "converged"


def newton(evaluate, x0, lo, hi, h, tol, max_evals=80, max_step=None, min_alpha=1.0 / 64):
    """Damped Newton on evaluate(x) -> (residual list, readings) or None (invalid).

    lo, hi: bounds; h: finite-difference steps; tol: tolerance per goal (also its scale);
    max_step: largest change per unknown in one step. Returns a Result."""
    t0 = time.perf_counter()
    n = len(x0)
    evals = 0
    history = []

    def ev(x):
        nonlocal evals
        evals += 1
        out = evaluate(x)
        history.append((list(x), None if out is None else list(out[0])))
        return out

    def merit(r):
        return sum((ri / ti) ** 2 for ri, ti in zip(r, tol))

    x = [min(max(v, lo[i]), hi[i]) for i, v in enumerate(x0)]
    cur = ev(x)
    if cur is None:
        return Result("start invalid", x, None, None, evals, 0, time.perf_counter() - t0, None, history)
    r, vals = cur
    cond0 = None
    iters = 0
    while True:
        if all(abs(ri) <= ti for ri, ti in zip(r, tol)):
            return Result("converged", x, vals, r, evals, iters, time.perf_counter() - t0, cond0, history)
        if evals + n + 1 > max_evals:
            return Result("budget", x, vals, r, evals, iters, time.perf_counter() - t0, cond0, history)
        iters += 1
        # forward-difference Jacobian of the scaled residual (backward where a bound or an invalid run is hit)
        cols = []
        for j in range(n):
            col = None
            for sign in (1.0, -1.0):
                step = sign * h[j]
                if not (lo[j] <= x[j] + step <= hi[j]):
                    continue
                xj = list(x)
                xj[j] += step
                out = ev(xj)
                if out is not None:
                    col = [(out[0][i] - r[i]) / step / tol[i] for i in range(len(r))]
                    break
            if col is None:
                return Result("no valid step", x, vals, r, evals, iters, time.perf_counter() - t0, cond0, history)
            cols.append(col)
        jac = _transpose(cols)
        if cond0 is None:
            cond0 = condition(jac)
        dx = lstsq(jac, [-ri / ti for ri, ti in zip(r, tol)])
        if dx is None:
            return Result("stalled", x, vals, r, evals, iters, time.perf_counter() - t0, cond0, history)
        if max_step is not None:
            scale = max([abs(d) / m for d, m in zip(dx, max_step)] + [1.0])
            dx = [d / scale for d in dx]
        alpha = 1.0
        m0 = merit(r)
        accepted = False
        while alpha >= min_alpha and evals < max_evals:
            xn = [min(max(x[i] + alpha * dx[i], lo[i]), hi[i]) for i in range(n)]
            if xn == x:
                break
            out = ev(xn)
            if out is not None and merit(out[0]) < m0:
                x, (r, vals) = xn, out
                accepted = True
                break
            alpha /= 2.0
        if not accepted:
            return Result("stalled", x, vals, r, evals, iters, time.perf_counter() - t0, cond0, history)


# ---- driving the session ------------------------------------------------------------------------
class Model:
    """A session plus a cache of the command values last applied, so an evaluation only
    sends the commands whose argument changed; every evaluation ends with `recompute`."""

    def __init__(self, project, log=None):
        self.project = Path(project).resolve()
        self.session = Session(self.project, log=log)
        # The program writes to standard error on some runs (a stalled train); fvdsession.py
        # never reads that pipe, so once it is full the program blocks and the session hangs.
        # A thread keeps it drained and counts what came through.
        self.stderr_lines = 0
        self.stderr_sample = []
        threading.Thread(target=self._drain, daemon=True).start()
        self.applied = {}
        self.commands_sent = 0
        self.errors = []

    def _drain(self):
        pipe = self.session._proc.stderr
        try:
            for line in iter(pipe.readline, b""):
                self.stderr_lines += 1
                if len(self.stderr_sample) < 5:
                    self.stderr_sample.append(line.decode("utf-8", "replace").rstrip())
        except (OSError, ValueError):
            pass

    def set(self, name, target, **args):
        key = (name, tuple(sorted(target.items())))
        value = tuple(sorted(args.items()))
        if self.applied.get(key) == value:
            return True
        reply = self.session.run_raw(name, target, **args)
        self.commands_sent += 1
        if not reply.get("ok"):
            self.errors.append((name, target, args, reply.get("error")))
            self.applied.pop(key, None)
            return False
        self.applied[key] = value
        return True

    def recompute(self):
        self.session.recompute()

    def end(self, section):
        return self.session.section_end(section)

    def measure(self, ids, node):
        return self.session.measures(ids, node=node)

    def close(self):
        self.session.close()


class Unknown:
    """One value the solver may change: setter(v) sends the command(s)."""

    def __init__(self, name, setter, lo, hi, h, max_step):
        self.name, self.setter, self.lo, self.hi, self.h, self.max_step = name, setter, lo, hi, h, max_step


class Problem:
    """Unknowns, goals (key, target, tolerance) and a reader(model) -> dict or None.
    One evaluation sets every unknown, recomputes the whole track and reads."""

    def __init__(self, model, unknowns, goals, reader):
        self.m, self.unknowns, self.goals, self.reader = model, unknowns, goals, reader

    def evaluate(self, x):
        for u, v in zip(self.unknowns, x):
            if u.setter(v) is False:
                return None
        self.m.recompute()
        r = self.reader()
        if r is None:
            return None
        if any(r.get(k) is None or (isinstance(r[k], float) and math.isnan(r[k])) for k, _, _ in self.goals):
            return None
        return [r[k] - t for k, t, _ in self.goals], r

    def solve(self, x0, max_evals=80):
        u = self.unknowns
        return newton(self.evaluate, list(x0), [a.lo for a in u], [a.hi for a in u], [a.h for a in u],
                      [tol for _, _, tol in self.goals], max_evals=max_evals, max_step=[a.max_step for a in u])


def section_nodes(model, first, last):
    """(first node, last node) of sections first..last (1-based); first 0 means the anchor."""
    start = 0 if first <= 1 else model.end(first - 1)["node"]
    return start, model.end(last)["node"]


def extreme_height(model, n0, n1):
    """Highest centre-line height between nodes n0 and n1 (golden-section search; the
    height must rise then fall there, as over a hill)."""
    g = (math.sqrt(5) - 1) / 2
    a, b = n0, n1
    hcache = {}

    def height(n):
        if n not in hcache:
            v = model.measure(["height"], n)["values"]["height"]
            hcache[n] = -math.inf if v is None else v
        return hcache[n]

    while b - a > 3:
        c = int(round(b - g * (b - a)))
        d = int(round(a + g * (b - a)))
        if c == d:
            d += 1
        if height(c) < height(d):
            a = c
        else:
            b = d
    best = max(range(a, b + 1), key=height)
    return height(best), best


def pitch_at(model, node):
    """Pitch of the centre line at a node, from the height and distance of two nodes."""
    m0 = model.measure(["height"], node)
    m1 = model.measure(["height"], node + 1)
    dh = m1["values"]["height"] - m0["values"]["height"]
    ds = m1["dist_m"] - m0["dist_m"]
    if ds <= 0:
        return 0.0
    return math.degrees(math.asin(max(-1.0, min(1.0, dh / ds))))


def pitch_peak(model, n0, n1, rising=True):
    """The pitch where the pitch rate changes sign between n0 and n1 (bisection on the
    `pitchRate` measure): the steepest point of a climb (rising) or of a drop."""
    def rate(n):
        v = model.measure(["pitchRate"], n)["values"]["pitchRate"]
        return 0.0 if v is None else v
    a, b = n0 + 1, n1 - 2
    ra, rb = rate(a), rate(b)
    sign = 1.0 if rising else -1.0
    if not (sign * ra > 0 and sign * rb < 0):
        # no sign change: the extreme is at an end
        return max(pitch_at(model, a), pitch_at(model, b), key=lambda p: sign * p)
    while b - a > 1:
        c = (a + b) // 2
        if sign * rate(c) > 0:
            a = c
        else:
            b = c
    return pitch_at(model, a)


def pitch_rate_reversals(model, n0, n1, step=50, dead=2.0):
    """How often the `pitchRate` measure changes sign between nodes n0 and n1 (sampled every
    `step` nodes, rates within +-dead deg/s ignored), or None if a rate is not a number.
    The measure is the rate of the reported pitch, which is wrapped at +-90 degrees: in a
    loop it reverses at the vertical although the track keeps turning the same way. Section
    ends give angles only modulo a turn, so a full loop between two ends reads as nothing;
    this count is the guard that sees it."""
    signs = []
    for n in list(range(n0 + 1, n1, step)) + [n1 - 1]:
        v = model.measure(["pitchRate"], n)["values"]["pitchRate"]
        if v is None:
            return None
        if abs(v) > dead:
            s = 1 if v > 0 else -1
            if not signs or signs[-1] != s:
                signs.append(s)
    return max(len(signs) - 1, 0)


def timed(fn):
    t0 = time.perf_counter()
    out = fn()
    return out, time.perf_counter() - t0
