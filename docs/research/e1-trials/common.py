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
"""Shared helpers of the trial scripts: folders, a result writer, the program's own timers."""
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
PROJECTS = HERE / "projects"
RESULTS = HERE / "results"
WORK = HERE / "work"          # throw-away files of a run; ignored by git (see .gitignore here)


class Report:
    """Prints lines and keeps them for results/<name>.txt."""

    def __init__(self, name):
        self.name = name
        self.lines = []
        self.t0 = time.perf_counter()

    def __call__(self, *parts):
        line = " ".join(str(p) for p in parts)
        print(line, flush=True)
        self.lines.append(line)

    def table(self, header, rows):
        self("| " + " | ".join(header) + " |")
        self("|" + "---|" * len(header))
        for r in rows:
            self("| " + " | ".join(str(c) for c in r) + " |")
        self("")

    def save(self):
        RESULTS.mkdir(exist_ok=True)
        self(f"(run time {time.perf_counter() - self.t0:.0f} s on the machine that ran it; "
             f"python {sys.version.split()[0]})")
        (RESULTS / f"{self.name}.txt").write_text("\n".join(self.lines) + "\n", encoding="utf-8")


def program_ms(model):
    """Milliseconds the program itself spent in run and recompute so far."""
    s = model.session.stats()
    return s["run_ms"] + s["recompute_ms"], s


def f(v, nd=2):
    if v is None:
        return ""
    return f"{v:.{nd}f}"
