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
"""Write the three import samples: a 30 m straight into a 90 degree curve of 40 m radius.

    py -3 tools/make-import-samples.py samples/import

See samples/import/README.md. The committed samples were made with this script;
the references next to them belong to those exact bytes.
"""
import math
import struct
import sys
from pathlib import Path

STRAIGHT = 30.0
RADIUS = 40.0
ARC = RADIUS * math.pi / 2
HEIGHT = 10.0
TOTAL = STRAIGHT + ARC


def at(s):
    """Position and unit direction at distance s along the path."""
    if s <= STRAIGHT:
        return (s, HEIGHT, 0.0), (1.0, 0.0, 0.0)
    t = (s - STRAIGHT) / RADIUS
    pos = (STRAIGHT + RADIUS * math.sin(t), HEIGHT, RADIUS - RADIUS * math.cos(t))
    return pos, (math.cos(t), 0.0, math.sin(t))


def stations(step):
    n = int(round(TOTAL / step))
    return [TOTAL * i / n for i in range(n + 1)]


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)

    # Point list for "Import from Pointlist" (core/nolimitsimporter.cpp,
    # importAsTxt): one point per line, "z x y", no newline after the last.
    lines = []
    for s in stations(2.0):
        (x, y, z), _ = at(s)
        lines.append(f"{z:.6f} {x:.6f} {y:.6f}")
    (out / "pointlist.txt").write_bytes("\n".join(lines).encode("ascii"))

    # NoLimits 2 CSV for "Import NoLimits 2 CSV" (core/secnlcsv.cpp, loadTrack):
    # a header line, then tab-separated No., position, front, left and up.
    rows = ['"No."\t"PosX"\t"PosY"\t"PosZ"\t"FrontX"\t"FrontY"\t"FrontZ"\t"LeftX"\t"LeftY"\t"LeftZ"\t"UpX"\t"UpY"\t"UpZ"']
    for i, s in enumerate(stations(1.0)):
        (x, y, z), (fx, fy, fz) = at(s)
        up = (0.0, 1.0, 0.0)
        left = (up[1] * fz - up[2] * fy, up[2] * fx - up[0] * fz, up[0] * fy - up[1] * fx)
        values = [x, y, z, fx, fy, fz, left[0], left[1], left[2], up[0], up[1], up[2]]
        rows.append(str(i + 1) + "\t" + "\t".join(f"{v:.6f}" for v in values))
    (out / "nl2.csv").write_bytes(("\n".join(rows) + "\n").encode("ascii"))

    # NoLimits 1 track for "Import NoLimits 1 Track" (core/nolimitsimporter.cpp,
    # importAsNlTrack), holding only the blocks the importer reads: BEZR (the
    # bezier vertices) and SEGM (one segment type per vertex, all "0", plain
    # track), big-endian as the importer reads them. Supports (FUND, FREN, TUBE)
    # are left out. Not a file NoLimits 1 itself would write.
    bez = b""
    points = stations(10.0)
    handle = (points[1] - points[0]) / 3.0
    for s in points:
        (x, y, z), (dx, dy, dz) = at(s)
        kp1 = (x - dx * handle, y - dy * handle, z - dz * handle)
        kp2 = (x + dx * handle, y + dy * handle, z + dz * handle)
        bez += struct.pack(">9f", x, y, z, *kp1, *kp2)
        bez += struct.pack(">f", 0.0)                    # roll
        bez += struct.pack(">???", True, True, False)    # contRoll, equalDist, relRoll
        bez += b"\0" * 17
    data = b"BEZR" + struct.pack(">i", len(bez) + 4) + b"\0" * 16 + struct.pack(">i", len(points)) + bez
    seg = b""
    for _ in points:
        seg += struct.pack(">i", 0) + b"\0" * 29 + b"\0" * 4
    data += b"SEGM" + b"\0" * 4 + struct.pack(">i", len(points)) + struct.pack(">?", False) + b"\0" * 16 + seg
    # the importer looks for these tags at every byte: none may appear by accident
    for tag in (b"SEGM", b"BEZR", b"FUND", b"FREN", b"TUBE"):
        assert data.count(tag) == (1 if tag in (b"SEGM", b"BEZR") else 0), tag
    (out / "nl1.nltrack").write_bytes(data)

    print(f"{len(lines)} points, {len(rows) - 1} CSV rows, {len(points)} bezier vertices")
    return 0


if __name__ == "__main__":
    sys.exit(main())
