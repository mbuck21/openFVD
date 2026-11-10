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
"""Check that core/ (the model) does not depend on the windows.

    py -3 tools/check-core.py [<repository folder>]

Fails, naming file and line, when a file in core/
  (a) includes a header that lives in ui/ or renderer/ (the list of names is
      read from those folders each time, including the ui_*.h that the .ui
      forms generate),
  (b) names gloParent or glView (the globals of the main window and the view),
  (c) includes a QtWidgets, QtOpenGL or QtPrintSupport header, or OpenGL/GLEW.
Also fails when lenassert.h, which core/ includes everywhere, does (c).

The proof that core/ really builds without the windows is the program in
tests/coreonly/ (tools/run-tests.sh builds it); this check is the quick guard
that names the offending line. Ends with PASS or FAIL; exit code 0 or 1.
"""

import os
import re
import sys

# (c): headers of the modules a model must not need. To extend: add the class
# name (its header has the same name) to WINDOW_CLASSES, or a module prefix to
# WINDOW_PREFIXES. QtGui classes (QColor, QImage, QPainter) are allowed.
WINDOW_CLASSES = set("""
QWidget QApplication QMainWindow QDialog QMessageBox QErrorMessage QInputDialog
QFileDialog QProgressDialog QTreeWidget QTreeWidgetItem QListWidget
QListWidgetItem QTableWidget QTableWidgetItem QTreeView QListView QTableView
QLabel QPushButton QToolButton QCheckBox QRadioButton QComboBox QSpinBox
QDoubleSpinBox QSlider QLineEdit QTextEdit QPlainTextEdit QScrollArea QFrame
QGroupBox QTabWidget QStackedWidget QSplitter QStatusBar QToolBar QMenu
QMenuBar QAction QActionGroup QShortcut QHeaderView QAbstractButton
QVBoxLayout QHBoxLayout QGridLayout QFormLayout QLayout QStyle QPrinter
QPrintDialog QGLWidget QGLFormat QGLContext QOpenGLWidget QGraphicsView
QGraphicsScene QSystemTrayIcon QDesktopWidget
""".split())
WINDOW_PREFIXES = ("QtWidgets", "QtOpenGL", "QtPrintSupport", "GL/", "glew", "windows.h")
WINDOW_HEADER_PATTERN = re.compile(r"^(QtWidgets|QtOpenGL|QtPrintSupport)$")

GLOBALS = re.compile(r"\b(gloParent|glView)\b")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def window_headers(repo):
    """Names of the headers in ui/ and renderer/ (and the ones uic generates)."""
    names = set()
    for folder in ("ui", "renderer"):
        path = os.path.join(repo, folder)
        if not os.path.isdir(path):
            continue
        for name in os.listdir(path):
            if name.endswith(".h"):
                names.add(name)
            if name.endswith(".ui"):
                names.add("ui_" + name[:-3] + ".h")
    # a name that core/ has too (or the repository root) is not ambiguous to the compiler
    own = set(os.listdir(os.path.join(repo, "core")))
    own.add("lenassert.h")
    return names - own


def is_window_include(target):
    base = target.replace("\\", "/").split("/")[-1]
    if base in WINDOW_CLASSES or base in ("QtWidgets", "QtOpenGL", "QtPrintSupport"):
        return True
    return any(target.startswith(p) for p in WINDOW_PREFIXES)


def problems_in(path, shown, windows, repo, ties):
    found = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for number, line in enumerate(f, 1):
            match = INCLUDE.match(line)
            if match:
                target = match.group(1)
                base = target.replace("\\", "/").split("/")[-1]
                if ties and base in windows:
                    found.append((shown, number, "includes %s, which lives in ui/ or renderer/" % target))
                if is_window_include(target):
                    found.append((shown, number, "includes %s, a window (QtWidgets/QtOpenGL/OpenGL) header" % target))
            if ties and GLOBALS.search(line):
                found.append((shown, number, "names %s" % GLOBALS.search(line).group(1)))
    return found


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    repo = os.path.abspath(repo)
    core = os.path.join(repo, "core")
    if not os.path.isdir(core):
        print("FAIL: no core/ folder under %s" % repo)
        return 1

    windows = window_headers(repo)
    found = []
    count = 0
    for name in sorted(os.listdir(core)):
        if name.endswith((".h", ".cpp")):
            count += 1
            found += problems_in(os.path.join(core, name), "core/" + name, windows, repo, True)
    lenassert = os.path.join(repo, "lenassert.h")
    if os.path.isfile(lenassert):
        count += 1
        found += problems_in(lenassert, "lenassert.h", windows, repo, False)

    for shown, number, text in found:
        print("%s:%d: %s" % (shown, number, text))
    print("check-core: %d file(s) looked at, %d problem(s)" % (count, len(found)))
    print("FAIL" if found else "PASS")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
