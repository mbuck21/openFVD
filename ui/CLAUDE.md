# ui/ (the windows)

Every Qt widget, dialog and `.ui` form of the program, the undo classes, and the batch mode that drives the windows without showing them. It also holds the vendored plot library `qcustomplot.*`.

Read the root `CLAUDE.md` first, then `docs/codebase-map.md` for the flows. The model is in `core/` (`core/CLAUDE.md`), the 3D view in `renderer/` (`renderer/CLAUDE.md`).

## What ui may depend on

- **Allowed:** `core/` (calls it freely), `renderer/` (`glViewWidget`, `trackMesh`), Qt widgets.
- **Reached by:** `renderer/` (it includes `mainwindow.h` and reads the globals below), `main.cpp`. Nothing in `core/` may include anything from here (`tools/check-core.py`).
- **Globals:** `gloParent` (the `MainWindow`) and `glView` (the `glViewWidget`) are defined in `mainwindow.cpp`. Most window classes and `renderer/` use them.
- **How the model talks back:** a window implements an interface from `core/` and the model calls it. `trackHandler` implements `trackListener`; `smoothUi` implements `smoothHandlerListener`; `projectWidget` implements `projectFileClient`.

## Main classes

- **`MainWindow`** (`mainwindow.*`): the main window and the menus (load, save, export, undo, redo). Created by `main.cpp` (a hidden one by `batchRunner`). Creates the `glViewWidget`, the options, export, conversion and 3D-export dialogs. Holds `project`, the `projectWidget`.
- **`projectWidget`** (`projectwidget.*`): the project: the list of `trackHandler`s, the ground texture path, the import entries. Loads and saves through `core/projectfile.*`.
- **`trackHandler`** (`trackhandler.*`): one track's window side. Creates and owns the `track` (`trackData`), its `trackWidget`, `graphWidget`, list row, `trackMesh` and `undoHandler`. Receives the track's notices: rebuilds the mesh, shows the status text, redraws the graphs.
- **`trackWidget`** (`trackwidget.*`): the track's panel: anchor values, section list, per-section fields. Owns the list of `sectionHandler`s and holds the `smoothUi` (`smoothScreen`). Most edit slots are here.
- **`sectionHandler`** (`sectionhandler.*`): one row of the section list; creates the section in the track when the panel's "add" is used.
- **`graphWidget`** (`graphwidget.*`) with `graphHandler`: the plots (QCustomPlot) of one track. Contains the `transitionWidget`.
- **`transitionWidget`** (`transitionwidget.*`): the transition editor boxes (length, change, shape, centre, tension).
- **`smoothUi`** (`smoothui.*`): the smoothing dialog; owns the list rows.
- **`exportUi`** (`exportui.*`): the export dialog; calls `core/elementfile.*`.
- **`TrackProperties`**, **`optionsMenu`**, **`importUi`**, **`conversionPanel`**, **`objectExporter`**: the other dialogs.
- **`undoAction`, `undoHandler`** (`undoaction.*`, `undohandler.*`): a track's undo list. Each step holds the track's state after the edit; undo and redo restore a state (`core/trackstate.h`, `docs/decisions/0011-undo-by-snapshot-commands-on-top.md`). The core history (`core/trackhistory.*`) replaces them in a later piece.
- **`rideSheet`** (`ridesheet.*`): the ride sheet panel, the third pane of `MainWindow`'s splitter (Visualize > Ride sheet shows and hides it). It shows the numbers of `core/ridesummary` for the current track and holds only a `track*`, never a pointer into its sections or nodes. It refreshes 150 ms after the last notice from `trackHandler` (`MainWindow::rideSheetChanged()`), and when the tab, the selected track or the unit option changes. No ride maths belongs in it.
- **`batchRunner`** (`batchrunner.*`) and **`batchDriver`** (`batchscenarios.*`): the `--batch-...` commands (`docs/testing.md`).

## Where a change lands

| Change | Open |
|---|---|
| New field or box on a panel | the `.ui` form, the `on_<object>_<signal>()` slot in its `.cpp`, and the undo parts below |
| Undoable value | the slot changes the model, then records one step at its end: `undoHandler::addAction(new undoAction(handler, type))`; an `eActionType` only names it in the reports |
| New section type in the panel | `trackwidget.*` (add and append slots, section frame), `trackwidget.ui`; the model side is in `core/CLAUDE.md` |
| Transition editor | `transitionwidget.*` and `.ui`; `graphwidget.*` for dragging on the plot |
| Export dialog | `exportui.*` and `.ui`; the file framing is in `core/elementfile.*` |
| Import | `projectwidget.*` (`importNLTrack()` and the other import entries), `importui.*`; the reading is `core/nolimitsimporter.*` |
| Dialog for options | `optionsmenu.*` and `.ui`; saved in `options.cfg` |
| A new notice from the model | `core/tracklistener.h`, then the implementation in `trackhandler.*` |
| New number on the ride sheet | `core/ridesummary.*` first, then a row in `ridesheet.*` (`addRow()`, `refreshNow()`) |
| New batch scenario | `batchscenarios.*`, called from `batchrunner.*` |

## Traps

- **Slots connect by name.** `on_<objectName>_<signal>()` is wired to the widget with that object name in the `.ui` form. Renaming a widget silently disconnects its slot.
- **The batch scenarios find widgets by those same object names** (`findChild` in `batchscenarios.cpp`). Renaming a widget breaks the scripted projects and the undo check as well. They drive only what a person uses, and they must keep passing unchanged.
- **`phantomChanges`.** Code that sets a box from the model sets `phantomChanges` first, so the box's own slot does not treat it as an edit by the user. A new slot that changes the model starts with `if(phantomChanges) return;`.
- **Every edit records one undo step, after the model work is finished** (`undoHandler::addAction`). An edit that is not a step calls `undoHandler::takeOutsideChange()`, or a later undo takes it back.
- **No pointer into a track survives an undo.** A restore deletes every section, transition and smoothing entry; `trackHandler::trackAboutToReset()` drops what the windows hold and `trackReset()` rebuilds it. A new holder of such a pointer must be added to both.
- **Undo is exact for every kind of edit**, and the undo check runs with nothing left out (`KNOWN_BROKEN` in `batchscenarios.cpp` is empty).
- **A `trackHandler` builds its `trackMesh` in its constructor**, and a mesh makes OpenGL buffers unless `glView->legacyMode` is set. Make handlers only once the GL context exists (`renderer/CLAUDE.md`).
- **Never edit `qcustomplot.cpp` or `qcustomplot.h`.** Vendored library (root rule 3).
- **A window that would be shown in batch mode** is marked "do not show on screen" first. A batch command fails if a window reached the screen (`docs/testing.md`).

## Check a change

```
tools\build-windows.cmd
py -3 tools/run-baseline.py --undo-check --fixtures --imports
```

It must end `PASS`. The scripted projects and the undo check exercise the panels, the transition editor, Track Properties and the smoothing dialog. What they do not cover (the 3D view, the options dialog, most of the importers) needs a look by hand with `build\dist\FVD.exe`; say in the pull request what you tried.
