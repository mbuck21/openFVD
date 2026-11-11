# 0010: The plan reworked around seven shared pieces, for FR-10 to FR-18

- Status: accepted
- Date: 2026-11-10
- Needs owner approval: no for the structure (how to build it and what stands on what; the architect's call under record 0006). **The order of the requests is the owner's:** the roadmap's order is a recommendation, and the roadmap lists what can move.

## Context

- The first roadmap was written for nine requests. Since then the owner added FR-10 (parametric elements, issue 28) and FR-11 to FR-18 (issue 30): the train as a train, close the circuit, energy margin, ride rules, import and convert, launches and special track, comfort, the ride sheet. FR-10 had been fitted into the old stages; FR-11 to FR-18 were in an issue only.
- Stage 1 is finished: the core runs without a window, the train catalogue exists, and research notes R1 to R6 are in.
- Read together, the old and new requests lean on a small number of things that do not exist yet:
  - five of them need a number worked out along the track and shown or compared with a limit (FR-3, FR-4, FR-13, FR-17, FR-18);
  - four need a solver (FR-7's pinned points, FR-10, FR-12, FR-15);
  - five need to know how a train moves over a track whose shape is already fixed (FR-11, FR-13, FR-15, FR-16's swing launches, FR-6), which the program cannot do: it only shapes a track from forces, at the one speed the design assumes;
  - seven additions want new fields in the saved file (pins, elements, ride rules, end-at-a-value transitions, launches and brakes, frozen elements, switches).
- The first roadmap had the solver inside one large piece (W2) in Stage 4, the special sections in the last stage with the block logic, and the analysis framework as findings only.

## Decision

1. **The plan names seven shared pieces and builds each once, before what stands on it:** the window-less core and the train data (done), the command list (T1), measures and findings (A1), the solver (G1), riding the finished track (P1, P2), and one way to add to the saved file (F4).
2. **A1 is widened from "findings" to "measures and findings".** A measure is a value at every point of the track, computed beside the nodes and never changing them. The along-track force is the first new one. The ride sheet (FR-18) is A1's first user and its proof, replacing the "one trivial check" of the first plan.
3. **The solver is a piece of its own, earlier.** W2 is split into G1 (the solver), G2 (pinned points), G3 (keep shape or keep forces) and G4 (transitions that end at a value), following R5's shortlist. G1 moves to Stage 3: it writes its results into the ordinary boxes, so it needs no change to the saved file. E1 (Stage 2) carries R5's two trials as well as R6's, so the solver is measured on the real core before it is designed.
4. **Close the circuit (FR-12) is the first slice of pinned points,** with the station as the pin, not a tool of its own.
5. **Riding the finished track is a second computation beside the design computation,** first with a point (P1), then with a train (P2). Its test is that it reproduces the design's own speeds and forces when given the design's own settings.
6. **The energy margin (FR-13) is measured on the finished shape, with P1.** Re-running the design with other friction, as issue 30 first suggested, would reshape every force section and so test a different ride. This moves FR-13 from "first" to Stage 3.
7. **Swing launches are designed for one pass and checked on all of them.** A force-vector design fixes the forces for one speed at each point; the other passes are ridden with P2.
8. **Nothing changes what a project computes or how it is saved before Stage 4.** Stage 4 opens with F4, one mechanism of tagged blocks for every later addition. Recommended to the owner: additions that do not change the computed track stay readable by the 2016 program; a file using an addition that changes the track gets a new version, so the 2016 program refuses it instead of computing a different ride. The owner decides (rule 4 in `CLAUDE.md`; the roadmap's question 6).
9. **Lifts, launches, brakes and trims (P3) are pulled forward from the last stage to Stage 4;** stations, transport and blocks stay with the operations stage.
10. **The new requests get experiments before designs,** as FR-10 did: a second research pack (R7 to R11) runs in Stage 2 beside the foundations.
11. **Seven stages instead of five.** 2: reach and measure. 3: see the ride, aim a section. 4: hold the ride together. 5: elements and rules. 6: the train. 7: operations and special track.

## Where this differs from the order suggested in issue 30

- FR-13 is in Stage 3, not first (point 6).
- FR-16's lifts, launches and brakes are in Stage 4, not second: they are the first new physics in the design and the first addition to the saved file, so they wait for F4 and the owner's choice about the file. They can move to Stage 3 at the owner's word, and F4 with them.
- FR-17 is earlier (Stage 3): it is measures only.
- FR-15's first step (the forces of an imported track) is earlier (Stage 3): it falls out of P1.
- The rest is as suggested: FR-18 first, FR-11's experiment early, FR-14 with the elements, FR-12 on the solver, the train, the fit and the special track last.

## Where enforced

- `docs/roadmap.md` (rewritten) and `docs/feature-requests.md` (FR-11 to FR-18 added).
- `STATUS.md`, "Focus" and "Waiting on the owner".
- Piece names A1 to A4, E1 to E3, T1 to T3, O1 to O3, F2, F3 and W1 keep their meaning from the first roadmap, so the research notes and issues that name them still read correctly. W2 no longer exists; where a note says W2, read G1 to G4.
