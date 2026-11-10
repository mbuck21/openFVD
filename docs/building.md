# Building FVD++ on Windows

Checked 2026-11-10 on the owner's PC (Windows 11 Home, x64, NVIDIA RTX 5070 with an OpenGL 4.6 driver, 250 GB free on C:). Nothing has been built yet: the toolchain is not installed. This file records what the project expects, what is here, and the options. The choice is the owner's (issue #1, label `owner`).

## What `fvd.pro` expects

- **qmake and Qt 5** with the `core gui widgets printsupport opengl` modules. The renderer uses the Qt 4-era `QGLWidget` family from the `QtOpenGL` module, which exists up to Qt 5.15 and is removed in Qt 6. So the target is **Qt 5.15.x**, the last Qt 5; the "tested with 5.4.0" in `fvd.pro` and the "5.2.1" in `readme.txt` are older.
- **A C++ compiler Qt was built with.** The committed `bin/win64/FVD.exe` is a MinGW build (it imports `msvcrt.dll`, not a Visual C++ runtime) with Qt linked statically, so the upstream author used **MinGW-w64**. MSVC should also compile the code, but nobody has tried; the code is C++03-era and has no MSVC-specific parts.
- **Three libraries outside the tree**, with Windows paths hard-coded in `fvd.pro` under `C:\Development\Libraries\`: GLEW 1.12.0 (`glew32.lib` and `glew32.dll`), glm 0.9.5.1 (headers only), lib3ds 20080909 (built as `lib3ds.dll`). Any build on this PC edits those three `INCLUDEPATH`/`LIBS` lines or sets them from environment variables.
- OpenGL 3.1 or better at run time for the visualisations; the program falls back to a legacy fixed-function mode below that.

## What is installed here

Checked with `where` and by listing the usual folders:

| Tool | Present |
|---|---|
| `qmake`, `qmake6`, Qt under `C:\Qt` | no |
| `cl` (MSVC), Visual Studio or Build Tools, `vswhere` | no |
| `g++`, `mingw32-make`, MSYS2 (`C:\msys64`) | no |
| `cmake`, `ninja`, `nmake` | no |
| `vcpkg`, Chocolatey, Scoop | no |
| `winget` | yes |
| `C:\Development\Libraries` (the path in `fvd.pro`) | no |
| Python 3.14, git 2.53, `gh` 2.100 | yes |

So no build was attempted. The committed binaries were checked instead.

## Do the committed binaries run?

**Yes.** `bin/win64/FVD.exe` (x64, built 2016-10-19, 19 MB with Qt inside; only `glew32.dll` beside it is needed) starts on this PC and gets a real OpenGL context. Its `fvd.log` after launch:

```
FVD++ v0.77 Logfile
found OpenGL Context:
Vendor: NVIDIA Corporation
Renderer: NVIDIA GeForce RTX 5070/PCIe/SSE2
Version: 4.6.0 NVIDIA 617.42
Samples: 1 4
GLEW initialization successful!
```

It writes `fvd.log` and `options.cfg` into the folder it runs from, so run it from a copy outside the repository or delete those two files before committing (`.gitignore` now lists both). `bin/osx64.zip` is the macOS app bundle and was not tested.

That binary is the baseline for any rebuild: a rebuilt FVD++ should open the same `.fvd` files and export the same elements before a change is trusted.

## Options for a toolchain on this PC

Sizes are approximate, from the installers' own figures. All three give a `qmake` that can read `fvd.pro` unchanged apart from the library paths.

1. **MSYS2 with its MinGW-w64 packages (recommended: smallest, matches the upstream build, one command per package).**
   - `winget install MSYS2.MSYS2` (about 100 MB download, 500 MB installed).
   - In the MSYS2 UCRT64 shell: `pacman -S mingw-w64-ucrt-x86_64-{gcc,make,qt5-base,glew,glm,lib3ds}` (Qt 5 base is about 60 MB download, 350 MB installed; the rest is small). Total under 2 GB on disk. All three external libraries come as packages, so the hard-coded paths in `fvd.pro` become `-lglew32 -l3ds` and the system include path.
   - Build: `qmake fvd.pro && mingw32-make -j`. Running the result needs the MinGW Qt DLLs on `PATH` or a `windeployqt` pass.
   - Caveat: MSYS2's Qt 5 is 5.15.x with the UCRT runtime; the upstream binary used the older `msvcrt`. Fine for a fresh build, but the two are not mixable.

2. **The official Qt Online Installer with Qt 5.15 and its bundled MinGW.**
   - Needs a free Qt account; download about 1.5 GB for Qt 5.15 MinGW 64-bit plus the MinGW 8.1 tool kit, 3 to 4 GB on disk. Includes Qt Creator, which opens `fvd.pro` directly and is how the upstream author worked.
   - GLEW, glm and lib3ds still have to be fetched and built by hand (GLEW ships prebuilt for MinGW; lib3ds 20080909 needs a `./configure && make` in MSYS2 or a hand-written qmake project; glm is headers only). Set the three paths in `fvd.pro`.
   - Best if the owner wants the IDE and the designer for the `.ui` forms.

3. **Visual Studio 2022 Build Tools with vcpkg.**
   - `winget install Microsoft.VisualStudio.2022.BuildTools` with the "Desktop development with C++" workload (about 2 GB download, 7 GB on disk), then vcpkg for `qt5-base`, `glew`, `glm`, `lib3ds` (vcpkg builds Qt from source: an hour or more and several GB).
   - Largest, slowest, and the least like the upstream build; only worth it if an MSVC debugger or a later move to Qt 6 and CMake is the plan.

Whichever is chosen, the follow-up work is the same: fix the three library paths in `fvd.pro` (or add a `libs.pri` the owner's machine provides), fix whatever a 2026 compiler and glm report (upstream issue altlenny/openFVD#5 says current glm breaks `core/track.cpp`), and compare the result with the 2016 binary on a sample project.

## When the toolchain is in place

Record the exact commands here, replacing this section: the shell to open, the `qmake` invocation, the make command, how to run the result and where `fvd.log` lands. Then update `CLAUDE.md` ("Build and run") and close the toolchain issue.

Not yet decided, and listed in `docs/maybe-later.md`: a GitHub Actions build (it would need the same toolchain on a Windows runner; MSYS2 is the lightest there too).
