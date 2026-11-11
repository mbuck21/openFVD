# 0009: Undo stays window code until the command list; F1f is a move, not a rewrite

- Status: accepted
- Date: 2026-11-10
- Needs owner approval: no (how to build it and in what order; the architect's call under record 0006). It narrows what Stage 1 delivers compared with record 0007, so the owner is told.

## Context

- Record 0007 planned seam S8 for piece F1f: undo resolves its target from stored indices and reports to a view interface, and the eight edits that do their work inside widget slots become core functions.
- By the time F1f was reached, pieces F1a to F1e had removed every other tie. The only files in `core/` that still named a window were `undoaction.cpp` (243 calls into the track panel, the graph pane, the transition editor and the main window) and `undohandler.cpp` (4).
- The undo round-trip check built in this stage found that undo is wrong today for eight kinds of edit (issue 31), one of which puts an invalid number into a transition, and that three of them make the program's output change from run to run.
- Stage 2 builds the command list (roadmap T1): every edit becomes a named command with typed parameters, and undo and redo record commands. That replaces `undoAction`'s 34-case switch altogether.
- Nothing in Stage 1 or planned for the start of Stage 2 needs undo without a window. Loading, computing, exporting and saving do not use it.

## Decision

1. **F1f moves `undoaction.*` and `undohandler.*` from `core/` to `ui/`, unchanged.** They are window code: they replay an edit by driving the same widgets the user drives. With the move, nothing in `core/` includes a window header or names `gloParent` or `glView`, which is what F1 is for.
2. **Seam S8 is not built in Stage 1.** Splitting each of the 34 undo cases into a model half and a view half would be careful work on a part that is known to be faulty and is about to be replaced. It would have to preserve the faults exactly to pass the stage's own rule, and Stage 2 would then throw the result away.
3. **The work S8 stood for moves into T1 (Stage 2):** each edit becomes a core function, written once as a command with its own undo; the eight defects of issue 31 are fixed there, one command at a time, each taken off the left-out list of the undo check as it is fixed.
4. **The roadmap's F1 line is corrected:** Stage 1 delivers a model that loads, computes, saves and exports with no window. "Every edit is a core function" is T1's.
5. **The undo list of a track is still owned by the track's view object** (`trackHandler`, in `ui/` since F1e). A core "track document" that holds a track together with its command history is designed with T1.

## Where enforced

- `docs/roadmap.md`, Stage 1 (F1) and Stage 2 (T1).
- `tools/check-core.py` (piece F1g): fails if a file in `core/` includes a `ui/` or `renderer/` header or names `gloParent` or `glView`.
- Issue 31 lists the undo defects and the order to fix them.
