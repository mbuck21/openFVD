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
"""The camelback of R6 section 2.2 on the real model: three force sections, valley to valley.

S1 pull-up and climb: 1 -> Gp over t1; hold Gp for th; Gp -> Ga over t2
S2 crest:             hold Ga for tair
S3 drop and pull-out: Ga -> Gp over t4; hold Gp for th; Gp -> 1 over t5
The unknowns are set through the session's commands (transition.setLength,
transition.setChange); the readings are taken after a whole-track recompute.
"""
from e1solver import (vertical_angle, extreme_height, pitch_peak, wrap180, Problem, Unknown,
                      pitch_rate_reversals, nearest_branch)

GUARD_PITCH = 80.0


def camelback_sections(Ga=0.0, Gp=3.5, t1=1.0, th=0.4, t2=0.5, tair=2.0, t4=None, t5=None,
                       shape="cubic", entry_force=1.0, crest_roll=None, prefix="cb"):
    from fvdtext import force
    t4 = t2 if t4 is None else t4
    t5 = t1 if t5 is None else t5
    return [
        force(f"{prefix} pull-up", [(shape, t1, Gp - entry_force), ("quartic", th, 0.0), (shape, t2, Ga - Gp)]),
        force(f"{prefix} crest", [("quartic", tair, 0.0)], roll=crest_roll),
        force(f"{prefix} drop", [(shape, t4, Gp - Ga), ("quartic", th, 0.0), (shape, t5, 1.0 - Gp)]),
    ]


class Camelback:
    """One camelback starting after section `base` (0: at the anchor) of a model's track."""

    def __init__(self, model, base=0, Ga=0.0, Gp=3.5, t1=1.0, th=0.4, mirrored=True, heading0=0.0):
        self.m = model
        self.base = base
        self.Ga, self.Gp, self.t1, self.th = Ga, Gp, t1, th
        self.mirrored = mirrored
        self.heading0 = heading0
        self.s1, self.s2, self.s3 = base + 1, base + 2, base + 3

    def _t(self, section, function, transition):
        return {"section": section, "function": function, "transition": transition}

    # ---- setting the unknowns ----
    def set_t2(self, v):
        ok = self.m.set("transition.setLength", self._t(self.s1, "normal", 3), length=round(v, 6))
        if self.mirrored:
            ok &= self.m.set("transition.setLength", self._t(self.s3, "normal", 1), length=round(v, 6))
        return ok

    def set_t4(self, v):
        return self.m.set("transition.setLength", self._t(self.s3, "normal", 1), length=round(v, 6))

    def set_tair(self, v):
        return self.m.set("transition.setLength", self._t(self.s2, "normal", 1), length=round(v, 6))

    def set_forces(self, Ga=None, Gp=None):
        """Sets the airtime and valley forces as targets (changes are computed from them)."""
        self.Ga = self.Ga if Ga is None else Ga
        self.Gp = self.Gp if Gp is None else Gp
        ok = self.m.set("transition.setChange", self._t(self.s1, "normal", 1), change=round(self.Gp - 1.0, 6))
        ok &= self.m.set("transition.setChange", self._t(self.s1, "normal", 3), change=round(self.Ga - self.Gp, 6))
        ok &= self.m.set("transition.setChange", self._t(self.s3, "normal", 1), change=round(self.Gp - self.Ga, 6))
        ok &= self.m.set("transition.setChange", self._t(self.s3, "normal", 3), change=round(1.0 - self.Gp, 6))
        return ok

    def set_ramp_lengths(self, t1, t2, t4, t5):
        ok = self.m.set("transition.setLength", self._t(self.s1, "normal", 1), length=round(t1, 6))
        ok &= self.m.set("transition.setLength", self._t(self.s1, "normal", 3), length=round(t2, 6))
        ok &= self.m.set("transition.setLength", self._t(self.s3, "normal", 1), length=round(t4, 6))
        ok &= self.m.set("transition.setLength", self._t(self.s3, "normal", 3), length=round(t5, 6))
        return ok

    # ---- reading ----
    def entry(self):
        if self.base == 0:
            m0 = self.m.measure(["height", "speed", "forceNormal"], 0)
            return {"y": m0["values"]["height"], "pitch_deg": 0.0, "heading_deg": self.heading0,
                    "speed_mps": m0["values"]["speed"], "node": 0, "normal_g": m0["values"]["forceNormal"]}
        e = self.m.end(self.base)
        return e

    def read(self, guard=True):
        """The element's readings, or None when the run is not a valid hill: a stall (not a
        number), an unwrapped pitch past the guard at a section end, or a steepest point
        past the guard."""
        ent = self.entry()
        ends = [self.m.end(s) for s in (self.s1, self.s2, self.s3)]
        if any(e["non_finite"] for e in ends) or any(e["speed_mps"] <= 0.05 for e in ends):
            return None
        theta = []
        ref = ent["pitch_deg"]
        heading0 = ent["heading_deg"] if self.base > 0 else self.heading0
        for e in ends:
            ref = vertical_angle(e, heading0, ref)
            theta.append(ref)
        n0 = ent["node"]
        # The drop's pull-out can turn a whole loop between the crest section's end and the
        # exit, which section ends cannot see. In a valid drop the reported pitch falls, then
        # rises to the exit: one reversal of its rate. A loop adds two.
        if guard:
            rev = pitch_rate_reversals(self.m, ends[1]["node"], ends[2]["node"])
            if rev is None or rev > 1:
                return None
        if guard:
            # climbing at the end of S1, descending at the end of S2 (the crest is inside it)
            if not (0.0 < theta[0] < GUARD_PITCH) or not (-GUARD_PITCH < theta[1] < 0.0) \
                    or abs(theta[2]) > GUARD_PITCH:
                return None
            up = pitch_peak(self.m, n0, ends[0]["node"], rising=True)
            down = pitch_peak(self.m, ends[1]["node"], ends[2]["node"], rising=False)
            if up > GUARD_PITCH or down < -GUARD_PITCH:
                return None
        else:
            up = down = None
        crest, crest_node = extreme_height(self.m, n0, ends[2]["node"])
        return {
            "exit_pitch": theta[2], "crest": crest - ent["y"], "crest_node": crest_node,
            "exit_height": ends[2]["y"] - ent["y"], "exit_speed": ends[2]["speed_mps"],
            "entry_speed": ent["speed_mps"], "exit_force": ends[2]["normal_g"],
            "crest_start_force": ends[0]["normal_g"], "max_pitch": up, "min_pitch": down,
            "exit_x": ends[2]["x"] - ent.get("x", 0.0), "exit_heading": ends[2]["heading_deg"],
            "exit_bank": wrap180(ends[2]["bank_deg"]), "theta": theta, "ends": ends, "entry": ent,
            "crest_abs": crest, "exit_abs": ends[2]["y"],
            "length": ends[2]["dist_m"] - (ent.get("dist_m") or 0.0),
        }

    # ---- solving ----
    def problem(self, goals, unknowns=("t2", "tair")):
        """goals: [(key, target, tolerance)], keys of read(). unknowns among t2, tair, t4."""
        spec = {"t2": (self.set_t2, 0.05, 6.0, 0.01, 0.5), "tair": (self.set_tair, 0.01, 12.0, 0.01, 2.0),
                "t4": (self.set_t4, 0.05, 6.0, 0.01, 0.5)}
        self.mirrored = "t4" not in unknowns
        return Problem(self.m, [Unknown(n, *spec[n]) for n in unknowns], goals, self.read)

    def level_tair(self, t2, starts):
        """The crest hold that gives a level exit at this t2 (t4 = t2): 1D Newton from each start."""
        mirrored = self.mirrored
        self.mirrored = True
        self.set_t2(t2)
        p = Problem(self.m, [Unknown("tair", self.set_tair, 0.01, 12.0, 0.01, 2.0)],
                    [("exit_pitch", 0.0, 0.05)], self.read)
        evals = 0
        out = None
        for s in starts:
            res = p.solve([s], max_evals=25)
            evals += res.evals
            if res.converged:
                out = res
                break
        if out is None:
            # bracket: the exit pitch on a coarse grid of crest holds, then Newton from each
            # sign change between two valid neighbours (the valid holds can be a narrow band)
            pts = []
            for tair in TAIR_GRID:
                o = p.evaluate([tair])
                evals += 1
                pts.append((tair, None if o is None else o[0][0]))
            for (a, fa), (b, fb) in zip(pts, pts[1:]):
                if fa is None or fb is None or fa * fb > 0:
                    continue
                res = p.solve([a + (b - a) * fa / (fa - fb)], max_evals=15)
                evals += res.evals
                if res.converged:
                    out = res
                    break
        self.mirrored = mirrored
        return out, evals

    def scan(self, t2_grid, key="crest"):
        """R6's scan for a valid start: for each t2, t_air for a level exit; returns
        [(t2, t_air or None, reading of key or None)] and the evaluations used."""
        out, evals, prev = [], 0, None
        for t2 in t2_grid:
            starts = [prev] if prev is not None else [2.0]
            res, n = self.level_tair(t2, starts)
            evals += n
            if res is not None:
                prev = res.x[0]
                out.append((t2, res.x[0], res.values[key]))
            else:
                prev = None
                out.append((t2, None, None))
        return out, evals


# 0.05 to 1.95 s in 0.1 s steps, then 2.2 to 3.95 s in 0.25 s steps
T2_GRID = [round(0.05 + 0.1 * i, 3) for i in range(20)] + [round(2.2 + 0.25 * i, 3) for i in range(8)]
TAIR_GRID = [0.25 * i for i in range(1, 33)]   # 0.25 to 8 s


# ---- the zero-g roll: a camelback whose crest section rolls ----------------------------------------
def zero_g_roll_sections(Ga=0.0, Gp=2.5, tair=2.0, rate=360.0, orientation="quaternion", t2=0.5):
    """A camelback with a roll on the crest: roll rate 0 -> rate -> 0 over t_air (two cubic
    halves), total roll = rate * t_air / 2."""
    secs = camelback_sections(Ga=Ga, Gp=Gp, t2=t2, tair=tair, crest_roll=[("cubic", tair / 2, rate),
                                                                     ("cubic", tair / 2, -rate)])
    for s in secs:
        s["orientation"] = orientation
    return secs


class ZeroGRoll(Camelback):
    """Unknowns t2, t_air, roll rate; goals exit level, crest height, roll 360."""

    def __init__(self, model, Ga=0.0, Gp=2.5, entry_bank=0.0):
        super().__init__(model, 0, Ga=Ga, Gp=Gp, t1=1.0)
        self.rate = 360.0
        self.tair = 2.0
        self.entry_bank = entry_bank

    def set_tair(self, v):
        self.tair = v
        ok = self.m.set("transition.setLength", self._t(self.s2, "normal", 1), length=round(v, 6))
        ok &= self.m.set("transition.setLength", self._t(self.s2, "roll", 1), length=round(v / 2, 6))
        ok &= self.m.set("transition.setLength", self._t(self.s2, "roll", 2), length=round(v / 2, 6))
        return ok

    def set_rate(self, v):
        self.rate = v
        ok = self.m.set("transition.setChange", self._t(self.s2, "roll", 1), change=round(v, 6))
        ok &= self.m.set("transition.setChange", self._t(self.s2, "roll", 2), change=round(-v, 6))
        return ok

    def read(self, guard=True):
        r = super().read(guard)
        if r is None:
            return None
        commanded = self.entry_bank + self.rate * self.tair / 2.0
        from e1solver import nearest_branch
        roll_end = nearest_branch(r["ends"][1]["bank_deg"], commanded)
        r["roll"] = roll_end - self.entry_bank
        r["exit_bank"] = wrap180(r["ends"][2]["bank_deg"])
        r["heading_drift"] = wrap180(r["ends"][2]["heading_deg"] - self.heading0)
        return r

    def problem(self, goals, unknowns=("t2", "tair", "rate")):
        spec = {"t2": (self.set_t2, 0.05, 6.0, 0.01, 0.5), "tair": (self.set_tair, 0.2, 12.0, 0.01, 2.0),
                "rate": (self.set_rate, 10.0, 1500.0, 1.0, 200.0)}
        self.mirrored = True
        return Problem(self.m, [Unknown(n, *spec[n]) for n in unknowns], goals, self.read)


# ---- the overbanked turn ------------------------------------------------------------------------------
def overbank_sections(n=1.0, climb=3.0, r=150.0, hold=1.5, q=150.0, orientation="quaternion"):
    """S1 pull-up and roll-in: normal 1 -> climb (0.8 s), hold (0.8 s), climb -> n (0.8 s);
    roll rate 0 for 0.8 s, then 0 -> r -> 0 (1.6 s): bank 0.8 r.
    S2 hold: normal n for `hold` s, no roll.
    S3 roll-out and pull-out: roll rate 0 -> -q -> 0 (1.6 s): bank change -0.8 q;
    normal n -> climb -> 1 (0.8 s each)."""
    from fvdtext import force
    return [
        force("ob climb", [("cubic", 0.8, climb - 1.0), ("quartic", 0.8, 0.0), ("cubic", 0.8, n - climb)],
              roll=[("quartic", 0.8, 0.0), ("cubic", 0.8, r), ("cubic", 0.8, -r)], orientation=orientation),
        force("ob hold", [("quartic", hold, 0.0)], orientation=orientation),
        force("ob out", [("cubic", 0.8, climb - n), ("cubic", 0.8, 1.0 - climb)],
              roll=[("cubic", 0.8, -q), ("cubic", 0.8, q)], orientation=orientation),
    ]


class Overbank:
    """Unknowns r (roll-in rate), hold, q (roll-out rate), optionally n (hold force);
    goals heading change 180, exit pitch 0, exit bank 0 (unwrapped), optionally exit height."""

    def __init__(self, model, n=1.0, climb=3.0, entry_bank=0.0, heading0=0.0):
        self.m = model
        self.n, self.climb = n, climb
        self.r, self.q = 150.0, 150.0
        self.entry_bank = entry_bank
        self.heading0 = heading0

    def _t(self, s, fn, k):
        return {"section": s, "function": fn, "transition": k}

    def set_r(self, v):
        self.r = v
        ok = self.m.set("transition.setChange", self._t(1, "roll", 2), change=round(v, 6))
        return ok and self.m.set("transition.setChange", self._t(1, "roll", 3), change=round(-v, 6))

    def set_q(self, v):
        self.q = v
        ok = self.m.set("transition.setChange", self._t(3, "roll", 1), change=round(-v, 6))
        return ok and self.m.set("transition.setChange", self._t(3, "roll", 2), change=round(v, 6))

    def set_hold(self, v):
        return self.m.set("transition.setLength", self._t(2, "normal", 1), length=round(v, 6))

    def set_n(self, v):
        self.n = v
        ok = self.m.set("transition.setChange", self._t(1, "normal", 3), change=round(v - self.climb, 6))
        return ok and self.m.set("transition.setChange", self._t(3, "normal", 1), change=round(self.climb - v, 6))

    def read(self):
        from e1solver import nearest_branch
        m0 = self.m.measure(["height"], 0)
        ends = [self.m.end(s) for s in (1, 2, 3)]
        if any(e["non_finite"] or e["speed_mps"] <= 0.05 for e in ends):
            return None
        heading, ref = [], self.heading0
        for e in ends:
            ref = nearest_branch(e["heading_deg"], ref)
            heading.append(ref)
        bank_refs = [self.entry_bank + 0.8 * self.r, self.entry_bank + 0.8 * self.r,
                     self.entry_bank + 0.8 * (self.r - self.q)]
        bank = [nearest_branch(e["bank_deg"], b) for e, b in zip(ends, bank_refs)]
        pitches = [e["pitch_deg"] for e in ends]
        return {"heading": heading[2] - self.heading0, "exit_pitch": pitches[2], "exit_bank": bank[2],
                "exit_height": ends[2]["y"] - m0["values"]["height"], "max_bank": bank[0],
                "pitches": pitches, "exit_speed": ends[2]["speed_mps"], "ends": ends,
                "length": ends[2]["dist_m"]}

    def problem(self, goals, unknowns=("r", "hold", "q")):
        spec = {"r": (self.set_r, 0.0, 400.0, 0.5, 40.0), "hold": (self.set_hold, 0.05, 8.0, 0.01, 1.0),
                "q": (self.set_q, -100.0, 400.0, 0.5, 40.0), "n": (self.set_n, -1.0, 4.0, 0.01, 0.5)}
        return Problem(self.m, [Unknown(k, *spec[k]) for k in unknowns], goals, self.read)


def solve_with_scan(cb, goals, unknowns, x0=None, key="crest", target=None, max_evals=80):
    """Newton from x0 (the previous solution); if there is none or it fails, the scan
    for a valid start (t2 grid, level exit), then Newton from the scan point whose `key`
    is nearest `target`. Returns (result, evaluations including the scan, scanned?)."""
    evals = 0
    if x0 is not None:
        res = cb.problem(goals, unknowns).solve(x0, max_evals=max_evals)
        evals += res.evals
        if res.converged:
            return res, evals, False
    scan, n = cb.scan(T2_GRID, key)
    evals += n
    valid = [s for s in scan if s[2] is not None]
    if not valid:
        return None, evals, True
    best = min(valid, key=lambda s: abs(s[2] - target))
    start = [best[0], best[1]] + ([best[0]] if "t4" in unknowns else [])
    res = cb.problem(goals, unknowns).solve(start, max_evals=max_evals)
    return res, evals + res.evals, True
