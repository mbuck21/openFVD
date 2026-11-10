# Building FVD++ on Windows

FVD++ builds on the owner's PC since 2026-11-10 with the MSYS2 UCRT64 toolchain (decision record 0003). The result was compared with the 2016 release binary on a sample track; see "What was verified" below and `docs/testing.md`.

## Build it

From a normal terminal in the repository folder:

```
tools\build-windows.cmd
```

or, inside the "MSYS2 UCRT64" shell from the Start menu:

```
tools/build-windows.sh
```

Either one runs qmake and make in `build/` and then collects a copy that runs on its own:

- `build/release/FVD.exe` is the program. It needs `C:\msys64\ucrt64\bin` on the `PATH` to find Qt.
- `build/dist/FVD.exe` is the same program with every DLL it needs beside it (about 75 MB). Double-click this one.

The first build takes a few minutes (the plot library alone is 25,000 lines). Later builds only recompile what changed; the script never empties `build/`, only `build/dist/`. For a clean rebuild, delete the `build` folder first. `build/` is ignored by git.

By hand, in the MSYS2 UCRT64 shell, the script does this:

```
mkdir -p build && cd build
qmake-qt5 ../fvd.pro CONFIG+=release
mingw32-make -j
```

Note the names: MSYS2 calls the Qt 5 tools `qmake-qt5` and `windeployqt-qt5`.

## The toolchain

Installed on the owner's PC on 2026-11-10:

| Piece | Version | How |
|---|---|---|
| MSYS2 | 2026-09-27 installer, in `C:\msys64` | `winget install MSYS2.MSYS2` |
| GCC (MinGW-w64, UCRT) | 16.2.0 | `pacman -S mingw-w64-ucrt-x86_64-gcc` |
| make | 4.4.1 | `mingw-w64-ucrt-x86_64-make` |
| Qt 5 | 5.15.19 | `mingw-w64-ucrt-x86_64-qt5-base` and `-qt5-tools` |
| GLEW | 2.3.1 | `mingw-w64-ucrt-x86_64-glew` |
| glm | 1.0.3 | `mingw-w64-ucrt-x86_64-glm` |

To set up another PC, install MSYS2, open the UCRT64 shell and run:

```
pacman -Sy
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,make,qt5-base,qt5-tools,glew,glm}
```

To update the toolchain later: `pacman -Syu` in the same shell. Rebuild and rerun the comparison in `docs/testing.md` afterwards, because a new compiler or glm can change results.

## What `fvd.pro` needed

Upstream's `fvd.pro` pointed at `C:\Development\Libraries\...` on the original author's machine. Now:

- On Windows the libraries are expected on the compiler's default search path, which is where the MSYS2 packages put them. A machine that keeps them elsewhere copies `libs.pri.example` to `libs.pri` (ignored by git) and sets the paths there.
- `DEFINES += GLM_ENABLE_EXPERIMENTAL`: glm 0.9.9 and later only expose the `gtx` functions the code uses (`glm::angle`, `glm::rotateY`, `glm::length2`) with this switch.
- `DEFINES += GLM_FORCE_CTOR_INIT`: glm 0.9.9 and later leave a default-constructed vector or matrix uninitialised; the glm 0.9.5 this code was written for zeroed them. Without this switch the program would compile and then compute with garbage.
- With those two switches the C++ compiled without a single error. No source file had to change for the compiler. (Upstream issue altlenny/openFVD#5 reported glm trouble in `core/track.cpp`; these two switches are the likely cure.)

## lib3ds and the `.3ds` model export

The experimental "Export Model" feature writes a `.3ds` file through lib3ds (the 2008 release, API 2.0). MSYS2 has no lib3ds package, so the default Windows build leaves the feature out: the menu item is still there and says that this build was made without lib3ds. Everything else is unaffected. Linux and macOS builds keep it as before.

To build with it, provide lib3ds (header `lib3ds.h` and a library `-l3ds`, through `libs.pri` if they are not on the default path) and run:

```
tools\build-windows.cmd CONFIG+=with3ds
```

How to get lib3ds for good (build the 2008 source once, vendor it into the repository, or replace the exporter) is an open question in `docs/maybe-later.md`.

## What was verified (2026-11-10)

- The build finishes with no errors. The warnings in the project's own files are listed in issue #8.
- `build/dist/FVD.exe` starts with only Windows on the `PATH`, gets an OpenGL 4.6 context and opens `samples/baseline/four-sections.fvd`, a project saved by the 2016 binary.
- The same project loaded and saved again by the 2016 binary and by the new build gives byte-identical `.fvd` files.
- The three exports of that project match the 2016 binary's: Spline byte for byte; NoLimits 2 in every one of its 230 numbers (the new C runtime prints `e+00` where the old printed `e+000`); Tangent in 21 of 22 segments, with one segment's control handles differing by under 1 mm. Details and how to repeat it: `docs/testing.md`.

## Known noise

- At start-up `fvd.log` gets ten lines saying "Any of the framebuffer attachment points are framebuffer incomplete." The 3D view has zero width for a moment before the window is laid out; the framebuffers are complete after the first resize. Harmless; listed with the defects to tidy.
- qmake prints "Failure to find: release/winicon_res.o" on a fresh build folder. It is generated a moment later.
- The program writes `fvd.log` in the folder it is started from and `options.cfg` next to the executable. Both are ignored by git.

## The committed 2016 binaries

`bin/win64/FVD.exe` (x64, built 2016-10-19 with MinGW and a static Qt 5.6.2; only `glew32.dll` beside it is needed) still runs on Windows 11 and is the reference the new build was compared against. `bin/osx64.zip` is the macOS app bundle and was not tested. Neither is rebuilt or replaced by this work (CLAUDE.md, rule 2).

## Other toolchains

Not used here, kept for reference: the Qt Online Installer with Qt 5.15 and its bundled MinGW gives Qt Creator and the form designer (3 to 4 GB, needs a Qt account, and GLEW and glm fetched by hand); Visual Studio Build Tools with vcpkg is the route if MSVC or a Qt 6 and CMake port ever becomes the plan (about 7 GB and a from-source Qt build). `fvd.pro` should work with both through `libs.pri`, but neither has been tried.
