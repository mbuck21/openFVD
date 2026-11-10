#!/usr/bin/env python3
"""Compare two FVD++ exports or saved projects and say how they differ.

    py -3 tools/compare_exports.py samples/baseline/four-sections.2016.nl2elem my-export.nl2elem

Understands the three file kinds FVD++ writes:

  .nl2elem  NoLimits 2 element (XML text). Numbers are compared as numbers, so
            "1.0e+000" (the 2016 binary's C runtime) equals "1.0e+00" (a current one).
  .nlelem   NoLimits 1 element (binary). Decoded into its 50-byte Bezier
            segments; reports the largest difference in millimetres.
  .fvd      Saved project (binary). Compared byte for byte.

Exit code 0 when the files match within the tolerance, 1 when they do not.
See docs/testing.md.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

# ui/exportui.cpp, doExport(): "MELE", 4-byte length, 64 nulls, 4-byte segment count.
NLELEM_HEADER = 76
NLELEM_FOOTER = 69
NLELEM_RECORD = 50
FIELDS = ("Kp1.x", "Kp1.y", "Kp1.z", "Kp2.x", "Kp2.y", "Kp2.z", "P1.x", "P1.y", "P1.z", "roll")


def compare_nl2elem(a: Path, b: Path, tol: float) -> bool:
    number = re.compile(r">\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*<")
    text_a, text_b = a.read_text(encoding="utf-8"), b.read_text(encoding="utf-8")
    nums_a = [float(x) for x in number.findall(text_a)]
    nums_b = [float(x) for x in number.findall(text_b)]
    if len(nums_a) != len(nums_b):
        print(f"DIFFERENT: {len(nums_a)} numbers against {len(nums_b)}")
        return False
    if number.sub("><", text_a) != number.sub("><", text_b):
        print("DIFFERENT: the XML structure differs (not only the numbers)")
        return False
    worst = max((abs(x - y) for x, y in zip(nums_a, nums_b)), default=0.0)
    same_text = text_a == text_b
    print(f"{len(nums_a)} numbers, largest difference {worst * 1000:.4f} mm"
          + ("" if same_text else " (the text differs only in how numbers are printed)" if worst == 0 else ""))
    return worst <= tol


def read_nlelem(path: Path):
    data = path.read_bytes()
    body = len(data) - NLELEM_HEADER - NLELEM_FOOTER
    if data[:4] != b"ELEM":  # "MELE" written back to front by writeBytes()
        print(f"warning: {path.name} does not start with the element magic")
    if body < 0 or body % NLELEM_RECORD:
        raise SystemExit(f"{path}: not an FVD++ .nlelem (size {len(data)})")
    records = []
    for i in range(body // NLELEM_RECORD):
        start = NLELEM_HEADER + i * NLELEM_RECORD
        floats = struct.unpack(">10f", data[start:start + 40])
        flags = data[start + 40:start + NLELEM_RECORD]
        records.append((floats, flags))
    return records


def compare_nlelem(a: Path, b: Path, tol: float) -> bool:
    rec_a, rec_b = read_nlelem(a), read_nlelem(b)
    if len(rec_a) != len(rec_b):
        print(f"DIFFERENT: {len(rec_a)} segments against {len(rec_b)}")
        return False
    worst, differing, ok = 0.0, 0, True
    for i, ((fa, flags_a), (fb, flags_b)) in enumerate(zip(rec_a, rec_b)):
        if flags_a != flags_b:
            print(f"segment {i}: roll flags differ")
            ok = False
        diffs = [(name, x, y) for name, x, y in zip(FIELDS, fa, fb) if x != y]
        if diffs:
            differing += 1
            for name, x, y in diffs:
                worst = max(worst, abs(x - y))
                print(f"segment {i} {name}: {x:.6f} against {y:.6f} ({abs(x - y) * 1000:.3f} mm)")
    print(f"{len(rec_a)} segments, {differing} differ, largest difference {worst * 1000:.4f} mm"
          " (roll is in radians, shown with the same factor)")
    return ok and worst <= tol


def compare_bytes(a: Path, b: Path) -> bool:
    da, db = a.read_bytes(), b.read_bytes()
    if da == db:
        print(f"identical, {len(da)} bytes")
        return True
    first = next((i for i, (x, y) in enumerate(zip(da, db)) if x != y), min(len(da), len(db)))
    print(f"DIFFERENT: sizes {len(da)} and {len(db)}, first difference at byte {first}")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--tolerance-mm", type=float, default=0.0,
                        help="largest difference that still counts as a match (default 0: exact numbers)")
    args = parser.parse_args()
    tol = args.tolerance_mm / 1000.0
    suffix = args.reference.suffix.lower()
    if suffix != args.candidate.suffix.lower():
        raise SystemExit("the two files have different extensions")
    if suffix == ".nl2elem":
        ok = compare_nl2elem(args.reference, args.candidate, tol)
    elif suffix == ".nlelem":
        ok = compare_nlelem(args.reference, args.candidate, tol)
    else:
        ok = compare_bytes(args.reference, args.candidate)
    print("MATCH" if ok else "NO MATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
