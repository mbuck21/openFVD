# Codebase map

What a session needs to find the right file in one read. Written 2026-11-10 from the code at upstream commit `4482a15` (altlenny/openFVD, 2023-02-06, "open FVD++ v0.79"). Terms: `docs/glossary.md`. Building: `docs/building.md`.

## What the program is

FVD++ builds roller-coaster track geometry from the forces the rider should feel (force vector design, after newton2). The designer chains **sections**; each section is integrated node by node at 1000 nodes per second of train time; the result is drawn in OpenGL, plotted, and exported to NoLimits 1 or 2 as a chain of Bézier segments.

## Build system and dependencies

- **`fvd.pro`** is the only build file: qmake, `TEMPLATE = app`, `TARGET = FVD`, Qt modules `core gui widgets printsupport opengl`. There is no CMake file. The automated tests are small qmake projects of their own under `tests/`, built and run by `tools/run-tests.cmd` (`tests/README.md`).
- **Qt 5.** `fvd.pro` says "tested with 5.4.0"; `readme.txt` says v0.77 moved to Qt 5.2.1. The renderer is written against the Qt 4-era `QGLWidget`, `QGLShaderProgram`, `QGLBuffer` and `QGLFormat` from the `QtOpenGL` module (`renderer/glviewwidget.h`, `renderer/mytexture.cpp`, `main.cpp`). Those classes exist through Qt 5.15 and are gone in Qt 6, so a Qt 6 port means moving to `QOpenGLWidget` and friends.
- **Libraries outside the tree** (none vendored; on Windows they come from the MSYS2 packages or a machine-local `libs.pri`, see `docs/building.md`; `fvd.pro` defines `GLM_ENABLE_EXPERIMENTAL` and `GLM_FORCE_CTOR_INIT` so current glm behaves like the 0.9.5 the code was written for):
  - **GLEW 1.12.0** (Windows and Linux only; macOS uses the system GL3 headers). `bin/win64/glew32.dll` ships beside the binary.
  - **glm 0.9.5.1** (header-only maths; `lenassert.h` includes `glm/glm.hpp` and `glm/ext.hpp` with `GLM_FORCE_RADIANS`). Upstream issue altlenny/openFVD#5 reports that a current glm no longer compiles `core/track.cpp`.
  - **lib3ds 20080909** (the 2.0 API, `lib3ds.h`), used only by `ui/objectexporter.cpp` for the experimental `.3ds` export. `bin/osx64.zip` carries `lib3ds-2.0.dylib`. Optional on Windows: compiled in only with `qmake CONFIG+=with3ds` (`FVD_WITH_3DS`).
  - System OpenGL and GLU (`-lOpenGL32 -lGlU32` on Windows).
- **Vendored in the tree:** `ui/qcustomplot.cpp` and `.h`, QCustomPlot 1.2.1 (25,000 lines, half the repository). Never edit it; treat it as a library.
- **Resources:** `resources.qrc` embeds the shaders, `metal.png`, `background.png`, the six `sky/*.jpg` faces and the train catalogue `data/traintypes.json` into the binary; `common::getResource()` in `osx/common.cpp` (built on every platform) resolves files such as `options.cfg` next to the executable.
- **Platform code:** `osx/*.mm` and `osx/Init.cpp` (macOS document model, built only under `macx`); `winicon.rc` and `fvd.ico` on Windows. `main.cpp` installs a message handler that writes `fvd.log` in the working directory on Windows and Linux, and logs the OpenGL vendor, renderer and version at start-up.
- **Compiler assumptions:** C++03-era code with Qt containers, raw `new`/`delete`, `float` everywhere, `#define` constants, and raw `struct` writes to disk (see "Saved project file").

## Directory layout

| Folder | What it holds | Main classes |
|---|---|---|
| `core/` | The model and the physics: tracks, sections, nodes, transition functions, save and load (`projectfile`), the NoLimits element files (`elementfile`), exporters, importers, undo, smoothing. No Qt widgets, but it includes `ui/` headers in places. | `track`, `section` and its six subclasses, `mnode`, `func`, `subfunc`, `trackHandler`, `smoothHandler`, `undoHandler`, `undoAction`, `projectFileClient`, `noLimitsImporter` |
| `renderer/` | The OpenGL 3 view and the track mesh builder, plus thin wrappers for shaders, textures and framebuffers. | `glViewWidget`, `trackMesh`, `myShader`, `myTexture`, `myFramebuffer` |
| `ui/` | Every Qt widget and its `.ui` form (12 forms, compiled by uic). Also the vendored QCustomPlot. | `MainWindow`, `projectWidget`, `trackWidget`, `graphWidget`, `graphHandler`, `transitionWidget`, `exportUi`, `importUi`, `smoothUi`, `optionsMenu`, `TrackProperties`, `conversionPanel`, `objectExporter`, `myTreeWidget`, `myQDoubleSpinBox`, `dragLabel` |
| `shaders/` | GLSL pairs (`.vert`/`.frag`): `track`, `floor`, `sky`, `simpleSM` (shadow map), `shadowVolume`, `normals`, `occlusion`, `oculus` (Rift, compiled out unless `USE_OVR`), `debug`. `metal.dat` is a texture blob. | |
| `sky/` | Skybox faces (the `.jpg` files are the ones in the qrc; the `.png` variants are unused leftovers). | |
| `osx/` | macOS app delegate and document handling; `common.cpp` (resource path helper) is built everywhere. | |
| `data/` | `traintypes.json`, the train catalogue: one entry per train type with its heartline, seating and restraint, each value with a source and a confidence. Read by `trainCatalogue` (`core/traintype.h`, QtCore only); shown in the Track Properties dialog. | |
| `tests/` | Automated tests, one folder each, QtCore-only console programs (`tests/README.md`). | |
| `tools/` | The build script, the test runner, `compare_exports.py` and `run-baseline.py` (`docs/testing.md`). | |
| `bin/` | Committed release binaries: `win64/FVD.exe` (static Qt, MinGW, built 2016-10-19, runs on Windows 11 as-is) and `osx64.zip`. See `docs/building.md`. | |
| Root | `main.cpp` (entry, logging, loads a `.fvd` given on the command line; a first argument starting with `--batch-` hands over to `batchRunner` in `ui/batchrunner.cpp`, which works without showing a window; its scripted test scenarios and the undo round trip are in `ui/batchscenarios.cpp`), `lenassert.h` (constants and the `lenAssert` macro, included everywhere), `fvd.pro`, `resources.qrc`, icons and textures. | |

## Data model

Ownership runs project, then track, then section, then node. Everything is owned by raw pointer; the widgets own the handlers.

- **Project** = `projectWidget` (`ui/projectwidget.h`): `QList<trackHandler*> trackList`, plus the ground-texture path. There is one project per main window.
- **`trackHandler`** (`core/trackhandler.h`): one per track. Bundles the model (`track* trackData`), its tab (`trackWidget*`), its plot (`graphWidget*`), its mesh (`trackMesh*`), its undo stack (`undoHandler*`) and the three track colours.
- **`track`** (`core/track.h`): the anchor node (`mnode* anchorNode`, with `startPos`, `startYaw`, `startPitch`), the global physics inputs `fHeart` (heartline height in metres), `fFriction`, `fResistance`, the ordered `QList<section*> lSections`, the smoothing list, the draw style (`enum trackStyle`: generic, generic flat, Vekoma, B&M, triangle, box, small flat, double spine) and the point-of-view position. The whole track is one continuous chain: each section's first node is a copy of the previous section's last node, and `updateTrack()` re-integrates from a given section and node onward.
- **`section`** (`core/section.h`): abstract base with `QVector<mnode> lNodes` (one node per 1/`F_HZ` second; `lNodes.reserve(180000)` at construction), a `rollFunc`, and the parameters of every subtype in one flat set of fields. `enum secType` lists the kinds; `track::newSection()` is the factory (`core/track.cpp`):
  - `secstraight`: fixed heartline length `fHLength`, roll function over distance.
  - `seccurved`: `fAngle`, `fRadius`, `fDirection`, `fLeadIn`, `fLeadOut`; roll over the arc.
  - `secforced`: the FVD section. `normForce` and `latForce` are functions of time (`bArgument == TIME`) or distance (`DISTANCE`) giving the target vertical and lateral G; the integrator rotates the direction each millisecond so the rider feels exactly those forces (`updateSection()` / `updateDistanceSection()` in `core/secforced.cpp`). Speed is either carried from the previous node (`bSpeed`) or held constant at `fVel`.
  - `secgeometric`: same shape, but `normForce` and `latForce` are reused to hold **pitch change** and **yaw change** in degrees per second (`eFunctype funcPitch`/`funcYaw`), and forces are derived afterwards.
  - `secbezier`: a list of `bezier_t` control points (`core/mnode.h`), used by the NoLimits 1 importer and for hand-placed splines; no roll function.
  - `secnlcsv`: a track sampled from a NoLimits 2 CSV export (`loadTrack()` reads tab-separated position, front and left vectors); physics are simulated over the fixed geometry.
  - `bOrientation` chooses `EULER` (roll measured against the world up vector, so yaw adds banking) or `QUATERNION` (roll relative to the track).
- **`mnode`** (`core/mnode.h`): one sample of the train. Position `vPos` (this is the point the physics follows, **the heartline**; `vNorm` points from the rider toward the rails, so `vPosHeart(fHeart) = vPos + fHeart * vNorm` is, despite its name, the **track centre line**, which is what the mesh draws and the exporters write), the frame `vDir`/`vLat`/`vNorm`, `fRoll`, `fVel`, `fEnergy`, the felt forces `forceNormal`/`forceLateral` in G, the per-node deltas (`fPitchFromLast`, `fYawFromLast`, `fRollSpeed`, `fDistFromLast`, ...) and running lengths. `getPitch()` and `getDirection()` return degrees. `F_HZ` (1000) in this header is the integration rate used everywhere.
- **`func`** and **`subfunc`** (`core/function.h`, `core/subfunction.h`): a transition curve is a `func` (typed by `eFunctype`: roll, normal, lateral, pitch, yaw) made of consecutive `subfunc` pieces. Each piece has an argument range (`minArgument`..`maxArgument`, seconds or metres), a `startValue`, a change `symArg`, a shape `eDegree` (linear, quadratic, cubic, quartic, quintic, sinusoidal, plateau, tozero, freeform), a shape parameter `arg1`, and the time-warp knobs `centerArg` and `tensionArg`. `subfunc::getValue()` is the whole maths of transitions in one `switch`. A piece can be `locked` so it stretches to the section end.
- **Smoothing:** `smoothHandler` entries on the track (`core/smoothhandler.h`) describe a roll-speed moving-average filter applied by `track::applyRollSmooth()`. `updateTrack` re-applies it only once `track::bAutoSmooth` is set (the smoothing dialog was opened, or the project was loaded). The smoothing dialog (`ui/smoothui.cpp`) owns the list rows and listens to each entry through `smoothHandlerListener`; the entry's name (`sName`) is what is saved.
- **Undo:** `undoHandler` keeps a stack of `undoAction` (`core/undoaction.h`), one `eActionType` per spin box or structural edit; `undoAction::doUndo()` is a large `switch` that re-applies values through the widgets.
- **Constants** live in `lenassert.h` (`F_G` = 9.80665, degree/radian factors) and `core/mnode.h` (`F_HZ`). Friction and air resistance are per-track fields edited in `ui/trackproperties.cpp`; the energy update is in each section's `updateSection()` (for example `core/secforced.cpp`, the lines using `parent->fFriction` and `parent->fResistance`).

### Saved project file (`.fvd`)

Binary, written field by field with the helpers in `core/exportfuncs.cpp` (`writeBytes`, `readInt`, `readFloat`, `readVec3`, ...). `writeBytes()` writes each block **back to front**, so numbers are big-endian in the file and a block of several values (a `vec3`, the three `QColor`s, the `"MELE"` tag of an export) appears in reverse order. Layout, from `writeProject()` (`core/projectfile.cpp`) and `track::saveTrack()`:

1. `"FVD"` then the version string `"v0.77"` (five bytes). `readProject()` accepts `v0.77` and the legacy `v0.30` (routed to `legacyLoad*` functions).
2. The ground-texture path: `int` length, then the bytes.
3. For each track, a `"TRC"` block: name (length-prefixed), the three `QColor` values **written as raw structs**, anchor position/roll/pitch/yaw/speed/normal/lateral, `fHeart`, `fFriction`, `fResistance`, draw flags, style, wireframe flag, POV position, the section count, then one section block per section, then the smoothing list, then `"EOT"`.
4. A section block starts with a three-letter tag (`STR`, `CUR`, `FRC`, `GEO`, `BEZ`, `CSV`) and holds that subtype's fields followed by its functions; a function is `"FUNC"`, a count, and one `subfunc` record per piece. The per-type writers are `saveSection()` in each `core/sec*.cpp`.
5. `"EOP"` ends the project.

Because structs and `QColor` are dumped raw, the format is tied to the compiler's layout and to the platform; the same functions also exist on `std::stringstream` for the undo system. `MainWindow::backupSave()` writes `<name>.fvd.bak` before every export.

## Main flows

| Flow | Entry point | Then |
|---|---|---|
| Start | `main()` in `main.cpp` | creates `MainWindow` (`ui/mainwindow.cpp`), which creates the GL view, the project tab, the export and options dialogs; `glViewWidget::initializeGL()` logs the GL context, runs `glewInit()` and calls `MainWindow::initProject()`. A `.fvd` path on the command line goes to `MainWindow::loadProject()`. With a first argument starting with `--batch-`, `main()` instead calls `batchRunner::run()` (`ui/batchrunner.cpp`) and leaves with its exit code: the main window is built but never shown, OpenGL is started with `glViewWidget::initializeWithoutShowing()`, and the project is loaded through `projectWidget::loadProject()` so that no message box can open. |
| Open project | `MainWindow::on_actionLoad_triggered()` | `projectWidget::loadProject()`, then `loadProjectFile()` and `readProject()` (`core/projectfile.cpp`), which ask the project panel for each new track and tell it what to show through `projectFileClient` (implemented by `projectWidget`), then `track::loadTrack()` per track, which creates each section with `track::appendSectionFromFile()` (the model work of the panel's "add"; the track's `trackListener` then adds the panel's row), then each section's `loadSection()` and `updateSection()`, then `track::updateTrack(0, 0)`; then the reader brings the smoothing entries up to date (`track::updateSmoothEntries()`) and applies roll smoothing. |
| Save project | `MainWindow::on_actionSave_triggered()` / `on_actionSave_As_triggered()` | `projectWidget::saveProject()`, then `saveProjectFile()` and `writeProject()` (`core/projectfile.cpp`), then `track::saveTrack()` per track (the panel copies the typed section names into each track just before). |
| Add or edit a section | `trackWidget` slots in `ui/trackwidget.cpp` (`addStraightSec()`, `on_curvedRadiusBox_valueChanged()`, ...) and `transitionWidget` slots in `ui/transitionwidget.cpp` for the function pieces | push an `undoAction`, change the model field, then `track::updateTrack(section, node)`. |
| Recompute geometry | `track::updateTrack()` in `core/track.cpp` | re-runs `updateSection()` on the changed section from the changed node and on every later section from node 0, re-applies smoothing, then `trackMesh::buildMeshes(fromNode)`. The status bar shows the milliseconds used. |
| Render | `glViewWidget::paintGL()` in `renderer/glviewwidget.cpp` | shadow passes, floor, sky, then `drawTrack()` per track using `trackMesh` vertex buffers; `curTrackShader` (menu Visualize, Ctrl+1 to Ctrl+6) picks which per-node value the track shader colours: nothing, velocity, roll speed, normal force, lateral force, flexion. `legacyMode` draws with fixed-function GL when no GL 3 context exists. Camera and fly mode: `moveCamera()`, `keyPressEvent()`. |
| Plot | `graphWidget` (`ui/graphwidget.cpp`) with one `graphHandler` per curve type (`enum graphType` in `ui/graphhandler.h`) | draws the per-node values with QCustomPlot; dragging on the plot edits the active `subfunc`. |
| Export to NoLimits | `MainWindow::on_actionExportAs_triggered()` opens `exportUi` (`ui/exportui.cpp`); `doFastExport()` for Ctrl+E | type 0 **NoLimits 2** (`.nl2elem`): `track::exportNL2Track()` fits a cubic spline through sampled heartline points and writes text; type 1 **Tangent** (`.nlelem`): `track::exportTrack4()`, then `mnode::exportNode()`, then `writeToExportFile()` in `core/exportfuncs.cpp` (binary NL1 element); type 2 **Spline** (`.nlelem`): `track::exportTrack3()`. The segment length and the RelRoll threshold come from the dialog; the file around each export (header, footer, lengths) is written by `writeNL2Element()`, `writeNL1TangentElement()` and `writeNL1SplineElement()` (`core/elementfile.cpp`), which batch mode calls too. |
| Export 3D model | `MainWindow::on_actionExport_Model_As_triggered()` opens `objectExporter` (`ui/objectexporter.cpp`) | `trackMesh::build3ds()` then lib3ds writes a `.3ds` file. Experimental. |
| Import | `projectWidget::importNLTrack()`, `importPointList()`, `importNoLimitsCSV()`, `importFromProject()` (`ui/projectwidget.cpp`) | `noLimitsImporter` (`core/nolimitsimporter.cpp`) reads `.nltrack` or a point-list text file into a `secbezier` it creates with `track::appendSectionFromFile()`; CSV goes to `secnlcsv::loadTrack()`. |
| Options | `optionsMenu` (`ui/optionsmenu.cpp`) | colours, mesh and shadow quality, field of view, undo depth, GL policy; persisted to `options.cfg` beside the executable. |
| Undo / redo | `MainWindow::on_actionUndo_triggered()`, then `undoHandler::doUndo()`, then `undoAction::doUndo()` | |

Global state to know about: `MainWindow* gloParent` (defined in `ui/mainwindow.cpp`, used from `core/` and `renderer/`) and `glViewWidget* glView`. Qt auto-connects slots named `on_<objectName>_<signal>()` to the widgets in the `.ui` forms, so renaming a widget in a form silently disconnects its slot.

## Where a change lands

- **New section type:** add to `enum secType` (`core/section.h`), a `secXxx` class with `updateSection()`, the five save/load overloads, `getMaxArgument()`, `isLockable()`, `isInFunction()`; the `switch` in `track::newSection()` and the tag `switch` in `track::loadTrack()` (`core/track.cpp`); a three-letter file tag; the add/append slots and the section frame in `ui/trackwidget.cpp` and `ui/trackwidget.ui`; `fvd.pro` (`SOURCES`/`HEADERS`); the `undoAction` types if it has editable fields; `transitionWidget::adjustLengthSteps()` if it has functions.
- **New transition shape:** `enum eDegree` in `core/subfunction.h`, a case in `subfunc::getValue()` and in `getMinValue()`/`getMaxValue()`/`isSymmetric()`, the combo box in `ui/transitionwidget.ui` and `on_transitionBox_currentIndexChanged()`.
- **New export format:** a `track::exportXxx()` in `core/track.cpp` (sample with `section::iFillPointList()` or `fFillPointList()`, read nodes with `getPoint()`), a case in `exportUi::doFastExport()` and `on_buttonBox_accepted()` with its file filter, and an item in `ui/exportui.ui`'s `exportTypeBox`. For a text format follow `exportNL2Track()`; for a binary one follow `exportTrack4()`. The header and footer around each export are written in `core/elementfile.cpp`, for the dialog and batch mode alike; a new format gets its function there.
- **New import format:** `noLimitsImporter` (`core/nolimitsimporter.cpp`) or a new section type like `secnlcsv`; the menu action in `ui/projectwidget.cpp`.
- **Physics constant or model:** `F_G` and angle factors in `lenassert.h`; `F_HZ` in `core/mnode.h` (changing it changes node counts, file contents and every `/F_HZ` in `core/`); gravity, friction and drag in the energy lines of each `updateSection()` (`core/secforced.cpp`, `core/secgeometric.cpp`, `core/secstraight.cpp`, `core/seccurved.cpp`); the felt-force formula in `mnode::calcSmoothForces()` and in `secforced::updateSection()`.
- **UI panel or field:** the `.ui` form in `ui/`, the matching `on_..._` slot in its `.cpp`, an `undoAction` type and the `undoHandler` "old value" field if it should be undoable, and `track::saveTrack()`/`loadTrack()` if the value is saved (which changes the file format).
- **Rendering or visualisation mode:** `shaders/track.frag` for the colouring, `glViewWidget::drawTrack()` for uniforms, `trackMesh::appendTrackNode()` for the per-vertex values, `MainWindow::useShader()` and the Visualize menu in `ui/mainwindow.ui` for the switch.
- **Track style (rail cross-section):** `enum trackStyle` in `core/track.h`, the pipe options in `trackMesh::buildMeshes()` (`renderer/trackmesh.cpp`), the combo box in `ui/trackproperties.ui`.
- **File format:** `track::saveTrack()`/`loadTrack()` and the per-section writers; bump the version string in `writeProject()` (`core/projectfile.cpp`) and add a branch in `readProject()` so old files still open.
- **Build or dependency:** `fvd.pro` only. See `docs/building.md`.

## Known rough edges (not yet fixed)

The defects a current compiler pointed at were fixed for issue #8. What a build still warns about in the project's own files: deprecated Qt 5 calls (`QWheelEvent::delta()`, `qVariantFromValue()`, `QTreeWidgetItem::setBackgroundColor()`), which matter for a Qt 6 port (issue #3), and a few variables that are set and never read.

- Raw-struct file writes, including `QColor` and `bool`, make `.fvd` files compiler- and platform-specific.
- `core/secbezier.cpp.orig` is a leftover merge file that is not in `fvd.pro`.
- `ui/mainwindow.h` keeps a set of `#define` type codes (`STR_SEC`, `CUR_SEC`, ...) that duplicate `enum secType` with different numbering.
- Every GL call assumes GLEW initialised; `exit(4)` on failure.
- The Tangent exporter's handle length is `radius * tan(angle / 2)` (`mnode::exportNode()`), which blows up as a segment turns toward 180 degrees: in the baseline's tight loop it gives handles of about 8 m on a 2 m segment (`docs/testing.md`).
- The saved project keeps `"v0.77"` as its format version (`writeProject()` in `core/projectfile.cpp`); that is the file format's number, not the program's.
