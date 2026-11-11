# Investigation projects

Small synthetic projects made to test one explanation each. They are not part of the baseline and `tools/run-baseline.py` does not read them. Each is a baseline project with a few numbers changed in the file (offsets counted from 0; the `.fvd` stores numbers big-endian, `docs/codebase-map.md`, "Saved project file"). None contains anything from the owner's own projects.

## N1: negative tension (`docs/research/n1-difference-from-2016-on-an-owner-project.md`)

| File | Made from | Changed |
|---|---|---|
| `n1-tension-force-chain.fvd` | `samples/baseline/gen-normal-shapes.fvd` | Byte 390: the change of the first force section's third normal-force transition, -0.5 to -2.5 (g). Byte 398: its tension, 0 to -1.4. |
| `n1-tension-loop.fvd` | `samples/baseline/four-sections.fvd` | Byte 413: the tension of the force section's normal-force transition, 0 to -1.2. |

A negative tension makes the program call `asinh` on a `float`. The rebuilt program's C runtime gives a slightly different last bit from the 2016 binary's for about a quarter of the arguments, and in these two projects (as in the owner's) that bit survives into the track. What the rebuilt program writes for them today, and what the research note predicts the 2016 binary writes, are in section 5 of the note, with the experiment to run on the 2016 binary and what each outcome would mean.

Once the 2016 binary's files for these projects exist, they can become baseline projects of their own.
