# 0003: Build with MSYS2 UCRT64; lib3ds becomes optional on Windows

- Status: accepted
- Date: 2026-11-10
- Needs owner approval: yes (the toolchain is the owner's choice of 2026-11-10, issue #1; the lib3ds part is given by merging the pull request that carries this record)

## Context

- The owner's PC had no C++ toolchain. `docs/building.md` listed three options; the owner chose MSYS2 on 2026-11-10.
- MSYS2 packages everything the program needs (GCC 16.2, Qt 5.15.19, GLEW 2.3.1, glm 1.0.3) except lib3ds, which only the experimental `.3ds` model export uses.
- Upstream's `fvd.pro` hard-coded library paths on the original author's machine.
- glm changed twice since the 0.9.5 the code was written for: the `gtx` functions moved behind `GLM_ENABLE_EXPERIMENTAL`, and default constructors stopped initialising (`GLM_FORCE_CTOR_INIT` restores it).

## Decision

- The supported Windows build is MSYS2 UCRT64 with its Qt 5.15 packages, run through `tools/build-windows.cmd` (or `.sh`). `docs/building.md` holds the versions and commands.
- `fvd.pro` takes libraries from the compiler's default search path on Windows, or from a machine-local `libs.pri` (ignored by git; `libs.pri.example` shows the shape). No machine's paths go into the repository.
- `fvd.pro` defines `GLM_ENABLE_EXPERIMENTAL` and `GLM_FORCE_CTOR_INIT` for every platform. Old glm ignores both. This does change Linux and macOS builds made with glm 0.9.9 or later: they now get the zeroed vectors and identity matrices the code was written for, where before they would have compiled (if at all) with uninitialised ones. Neither platform has been built from this fork.
- On Windows the `.3ds` export is compiled in only with `qmake CONFIG+=with3ds` (which defines `FVD_WITH_3DS` and links `-l3ds`). Without it `ui/objectexporter.cpp` shows a message instead of exporting. Linux and macOS always define `FVD_WITH_3DS`, as before. This is the only C++ change the build needed.
- The 2016 binary stays the reference. `samples/baseline/` holds a project and its exports made with it, and `docs/testing.md` says how a build is compared with them. The first MSYS2 build matched (one Tangent segment within 1 mm, explained there).
- Staying on Qt 5.15 or porting to Qt 6 is still open (issue #3).

## Where enforced

- `fvd.pro`, `libs.pri.example`, `.gitignore`.
- `tools/build-windows.cmd`, `tools/build-windows.sh`, `tools/compare_exports.py`.
- `docs/building.md`, `docs/testing.md`, `samples/baseline/README.md`.
- `ui/objectexporter.cpp` (the `FVD_WITH_3DS` guard).
