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
"""A two-dimensional stand-in for FVD++'s force section, for research R6.

This is NOT the program. It models one point on the heartline moving in the
vertical plane, unbanked, the way core/secforced.cpp (updateSection(), time
argument) integrates a force section, so that parametric-element ideas can be
tried before the window-less core exists. Standard library only.

What is mirrored from secforced::updateSection() (time argument):
  * 1000 steps per second (F_HZ), g = 9.80665 (F_G, lenassert.h).
  * The prescribed normal force is read at t = (i+1)/F_HZ for step i.
  * Turning: angle per step = (n - cos(pitch)) * g / F_HZ / estVel, where
    estVel is the heartline distance moved in the previous step times F_HZ
    (the previous node's speed if that distance is zero), as in the code.
  * Position: the rail point (vPos + fHeart * vNorm) advances by the mean of the
    old and new directions times the previous speed / F_HZ; the heartline point
    follows the rotation of the normal.
  * Length for friction is the rail distance (fDistFromLast, fTotalLength).
  * Energy: E -= v^3 / F_HZ * fResistance; then
    v = sqrt(2 (E - g (y_ref + fTotalLength * fFriction))), where y_ref is the
    point 0.9 * fHeart from the heartline toward the rail (vPosHeart(0.9 fHeart)).
  * The node's stored normal force is recomputed from the geometry,
    n = cos(pitch) + dpitch * heartDist * F_HZ^2 / g, and the next section's
    first transition starts from that value (translateValues()).
  * Transitions store a change, not an end value; shapes as in
    subfunc::getValue(): linear, cubic, quintic (arg1 = 0), sinusoidal.
  * The node count of a section is int(duration * F_HZ + 0.5).
  * The anchor's energy: 0.5 v^2 + g (y - 0.9 fHeart cos(pitch)) with
    fTotalLength = 0 (track.cpp; the real anchor uses pitch 0 here).

Simplifications (each is listed again in docs/research/r6-parametric-elements.md):
  * 2D: no roll, no lateral force, no yaw. The frame is the pitch angle alone,
    kept as a continuous angle (the program's getPitch() wraps at +-90 degrees;
    it is not used here).
  * Python floats are 64-bit; the program uses 32-bit floats.
  * Only time-argument force sections; no geometric, straight or curved
    sections, no distance argument, no smoothing, no time warp, no locked
    transitions.
  * No stall handling like the program's (a negative energy argument gives NaN
    there); here the run stops and reports a stall.
"""

import math

F_HZ = 1000.0
F_G = 9.80665


class Track:
    """The track-wide inputs (track.cpp defaults)."""

    def __init__(self, heart=1.1, friction=0.03, resistance=2e-5):
        self.heart = heart
        self.friction = friction
        self.resistance = resistance


class Node:
    __slots__ = ("x", "y", "th", "v", "E", "L", "hd", "n", "t")

    def copy(self):
        o = Node()
        for k in Node.__slots__:
            setattr(o, k, getattr(self, k))
        return o


def anchor(track, v, y=0.0, x=0.0, pitch_deg=0.0, n=None):
    """An entry node like track::anchorNode (heartline point at (x, y))."""
    a = Node()
    a.x, a.y = x, y
    a.th = math.radians(pitch_deg)
    a.v = v
    a.L = 0.0
    a.hd = 0.0
    a.t = 0.0
    a.E = 0.5 * v * v + F_G * (y - 0.9 * track.heart * math.cos(a.th))
    a.n = math.cos(a.th) if n is None else n
    return a


def shape(kind, u):
    """Normalised transition shapes from subfunc::getValue(), 0..1 -> 0..1."""
    if kind == "linear":
        return u
    if kind == "cubic":
        return u * u * (3.0 - 2.0 * u)
    if kind == "quintic":
        return u * u * u * (10.0 + u * (-15.0 + u * 6.0))
    if kind == "sinusoidal":
        return 0.5 * (1.0 - math.cos(math.pi * u))
    raise ValueError(kind)


# Peak slope of each shape (change per unit of normalised time). The onset rate
# of a transition of change dG over T seconds is PEAK_SLOPE * |dG| / T g/s.
PEAK_SLOPE = {"linear": 1.0, "cubic": 1.5, "quintic": 1.875, "sinusoidal": math.pi / 2}


class Stall(Exception):
    pass


class Result:
    """What one run of a section list reports."""

    def __init__(self):
        self.nodes = []        # exit node of each section
        self.ymax = -1e9
        self.ymax_x = 0.0
        self.xmax_pitch = 0.0  # largest |pitch| seen, degrees
        self.peak_onset = 0.0  # largest |dn/dt| of the prescribed force, g/s
        self.stall = False
        self.trace = None


def run(track, start, sections, trace_every=0):
    """Integrate a list of force sections from node `start`.

    sections: list of sections; a section is a list of transitions
    (kind, duration_s, change_g). Returns a Result; raises nothing.
    """
    h = track.heart
    mu = track.friction
    k = track.resistance
    res = Result()
    if trace_every:
        res.trace = []
    nd = start.copy()
    x, y, th, v, E, L, hd = nd.x, nd.y, nd.th, nd.v, nd.E, nd.L, nd.hd
    n_rec = nd.n
    t_abs = nd.t
    ymax, ymax_x, maxp, onset = y, x, abs(th), 0.0
    c = math.cos(th)
    s = math.sin(th)
    inv2hz = 1.0 / (2.0 * F_HZ)
    step = 0
    for sec in sections:
        total = sum(d for (_, d, _) in sec)
        num = int(total * F_HZ + 0.5)
        # translateValues(): first transition starts at the entry node's force,
        # later ones chain from the previous one's end value.
        bounds = []
        sv = n_rec
        tmin = 0.0
        for (kind, d, ch) in sec:
            bounds.append((tmin, tmin + d, kind, sv, ch))
            sv += ch
            tmin += d
        j = 0
        prev_target = n_rec
        for i in range(num):
            tt = (i + 1) / F_HZ
            while j < len(bounds) - 1 and bounds[j][1] < tt:
                j += 1
            a0, a1, kind, sv0, ch = bounds[j]
            u = (tt - a0) / (a1 - a0) if a1 > a0 else 1.0
            if u > 1.0:
                u = 1.0
            elif u < 0.0:
                u = 0.0
            n = sv0 + ch * shape(kind, u)
            if i > 0:
                o = abs(n - prev_target) * F_HZ
                if o > onset:
                    onset = o
            prev_target = n
            est = hd * F_HZ if hd > 1.1920929e-07 else v
            dth = (n - c) * F_G / F_HZ / est
            th2 = th + dth
            c2 = math.cos(th2)
            s2 = math.sin(th2)
            rx = (c2 + c) * v * inv2hz
            ry = (s2 + s) * v * inv2hz
            dx = rx + h * (s - s2)
            dy = ry + h * (c2 - c)
            x += dx
            y += dy
            L += math.sqrt(rx * rx + ry * ry)
            hd = math.sqrt(dx * dx + dy * dy)
            E -= v * v * v / F_HZ * k
            arg = 2.0 * (E - F_G * (y - 0.9 * h * c2 + L * mu))
            if arg <= 0.0:
                res.stall = True
                v = 0.0
                break
            v = math.sqrt(arg)
            n_rec = c2 + dth * hd * F_HZ * F_HZ / F_G
            th, c, s = th2, c2, s2
            if y > ymax:
                ymax, ymax_x = y, x
            if abs(th) > maxp:
                maxp = abs(th)
            step += 1
            if trace_every and step % trace_every == 0:
                res.trace.append((t_abs + step / F_HZ, x, y, math.degrees(th), v, n_rec))
        out = Node()
        out.x, out.y, out.th, out.v, out.E, out.L, out.hd, out.n = x, y, th, v, E, L, hd, n_rec
        out.t = t_abs + step / F_HZ
        res.nodes.append(out)
        if res.stall:
            break
    res.ymax, res.ymax_x = ymax, ymax_x
    res.xmax_pitch = math.degrees(maxp)
    res.peak_onset = onset
    return res


# ---------------------------------------------------------------- solvers

class SolveReport:
    def __init__(self):
        self.ok = False
        self.x = None
        self.residual = None
        self.evals = 0
        self.iters = 0
        self.reason = ""


def damped_newton(fun, x0, lo, hi, scale, fd_step, tol, max_iter=30):
    """Damped Newton with a forward-difference Jacobian and backtracking.

    fun(x) returns a list of residuals (already divided by their tolerance
    scale is applied here) or None when the run is invalid (stall, shape
    guard). lo/hi clamp each unknown. Convergence: every |r_i| / scale_i < tol_i.
    """
    rep = SolveReport()
    nvar = len(x0)

    def f(xx):
        rep.evals += 1
        r = fun(xx)
        if r is None:
            return None
        return [ri / si for ri, si in zip(r, scale)]

    def clamp(xx):
        return [min(max(xi, l), u) for xi, l, u in zip(xx, lo, hi)]

    def norm(r):
        return math.sqrt(sum(ri * ri for ri in r))

    x = clamp(list(x0))
    r = f(x)
    if r is None:
        rep.reason = "start invalid"
        rep.x = x
        return rep
    for it in range(max_iter):
        rep.iters = it
        if all(abs(ri) * si < ti for ri, si, ti in zip(r, scale, tol)):
            rep.ok = True
            break
        # Jacobian by forward differences (one run per unknown)
        J = [[0.0] * nvar for _ in range(len(r))]
        bad = False
        for j in range(nvar):
            xp = list(x)
            hstep = fd_step[j] if x[j] + fd_step[j] <= hi[j] else -fd_step[j]
            xp[j] += hstep
            rp = f(xp)
            if rp is None:
                bad = True
                break
            for i_ in range(len(r)):
                J[i_][j] = (rp[i_] - r[i_]) / hstep
        if bad:
            rep.reason = "Jacobian run invalid"
            break
        dx = solve_linear(J, [-ri for ri in r])
        if dx is None:
            rep.reason = "singular Jacobian"
            break
        lam = 1.0
        accepted = False
        n0 = norm(r)
        while lam > 1.0 / 64:
            xn = clamp([xi + lam * di for xi, di in zip(x, dx)])
            rn = f(xn)
            if rn is not None and norm(rn) < n0 * (1.0 - 0.1 * lam):
                x, r = xn, rn
                accepted = True
                break
            lam *= 0.5
        if not accepted:
            rep.reason = "no descent (stuck)"
            break
    else:
        rep.reason = "iteration limit"
    if all(abs(ri) * si < ti for ri, si, ti in zip(r, scale, tol)):
        rep.ok = True
        rep.reason = "converged"
    rep.x = x
    rep.residual = [ri * si for ri, si in zip(r, scale)]
    return rep


def solve_linear(A, b):
    """Gaussian elimination with partial pivoting; None if singular."""
    n = len(b)
    M = [list(A[i]) + [b[i]] for i in range(n)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r_: abs(M[r_][col]))
        if abs(M[piv][col]) < 1e-12:
            return None
        M[col], M[piv] = M[piv], M[col]
        for r_ in range(col + 1, n):
            fct = M[r_][col] / M[col][col]
            for c_ in range(col, n + 1):
                M[r_][c_] -= fct * M[col][c_]
    xs = [0.0] * n
    for r_ in range(n - 1, -1, -1):
        xs[r_] = (M[r_][n] - sum(M[r_][c_] * xs[c_] for c_ in range(r_ + 1, n))) / M[r_][r_]
    return xs


def bracketed_root(fun, a, b, fa=None, fb=None, tol=1e-4, max_eval=40):
    """Illinois (bracketed secant) on [a, b]; fun may return None (invalid).

    Returns (root, evaluations) or (None, evaluations).
    """
    ev = 0
    if fa is None:
        fa = fun(a)
        ev += 1
    if fb is None:
        fb = fun(b)
        ev += 1
    if fa is None or fb is None or fa * fb > 0:
        return None, ev
    side = 0
    for _ in range(max_eval):
        c = (a * fb - b * fa) / (fb - fa)
        fc = fun(c)
        ev += 1
        if fc is None:
            c = 0.5 * (a + b)
            fc = fun(c)
            ev += 1
            if fc is None:
                return None, ev
        if abs(fc) < tol:
            return c, ev
        if fc * fb > 0:
            b, fb = c, fc
            if side == -1:
                fa *= 0.5
            side = -1
        else:
            a, fa = c, fc
            if side == 1:
                fb *= 0.5
            side = 1
        if abs(b - a) < 1e-4:
            return c, ev
    return None, ev
