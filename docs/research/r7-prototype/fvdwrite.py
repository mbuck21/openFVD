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
"""A small writer for FVD++ project files (.fvd, version v0.77), for R7.

It writes one track made only of force sections (time argument, quaternion
orientation, physics speed), each normal-force and lateral-force transition a
cubic piece, with no roll smoothing. The layout follows track::saveTrack(),
secforced::saveSection(), func::saveFunction() and subfunc::saveSubFunc() in
core/: every block is written back to front, so numbers are big-endian and a
vec3 appears as z, y, x. The three colours are copied from
samples/baseline/four-sections.fvd (raw QColor bytes).

Used by make_shapes.py; not a general tool.
"""
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]

LINEAR, CUBIC, QUARTIC = 0, 2, 3


def _i(v):
    return struct.pack('>i', int(v))


def _f(v):
    return struct.pack('>f', float(v))


def _b(v):
    return b'\x01' if v else b'\x00'


def _s(text):
    raw = text.encode('ascii')
    return _i(len(raw)) + raw


def _colours():
    """The 48 colour bytes of the reference project's track."""
    d = (REPO / 'samples' / 'baseline' / 'four-sections.fvd').read_bytes()
    i = d.find(b'TRC') + 3
    n = struct.unpack('>i', d[i:i + 4])[0]
    i += 4 + n
    return d[i:i + 48]


def _subfunc(degree, t0, t1, start, change, locked=False):
    return (_i(degree) + _f(t0) + _f(t1) + _f(start) + _f(0.0) + _f(change)
            + _f(0.0) + _f(0.0) + _b(locked))


def _func(pieces):
    """pieces: list of (degree, t0, t1, start, change)."""
    out = b'FUNC' + _i(len(pieces))
    for p in pieces:
        out += _subfunc(*p)
    return out


def _chain(start, steps, degree=CUBIC):
    """steps: list of (duration_s, change). Returns subfunc tuples in time."""
    t = 0.0
    v = start
    out = []
    for dur, change in steps:
        out.append((degree, t, t + dur, v, change))
        t += dur
        v += change
    return out, t


def forced_section(name, normal_steps, roll_steps=None, start_normal=1.0,
                   fixed_speed=None):
    """A force section. normal_steps / roll_steps: [(duration, change), ...].

    roll_steps are changes of the roll speed in degrees per second (cubic
    pieces); the list, if given, must have the same total duration. The
    lateral force is held at 0. fixed_speed: None for physics speed, or a
    speed in m/s the section holds (as a lift does)."""
    nf, total = _chain(start_normal, normal_steps)
    lf = [(CUBIC, 0.0, total, 0.0, 0.0)]
    if roll_steps:
        roll, rt = _chain(0.0, roll_steps)
        assert abs(rt - total) < 1e-6, (name, rt, total)
    else:
        roll = [(QUARTIC, 0.0, total, 0.0, 0.0)]
    physics = fixed_speed is None
    out = b'FRC' + _b(physics) + _s(name)
    out += _f(10.0 if physics else fixed_speed) + _i(int(total * 1000 + 0.5))
    out += _b(False) + _b(False)  # QUATERNION, TIME
    out += _func(roll) + _func(nf) + _func(lf)
    return out


def write_project(path, sections, start_y=0.0, speed=10.0, heart=1.1,
                  friction=0.03, resistance=2e-5, name='r7 track'):
    out = b'FVD' + b'v0.77' + _s(':/background.png')
    out += b'TRC' + _s(name) + _colours()
    out += _f(0.0) + _f(start_y) + _f(0.0)  # startPos written z, y, x
    out += _f(0.0) + _f(0.0) + _f(0.0)      # roll, pitch, yaw
    out += _f(speed) + _f(1.0) + _f(0.0)    # speed, normal, lateral
    out += _f(heart) + _f(friction) + _f(resistance)
    out += _b(True) + _i(0) + _i(0) + _b(False)
    out += _f(0.0) + _f(0.0)
    out += _i(len(sections))
    for s in sections:
        out += s
    out += _i(0) + b'EOT' + b'EOP'
    Path(path).write_bytes(out)
