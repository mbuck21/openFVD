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
"""Build small FVD++ projects for the E1 trials, as project text.

A layout is a list of sections made with force(), straight(), curve() and
geometric(); write_project() turns it into the project text of
docs/project-text-format.md and lets `fvdcore fromtext` write the .fvd, so the
program itself does the binary writing. A function is given as a list of
steps (shape, duration, change); durations are seconds (time argument),
metres (a straight's roll function) or degrees (a curve's roll function).
"""
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
FVDCORE = REPO / "build" / "dist" / "fvdcore.exe"

_SHAPE_PARAMETER = {"quartic": -10.0, "quintic": 0.0, "quadratic": 1.0, "plateau": 1.0}


def _function(steps):
    out, t = [], 0.0
    for shape, duration, change in steps:
        out.append({"shape": shape, "from": t, "to": t + duration, "change": change,
                    "center": 0, "tension": 0, "locked": False,
                    "shapeParameter": _SHAPE_PARAMETER.get(shape, 0.0)})
        t += duration
    return out, t


def _flat(total):
    return [("quartic", total, 0.0)]


def _locked(function):
    """Locks the last transition, so it stretches to the end of the section: a section's
    length is that of its shortest unlocked function, so a flat roll or lateral function
    must be locked for a change of the force function's lengths to change the section."""
    function[-1]["locked"] = True
    return function


def _smooth():
    return {"active": False, "iterations": 1, "length": 400}


def force(name, normal, roll=None, lateral=None, orientation="quaternion", fixed_speed=None):
    """A force section on time. normal, roll, lateral: [(shape, seconds, change), ...];
    roll is a roll rate in deg/s, the forces in g. Missing functions are flat."""
    nf, total = _function(normal)
    rf, rt = _function(roll) if roll else (_locked(_function(_flat(total))[0]), total)
    lf, lt = _function(lateral) if lateral else (_locked(_function(_flat(total))[0]), total)
    if roll and lateral:
        raise ValueError("at most two functions may be given besides a locked one")
    return {"type": "force", "name": name, "argument": "time", "orientation": orientation,
            "speedMode": "fixed" if fixed_speed else "physics",
            "speed": float(fixed_speed or 10.0),
            "normalForce": nf, "lateralForce": lf, "roll": rf, "smoothing": _smooth()}


def geometric(name, pitch, yaw=None, roll=None, orientation="quaternion", fixed_speed=None):
    """A geometric section on time: pitch and yaw rates in deg/s."""
    pf, total = _function(pitch)
    yf = _function(yaw)[0] if yaw else _locked(_function(_flat(total))[0])
    rf = _function(roll)[0] if roll else _locked(_function(_flat(total))[0])
    return {"type": "geometric", "name": name, "argument": "time", "orientation": orientation,
            "speedMode": "fixed" if fixed_speed else "physics",
            "speed": float(fixed_speed or 10.0),
            "pitchRate": pf, "yawRate": yf, "roll": rf, "smoothing": _smooth()}


def straight(name, length, fixed_speed=None):
    rf, _ = _function(_flat(length))
    return {"type": "straight", "name": name, "speedMode": "fixed" if fixed_speed else "physics",
            "speed": float(fixed_speed or 10.0), "roll": rf, "smoothing": _smooth()}


def curve(name, angle, radius, direction, lead_in=0.0, lead_out=0.0, fixed_speed=None,
          orientation="quaternion"):
    """direction: 90 turns right (heading +90), -90 left; see the README for up and down."""
    rf, _ = _function(_flat(angle))
    return {"type": "curved", "name": name, "radius": float(radius), "direction": float(direction),
            "leadIn": float(lead_in), "leadOut": float(lead_out), "orientation": orientation,
            "speedMode": "fixed" if fixed_speed else "physics",
            "speed": float(fixed_speed or 10.0), "roll": rf, "smoothing": _smooth()}


def project_text(sections, speed=20.0, y=0.0, pitch=0.0, roll=0.0, yaw=0.0, heart=1.1,
                 friction=0.03, resistance=2e-5, name="e1 track"):
    track = {
        "name": name,
        "colors": {"activeSection": {"alpha": 255, "color": "#ff3333"},
                   "default": {"alpha": 255, "color": "#141482"},
                   "transition": {"alpha": 255, "color": "#33ff33"}},
        "anchor": {"position": [0, y, 0], "roll": roll, "pitch": pitch, "yaw": yaw,
                   "speed": speed, "normalForce": 1, "lateralForce": 0},
        "heartline": heart, "friction": friction, "resistance": resistance,
        "drawTrack": True, "drawHeartline": 0, "style": "generic", "wireframe": False,
        "povPosition": [0, 0], "sections": sections,
        "smoothing": {"custom": [], "wholeTrack": _smooth()},
    }
    return {"format": "fvd-project-text", "formatVersion": 1, "binaryVersion": "v0.77",
            "groundTexture": ":/background.png", "tracks": [track]}


def write_project(path, sections, **anchor):
    """Writes <path> (.fvd) through `fvdcore fromtext`; the text is kept beside it as .json."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text_path = path.with_suffix(".json")
    text_path.write_text(json.dumps(project_text(sections, **anchor), indent=1) + "\n", encoding="utf-8")
    result = subprocess.run([str(FVDCORE), "fromtext", str(text_path), str(path), "--force"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"fromtext refused {text_path}: {result.stderr.strip()}")
    return path
