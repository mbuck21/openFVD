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
                                [--undo-check] [--fixtures] [--imports]
    py -3 tools/run-baseline.py --core [--core-exe <fvdcore.exe>] [--extra <dir>] [--keep <dir>]

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

--undo-check  then runs FVD.exe --batch-undo-check on every project in
              samples/baseline/: scripted edits through the windows, undo all,
              redo all; each must report OK (the files after undoing equal the
              files before the edits, byte for byte, and the same after redoing).
--fixtures    then runs FVD.exe --batch-make-fixtures and compares each generated
              project with the committed samples/baseline/gen-<name>.fvd, byte
              for byte. Until the generated projects are committed it says so and
              does not fail.
--imports     then runs FVD.exe --batch-import on every input in samples/import/
              and compares the files and the report it writes with the
              <stem>.ref.<kind> files there, byte for byte (references recorded
              with a rebuilt program, not the 2016 binary; see the README there).

--core        runs every comparison above with the console program fvdcore
              (cli/, built from core/ alone; build/dist/fvdcore.exe, else the one
              tools/run-tests.sh builds into build/tests/coreonly/) in place of
              FVD.exe --batch-export:
                  fvdcore export <project> <temporary folder>
              It writes the same files, so the same references apply. --core-exe
              names another fvdcore. It does not combine with --undo-check,
              --fixtures or --imports, which need the windows.

Prints one line per comparison and ends with PASS or FAIL (exit code 0 or 1).
See docs/testing.md.
"""
import argparse
import contextlib
import hashlib
import io
import json
import os
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
IMPORTS = ROOT / "samples" / "import"
IMPORT_KINDS = {".nltrack": "nltrack", ".txt": "txt", ".csv": "csv"}
# Inputs whose result changes from one run to the next: only checked to import without error.
UNSTABLE_IMPORTS = {
    "pointlist.txt": "the point-list importer leaves some values of the first and last point unset, "
                     "so the exports change from run to run",
}
DEFAULT_EXE = ROOT / "build" / "dist" / "FVD.exe"
CORE_DEPLOYED = ROOT / "build" / "dist" / "fvdcore.exe"  # tools/build-windows.sh puts fvdcore beside FVD.exe
CORE_DIR = ROOT / "build" / "tests" / "coreonly"  # where tools/run-tests.sh builds it (the older place)
TIME_LIMIT_S = 60
# The undo check computes the whole track for every undo and redo (about 115
# of them); on gen-geometric-distance, whose scripted edits leave long sections
# without a valid speed, one run takes about 50 s.
UNDO_TIME_LIMIT_S = 180
FIXTURE_PREFIX = "gen-"
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


def default_core_exe() -> Path:
    """build/dist/fvdcore.exe when it exists, else fvdcore as tools/run-tests.sh
    builds it (in a release folder on Windows)."""
    if CORE_DEPLOYED.is_file():
        return CORE_DEPLOYED
    for name in ("release/fvdcore.exe", "fvdcore.exe", "release/fvdcore", "fvdcore"):
        if (CORE_DIR / name).is_file():
            return CORE_DIR / name
    return CORE_DIR / "release" / "fvdcore.exe"


def core_environment() -> dict:
    """The copy in build/dist/ has its Qt libraries beside it; the one tools/run-tests.sh
    builds does not and finds them on the PATH. Adds the MSYS2 UCRT64 folder, where
    docs/building.md installs Qt, when a PATH without it was inherited."""
    env = dict(os.environ)
    extra = [str(Path(found).parent) for found in [shutil.which("qmake-qt5")] if found]
    extra.append("C:/msys64/ucrt64/bin")
    env["PATH"] = os.pathsep.join([folder for folder in extra if Path(folder).is_dir()] + [env.get("PATH", "")])
    return env


def run_project(exe: Path, project: Path, ref_infix: str, record: bool,
                tolerances: dict, keep: Optional[Path], core: bool = False) -> bool:
    """Batch-exports one project and compares every file it wrote. True when all match."""
    stem = project.name[:-len(".fvd")]
    before = file_hash(project)
    with tempfile.TemporaryDirectory(prefix="fvd-baseline-") as tmp:
        work = Path(tmp)
        out = work / "out"
        try:
            command = [str(exe), "export", str(project), str(out)] if core \
                else [str(exe), "--batch-export", str(project), str(out)]
            proc = subprocess.run(command,
                                  cwd=work, env=core_environment() if core else None, timeout=TIME_LIMIT_S,
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


def run_batch(exe: Path, args: list, work: Path, limit: int = TIME_LIMIT_S):
    """Runs FVD.exe with batch arguments in work. Returns the exit code, or None after the time limit."""
    try:
        proc = subprocess.run([str(exe)] + args, cwd=work, timeout=limit,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return proc.returncode
    except subprocess.TimeoutExpired:
        return None


def report_lines(path: Path) -> list:
    return path.read_text(encoding="utf-8").splitlines() if path.is_file() else []


def undo_check(exe: Path, project: Path, keep: Optional[Path]) -> bool:
    """Runs the undo and redo round trip on one project. True when it reports OK."""
    stem = project.name[:-len(".fvd")]
    before = file_hash(project)
    with tempfile.TemporaryDirectory(prefix="fvd-undo-") as tmp:
        work = Path(tmp)
        out = work / "out"
        code = run_batch(exe, ["--batch-undo-check", str(project), str(out)], work, UNDO_TIME_LIMIT_S)
        report = out / f"{stem}.undo.batch.txt"
        if keep is not None and report.is_file():
            keep.mkdir(parents=True, exist_ok=True)
            shutil.copy2(report, keep / report.name)
        if code is None:
            print(f"UNDO FAIL {project.name}: no result after {UNDO_TIME_LIMIT_S} s, the program was stopped")
            return False
        if file_hash(project) != before:
            print(f"UNDO FAIL {project.name}: the undo check changed the project file")
            return False
        lines = report_lines(report)
        last = lines[-1] if lines else "no result file"
        summary = next((line for line in lines if line.startswith("undo action types exercised")), "")
        summary = summary.split(":")[0].replace("undo action types exercised", "action types exercised")
        if code != 0 or last != "OK":
            print(f"UNDO FAIL {project.name}: exit code {code}, {last}")
            for line in lines:
                if line.startswith("A2 ") or line.startswith("B2 ") or "FAILED" in line or "not available" in line:
                    print(f"            {line}")
            return False
        a2 = next((line for line in lines if line.startswith("A2 ")), "")
        b2 = next((line for line in lines if line.startswith("B2 ")), "")
        print(f"UNDO OK   {project.name}: {a2}; {b2}; {summary}")
        return True


def fixtures_check(exe: Path, keep: Optional[Path]) -> bool:
    """Regenerates the scripted test projects and compares them with the committed ones."""
    committed = sorted(BASELINE.glob(f"{FIXTURE_PREFIX}*.fvd"))
    committed = [p for p in committed if p in projects_in(BASELINE)]
    with tempfile.TemporaryDirectory(prefix="fvd-fixtures-") as tmp:
        work = Path(tmp)
        out = work / "out"
        code = run_batch(exe, ["--batch-make-fixtures", str(out)], work)
        report = out / "make-fixtures.batch.txt"
        if keep is not None and out.is_dir():
            keep.mkdir(parents=True, exist_ok=True)
            for produced in out.iterdir():
                shutil.copy2(produced, keep / produced.name)
        if code is None:
            print(f"FIXTURES FAIL: no result after {TIME_LIMIT_S} s, the program was stopped")
            return False
        lines = report_lines(report)
        last = lines[-1] if lines else "no result file"
        if code != 0 or last != "OK":
            print(f"FIXTURES FAIL: exit code {code}, {last}")
            return False
        generated = sorted(out.glob(f"{FIXTURE_PREFIX}*.fvd"))
        print(f"generated {len(generated)} projects: {', '.join(p.name for p in generated)}")
        if not committed:
            print(f"note      no {FIXTURE_PREFIX}*.fvd is committed in {BASELINE} yet: nothing to compare "
                  f"(the lead commits them with their 2016 exports)")
            return True
        all_ok = True
        names = {p.name for p in generated}
        for reference in committed:
            if reference.name not in names:
                print(f"NO MATCH  {reference.name}: committed, but this build no longer generates it")
                all_ok = False
        for candidate in generated:
            reference = BASELINE / candidate.name
            if not reference.is_file():
                print(f"NO REFERENCE {candidate.name}: generated, but not committed in {BASELINE}")
                all_ok = False
                continue
            ok, details = compare(reference, candidate, {})
            summary = details[-1] if details else ""
            print(f"{'MATCH' if ok else 'NO MATCH':9} {candidate.name} against the committed file (exact): {summary}")
            all_ok = all_ok and ok
        return all_ok


def import_check(exe: Path, source: Path, keep: Optional[Path]) -> bool:
    """Imports one input into the default project and compares what was written. True when all match."""
    kind = IMPORT_KINDS[source.suffix.lower()]
    stem = source.stem
    with tempfile.TemporaryDirectory(prefix="fvd-import-") as tmp:
        work = Path(tmp)
        out = work / "out"
        code = run_batch(exe, ["--batch-import", kind, str(source), str(out)], work)
        if keep is not None and out.is_dir():
            keep.mkdir(parents=True, exist_ok=True)
            for produced in out.iterdir():
                shutil.copy2(produced, keep / produced.name)
        if code is None:
            print(f"IMPORT FAIL {source.name}: no result after {TIME_LIMIT_S} s, the program was stopped")
            return False
        report = out / f"{stem}.batch.txt"
        lines = report_lines(report)
        last = lines[-1] if lines else "no result file"
        if code != 0 or last != "OK":
            print(f"IMPORT FAIL {source.name}: exit code {code}, {last}")
            return False
        if source.name in UNSTABLE_IMPORTS:
            print(f"IMPORT OK {source.name}: imported; not compared ({UNSTABLE_IMPORTS[source.name]})")
            return True
        names = [line[len("wrote "):] for line in lines if line.startswith("wrote ")] + [report.name]
        all_ok = True
        for name in names:
            ref_name = stem + ".ref" + name[len(stem):]
            reference = IMPORTS / ref_name
            label = f"{name} against {ref_name}"
            if not reference.is_file():
                print(f"NO REFERENCE {label}")
                all_ok = False
                continue
            ok, details = compare(reference, out / name, {})
            summary = details[-1] if details else ""
            print(f"{'MATCH' if ok else 'NO MATCH':9} {label} (exact): {summary}")
            if not ok:
                for detail in details[:-1][:SHOWN_DIFFERENCES]:
                    print(f"            {detail}")
            all_ok = all_ok and ok
        return all_ok


def import_inputs():
    if not IMPORTS.is_dir():
        return []
    return sorted(p for p in IMPORTS.iterdir()
                  if p.suffix.lower() in IMPORT_KINDS and ".ref." not in p.name)


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
    parser.add_argument("--undo-check", action="store_true",
                        help="also run the undo and redo round trip on every project in samples/baseline/")
    parser.add_argument("--fixtures", action="store_true",
                        help="also regenerate the scripted test projects and compare them with the committed ones")
    parser.add_argument("--imports", action="store_true",
                        help="also import every input in samples/import/ and compare with the references there")
    parser.add_argument("--core", action="store_true",
                        help="export with fvdcore (cli/, core/ alone; build/dist/fvdcore.exe) instead of FVD.exe --batch-export")
    parser.add_argument("--core-exe", type=Path,
                        help="the fvdcore to test (default: where tools/run-tests.sh builds it)")
    args = parser.parse_args()

    if args.core and (args.undo_check or args.fixtures or args.imports):
        print("--core does not combine with --undo-check, --fixtures or --imports: "
              "those drive the windows, which fvdcore does not have")
        print("FAIL")
        return 1
    if args.core_exe is not None and not args.core:
        parser.error("--core-exe only applies together with --core")
    if args.core:
        exe = (args.core_exe if args.core_exe is not None else default_core_exe()).resolve()
    else:
        exe = args.exe.resolve()
    if not exe.is_file():
        if args.core:
            print(f"{exe} not found: build it with tools\\run-tests.cmd or give --core-exe")
        else:
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
        passed = run_project(exe, project.resolve(), infix, record, tolerances, keep, args.core) and passed

    if args.undo_check:
        print("--- undo and redo round trip (--batch-undo-check)")
        for project in projects_in(BASELINE):
            passed = undo_check(exe, project.resolve(), keep) and passed

    if args.fixtures:
        print("--- scripted test projects (--batch-make-fixtures)")
        passed = fixtures_check(exe, keep) and passed

    if args.imports:
        print("--- imports (--batch-import)")
        inputs = import_inputs()
        if not inputs:
            print(f"IMPORT FAIL no inputs found in {IMPORTS}")
            passed = False
        for source in inputs:
            passed = import_check(exe, source.resolve(), keep) and passed

    print("PASS" if passed else "FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
