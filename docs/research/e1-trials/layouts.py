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
"""Pieces of the test layouts: a first drop that is solved to end level at a given height,
a banked turn, and readers for them.

The lift. A chain lift in FVD is sections held at the lift speed, and the train leaves
its top at that speed whatever its height, so raising the lift alone moves the whole
ride up without changing it. The designer's "lift 1 m higher" keeps the valley of the
first drop on the ground, so the drop is re-solved with it, and that is an energy
change for everything after. The test layouts therefore start at the top of the lift
(the anchor, level, at the lift speed): "lift +1 m" is the anchor 1 m higher with the
first drop re-solved to its old valley height. That is the same track as a longer
held-speed lift straight, without its 20,000 nodes at 5 m/s (lift_sections() builds
the real lift, for the timing of a long ride).
"""
from e1solver import Problem, Unknown, vertical_angle, wrap180, nearest_branch
from fvdtext import force, straight, curve

LIFT_SPEED = 5.0


def lift_sections(length, speed=LIFT_SPEED):
    return [curve("lift up", 30, 20, 0, fixed_speed=speed),
            straight("lift", length, fixed_speed=speed),
            curve("lift top", 30, 20, 180, fixed_speed=speed)]


def drop_section(hold=1.2, valley_hold=0.6, valley=3.5, name="first drop"):
    """Pitch over at 0 g (hold), pull out at the valley force (valley_hold), back to 1 g."""
    return force(name, [("cubic", 0.6, -1.0), ("quartic", hold, 0.0), ("cubic", 0.8, valley),
                        ("quartic", valley_hold, 0.0), ("cubic", 0.8, 1.0 - valley)])


def turn_section(bank=60.0, n=2.0, hold=1.5, name="turn", orientation="quaternion"):
    """A banked turn: roll in to `bank` over 1.6 s while the force rises to n, hold, roll out."""
    r = bank / 0.8
    return force(name, [("cubic", 1.6, n - 1.0), ("quartic", hold, 0.0), ("cubic", 1.6, 1.0 - n)],
                 roll=[("cubic", 0.8, r), ("cubic", 0.8, -r), ("quartic", hold, 0.0),
                       ("cubic", 0.8, -r), ("cubic", 0.8, r)], orientation=orientation)


def raise_start(model, delta, text_y):
    """Moves the start of the track up by delta. The anchor.setPositionY command's height is
    not the project text's anchor position: setting it to the text's value raises the track
    by the heartline height (1.1 m; see the note), so the command gets text_y - 1.1 + delta
    and the move is checked on node 0."""
    h0 = model.measure(["height"], 0)["values"]["height"]
    ok = model.set("anchor.setPositionY", {}, y=round(text_y - 1.1 + delta, 4))
    model.recompute()
    moved = model.measure(["height"], 0)["values"]["height"] - h0
    if not ok or abs(moved - delta) > 1e-3:
        raise RuntimeError(f"start moved {moved:.4f} m, not {delta} m")
    return moved


def end_state(model, section, heading0=0.0, ref_pitch=0.0):
    e = model.end(section)
    if e["non_finite"]:
        return None
    e = dict(e)
    e["theta"] = vertical_angle(e, heading0, ref_pitch)
    return e


class Turn:
    """A turn_section() solved for a heading change and a level exit: unknowns the force
    held in the turn (n) and the hold time."""

    def __init__(self, model, section, bank=60.0, n=2.0):
        self.m, self.s, self.bank, self.n = model, section, bank, n

    def t(self, fn, k):
        return {"section": self.s, "function": fn, "transition": k}

    def set_n(self, v):
        self.n = v
        ok = self.m.set("transition.setChange", self.t("normal", 1), change=round(v - 1.0, 6))
        return ok and self.m.set("transition.setChange", self.t("normal", 3), change=round(1.0 - v, 6))

    def set_hold(self, v):
        ok = self.m.set("transition.setLength", self.t("normal", 2), length=round(v, 6))
        return ok and self.m.set("transition.setLength", self.t("roll", 3), length=round(v, 6))

    def read(self):
        a = end_state(self.m, self.s - 1)
        e = self.m.end(self.s)
        if a is None or e["non_finite"]:
            return None
        # unwrapped against the goal: a turn that misses by more than 180 degrees is not told apart
        return {"turned": nearest_branch(e["heading_deg"] - a["heading_deg"], self.ref),
                "exit_pitch": e["pitch_deg"], "exit_abs": e["y"]}

    ref = 0.0

    def problem(self, turned):
        self.ref = turned
        u = [Unknown("n", self.set_n, 0.5, 5.0, 0.01, 0.5), Unknown("hold", self.set_hold, 0.05, 10.0, 0.01, 1.0)]
        return Problem(self.m, u, [("turned", turned, 0.05), ("exit_pitch", 0.0, 0.05)], self.read)


def connector_section(T=3.0, name="connector"):
    """A force section of two halves; each half carries a quartic bump of the normal force
    (1 g -> 1 + a -> 1 g) and of the lateral force (0 -> l -> 0). With all bumps 0 it is a
    level 1 g run, which on level track is straight. The slack piece of kind (b)."""
    return force(name, [("quartic", T / 2, 0.0), ("quartic", T / 2, 0.0)],
                 lateral=[("quartic", T / 2, 0.0), ("quartic", T / 2, 0.0)])


class Pin:
    """The pose at the end of a section, as a dict of goals."""

    KEYS = ("x", "y", "z", "heading", "pitch", "bank")

    @staticmethod
    def read(model, section, ref_heading):
        e = model.end(section)
        if e["non_finite"]:
            return None
        return {"x": e["x"], "y": e["y"], "z": e["z"], "heading": nearest_branch(e["heading_deg"], ref_heading),
                "pitch": e["pitch_deg"], "bank": wrap180(e["bank_deg"]), "speed": e["speed_mps"], "end": e}


class SlackStraightCurve:
    """Slack of kind (a): a straight's length plus a curve's angle and radius; goals x, z, heading."""

    def __init__(self, model, straight_section, curve_section):
        self.m, self.s, self.c = model, straight_section, curve_section

    def unknowns(self):
        return [Unknown("straight length", lambda v: self.m.set("straight.setLength", {"section": self.s}, length=round(v, 5)),
                        0.03, 500.0, 0.05, 20.0),
                Unknown("curve angle", lambda v: self.m.set("curve.setAngle", {"section": self.c}, angle=round(v, 5)),
                        1.0, 180.0, 0.05, 10.0),
                Unknown("curve radius", lambda v: self.m.set("curve.setRadius", {"section": self.c}, radius=round(v, 5)),
                        5.0, 500.0, 0.1, 20.0)]

    goal_keys = ("x", "z", "heading")


class SlackConnector:
    """Slack of kind (b): one connector force section; unknowns its duration and the four
    bumps; goals x, y, z, heading, pitch."""

    def __init__(self, model, section):
        self.m, self.s = model, section

    def t(self, fn, k):
        return {"section": self.s, "function": fn, "transition": k}

    def set_T(self, v):
        ok = True
        for fn in ("normal", "lateral"):
            for k in (1, 2):
                ok &= self.m.set("transition.setLength", self.t(fn, k), length=round(v / 2, 6))
        return ok

    def unknowns(self):
        return [Unknown("duration", self.set_T, 0.5, 20.0, 0.01, 1.0),
                Unknown("normal bump 1", lambda v: self.m.set("transition.setChange", self.t("normal", 1), change=round(v, 6)),
                        -3.0, 4.0, 0.005, 0.5),
                Unknown("normal bump 2", lambda v: self.m.set("transition.setChange", self.t("normal", 2), change=round(v, 6)),
                        -3.0, 4.0, 0.005, 0.5),
                Unknown("lateral bump 1", lambda v: self.m.set("transition.setChange", self.t("lateral", 1), change=round(v, 6)),
                        -3.0, 3.0, 0.005, 0.5),
                Unknown("lateral bump 2", lambda v: self.m.set("transition.setChange", self.t("lateral", 2), change=round(v, 6)),
                        -3.0, 3.0, 0.005, 0.5)]

    goal_keys = ("x", "y", "z", "heading", "pitch")


class Drop:
    """The first drop (one force section) solved for a level exit at an absolute height."""

    def __init__(self, model, section):
        self.m, self.s = model, section

    def t(self, k):
        return {"section": self.s, "function": "normal", "transition": k}

    def read(self):
        a = end_state(self.m, self.s - 1) if self.s > 1 else {"heading_deg": 0.0, "pitch_deg": 0.0}
        e = self.m.end(self.s)
        if a is None or e["non_finite"]:
            return None
        e = dict(e)
        e["theta"] = vertical_angle(e, a["heading_deg"], a["pitch_deg"])
        if abs(e["theta"]) > 80:
            return None
        return {"exit_pitch": e["theta"], "exit_abs": e["y"], "exit_speed": e["speed_mps"]}

    def problem(self, height):
        u = [Unknown("hold", lambda v: self.m.set("transition.setLength", self.t(2), length=round(v, 6)),
                     0.05, 6.0, 0.01, 0.5),
             Unknown("valley_hold", lambda v: self.m.set("transition.setLength", self.t(4), length=round(v, 6)),
                     0.01, 6.0, 0.01, 0.5)]
        return Problem(self.m, u, [("exit_pitch", 0.0, 0.05), ("exit_abs", height, 0.05)], self.read)
