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
"""Trial E: re-close after an early edit (R5 C2's trial; the first slice of pins and of
"close the circuit", FR-12).

Two layouts that come back towards their start, each built from the top of the lift
(anchor 46 m, 5 m/s; see layouts.py for why):
  L1 out and back: first drop, camelback (0 g, crest 36 m), a 180 degree banked
     turnaround, a run back, then the slack.
  L2 twister: first drop, a 90 degree left turn, camelback (-0.6 g, crest 26 m), a
     second 90 degree left turn, a run back, then the slack.
The end of the track is pinned (position, heading, pitch, bank). Two kinds of slack:
  (a) a straight's length plus a flat curve's angle and radius (goals x, z, heading);
  (b) one connector force section: its duration and two bumps each of normal and lateral
      force (goals x, y, z, heading, pitch).
Edits: the first drop 1 m deeper; the lift 1 m higher (valley kept on the ground); the
camelback's airtime 0.2 g stronger. Policies: "slack only" (nothing else re-solves) and
"contract + slack" (the camelback and the turns re-solve their exit first).
Then how large a lift change each slack absorbs. Writes results/trial_e.txt.
Run: py -3 docs/research/e1-trials/trial_e.py
"""
import math
import time

from common import Report, PROJECTS, f
from e1solver import Model, Problem
from elements import Camelback, camelback_sections, solve_with_scan
from fvdtext import write_project, straight, curve
from layouts import (drop_section, turn_section, connector_section, Drop, Turn, Pin,
                     SlackStraightCurve, SlackConnector, raise_start)

TOP = 46.0
# 3 cm and 0.05 deg: a slack whose duration is an unknown moves its end in whole-millisecond
# node steps, 2.5 cm at 25 m/s, so a tighter position tolerance is met only by luck
TOL = {"x": 0.03, "y": 0.03, "z": 0.03, "heading": 0.05, "pitch": 0.05}
SANE_NORMAL = (-1.0, 4.5)
SANE_LATERAL = 1.0


class Layout:
    """Section numbers and contract of one layout."""

    def __init__(self, name):
        self.name = name
        if name == "L1":
            self.cb_base, self.ga, self.crest = 1, 0.0, 36.0
            self.turns = [(5, 180.0)]
            self.back = 6
        else:
            self.cb_base, self.ga, self.crest = 2, -0.6, 26.0
            self.turns = [(2, -90.0), (6, -90.0)]
            self.back = 7

    def sections(self, slack):
        if self.name == "L1":
            secs = ([drop_section()] + camelback_sections(Ga=0.0, prefix="hill") +
                    [turn_section(bank=60.0, name="turnaround"), straight("run back", 60.0)])
        else:
            secs = ([drop_section(), turn_section(bank=-45.0, n=1.5, name="left 1")] +
                    camelback_sections(Ga=-0.6, prefix="hill") +
                    [turn_section(bank=-45.0, n=1.5, name="left 2"), straight("run back", 40.0)])
        if slack == "a":
            secs += [straight("slack straight", 20.0), curve("slack curve", 30.0, 120.0, -90.0)]
        else:
            secs += [connector_section(3.0)]
        return secs

    def slack(self, model, kind):
        n = self.back
        return SlackStraightCurve(model, n + 1, n + 2) if kind == "a" else SlackConnector(model, n + 1)

    def last(self, kind):
        return self.back + (2 if kind == "a" else 1)

    def contract(self, model, x):
        """Re-solves the camelback (exit level at height 0, crest height) and the turns
        (heading change, level exit), front to back. x: their previous solutions."""
        evals, ok = 0, True
        for i, (sec, turned) in enumerate(self.turns):
            if sec <= self.cb_base:
                r = Turn(model, sec).problem(turned).solve(x["turn%d" % i], max_evals=40)
                evals += r.evals
                ok &= r.converged
        cb = Camelback(model, self.cb_base, Ga=self.ga)
        goals = [("exit_pitch", 0.0, 0.05), ("crest_abs", self.crest, 0.05), ("exit_abs", 0.0, 0.05)]
        r, n, _ = solve_with_scan(cb, goals, ("t2", "tair", "t4"), x0=x["cb"], key="crest_abs", target=self.crest)
        evals += n
        ok &= r is not None and r.converged
        for i, (sec, turned) in enumerate(self.turns):
            if sec > self.cb_base:
                r = Turn(model, sec).problem(turned).solve(x["turn%d" % i], max_evals=40)
                evals += r.evals
                ok &= r.converged
        return evals, ok


def build(rep, lay, kind):
    path = write_project(PROJECTS / f"e-{lay.name}-start-{kind}.fvd", lay.sections(kind), speed=5.0, y=TOP)
    m = Model(path)
    x = {}
    r = Drop(m, 1).problem(0.0).solve([1.2, 0.6])
    x["drop"] = r.x
    msgs = [f"drop {r.status} ({r.evals})"]
    for i, (sec, turned) in enumerate(lay.turns):
        if sec <= lay.cb_base:
            t = Turn(m, sec, bank=-45.0, n=1.5).problem(turned).solve([1.5, 1.5])
            x["turn%d" % i] = t.x
            msgs.append(f"turn {turned:+.0f} {t.status} ({t.evals})")
    cb = Camelback(m, lay.cb_base, Ga=lay.ga)
    cb.set_forces(Ga=lay.ga)
    goals = [("exit_pitch", 0.0, 0.05), ("crest_abs", lay.crest, 0.05), ("exit_abs", 0.0, 0.05)]
    r, n, _ = solve_with_scan(cb, goals, ("t2", "tair", "t4"), key="crest_abs", target=lay.crest)
    x["cb"] = r.x
    msgs.append(f"camelback {r.status} ({n})")
    for i, (sec, turned) in enumerate(lay.turns):
        if sec > lay.cb_base:
            start = [2.0, 1.5] if abs(turned) > 100 else [1.5, 1.5]
            t = Turn(m, sec).problem(turned).solve(start, max_evals=60)
            x["turn%d" % i] = t.x
            msgs.append(f"turn {turned:+.0f} {t.status} ({t.evals}, n {t.x[0]:.2f} g, hold {t.x[1]:.2f} s)")
    m.recompute()
    out = PROJECTS / f"e-{lay.name}-{kind}.fvd"
    m.session.save(out, force=True)
    e = m.end(lay.last(kind))
    msgs.append(f"end at x {e['x']:.1f}, y {e['y']:.1f}, z {e['z']:.1f} m, heading {e['heading_deg']:.1f} deg, "
                f"{e['speed_mps']:.1f} m/s, node {e['node']}")
    m.close()
    rep(f"{lay.name} with slack ({kind}): " + "; ".join(msgs))
    return out, x


def slack_peaks(model, lay, kind):
    n0 = model.end(lay.back)["node"]
    n1 = model.end(lay.last(kind))["node"]
    lo, hi, lat = math.inf, -math.inf, 0.0
    for n in range(n0, n1 + 1, 10):
        v = model.measure(["forceNormal", "forceLateral"], n)["values"]
        if v["forceNormal"] is None:
            return None
        lo, hi = min(lo, v["forceNormal"]), max(hi, v["forceNormal"])
        lat = max(lat, abs(v["forceLateral"]))
    return lo, hi, lat


def miss(pin, now):
    if now is None:
        return None
    d = math.sqrt(sum((now[k] - pin[k]) ** 2 for k in ("x", "y", "z")))
    return d, now["heading"] - pin["heading"], now["pitch"] - pin["pitch"], now["bank"] - pin["bank"], now["y"] - pin["y"]


def fmt_miss(mm):
    if mm is None:
        return ["stall", "", "", ""]
    return [f(mm[0] * 100, 1), f(mm[1], 3), f(mm[2], 3), f(mm[3], 3)]


def apply_edit(m, lay, edit, size, x):
    """Makes the edit; returns the evaluations of the first-drop re-solve it includes."""
    if edit == "lift":
        raise_start(m, size, TOP)
        r = Drop(m, 1).problem(0.0).solve(x["drop"], max_evals=40)
        return r.evals
    if edit == "drop":
        r = Drop(m, 1).problem(-size).solve(x["drop"], max_evals=40)
        return r.evals
    if edit == "airtime":
        cb = Camelback(m, lay.cb_base, Ga=lay.ga)
        cb.set_forces(Ga=lay.ga - size)
        return 0
    raise ValueError(edit)


def reclose(path, lay, kind, edit, size, x, policy, slack_start=None):
    m = Model(path)
    m.recompute()
    last = lay.last(kind)
    pin = Pin.read(m, last, 0.0)
    pin_peaks = slack_peaks(m, lay, kind)
    sl = lay.slack(m, kind)
    u = sl.unknowns()
    x0 = slack_start or [float(v) for v in BASE_SLACK[kind]]
    drop_evals = apply_edit(m, lay, edit, size, x)
    m.recompute()
    before = miss(pin, Pin.read(m, last, pin["heading"]))
    t0 = time.perf_counter()
    c_evals, c_ok = 0, True
    if policy == "contract + slack":
        c_evals, c_ok = lay.contract(m, x)
    goals = [(k, pin[k], TOL[k]) for k in sl.goal_keys]
    reader = lambda: Pin.read(m, last, pin["heading"])
    res = Problem(m, u, goals, reader).solve(x0, max_evals=120)
    secs = time.perf_counter() - t0
    m.recompute()
    after = miss(pin, Pin.read(m, last, pin["heading"]))
    peaks = slack_peaks(m, lay, kind)
    m.close()
    return {"pin_peaks": pin_peaks, "before": before, "after": after, "res": res, "c_evals": c_evals,
            "c_ok": c_ok, "secs": secs, "peaks": peaks, "drop_evals": drop_evals}


BASE_SLACK = {"a": [20.0, 30.0, 120.0], "b": [3.0, 0.0, 0.0, 0.0, 0.0]}


def sane(peaks):
    return peaks is not None and SANE_NORMAL[0] <= peaks[0] and peaks[1] <= SANE_NORMAL[1] and peaks[2] <= SANE_LATERAL


def fmt_peaks(p):
    return "stall" if p is None else f"{p[0]:.2f} / {p[1]:.2f} / {p[2]:.2f}"


def main():
    rep = Report("trial_e")
    layouts = {}
    for name in ("L1", "L2"):
        lay = Layout(name)
        for kind in ("a", "b"):
            layouts[(name, kind)] = (lay,) + build(rep, lay, kind)
    rep("")
    rows = []
    for (name, kind), (lay, path, x) in layouts.items():
        for edit, size in (("drop", 1.0), ("lift", 1.0), ("airtime", 0.2)):
            for policy in ("slack only", "contract + slack"):
                r = reclose(path, lay, kind, edit, size, x, policy)
                res = r["res"]
                rows.append([name, kind, {"drop": "first drop 1 m deeper", "lift": "lift 1 m higher",
                                          "airtime": "airtime 0.2 g stronger"}[edit], policy]
                            + fmt_miss(r["before"]) + [res.status] + fmt_miss(r["after"])
                            + [fmt_peaks(r["pin_peaks"]), fmt_peaks(r["peaks"]),
                               f"{r['c_evals']}" + ("" if r["c_ok"] else " (failed)"), res.evals, f(r["secs"], 2),
                               ", ".join(f"{v:.3f}" for v in res.x)])
                print(rows[-1], flush=True)
    rep("E1. Re-closing after one edit. Miss = changed end minus pin: position distance (cm), heading, pitch, bank "
        "(deg). Slack forces: lowest and highest normal force / largest lateral force (g) over the slack piece, in "
        "the pinned layout and after the re-close. 'contract evals' are the elements' re-solves before the slack's; "
        "'slack evals' the slack solve's (started from the pinned layout's slack values). Slack (a) cannot change "
        "height, pitch or bank: those misses stay unless the contract restores them.")
    rep.table(["layout", "slack", "edit", "policy", "miss before cm", "heading", "pitch", "bank", "slack solve",
               "miss after cm", "heading", "pitch", "bank", "slack forces pinned", "slack forces after",
               "contract evals", "slack evals", "solve s", "slack values"], rows)

    # how large a lift change each slack absorbs (L1), with continuation from the previous size
    rows = []
    for kind, policy in (("a", "contract + slack"), ("b", "slack only"), ("b", "contract + slack")):
        lay, path, x = layouts[("L1", kind)]
        for direction in (1.0, -1.0):
            prev = None
            for size in (1.0, 2.0, 4.0, 8.0, 16.0):
                r = reclose(path, lay, kind, "lift", direction * size, x, policy, slack_start=prev)
                res = r["res"]
                ok = res.converged and sane(r["peaks"]) and r["c_ok"]
                if res.converged:
                    prev = res.x
                rows.append([kind, policy, f"{direction * size:+.0f} m", res.status, res.evals,
                             fmt_miss(r["after"])[0], fmt_peaks(r["peaks"]),
                             "yes" if ok else ("forces out of range" if res.converged else "no")])
                print(rows[-1], flush=True)
                if not res.converged and not r["c_ok"]:
                    break
    rep(f"E2. How large a lift change the slack absorbs, L1 (the first drop keeps its valley on the ground). "
        f"Sane: normal force {SANE_NORMAL[0]} to {SANE_NORMAL[1]} g and lateral at most {SANE_LATERAL} g over the "
        "slack piece. Each size starts from the previous size's slack solution.")
    rep.table(["slack", "policy", "lift change", "slack solve", "slack evals", "miss after cm",
               "slack forces (min / max normal / lateral g)", "absorbed"], rows)
    rep.save()


if __name__ == "__main__":
    main()
