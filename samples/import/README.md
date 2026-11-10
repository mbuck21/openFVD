# Import samples

Inputs for the three track imports of the project panel ("Import NoLimits 1 Track", "Import from Pointlist", "Import NoLimits 2 CSV"), so that a change to the import code can be checked. No baseline project contains an imported section, so the baseline does not cover them.

All three describe the same short, flat piece of track at 10 m height: a 30 m straight into a 90 degree curve of 40 m radius. They were written on 2026-11-10 by `py -3 tools/make-import-samples.py samples/import`, which follows the importers' reading code:

| File | Import | Format |
|---|---|---|
| `nl1.nltrack` | NoLimits 1 Track (`core/nolimitsimporter.cpp`, `importAsNlTrack`) | Only the two blocks the importer reads: `BEZR` (10 bezier vertices, 10 m apart) and `SEGM` (one plain segment per vertex), big-endian. Not a complete NoLimits 1 file: no supports, no other blocks. |
| `pointlist.txt` | Pointlist (`importAsTxt`) | 47 points 2 m apart, one per line as `z x y`, no newline after the last point. |
| `nl2.csv` | NoLimits 2 CSV (`core/secnlcsv.cpp`, `loadTrack`) | A header line, then 94 tab-separated rows 1 m apart: number, position, front, left and up vectors. |

## The check

`py -3 tools/run-baseline.py --imports` runs, for each input,

```
FVD.exe --batch-import <nltrack|txt|csv> <input> <outdir>
```

which starts from the default project, imports the file into a new track exactly as the menu entry does after its file dialog, and writes the three exports of the new track, the re-saved project and a report (what the section list shows, the undo list, the Undo and Redo menu entries). Each file is compared byte for byte with `<stem>.ref.<kind>` here.

The references were recorded on 2026-11-10 with a program rebuilt from the fork (the commit that added the batch import, MSYS2 UCRT64, GCC 16.2, Qt 5.15.19), not with the 2016 binary: they say "the import does what it did before this change", not "what the 2016 program did".

`pointlist.txt` has no references: the point-list importer leaves some values of the first and last point unset (their roll, their flags and their speed), so its exports change from one run to the next. The check only makes sure it imports without error.

This folder is marked in `.gitattributes` so that git never converts its line endings.
