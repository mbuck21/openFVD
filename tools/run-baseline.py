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
"""Run a build's batch export on every baseline project and compare with the references.

    py -3 tools/run-baseline.py [--exe <FVD.exe>] [--extra <dir>] [--record] [--keep <dir>]

For every *.fvd in samples/baseline/ (and in the --extra folder) this runs
    FVD.exe --batch-export <project> <temporary folder>
which writes the three exports of every track and a re-saved project, and then
compares each file with its reference using tools/compare_exports.py:

  samples/baseline/<stem>.2016.<kind>   the 2016 release binary's files (a missing
                                        one fails, except the re-saved project)
  <extra dir>/<stem>.ref.<kind>         references for projects kept out of the
                                        repository; --record writes the missing ones
                                        from this build (never overwrites)

Tolerances: exact, unless tolerances.json next to the references lists the file.
Prints MATCH or NO MATCH per file and ends with PASS or FAIL (exit code 0 or 1).
See docs/testing.md.
"""
import argparse
import contextlib
import hashlib
import io
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.dont_write_bytecode = True  # keep tools/__pycache__ out of the repository
sys.path.insert(0, str(TOOLS))
import compare_exports  # noqa: E402  (the decoding lives there)

BASELINE = ROOT / "samples" / "baseline"
DEFAULT_EXE = ROOT / "build" / "dist" / "FVD.exe"
TIME_LIMIT_S = 60
SHOWN_DIFFERENCES = 6


def load_tolerances(folder: Path) -> dict:
    path = folder / "tolerances.json"
    if not path.is_file():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(reference: Path, candidate: Path, tolerance: dict):
    """Returns (matched, lines printed by compare_exports)."""
    tol_m = float(tolerance.get("tolerance_mm", 0.0)) / 1000.0
    tol_rad = float(tolerance.get("tolerance_rad", 0.0))
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        try:
            suffix = reference.suffix.lower()
            if suffix == ".nl2elem":
                ok = compare_exports.compare_nl2elem(reference, candidate, tol_m)
            elif suffix == ".nlelem":
                ok = compare_exports.compare_nlelem(reference, candidate, tol_m, tol_rad)
            else:
                ok = compare_exports.compare_bytes(reference, candidate)
        except SystemExit as error:  # read_nlelem() gives up on a malformed file
            print(error)
            ok = False
    return ok, [line for line in out.getvalue().splitlines() if line.strip()]


def run_project(exe: Path, project: Path, ref_infix: str, record: bool,
                tolerances: dict, keep: Optional[Path]) -> bool:
    """Batch-exports one project and compares every file it wrote. True when all match."""
    stem = project.name[:-len(".fvd")]
    before = file_hash(project)
    with tempfile.TemporaryDirectory(prefix="fvd-baseline-") as tmp:
        work = Path(tmp)
        out = work / "out"
        try:
            proc = subprocess.run([str(exe), "--batch-export", str(project), str(out)],
                                  cwd=work, timeout=TIME_LIMIT_S,
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            print(f"FAILED    {project.name}: no result after {TIME_LIMIT_S} s, the program was stopped")
            return False

        if keep is not None:
            keep.mkdir(parents=True, exist_ok=True)
            if out.is_dir():
                for produced in out.iterdir():
                    shutil.copy2(produced, keep / produced.name)
            if (work / "fvd.log").is_file():
                shutil.copy2(work / "fvd.log", keep / f"{stem}.fvd.log")

        if file_hash(project) != before:
            print(f"FAILED    {project.name}: the batch export changed the project file")
            return False

        report = out / f"{stem}.batch.txt"
        lines = report.read_text(encoding="utf-8").splitlines() if report.is_file() else []
        if not lines or lines[-1] != "OK" or code != 0:
            last = lines[-1] if lines else "no result file"
            print(f"FAILED    {project.name}: exit code {code}, {last}")
            return False
        for line in lines[:-1]:
            if not line.startswith("wrote "):
                print(f"note      {project.name}: {line}")

        all_ok = True
        written = [line[len("wrote "):] for line in lines if line.startswith("wrote ")]
        for name in written:
            candidate = out / name
            ref_name = stem + ref_infix + name[len(stem):]
            reference = project.parent / ref_name
            label = f"{name} against {ref_name}"
            if not reference.is_file():
                if record:
                    shutil.copy2(candidate, reference)
                    print(f"RECORDED  {ref_name} (from this build)")
                elif ref_infix == ".2016" and name.endswith(".resaved.fvd"):
                    # optional: a re-saved project from the 2016 binary is compared only when present
                    print(f"no ref    {name} ({candidate.stat().st_size} bytes; there is no {ref_name})")
                else:
                    hint = " (--record keeps this build's file as the reference)" if ref_infix == ".ref" else ""
                    print(f"NO REFERENCE {label}{hint}")
                    all_ok = False
                continue
            tolerance = tolerances.get(ref_name, {})
            ok, details = compare(reference, candidate, tolerance)
            limits = []
            if tolerance.get("tolerance_mm"):
                limits.append(f"{tolerance['tolerance_mm']} mm")
            if tolerance.get("tolerance_rad"):
                limits.append(f"{tolerance['tolerance_rad']} rad")
            limit = f" (tolerance {', '.join(limits)})" if limits else " (exact)"
            summary = details[-1] if details else ""
            print(f"{'MATCH' if ok else 'NO MATCH':9} {label}{limit}: {summary}")
            if not ok:
                for detail in details[:-1][:SHOWN_DIFFERENCES]:
                    print(f"            {detail}")
                if len(details) - 1 > SHOWN_DIFFERENCES:
                    print(f"            ... {len(details) - 1 - SHOWN_DIFFERENCES} more lines")
            all_ok = all_ok and ok
        return all_ok


def projects_in(folder: Path):
    return sorted(p for p in folder.glob("*.fvd")
                  if not p.name.endswith(".resaved.fvd") and ".ref." not in p.name and ".2016." not in p.name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Batch-export every baseline project with a build and compare "
                                                 "the files with the references (docs/testing.md).")
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE,
                        help="the FVD.exe to test (default: build/dist/FVD.exe in this repository)")
    parser.add_argument("--extra", type=Path,
                        help="also test every *.fvd in this folder, against <stem>.ref.<kind> files there")
    parser.add_argument("--record", action="store_true",
                        help="with --extra: write the references that do not exist yet from this build")
    parser.add_argument("--keep", type=Path, help="copy the files the build wrote into this folder")
    args = parser.parse_args()

    exe = args.exe.resolve()
    if not exe.is_file():
        print(f"{exe} not found: build first (docs/building.md) or give --exe")
        print("FAIL")
        return 1
    if args.record and args.extra is None:
        parser.error("--record only applies to --extra projects; the 2016 references are never rewritten")
    keep = args.keep.resolve() if args.keep else None

    runs = [(p, ".2016", False, load_tolerances(BASELINE)) for p in projects_in(BASELINE)]
    if not runs:
        print(f"no projects found in {BASELINE}")
        print("FAIL")
        return 1
    if args.extra is not None:
        extra = args.extra.resolve()
        if not extra.is_dir():
            print(f"{extra} is not a folder")
            print("FAIL")
            return 1
        extra_projects = projects_in(extra)
        if not extra_projects:
            print(f"note      no projects found in {extra}")
        runs += [(p, ".ref", args.record, load_tolerances(extra)) for p in extra_projects]

    print(f"testing {exe}")
    passed = True
    for project, infix, record, tolerances in runs:
        print(f"--- {project}")
        passed = run_project(exe, project.resolve(), infix, record, tolerances, keep) and passed

    print("PASS" if passed else "FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
