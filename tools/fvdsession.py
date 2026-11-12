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
"""Drive `fvdcore session` from Python (standard library only).

    from fvdsession import Session
    with Session("samples/baseline/four-sections.fvd") as s:
        s.run("transition.setChange", {"section": 2, "function": "roll", "transition": 1}, change=45.0)
        print(s.section_end(2)["heading_deg"])
        s.undo()

Each method sends one request line and reads one reply line. A reply with
"ok": false raises SessionError with the program's words; the methods whose
names end in _raw return the reply instead. Run as a script it does a short
demonstration on samples/baseline/four-sections.fvd and ends with a line OK.
The operations are in cli/README.md.
"""
import json
import subprocess
import sys
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "build" / "dist" / "fvdcore.exe"


class SessionError(Exception):
    """The session answered {"ok": false, "error": ...}; the text is the error."""

    def __init__(self, message: str, reply: Optional[dict] = None):
        super().__init__(message)
        self.reply = reply


class Session:
    def __init__(self, project, track: int = 1, exe=None, log=None):
        """Starts `fvdcore session <project> --track <track>`. exe defaults to build/dist/fvdcore.exe
        beside this script's repository; log (a path) keeps the program's debug output."""
        exe = Path(exe) if exe is not None else DEFAULT_EXE
        if not exe.is_file():
            raise FileNotFoundError(f"{exe} not found: build first (docs/building.md) or give exe=")
        command = [str(exe), "session", str(project), "--track", str(track)]
        if log is not None:
            command += ["--log", str(log)]
        self._proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, cwd=str(exe.parent))

    # ---- the wire -------------------------------------------------------------------------
    def request_raw(self, **request) -> dict:
        """Sends one request and returns the reply, whether or not it says ok."""
        if self._proc is None or self._proc.poll() is not None:
            raise SessionError("the session has ended" + self._stderr())
        line = json.dumps(request, separators=(",", ":")) + "\n"
        try:
            self._proc.stdin.write(line.encode("utf-8"))
            self._proc.stdin.flush()
            reply = self._proc.stdout.readline()
        except (BrokenPipeError, OSError):
            reply = b""
        if not reply:
            raise SessionError("the session ended without an answer" + self._stderr())
        return json.loads(reply.decode("utf-8"))

    def request(self, **request) -> dict:
        reply = self.request_raw(**request)
        if not reply.get("ok"):
            raise SessionError(reply.get("error", "failed"), reply)
        return reply

    def _stderr(self) -> str:
        try:
            self._proc.wait(timeout=2)
            text = self._proc.stderr.read().decode("utf-8", "replace").strip()
        except Exception:
            text = ""
        return f": {text}" if text else ""

    # ---- the operations ---------------------------------------------------------------------
    def describe(self) -> list:
        return self.request(op="describe")["commands"]

    def run(self, name: str, target: Optional[dict] = None, track: Optional[int] = None, **args) -> dict:
        return self._run(name, target, track, args, True)

    def run_raw(self, name: str, target: Optional[dict] = None, track: Optional[int] = None, **args) -> dict:
        return self._run(name, target, track, args, False)

    def _run(self, name, target, track, args, check):
        request = {"op": "run", "name": name, "args": args}
        if target is not None:
            request["target"] = target
        if track is not None:
            request["track"] = track
        return self.request(**request) if check else self.request_raw(**request)

    def undo(self, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "undo"}, track))

    def redo(self, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "redo"}, track))

    def history(self, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "history"}, track))

    def begin(self, label: str = "Session step", track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "begin", "label": label}, track))

    def end(self, commit: bool = True, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "end", "commit": commit}, track))

    def recompute(self, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "recompute"}, track))

    def section_end(self, section: int, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "sectionEnd", "section": section}, track))

    def section_end_raw(self, section: int, track: Optional[int] = None) -> dict:
        return self.request_raw(**self._with_track({"op": "sectionEnd", "section": section}, track))

    def summary(self, sections: bool = False, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "summary", "sections": sections}, track))["summary"]

    def measures(self, ids, node: Optional[int] = None, at_m: Optional[float] = None,
                 at_s: Optional[float] = None, track: Optional[int] = None) -> dict:
        request = {"op": "measures", "ids": list(ids)}
        for key, value in (("node", node), ("at_m", at_m), ("at_s", at_s)):
            if value is not None:
                request[key] = value
        return self.request(**self._with_track(request, track))

    def check(self, track: Optional[int] = None) -> dict:
        return self.request(**self._with_track({"op": "check"}, track))

    def save(self, path, force: bool = False) -> dict:
        return self.request(op="save", path=str(path), force=force)

    def save_raw(self, path, force: bool = False) -> dict:
        return self.request_raw(op="save", path=str(path), force=force)

    def totext(self, path, force: bool = False) -> dict:
        return self.request(op="totext", path=str(path), force=force)

    def export(self, directory, force: bool = False) -> dict:
        return self.request(op="export", dir=str(directory), force=force)

    def stats(self) -> dict:
        return self.request(op="stats")

    def close(self) -> None:
        """Ends the session (op quit) and waits for the program."""
        if self._proc is None:
            return
        try:
            if self._proc.poll() is None:
                self._proc.stdin.write(b'{"op":"quit"}\n')
                self._proc.stdin.flush()
                self._proc.stdout.readline()
        except (BrokenPipeError, OSError):
            pass
        try:
            self._proc.stdin.close()
        except OSError:
            pass
        try:
            self._proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self._proc.kill()
        for pipe in (self._proc.stdout, self._proc.stderr):
            try:
                pipe.close()
            except OSError:
                pass
        self._proc = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    @staticmethod
    def _with_track(request: dict, track: Optional[int]) -> dict:
        if track is not None:
            request["track"] = track
        return request


def demo() -> int:
    project = ROOT / "samples" / "baseline" / "four-sections.fvd"
    with Session(project) as s:
        commands = s.describe()
        print(f"describe: {len(commands)} commands, for example {commands[0]['name']}")
        spec = next(c for c in commands if c["name"] == "transition.setChange")
        print(f"transition.setChange takes {[p['name'] for p in spec['parameters']]}")
        # section 3 of four-sections.fvd is a force section; its normal-force transition 1
        target = {"section": 3, "function": "normal", "transition": 1}
        before = s.section_end(3)
        print(f"section 3 ends at pitch {before['pitch_deg']:.3f} deg, speed {before['speed_mps']:.3f} m/s")
        s.run("transition.setChange", target, change=1.5)
        changed = s.section_end(3)
        print(f"after change=1.5 it ends at pitch {changed['pitch_deg']:.3f} deg")
        undone = s.undo()
        print(f"undo: done {undone['done']} ({undone['label']})")
        after = s.section_end(3)
        print(f"after undo it ends at pitch {after['pitch_deg']:.3f} deg")
        bad = s.run_raw("no.suchCommand", target)
        print(f"a wrong name answers: {bad['error']}")
        history = s.history()
        print(f"history: {history['steps']} at position {history['position']}")
        stats = s.stats()
        print(f"stats: {stats['requests']:.0f} requests, run {stats['run_ms']:.2f} ms")
        ok = (changed["pitch_deg"] != before["pitch_deg"] and after["pitch_deg"] == before["pitch_deg"]
              and undone["done"] and not bad["ok"])
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(demo())
