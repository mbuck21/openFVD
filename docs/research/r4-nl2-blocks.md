# R4: NoLimits 2 blocks and block scripting

Research for FR-6 (throughput simulation with blocks, roadmap pieces O1, O2, O3). All web sources were accessed on 2026-11-10. Confidence is high, medium or low per claim. "Help" means the official NoLimits 2 help at `https://nolimitscoaster.com/nolimits2/help/`.

## Summary

1. NoLimits 2 (NL2) has a small set of operation sections (station, lift, brake, transport, storage, switch, transfer table). The built-in Automatic Block Mode is simple: one train per block, a train moves on only when the next block is clear. Its parameters are documented, but mostly without units.
2. Custom block logic is a Java-like script that implements `BlockSystemController` and drives `Section`, `Block` and `Train` objects frame by frame. In scripted mode the automatic block mode is off, so the script must redo everything it did. The API is documented publicly; the language is not Java and has no stated licence.
3. The `.nl2elem` file FVD++ writes carries geometry only (vertices and roll points). It cannot mark a section as station, lift or brake. The `.nl2park` format is not public. So FVD++ cannot hand NL2 a ready-made operating layout; the owner would set up sections inside NL2.
4. Recommended: option (c), NL2's standard automatic block behaviour and section parameters reproduced in FVD++, with no scripting. Option (b) (a small fixed API subset) can be added later if the owner wants it. Option (a) (run NL2 scripts unchanged) is very large and cannot be verified well.
5. For Stage 5: rescope FR-6 to "simulate NL2's automatic block system and report riders per hour", and treat "the same custom script runs in both programs" as out of reach for now. The owner decides (roadmap decision 5).

---

## 1. How NoLimits 2 models operation

### Section types and parameters

Sources: Help [Editor](https://nolimitscoaster.com/nolimits2/help/pages/editor.html), [Track Tab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html), and the per-section pages linked below. Units are not stated on any of these pages (high confidence that they are not stated).

| Section | Is it a block? | Parameters the help lists | Source |
|---|---|---|---|
| Station | Always a block (high) | Wait time: average, minimum, maximum, deviation (a random draw per dispatch). Shuttle mode with a pass counter. Extra block length (moves the block border past the section end). Brake device (friction or magnetic, deceleration). Transport device (type, speed, acceleration, deceleration), optional launch with its own acceleration and a maximum speed. Synchronised dispatch across stations. Unloading-only. | [stationsettings](https://nolimitscoaster.com/nolimits2/help/pages/stationsettings.html), [tracktab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html) |
| Lift | Usually a block; a "No Block" mode makes it behave like a transport (high) | Speed, acceleration, deceleration, drive device (chain, friction wheels, none), anti-rollback, shuttle gentle second-pass release, dive-coaster drop release, extra block length. If the next block is not clear the train stops near the top (medium: the Track Tab page says so; the exact stopping point is not defined). | [liftsettings](https://nolimitscoaster.com/nolimits2/help/pages/liftsettings.html), [tracktab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html) |
| Brake | Block by default; "Trim Brake" mode takes it out of the block system (high) | Mode (block or trim), complete stop (checkbox), wait time after a complete stop, speed limit, hysteresis (brake engages above limit plus hysteresis, releases at limit minus hysteresis), deceleration, optional transport device (speed, acceleration, deceleration), launch (acceleration, maximum speed), trigger position along the section, trigger car (first or middle), extra block length (block brakes only). | [brakesettings](https://nolimitscoaster.com/nolimits2/help/pages/brakesettings.html) |
| Transport | Not a block and cannot stop a train (high) | Speed, acceleration, deceleration, device type; shuttle mode with pass counter, maximum speed, "speeding down". | [transportsettings](https://nolimitscoaster.com/nolimits2/help/pages/transportsettings.html), [tracktab](https://nolimitscoaster.com/nolimits2/help/pages/tracktab.html) |
| Storage | Not part of the block system in Automatic Block Mode; the simulation still tracks whether it is occupied, and automatic mode does not move trains in or out (high) | Optional transport device and type, generated building. | [storagesettings](https://nolimitscoaster.com/nolimits2/help/pages/storagesettings.html) |
| Switch track | Not stated (low) | Default and current track, direction set (left/right etc.), style (shift or rotate). The page says nothing about control or blocks. | [switchtracksettings](https://nolimitscoaster.com/nolimits2/help/pages/switchtracksettings.html) |
| Transfer table | Transfer brake sections "always function as block brakes" (high) | Number of tracks, default and current track, length, distance between tracks. | [transfertablesettings](https://nolimitscoaster.com/nolimits2/help/pages/transfertablesettings.html) |

Launch and LSM are not separate section types. They are options on stations, brakes and transport sections ("Launch" checkbox, LIM/LSM device type). Confidence high; same pages as above.

"Trim" is the Trim Brake mode of a brake section, plus a scripting call `setBrakesTrim()` ("brakes switch automatically based on train speed", [Section API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Section.html), high).

### Automatic Block Mode

- Operation modes in play mode: Automatic (default), Manual Block, Full Manual. In Manual Block, trains do not advance until the operator presses the block's button, and all trains must be stopped before the mode takes effect. Manual Block is not available for shuttle coasters. ([Play Mode](https://nolimitscoaster.com/nolimits2/help/pages/play.html), high)
- Block status light: off when empty, on when occupied, flashing when a train approaches. (Same page, high)
- Station dispatch: a train leaves only when gates and harnesses are closed, the platform is lowered (some styles), seats are locked (some flying styles) and the next block is unoccupied; dispatch is automatic or manual per station. (Same page, high)
- Only one train per block, and the help advises at least one more block than trains. ([Editor](https://nolimitscoaster.com/nolimits2/help/pages/editor.html), high)
- Where a block's border sits: by default at the end of its section, shifted outward by "Extra Block Length" (for example, past the top of a launch). (Station, lift and brake pages above, high.) The help does not define the rule for where blocks begin and end beyond this (low that anything more is known; see section 6).
- Block violation: the official help does not describe it. A Steam user report says that with 2 blocks and 2 trains, advancing a train by hand produced a "block violation" warning, and adding a block brake fixed it ([Steam thread](https://steamcommunity.com/app/301320/discussions/0/2333276539602423517/), low: single user report, secondhand through a search summary). The scripting API has a per-block switch for a "multiple trains on a block" warning ([Block API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Block.html), high), so the warning exists and is a warning, not a documented enforced stop.

---

## 2. Scripted block logic

### Language and tooling

| Fact | Source | Confidence |
|---|---|---|
| The language is run by the NoLimits Virtual Machine (NLVM). Source files are `.nlvm` text, one class per file, file name equal to class name. | [Scripting basics](https://nolimitscoaster.com/nolimits2/help/pages/scriptingbasics.html), [File formats](https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html) | high |
| Syntax is Java-like, but it is its own language: no generics, no exceptions, no enums, no `goto`, no nested or anonymous classes, no threads inside one script, a much smaller standard library (math, vector, hash map, string tools, `nlvm.lang`, `nlvm.util`, `nlvm.math3d`). | [Scripting language](https://nolimitscoaster.com/nolimits2/help/pages/scriptinglanguage.html), [API index](https://nolimitscoaster.com/nolimits2/help/api/index.html) | high |
| There is no separate compile step: the sources run directly. (The task brief said "compiled by the built-in compiler"; the help says otherwise. The VM may compile internally; the help does not say.) | [Scripting language](https://nolimitscoaster.com/nolimits2/help/pages/scriptinglanguage.html), [Introduction](https://nolimitscoaster.com/nolimits2/help/pages/scriptingintroduction.html) | high that the docs say this; low on internals |
| Scripts attach to a coaster in Coaster Properties, a `.nl2script` description file (XML) is optional for play-mode scripts. | [Scripting basics](https://nolimitscoaster.com/nolimits2/help/pages/scriptingbasics.html) | high |
| API documentation is HTML in the install folder, and public at the vendor site (`.../nolimits2/help/api/index.html`). A community mirror exists at `nl2docs.arborarcade.com`. | [API index](https://nolimitscoaster.com/nolimits2/help/api/index.html), [mirror](https://nl2docs.arborarcade.com/api/index.html) | high |

### Classes a block script uses

| Class | What it does | Source |
|---|---|---|
| `Script` | Base class. Hooks: `onInit()`, `onLateInit()`, `onNextFrame(float tick)` (called every frame, `tick` = seconds since last call), `onLateUpdate()`, `onExit()`. Field `sim` gives the simulator API. | [Script API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Script.html) (high) |
| `BlockSystemController` (interface) | Callbacks from the control panel: `onAutoMode`, `onManualBlockMode`, `onFullManualMode` (coaster), `onAdvanceFWDButton`, `onAdvanceBWDButton` (block). Registered with `Coaster.setBlockSystemController()`, only valid in scripted operation mode. | [API page](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/BlockSystemController.html) (high) |
| `Coaster` | `getBlock(name)`, `getSection(name)`, `getTrainCount()`, `getTrainAt(i)`, `findNearestTrain`, `getBlockSystemMode`/`setBlockSystemMode`, `isScriptedOperationMode`, `requestReset`. | [Coaster API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Coaster.html) (high) |
| `Block` | Per-block panel state: name, section, number of trains on it, lamp state, a custom integer state registered with display text (`registerState`, `setState`), show/enable the FWD/BWD buttons, semi-manual move helpers, multiple-trains warning switch. It has no "is next block clear" method; the script works that out. | [Block API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Block.html) (high) |
| `Section` | 71 methods. Queries: is station, lift, brake on, brake complete stop and wait time, trains on section, train before/past centre, brake trigger, lift trigger, distance from start or end. Commands: brakes off / on / trim; lift off / forward / backward / idle; transports off / standard / launch / dependent on brake; station entering/leaving, next-block clear or occupied, manual dispatch, gates, harness, platform, seat lock. | [Section API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Section.html) (high) |
| `Train` | Read-mostly: speed (negative backwards), car and bogie count and matrices, train index, harness state, lashed-to-track flag, onboard sound. No command to drive the train; motion comes from the devices on sections. | [Train API](https://nolimitscoaster.com/nolimits2/help/api/com/nolimitscoaster/Train.html) (high) |
| Others | `SpecialTrack`, `TrackPos`, `TrackTrigger` and listener, `Park`, `Entity`, `Tools`, sound and video classes. Not fetched; their content is unknown to this report. | [API index](https://nolimitscoaster.com/nolimits2/help/api/index.html) (high that they exist) |

### Shape of a typical custom block script

(From the API above and a forum description; no full script was read. Medium confidence.)

- The class extends `Script` and implements `BlockSystemController`; in `onInit()` it looks up blocks and sections by name and registers itself with the coaster. ([Search result pointing to a community script](https://pastebin.com/tKCCpXAE), low: not opened.)
- A state machine per block (idle, train arriving, waiting for next block, releasing, and so on) is stepped from `onNextFrame`. Custom states are registered so the panel shows readable text.
- Each frame, for each block, the script reads which trains are on its section and where (before or past centre, brake trigger, lift trigger), decides whether the next block is free, then calls the section commands: brakes on or off, lift or transport on, station "leaving", "next block clear". It also sets switches and transfer tables by the same kind of calls.
- Scripted mode switches Automatic Block Mode off, so the script has to reimplement what automatic mode did: check the next block, hold or release brakes, poll until clear, decide dispatch. One forum poster reports about 500 lines for one train leaving a station and stopping at a block brake, and an estimated 1,000 to 1,500 lines for an automatic transfer table ([Hard truths about scripting](https://coastercrazy.com/forums/hard-truths-about-scripting-t32467.html), medium: one person's account).

### Well-known examples

The brief mentioned shuttle and switch examples shipping with NL2. Not confirmed: the help pages fetched do not name any shipped example, and searches found a community "script park", a community GitHub collection with no block example seen ([ryandougherty/Nolimits-2-Scripts](https://github.com/ryandougherty/Nolimits-2-Scripts), low), a community script generator for block logic ([NoLimits Central](https://nolimitscentral.com/forum/topics/4520), low), and the statement that a plain shuttle coaster needs no script, only the shuttle mode ([Steam help thread](https://steamcommunity.com/app/301320/discussions/0/35221584442377211/), low).

### Licence terms

- The scripting introduction states no licence for the language or API, only trademark notices for Java and Oracle ([Introduction](https://nolimitscoaster.com/nolimits2/help/pages/scriptingintroduction.html), high that nothing is stated on that page).
- The File Formats page states the `.nl2park` structure "is not public" and says nothing about third-party tools ([File formats](https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html), high).
- No end-user licence agreement was found or read. Whether reimplementing the API surface in another program is permitted is therefore unknown (see section 6). The scripts users write belong, as far as anything found says, to their authors; nothing was found on sharing terms.

---

## 3. File formats

| Question | Answer | Source | Confidence |
|---|---|---|---|
| Is block/section information in `.nl2park`? | Almost certainly yes (coasters are stored there), but the structure "is not public and may change". The page does not mention blocks or sections. | [File formats](https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html) | high (for what the page says) |
| `.nl2pkg` | A ZIP-compatible bundle: a park file, its resources and an `index.xml` (park path, preview, author, description, encryption status). The park file and resources may be encrypted. | same | high |
| Could FVD++ read or write blocks from outside? | Writing: no documented way. Reading: only by reverse engineering a format the vendor says is private and changeable. A third-party C++ library for NL1 and NL2 files exists ([libnolimits](https://github.com/geforcefan/libnolimits)); whether it handles NL2 park sections was not checked. | [libnolimits](https://github.com/geforcefan/libnolimits) | low |
| What does `.nl2elem` hold? | XML with "vertex and roll point data" and an embedded preview, no references to other files. | [File formats](https://nolimitscoaster.com/nolimits2/help/pages/fileformats.html) | high |
| What does FVD++ write? | In `core/track.cpp` (`exportNL2Track`, writes the element body) and `ui/exportui.cpp` (`doNL2Export`, writes the wrapper), the file is `<root><element><description>FVD++ Export Data</description>` followed by `<vertex>` entries (x, y, z, and an optional `<strict>`) and `<roll>` entries (up vector, right vector, coordinate, strict). Nothing else is written. | code, this repository | high |

Conclusion: an element file carries geometry only. It cannot mark a section as station, lift, brake or transport, and no such tag appears in the format list. (The format page lists XML formats by content and none for an element carries section types; high for what is documented. A hidden extra tag in NL2's own exports was not checked against a real file: see section 6.) After import, the owner would set section types in NL2's editor ("Track Tab" section settings). So O1 can model and simulate station, lift and brake sections in FVD++, but its "exported to NL2 as such where the format allows" can only mean a report or a settings list the owner types in.

---

## 4. Feasibility of "the same script runs in both"

| | (a) Run NL2 scripts unchanged | (b) Defined API subset, scripts ported by hand | (c) Standard automatic behaviour and section parameters only; layout exported, real script runs in NL2 |
|---|---|---|---|
| What must be built | An interpreter for NLVM (own grammar, smaller library) and a reimplementation of every API class a script touches (`Script`, `Coaster`, `Block`, `Section`, `Train`, vectors, hash map, strings, triggers), plus a train and device physics model that behaves like NL2's so the scripts' decisions (positions, triggers) happen at the same moments. | A small interpreter or an FVD++ native scripting layer exposing the calls in section 2 that a block controller needs (about 20 `Section` calls, a handful of `Block` and `Coaster` calls, `Train` speed), with a written list of what is supported. | Section kinds with the parameters in section 1; a block graph; a block-clear rule; station dispatch with the wait-time draw; per-train motion on each section type. |
| Rough size | Very large (many months of work; the API has 71 `Section` methods alone and 20+ other classes, and has grown across releases: `Train.getCoaster` is "since 2.5.7.0"). | Large (a few months; more than (c) by the interpreter). | Medium (the bulk of O1 and O2, about weeks to a few months); this is needed under (a) and (b) too. |
| Main unknowns | The exact language semantics (only prose is available); how closely NL2's internal physics, trigger positions and station handler must be matched; API additions in later releases. | Which calls real scripts actually use (needs a sample of real scripts); porting effort per script. | NL2's exact automatic-mode rules (block borders, release timing, what a lift does when blocked); units of every parameter (the help gives none). |
| Legal and licensing | Reimplementing a vendor's API and VM: no licence terms were found either way; documentation is public but no permission statement. Ask the vendor before shipping. Risk of being read as copying. | Same, smaller surface. | Lowest: only observed behaviour and published setting names are used. |
| How to test equal behaviour | Run the same script in NL2 and here and compare event logs (block entry and exit times, dispatch times) on several layouts. Needs NL2 and the owner's hands, because there is no way to run NL2 headless that was found. | Same comparison, on the subset. | Build a test coaster in NL2 in automatic mode, record block and dispatch times (by hand from the panel or a one-off script that logs them), and compare with the FVD++ run within a stated tolerance. |

Recommendation: **(c)**. It delivers the owner's real aim, riders per hour for a layout, at a size that fits Stage 5, uses nothing that needs the vendor's permission, and leaves a path to (b) later. "The same custom script works in both programs" should be dropped from FR-6 or marked as a later extension; a custom script only matters for layouts that NL2's automatic mode cannot handle (transfer tables, complex switching), and those are the layouts hardest to simulate exactly. Choosing between (c) and a bigger option is the owner's call (roadmap decision 5).

If (b) is wanted later, first collect several real scripts from the owner to see which calls they use. Do not start (a).

---

## 5. Throughput

Standard estimate (general engineering practice, not from NL2 sources; medium confidence, from the usual definition):

- Theoretical capacity (riders per hour) = 3600 / dispatch interval (seconds) x riders per train.
- Dispatch interval is the largest of: the station cycle (unload, load, restraint checks, plus the dispatch wait), the time a train needs to clear each block (travel time across the block plus brake hold and release time), and the minimum headway that keeps one train per block. The slowest block sets the pace.
- With N trains on M blocks, N must be below M (NL2's help gives the same rule: at least one more block than trains, [Editor](https://nolimitscoaster.com/nolimits2/help/pages/editor.html), high).
- Real throughput is below capacity because of rider loading variation, stops and breakdowns.

Inputs a simulation needs:

| Input | Where it comes from |
|---|---|
| Track geometry, speeds along it | FVD++ already computes this |
| Section kind and parameters (section 1) | O1 |
| Train: number of cars, length, riders per train | FR-4 part 2 / A4 train model |
| Number of trains and starting blocks | user |
| Station times: average, minimum, maximum, deviation, per-dispatch checks | station section settings |
| Block borders and extra block length | O1 / block editor |
| Brake complete-stop wait time, speed limit, hysteresis, deceleration | brake settings |
| Lift and transport speeds, accelerations | lift and transport settings |

What NL2 reports: the play-mode pages and the telemetry server list speed, position, g-forces, braking state and station state, but **no throughput, rider counts or block occupancy statistics** ([Telemetry](https://nolimitscoaster.com/nolimits2/help/pages/telemetry.html), [Play Mode](https://nolimitscoaster.com/nolimits2/help/pages/play.html), medium: absence on the pages fetched, not a search of the whole program). So there is no NL2 number to compare riders per hour against; comparison has to be on times (block entry, dispatch) recorded by hand or by a script.

---

## 6. Could not establish

- Exact rules of Automatic Block Mode: where block borders go for each section combination, release timing, behaviour on a block violation. The help pages fetched describe none of this, and the "Block explanation" page referred to from the Editor page was not found.
- Units for every numeric setting (speeds, accelerations, wait times, extra block length). The help does not state them.
- Whether the help documents "launch" or "LSM" as separate section types: it does not, they are options on other sections.
- Switch track control: the switch and transfer table help pages say nothing about how they are controlled or how they relate to blocks (except that transfer brakes are block brakes). The `Section` API fetched has no switch call; the switch and transfer table calls may be on `SpecialTrack`, which was not read.
- Whether NL2 ships example block scripts: not found. Contents of the community scripts: not read.
- The licence or end-user terms for NL2, its API documentation and user scripts; whether an independent implementation of the API is allowed. Not found in the pages read. The vendor would have to be asked.
- The internal structure of `.nl2park` and how blocks are stored there, and whether the NL2 element exporter or importer carries any section type tag beyond vertex and roll. Not tested against a real NL2 export.
- Whether NL2 compiles scripts internally (the docs say "no compile step").
- Whether scripting or multi-train play depends on the Standard or Professional edition. The Professional page found mentions only 360 degree video.
- Details of API classes not fetched (`SpecialTrack`, `TrackTrigger`, `Tools`, `Park`) and of the full `Coaster` class.
- Two search results were used only at the level of the search summary (the Steam "multiple trains on a block" thread, the pastebin script). They are marked low.
