# FVD++, mbuck21's fork

This is a fork of [altlenny/openFVD](https://github.com/altlenny/openFVD), Stephan "Lenny" Alt's FVD++ v0.79: a force vector design tool for roller-coaster track that exports to NoLimits 1 and 2. The original project was last changed in 2023. Its own README follows below, unchanged.

## About this fork

I'm a roller coaster enthusiast and a mechanical engineer, not a software developer. I use FVD++ to design coasters for NoLimits 2, and I am using [Claude Code](https://claude.com/claude-code) to keep the program working and to extend it. I decide what the program should do and try the results; nearly all of the code and documentation changes here are written by Claude, with every change checked against the original program's output before it is merged.

It is a hobby project and a work in progress. There is no warranty, and nothing here is an engineering tool for real rides.

## What the fork is trying to do

- **Keep FVD++ building and running** on a current Windows PC.
- **Make force vector design faster without giving up detail.** This is the main goal: less rework when an element early in the ride changes, and tools that hit a target (a height, a heading, a closed circuit) without trial and error.
- **Check a design while it is being made:** minimum curve radius, and ride forces against the published limits, first at the heartline and later at every seat of the train.
- **Know about trains:** heartline heights and other data per train type, so the designer picks a train and does not type numbers.
- **Let the program be driven without clicking,** so it can be tested automatically and an AI agent can help build a layout.
- Further out: stations, lifts, brakes and blocks, and a throughput estimate.

The plan and its order are in [`docs/roadmap.md`](docs/roadmap.md); what was asked for is in [`docs/feature-requests.md`](docs/feature-requests.md); where things stand today is in [`STATUS.md`](STATUS.md).

## What is different from the original so far

**Fixed**

- **It builds again.** One command builds the program with a current toolchain (MSYS2 UCRT64: GCC 16, Qt 5.15, glm 1.0, GLEW 2.3). The original pointed at library folders on its author's PC and no longer compiled against current glm. See [`docs/building.md`](docs/building.md).
- **Nine defects that a current compiler found** in the original code, among them a function in the NoLimits 2 CSV import that returned no value and used a counter before setting it, an export file name that was read after its memory had been released, and several pointers used before they were set. The fixes change no output: the exports are byte for byte the same before and after.
- **Undo works for every edit of a track.** The original had no undo for some edits (the smoothing dialog, colours, renaming) and a wrong one for eight others. Every edit is now a named command that records its own undo step, and undo puts the design back exactly. A number typed digit by digit is one step, and the Edit menu names the step it will undo.
- **What is on screen is what a reopen gives.** In the original an edit could leave the track on screen different from the one that saving and reopening produced (by up to 12 m in one measured case). Every edit now ends in the reopened state. Cancel in Track Properties and in the smoothing dialog really cancels.
- **Checked against the 2016 release, and made to match it exactly.** A rebuild with a current toolchain came out a few centimetres different on a project with a negative transition tension, because one maths function now comes from Windows where the 2016 program carried its own. The fork computes it exactly as the 2016 program did. On every project tested, the exports and the re-saved project are identical to the 2016 program's. See [`docs/testing.md`](docs/testing.md).

**Added**

- **A batch mode:** `FVD.exe --batch-export project.fvd outdir` writes the NoLimits 2 and NoLimits 1 exports with no window and no clicking, and `tools/run-baseline.py` compares them with the reference files in one command.
- **A train catalogue** ([`data/traintypes.json`](data/traintypes.json)): NoLimits 2's train styles and the main models of Mack, Vekoma, B&M, Intamin and RMC, each heartline with its source and how sure it is. Most values are not independently verified and say so.
- **A "Train type" list in Track Properties** that fills in the heartline height from the catalogue.
- **The model runs without a window.** The track model, the physics, the project file and the exporters no longer depend on the program's windows; a small console program built from them alone loads, computes, exports and saves with identical results. Nothing a user sees changes; it is the foundation for the checks and the automation that come next.
- **A command-line program, `fvdcore`** ([`cli/README.md`](cli/README.md)): export, a table of points, compare two versions of a project, the ride's key numbers, measures and checks, all with no window; and a session mode in which another program holds a project open and edits it through the same named commands the windows use.
- **The project as readable text** (JSON), written and read back with nothing lost, beside the unchanged `.fvd` file ([`docs/project-text-format.md`](docs/project-text-format.md)).
- **Measures and findings:** numbers worked out along the track without changing it (the along-track force, force changes, roll rate and more), drawn as curves on the plot and as colour on the 3D track.
- **A ride sheet:** length, height, biggest drop, duration, top speed, seconds of airtime, peak g in each direction and inversions, on a panel that updates while editing.
- **A safety net in one command:** eight test projects compared with the 2016 program, an undo and redo round trip, a check that every command leaves the track as a reopen would, the importers, and automated tests (`tools/run-baseline.py`, `tools/run-tests.cmd`).
- **Research notes** behind the planned features in [`docs/research/`](docs/research/): heartlines, minimum radii, ride-force limits (ASTM F2291 and EN 13814), NoLimits 2 block logic, how force vector design could be made faster, parametric elements, how much a whole train differs from the single point the program moves, friction and margins, launches, lifts and brakes, comfort, and trials of a goal-seeking solver on the real program.
- **Documentation for working on the code:** a map of the code base, a glossary, how to build and test, and a record of the decisions taken (`CLAUDE.md`, [`docs/`](docs/README.md)).

**Not maintained here**

- The macOS build. The files are still in the tree, untouched.
- The `.3ds` model export on Windows is left out of the default build (it needs a library that is no longer packaged).

The files under `bin/` are still the original author's 2016 release, kept as the reference the rebuilt program is compared with.

## Licence

GPL-3.0, as the original. See [`LICENSE`](LICENSE).

---

*The original README:*

```
####################
# open FVD++ v0.79 #
####################

Stephan (Lenny) Alt, 2012-2016, alt.stephan@web.de

#########
# Intro #
#########

Welcome to FVD++, a next generation FVD tool inspired by newton2 (by Keith (Entropy) Cohn).
It is mainly aimed for coaster enthusiasts creating sophisticated force and geometry based coasters.
That said the tool is still development and I take no warranty of its usefulness in its current state (see License information below).

#############
# Changelog #
#############

v0.79:
- added nolimits 2 csv import
- fixed renderer bugs when importing nl1 track or nl2 csv files on osx
- fixed compilation errors on clang
- reverted performance improvements due to fatal bugs

v0.78:
- fixed export of straight sections with non zero roll transitions
- fixed camera stuttering
- fixed distance based forced sections
- first 64bit Release

v0.77 (open Source release):
- fixed export of individual sections
- fixed force based sections not saving constant speed
- changed to Qt 5.2.1, qcustomplot 1.2.1 and glm 0.9.5.1-1

v0.76 Beta:
- fixed tozero transitions to only work on the first section properly
- fixed velocity problems on some section types
- fixed NL2 exporter exporting non rotated sections (when exporting a subset of FVD sections)

v0.75 Beta:
- double spine track style
- new exporter type: NL2 exporter
- NL2 style friction parameters
- EXPERIMENTAL: export coaster to .3ds file format
- EXPERIMENTAL: "roll back to zero" transition type for force and geometric time based sections

v0.7b Beta:
- fixed having no control over the camera in OpenGL 2 mode
- FVD now loads a double-clicked project on startup if the .fvd file format is asociated with the FVD++ executable
- fixed camera having gimbal lock related issues

v0.7a Beta:
- fixed unsufficient mesh update in combination with smoothing
- fixed spine and rails disappearing at the end of the track on rare occasions

v0.7 Beta:
- added fully rendered tracks as well as several track styles
- improved drag and speed calculations
- new keybindings during fly mode (change POV position by pressing the arrow keys, B toggles the NoLimits 1 building border, F goes into fullscreen)
- several graphics options
- straight segments are now exported as one NoLimits 1 segment (please tell me if you really need the old system back)
- performance increase in track generation
- imported NoLimits 1 tracks have now simulated physics
- transition editor with mutliple y-axes

v0.6 Beta:
- added a rollspeed smoothing tool
- improved both tangent and spline exporter by a significant margin
- improved coaster physics (you can load 0.5x .fvd files but your track may be misshapen due to the new physics engine)

v0.51 Beta:
- fixed two bugs leading to crashes on most Mac and some Windows systems
- minor bug fix in the options menu and section list

v0.5 Beta:
- all new GUI
    - project tab lets you manage multiple tracks in one project
    - each track has its own management tab
    - improved graph window with tons more options
- import tracks from other FVD++ projects
- import from *.nltrack format (for now FVD++ assumes a closed track and calculates no physics)
- customizable background and track colors
- customizable ground texture (only viewable in OpenGL 3 mode)
- additional anchor point options
- improved track flexion visualization

v0.31 Beta:
- better Mac compatibility (enabled earlier OpenGL versions and removed some external dependencies)
- fixed issue with symmetric functions

v0.3 Beta:
- improved visuals for non legacy OpenGL (improved shadows and selected subsection gets a green highlight)
- added undo / redo support
- added options window
- added conversion panel window
- added roll orientation option
- added function argument option
- fixed spinning camera bug which occured on some systems
- reworked tangent exporter (new standard exporter for now)
- new Mac version (now also with OpenGL 3 support)

v0.14a Alpha:
- fixed floor in PoV mode (legacy OpenGL only)

v0.14 Alpha:
- trackwork now invariant under position and rotation changes of the anchor point
- improved visuals for non legacy OpenGL (underground track distinguishable)
- Anchor Node represents now the anchor of the track rather than the anchor of the Heartline

v0.12 Alpha:
- Mac version (64 bit only)
- new exporter option (export as spline or export with preserving tangents)
- fixed a slight bug in straight sections using non zero roll functions (section didn't preserve length of heartline)

v0.11 Alpha:
- new exporter option (threshold angle at which the exporter decides to switch to RelRoll option)
- Compatibility with OpenGL version 3.0 and lower. Visualizations will automatically be disabled! Check *.log file which OpenGL context could be created on your machine
- Fixed a bug on functions the train will never be able to reach

v0.1 Alpha:
- introduction of 4 different section types (straight, curved, force based, geometry based)
- introduction of 6 different transition types (with 12 different flavors)
- timewarping of all transition types
- Exporter to the *.nlelem format with per section exporting and variable segment lengths
- save to and load from a lightweight data format (*.fvd files) with autosaves to a (*.bak) file each time an export is performed.
- OpenGL shaders visualising important data (like forces, roll speed and track flexion).
- PoV mode enabling the ability to check the track inside FVD++


###################
# Having Trouble? #
###################

#### I can't use visualizations ####
- The visualizations use OpenGL 3.1 shader technology, make sure your graphics card / drivers support OpenGL 3.1


#### I don't know how to do XY ####
- read through the official documentation, this document should explain nearly every feature of FVD++
- Visit the FVD++ Thread on NoLimitsExchange (http://forum.nolimits-exchange.com/)


#######################
# Found a Bug / Crash #
#######################

Please let me know if you found a bug / crash either by posting in the NLE forum or by mailing me (alt.stephan@web.de).
Please include as much information as possible, try to save the track that bugged out and send me the saved .fvd file.
If the program crashed try to recreate the crash scenario and describe how you got the program to crash in a detailed way.


#############
# Thanks to #
#############

- Lucas van den Bosch for creating the official documentation and youtube tutorial channel
- Ercan "geforcefan" Akyürek for deploying and testing the tool on Mac systems
 

#######################
# License information #
#######################

FVD++, an advanced coaster design tool for NoLimits and NoLimits2
Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
```
