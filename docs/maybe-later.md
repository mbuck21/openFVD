# Maybe later

Ideas deliberately left out. Add here instead of widening a pull request. Each entry says what would trigger doing it.

## Added 2026-11-10 (the set-up session)

- **A GitHub Actions build workflow.** Not added yet: the toolchain is undecided (`docs/building.md`), the account's Actions minutes were exhausted until 2026-11-01, and a Qt build on a Windows runner costs ten minutes or more per run. Trigger: the toolchain is chosen and the local build works; then a `workflow_dispatch` MSYS2 build on `windows-latest` that uploads `FVD.exe` as an artifact, run by hand before a release, not on every push.
- **`docs/testing.md`** and a first automated test: a tiny command-line driver that loads a `.fvd`, exports an element and compares it with a stored fixture. Trigger: the first build; without a build there is nothing to run.
- **Port the renderer to Qt 6** (`QOpenGLWidget`, `QOpenGLShaderProgram`, `QOpenGLBuffer`, `QOpenGLFunctions`; `renderer/glviewwidget.*`, `renderer/mytexture.cpp`, `main.cpp`). Qt 5.15 still builds the code and is in long-term support, so this is not needed to get going. Trigger: Qt 5.15 becomes hard to install, or a feature needs Qt 6.
- **Replace the hard-coded `C:\Development\Libraries` paths in `fvd.pro`** with a `libs.pri` that each machine provides, or with pkg-config on MSYS2. Trigger: the first build on this PC (it is the first thing that fails).
- **Upgrade QCustomPlot** from 1.2.1 (2014) to a current 2.x: its API changed, so `ui/graphwidget.cpp` and `ui/graphhandler.cpp` would need edits. Trigger: a plot feature the old version cannot do, or a Qt 6 port.
- **A portable `.fvd` format** (explicit little-endian fields instead of raw structs and `QColor`), or a text or JSON sidecar. Trigger: a file saved on one platform fails to open on another, or a change to the model that would break the raw layout anyway.
- **Rewrite the NoLimits 2 CSV import and the `.nl2elem` export against the current NL2 format**; the code dates from 2016 and 2023. Trigger: an export that NoLimits 2 refuses.
- **Drop the Oculus Rift code** (`USE_OVR` blocks in `renderer/glviewwidget.*`, `shaders/oculus.*`): it targets the 2013 developer kit API. Trigger: it gets in the way of a renderer change.
- **Delete `core/secbezier.cpp.orig`** and the unused `sky/*.png` files. Trigger: the first pull request that touches those folders.
- **Fix the version string** in `main.cpp` ("v0.77") and the file-format version it writes. Trigger: the first release from this fork.
- **A macOS build on this fork.** The owner is on Windows. Trigger: someone asks (upstream issues 2 and 6 did).
