# renderer/ (the 3D view)

The OpenGL view of the project and the builder of each track's 3D mesh, plus thin wrappers for shaders, textures and framebuffers. It shows the model and changes none of it, except the point-of-view position (`track::povPos`) when the arrow keys move it.

Read the root `CLAUDE.md` first, then `docs/codebase-map.md` (the "Render" flow). The model is in `core/` (`core/CLAUDE.md`), the windows in `ui/` (`ui/CLAUDE.md`). The GLSL files are in `shaders/` and are embedded by `resources.qrc`.

## What renderer may depend on

- **Allowed:** `core/` (reads `track`, `section`, `mnode`), Qt (`QGLWidget` and the `QtOpenGL` classes), GLEW, glm.
- **Reaches into `ui/`:** it includes `mainwindow.h` and `optionsmenu.h` and reads the globals `gloParent` (track list, current track, selected transition, options such as `meshQuality` and `fov`) and `glView` (for `legacyMode`). This is the one folder allowed to do that; `core/` is not (`tools/check-core.py`).
- **Reached by:** `ui/`. `trackHandler` creates the `trackMesh`; `MainWindow` creates the `glViewWidget`; `objectExporter` calls `trackMesh::build3ds()`.
- **How the model reaches it:** only through `trackHandler::trackGeometryChanged()` (`ui/trackhandler.cpp`), which calls `trackMesh::buildMeshes(fromNode)`. The model does not know about meshes.

## Main classes

- **`glViewWidget`** (`glviewwidget.*`): the 3D widget (a `QGLWidget`). Owns the camera, the shadow, sky and floor passes, the shaders and textures. Created by `MainWindow`. `paintGL()` draws every track in `gloParent->getTrackList()`; `initializeGL()` ends by calling `MainWindow::initProject()`.
- **`trackMesh`** (`trackmesh.*`): the vertex data and OpenGL buffers of one track (rails, cross ties, supports, shadows, heartline). Created by `trackHandler`, rebuilt on every geometry change, from the changed node onward. About 3,700 lines.
- **`myShader`, `myTexture`, `myFramebuffer`:** small wrappers for GLSL programs, textures and framebuffer objects. `shaders.h` holds the compile-log helpers (it defines functions in a header; include it from `myshader.cpp` only).

## Where a change lands

| Change | Open |
|---|---|
| How a track looks (colouring by speed, force and so on) | `shaders/track.frag`; uniforms in `glViewWidget::drawTrack()`; per-vertex values in `trackMesh::appendTrackNode()`; the switch in `MainWindow::useShader()` and the Visualize menu in `ui/mainwindow.ui` |
| Rail cross-section | `enum trackStyle` (`core/track.h`) and the pipe options in `trackMesh::buildMeshes()` |
| Camera, controls | `glViewWidget::moveCamera()`, `keyPressEvent()`, `mousePressEvent()` |
| Sky, floor, shadows | the `draw...` functions in `glviewwidget.cpp` and the matching pair in `shaders/` |
| 3D model export | `trackMesh::build3ds()` here; the dialog is `ui/objectexporter.*` |
| Mesh quality | `trackMesh::buildMeshes()` reads `gloParent->mOptions->meshQuality` |

## Traps

- **The GL context must exist before a mesh is built.** `trackMesh`'s constructor and `init()` call `glGenBuffers()` and `glGenVertexArrays()` unless `glView->legacyMode` is set. A `trackHandler` makes its mesh in its constructor, so no `trackHandler` may be made before OpenGL is up.
- **Batch mode has no visible window.** `glViewWidget::initializeWithoutShowing()` creates the native window hidden and runs `initializeGL()`, which `batchRunner` calls before it loads anything (`ui/batchrunner.cpp`, `docs/testing.md`). Keep it working when you change the start-up.
- **`legacyMode`** is set once, in the `MainWindow` constructor, from the options' GL policy and the context's version. When it is true the program draws with fixed-function GL (`legacyDrawFloor()`, `legacyDrawTrack()`) and `trackMesh` makes no buffers. Every GL change must keep both paths working, or say which one it left alone.
- **`glewInit()` failing ends the program** (`exit(4)` in `initializeGL()`).
- **`QGLWidget` is Qt 5 only.** A Qt 6 port moves to `QOpenGLWidget` (`docs/codebase-map.md`).
- **The Oculus code** (`drawOculus()`, `riftMode`) is compiled out unless `USE_OVR` is defined. It is not built or tested.

## Check a change

The baseline does not compare anything drawn, but every batch run builds each track's mesh, so a crash or a bad buffer call in `trackMesh` shows up there:

```
tools\build-windows.cmd
py -3 tools/run-baseline.py --undo-check --fixtures --imports
```

It must end `PASS`. Then look at it: start `build\dist\FVD.exe`, open `samples/baseline/four-sections.fvd`, turn the view, try the Visualize menu, and switch the GL policy in the options once if your change touches the drawing paths. Say in the pull request what you looked at.
