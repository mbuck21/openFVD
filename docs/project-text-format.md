# The project as text

A coaster can be written as a text document beside the binary `.fvd` file: JSON, readable and comparable by a person and by an agent, and writable by either (feature request FR-5). The binary format does not change. The text is made from the loaded model and read back into it with the same calls a binary load uses, so **binary to text to binary gives the same bytes**: `tests/projecttext/` checks that on every saved project in `samples/`.

Code: `core/projecttext.h` and `core/projecttext.cpp`. The functions mirror the binary ones in `core/projectfile.h`:

| Text | Binary | What |
|---|---|---|
| `writeProjectText(QByteArray&, texPath, tracks, client)` | `writeProject()` | the project into text; `"Project Saved!"` or `"Error: ..."` |
| `readProjectText(const QByteArray&, client)` | `readProject()` | text into new tracks from the client; `"Load Successful."`, `"Warning: ..."` or `"Error: ..."` |
| `saveProjectTextFile(fileName, texPath, tracks, client)` | `saveProjectFile()` | the same through a file |
| `loadProjectTextFile(fileName, client)` | `loadProjectFile()` | the same through a file |

They call the `projectFileClient` at the same points as the binary functions. One difference: the reader checks the **whole** text before it calls the client, so a refused text leaves the open project as it was. The writer runs those same checks on the text it has just made and returns an error, not a text, when the reader would refuse it (`Error: the project cannot be written as text, because the text would not read back: ...`), so the program never writes a text it cannot read. No window offers the text form yet; it is a function of the model for now.

## Rules of the text

- **Encoding:** UTF-8, JSON, four-space indentation, a newline at the end.
- **Key order:** Qt's `QJsonObject` keeps keys in alphabetical order, so every object is written alphabetically. The order of keys does not matter when reading.
- **Numbers:** every number in the model is a 32-bit float. It is written as the shortest decimal that reads back to exactly the same float, so `0.03` and not `0.029999999329447746`. Reading gives the identical float. JSON has no negative zero, infinity or NaN; those are written as the strings `"-0"`, `"inf"`, `"-inf"` and `"nan"`. When reading, `"-0"` is accepted, but an input that is infinite, NaN or too large for a float is refused (see "Checked ranges"); a value under `recalculatedOnLoad` is not read at all.
- **Whole numbers** (node counts, smoothing settings) must be written without a fraction.
- **Names of choices** are words, checked on reading: a wrong one is refused with the list of the right ones.
- **Unknown keys** are accepted and ignored, with a warning that names the place (`Warning: track 1, section 2 (curved): unknown key "colour" was ignored.`).
- **Refused texts:** a key that is missing, a value of the wrong type, an unknown section type, shape or style, a freeform transition, a value outside its checked range (below), or text that is not JSON is refused with a message that names the place, for example `Error: track 1, section 3 (force), roll, transition 1: "shape" is "wobbly", which is not one of ...`.
- **Recalculated values:** some values in the model are worked out again whenever a project is loaded. The text still shows them, so that a reader can see them and the round trip stays exact, but under a key `recalculatedOnLoad` inside the object they belong to. **Editing anything under `recalculatedOnLoad` changes nothing**; the reader skips it. To change a section's length, change the `to` of its transitions.

## Recalculated on load

Each was checked against the code and is proved by the byte-identity test (a value placed here wrongly would make it fail).

| Value | Why it cannot be an input |
|---|---|
| A transition's `startValue` | Every section's `updateSection()` sets the first transition's start from the node before it (the force, roll rate, pitch or yaw rate it arrives with) and each later one from the end of the one before (`func::translateValues()`). |
| A `tozero` transition's `change` | It is always minus its start value (`subfunc::translateValues()`). |
| A straight's `length` | It is the end of its roll function (`secstraight::updateSection()`). |
| A curve's `angle` | It is the end of its roll function (`seccurved::updateSection()`). |
| A force or geometric section's `timeNodes`, when the argument is time | It is the shortest unlocked function's end times 1000 (`secforced::updateSection()`, `secgeometric::updateSection()`). With the argument distance the program does not touch it, so it is an ordinary key there. |
| A smoothing entry's `name`, `fromNode` and `toNode`, for the whole track and for each section | They follow the track's name, the section's name and where the section lies (`smoothHandler::update()`). Custom regions keep all three. |

## Checked ranges

A text can hold values the windows never allow, and some of them would crash the program, make it loop forever or ask for more memory than the PC has. The reader refuses those before it touches the open project, with the place and the allowed range in the message, for example `Error: track 1, smoothing, wholeTrack: "iterations" is 0, which is outside the allowed range 1 to 16.` Where a box in the windows sets the value, the range is that box's, widened to cover every display unit (a speed box allows 0.1 to 500 in km/h, mph or m/s). Every value of every saved project in `samples/` lies inside these ranges.

| Value | Allowed | Why |
|---|---|---|
| Every number read as an input | finite, within a float | infinity and NaN were never inputs; a number too large for a float would become infinity. One exception: the `shapeParameter` of a shape that does not use it (below) |
| Anchor `speed` | 0.025 to 500 m/s | the anchor speed box; `track::updateAnchorGeometrics()` divides by it, and a NoLimits CSV section spaces its nodes by it |
| A section's `speed` when `speedMode` is `fixed` | 0.025 to 500 m/s | the speed boxes; a force section over distance held at 0 m/s never ends. With `physics` the stored value is not used and only has to be finite. |
| `heartline` | -5 to 5 m | Track Properties |
| `friction` | 0 to 2 | Track Properties |
| `resistance` | 0 to 0.001 | Track Properties (the box shows it times 100000) |
| `drawHeartline` | 0, 1 or 2 | the three choices of Track Properties |
| A curve's `radius` | 0.3 to 500 m | the radius box (1 foot to 500 m); at zero or below the curve never reaches its angle |
| A curve's `leadIn`, `leadOut` | 0 to 500 degrees | the lead-in and lead-out boxes |
| A function's first `from` | 0 | every function starts at 0 |
| A later transition's `from` | at most 64 float steps (64 last digits) from the `to` before it; may be left out | a transition starts where the one before ends (below) |
| Each transition's `to` | after where it starts (a length above 0) | a transition of no length divides by zero |
| The end of a function (its last `to`) | time: 2000 s; distance and straight: 10000 m; curve: 5000 degrees | memory: a time-based section has 1000 nodes a second; 5000 degrees is the angle box's limit |
| `center`, `tension` | -10 to 10 | the transition editor's boxes |
| `locked` | never on a straight or a curve; on a force or geometric section, at least one of its three functions without a locked transition | the windows allow neither (`isLockable()`); with all three locked the section has no end |
| A held-speed straight, curve or distance-based section | at most 2000000 nodes (length times 1000 divided by the held speed) | memory |
| A NoLimits CSV section | at most 2000000 nodes (its length times 1000 divided by the anchor speed) | memory |
| A Bezier section | its control polygon at most 20000 m long | memory (up to 1000 nodes a metre) |
| Smoothing `length` | 0 to 6000 | the smoothing dialog (0 to 6 s) |
| Smoothing `iterations` | 1 to 16 | the smoothing dialog; the filter divides the length by it |
| A custom region's `fromNode`, `toNode` | 0 to 10000000, and `toNode` not before `fromNode` | the filter keeps `toNode - fromNode` values in memory |

Judged harmless and so not limited beyond being finite: positions, angles (roll, pitch, yaw, a curve's direction), the anchor's forces, transition `change` and `shapeParameter` (a broken loop with extreme forces is a legitimate design; an odd shape parameter such as a quadratic's 0.5 gives infinite values, which the sections' loops stop on without harm), Bezier rolls and support points, a distance section's `timeNodes` (never used), a custom region reaching past the track's end (`track::getPoint()` stops at the last node), a region shorter than its filter (`track::applyRollSmoothFilter()` logs "Smoothing not possible" and leaves it, as it does when the window makes one), and the number of points or transitions (each one is written out in the text, so the memory it takes is bounded by the text itself).

## The project

| Key | Type | What |
|---|---|---|
| `format` | text | always `"fvd-project-text"` |
| `formatVersion` | whole number | always `1`; a text of another version is refused |
| `binaryVersion` | text | the binary format this text maps to, always `"v0.77"` |
| `groundTexture` | text | the ground texture's path (a `:/` name is built into the program) |
| `tracks` | list of tracks | in the order of the project |
| `additions` | object, optional | the project's own additions: what the binary file keeps under `"project"` in its appendix after `EOP` (`docs/codebase-map.md`, "Saved project file"). Written only when there are some, so the text of a project without additions is as before. Kept exactly as it is, keys the program does not know included |

## A track

| Key | Type | Unit | What |
|---|---|---|---|
| `name` | text | | the track's name |
| `colors` | object | | `default`, `activeSection`, `transition`: one colour each (below) |
| `anchor` | object | | where the track starts (below) |
| `heartline` | number | m | heartline height |
| `friction` | number | | rolling friction coefficient |
| `resistance` | number | | air resistance coefficient |
| `drawTrack` | true/false | | the track is drawn |
| `drawHeartline` | whole number | | the heartline drawing setting, as the program keeps it |
| `style` | text | | the rail style: `generic`, `genericflat`, `vekoma`, `bm`, `triangle`, `box`, `smallflat`, `doublespine` |
| `wireframe` | true/false | | the track is drawn as a wireframe |
| `povPosition` | [x, y] | | the point-of-view setting, as the program keeps it |
| `sections` | list of sections | | in the order of the track |
| `smoothing` | object | | `wholeTrack`: the smoothing entry of the whole track; `custom`: a list of custom regions (below) |
| `additions` | object, optional | | the track's additions, one key per feature: what the binary file keeps for this track in its appendix after `EOP`. Written only when the track has some. Read and written back exactly as they are, keys this program does not know included (no warning for those: they belong to a later feature); anything but an object is refused |

The additions are JSON as they stand, not numbers of the model: the rules above for floats (shortest decimal, `"-0"`, `"inf"`) do not apply inside them. Text to model to text and binary to text to binary both give them back exactly (`tests/appendix`).

### A colour

| Key | Type | What |
|---|---|---|
| `color` | text | `"#rrggbb"` |
| `alpha` | whole number | 0 to 255 |
| `exactBytes` | text, optional | 32 hexadecimal digits: the 16 bytes of the Qt colour object as the binary file stores them (in memory order). Written only when they are not the plain 8-bit colour (16-bit channels, another colour model, or bytes left over in the object's padding), so that the binary file comes out the same. If it does not agree with `color` and `alpha`, those two win and a warning says so. |

### The anchor

| Key | Type | Unit | What |
|---|---|---|---|
| `position` | [x, y, z] | m | the stored start point (y is up), `track::startPos`: the heartline point of the anchor. It is not the point the window's anchor boxes show and the `anchor.setPositionX`, `anchor.setPositionY` and `anchor.setPositionZ` commands take, which is the track point (the centre of the rails): `track::anchorStartPos()` moves that point by the heartline offset, `fHeart` along the anchor's up direction (given by its pitch and roll, and the yaw). For a level, upright anchor `x` and `z` are the typed ones and `y` is the typed height plus `fHeart`. |
| `roll` | number | degrees | start roll |
| `pitch` | number | degrees | start pitch |
| `yaw` | number | degrees | start direction |
| `speed` | number | m/s | start speed |
| `normalForce` | number | g | vertical force at the start |
| `lateralForce` | number | g | lateral force at the start |

## A section

Every section has `type` and `smoothing`; the other keys depend on the type.

| Key | Types | Type | Unit | What |
|---|---|---|---|---|
| `type` | all | text | | `straight`, `curved`, `force`, `geometric`, `bezier`, `nolimitscsv` |
| `name` | all but `nolimitscsv` | text | | the section's name (a NoLimits CSV section has none in the file; it is `"unnamed"` after a load) |
| `speedMode` | straight, curved, force, geometric | text | | `physics`: the speed follows from energy, friction and air resistance; `fixed`: held at `speed` |
| `speed` | the same | number | m/s | the held speed |
| `radius` | curved | number | m | |
| `direction` | curved | number | degrees | which way the curve bends, turned about the track |
| `leadIn`, `leadOut` | curved | number | degrees | the eased part at each end |
| `orientation` | curved, force, geometric | text | | `quaternion` (roll relative to the track) or `euler` (roll against the world's up, so yaw adds banking) |
| `argument` | force, geometric | text | | `time` or `distance`: what the functions run over |
| `timeNodes` | force, geometric with `distance` | whole number | | kept as the file has it; unused by a distance section |
| `roll` | straight, curved, force, geometric | list of transitions | degrees per second (per metre with the argument distance) | the roll rate |
| `normalForce`, `lateralForce` | force | list of transitions | g | the forces the rider feels |
| `pitchRate`, `yawRate` | geometric | list of transitions | degrees per second (per metre with the argument distance) | the model keeps these in the same fields as a force section's forces |
| `points` | bezier | list | | `point`, `controlPoint1`, `controlPoint2` ([x, y, z] in m), `roll` (degrees), `continuousRoll`, `relativeRoll` (true/false) |
| `supports` | bezier | list of [x, y, z] | m | |
| `points` | nolimitscsv | list | | `position` (m), `direction`, `lateral` (unit vectors), each [x, y, z] |
| `smoothing` | all | object | | this section's smoothing entry (below) |
| `recalculatedOnLoad` | straight: `length` (m); curved: `angle` (degrees); force and geometric with time: `timeNodes` | | | see above |

### A transition

A function is a list of transitions, one after the other. The argument (`from`, `to`) is in seconds for a force or geometric section with the argument time, in metres with the argument distance and on a straight, and in degrees of arc on a curve.

| Key | Type | What |
|---|---|---|
| `shape` | text | `linear`, `quadratic`, `cubic`, `quartic`, `quintic`, `sinusoidal`, `plateau`, `tozero`; `freeform` is refused (below) |
| `from` | number | where it starts: 0 for the first; for a later one the `to` before it, and it may be left out (below) |
| `to` | number | where it ends |
| `change` | number | how much the value changes over the transition, in the function's unit (not for `tozero`) |
| `shapeParameter` | number | the shape's own setting (quadratic, quartic, quintic, plateau). The other shapes never read it and the program never sets it for them, so a new linear, cubic or sinusoidal transition keeps whatever the memory held, and the project file saves that. So for those shapes any value is kept, `"inf"` and `"nan"` included; for the four that use it, it must be finite, and a model holding a non-finite one is not written. A NaN is read back as the standard NaN, so in the rare case that the memory held another NaN the binary file can differ in those four bytes; nothing is computed from them. |
| `center` | number | moves the middle of the transition (time-warp, "center") |
| `tension` | number | steepens or flattens the transition (time-warp, "tension") |
| `locked` | true/false | the transition stretches to the end of the section |
| `recalculatedOnLoad` | object | `startValue`; `change` for `tozero` |

**A later transition's `from`** is written for every transition and read as it is. In a model the program loaded it is normally the `to` before it, but the last transition of a function can be one last digit away: the binary loader keeps it as the file had it. So it is a stored value, not a recalculated one. When editing by hand:

- leave `from` out of a later transition and it starts at the `to` before it;
- if it is given, it must lie within 64 float steps of the `to` before it (64 steps from one float to the next, about 0.00003 near 5). Further away is refused, for example `Error: track 1, section 3 (force), normalForce, transition 3: transition 3 starts at 5.0099988 but the one before ends at 4.99999857; make them equal or leave "from" out.` To move where a transition starts, change the `to` of the one before.

The writer applies the same rule: a model whose gap is beyond it is not written (`Error: ...: it starts at X but the one before ends at Y, more than 64 float steps apart; the text could not be read back.`), so the program never writes a text it would refuse. The tolerance is far above what the program produces: `tests/projecttext` opens and saves every sample, and models built with awkward ends, 20 times in a row and the gap never passed 2 float steps, so 64 leaves a wide margin while a hand-typed start that is really wrong is still caught.

A text written by the first version of this format has a later `from` under `recalculatedOnLoad`; it still reads, the value there is ignored and the transition starts at the `to` before it. The version number stayed at 1, since both shapes read.

**Why a text round trip is exact but a binary one may not be.** When the program opens a binary file it adds the transitions of a function one at a time with `func::appendSubFunction()`, and each addition moves every later transition by (end before + its end − its start). In single precision that sum is not always the end it started from: a transition from 4.9999986 to 5.999999 becomes one ending at 5.9999995. So a project opened and saved, then opened and saved again, can have one transition end moved by one last digit; the original program does exactly the same, and the comparison with the 2016 program depends on it, so the binary load keeps it. Reading a text adds the transitions the same way but then sets every `from` and `to` to the text's value with no arithmetic, so the model is exactly the one the text was written from, and text to model to text gives the same text.

**Freeform transitions** cannot be written or read. The original program does not save their control points in the `.fvd` file either, so they come back broken from a binary file; writing a project that holds one fails with a message naming it.

### A smoothing entry

| Key | Type | What |
|---|---|---|
| `length` | whole number | the filter length (400 for a new entry) |
| `iterations` | whole number | how many times the filter runs |
| `active` | true/false | the entry is applied |
| `name`, `fromNode`, `toNode` | text, whole numbers | custom regions only; for the whole track and the sections they are under `recalculatedOnLoad` |

## Example

A track with a straight and a force section, shortened: the anchor's colour block, the second track and the smoothing of the straight are left out.

```json
{
    "binaryVersion": "v0.77",
    "format": "fvd-project-text",
    "formatVersion": 1,
    "groundTexture": ":/background.png",
    "tracks": [
        {
            "anchor": {
                "lateralForce": 0,
                "normalForce": 1,
                "pitch": 0,
                "position": [0, 5, 0],
                "roll": 0,
                "speed": 10,
                "yaw": 0
            },
            "drawHeartline": 0,
            "drawTrack": true,
            "friction": 0.03,
            "heartline": 1.1,
            "name": "unnamed Track",
            "povPosition": [0, 0],
            "resistance": 2e-05,
            "sections": [
                {
                    "name": "unnamed",
                    "recalculatedOnLoad": { "length": 10 },
                    "roll": [
                        {
                            "center": 0, "change": 0, "from": 0, "locked": false,
                            "recalculatedOnLoad": { "startValue": 0 },
                            "shape": "quartic", "shapeParameter": -10, "tension": 0, "to": 10
                        }
                    ],
                    "speed": 10,
                    "speedMode": "fixed",
                    "type": "straight"
                },
                {
                    "argument": "time",
                    "lateralForce": [
                        {
                            "center": 0, "change": 0, "from": 0, "locked": false,
                            "recalculatedOnLoad": { "startValue": 0.0076039154 },
                            "shape": "quartic", "shapeParameter": -10, "tension": 0, "to": 1
                        }
                    ],
                    "name": "unnamed",
                    "normalForce": [
                        {
                            "center": 0, "change": 2, "from": 0, "locked": false,
                            "recalculatedOnLoad": { "startValue": 1 },
                            "shape": "cubic", "shapeParameter": 1.875, "tension": 0, "to": 1
                        }
                    ],
                    "orientation": "quaternion",
                    "recalculatedOnLoad": { "timeNodes": 1000 },
                    "roll": [
                        {
                            "center": 0, "change": 0, "from": 0, "locked": false,
                            "recalculatedOnLoad": { "startValue": 0 },
                            "shape": "quartic", "shapeParameter": -10, "tension": 0, "to": 1
                        }
                    ],
                    "smoothing": {
                        "active": false, "iterations": 1, "length": 400,
                        "recalculatedOnLoad": { "fromNode": 3847, "name": "unnamed", "toNode": 4846 }
                    },
                    "speed": 10,
                    "speedMode": "physics",
                    "type": "force"
                }
            ],
            "smoothing": {
                "custom": [],
                "wholeTrack": {
                    "active": false, "iterations": 1, "length": 400,
                    "recalculatedOnLoad": { "fromNode": 0, "name": "unnamed Track", "toNode": 5847 }
                }
            },
            "style": "generic",
            "wireframe": false
        }
    ]
}
```

The program writes every list and object one item per line; the example is folded by hand to fit. Both read the same.

## How reading builds the model

The reader turns the text into plain data first, checking every key. Only then does it call the client and build each track as `track::loadTrack()` and `readProject()` do: the anchor's fields and the same anchor calls; per section `track::appendSectionFromFile()`, the fields, the transitions (the first one filled in, each further one appended with `func::appendSubFunction()` and filled in, then every `from` and `to` set to the text's values as they are), then `updateSection()`; the smoothing entries by position (0 the whole track, 1 to n the sections, then new custom regions); `updateTrack(0, 0)`; then `bAutoSmooth`, `updateSmoothEntries()`, `projectTrackSmoothingUpdated()` and `applyRollSmooth()`. A track's `additions` are set on it before anything else, and the project's go to `projectAdditionsRead()` before the first track is made. A NoLimits CSV section keeps its points private, so they pass through its own stream save and load functions.
