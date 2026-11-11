# The project as text

A coaster can be written as a text document beside the binary `.fvd` file: JSON, readable and comparable by a person and by an agent, and writable by either (feature request FR-5). The binary format does not change. The text is made from the loaded model and read back into it with the same calls a binary load uses, so **binary to text to binary gives the same bytes**: `tests/projecttext/` checks that on every saved project in `samples/`.

Code: `core/projecttext.h` and `core/projecttext.cpp`. The functions mirror the binary ones in `core/projectfile.h`:

| Text | Binary | What |
|---|---|---|
| `writeProjectText(QByteArray&, texPath, tracks, client)` | `writeProject()` | the project into text; `"Project Saved!"` or `"Error: ..."` |
| `readProjectText(const QByteArray&, client)` | `readProject()` | text into new tracks from the client; `"Load Successful."`, `"Warning: ..."` or `"Error: ..."` |
| `saveProjectTextFile(fileName, texPath, tracks, client)` | `saveProjectFile()` | the same through a file |
| `loadProjectTextFile(fileName, client)` | `loadProjectFile()` | the same through a file |

They call the `projectFileClient` at the same points as the binary functions. One difference: the reader checks the **whole** text before it calls the client, so a refused text leaves the open project as it was. No window offers the text form yet; it is a function of the model for now.

## Rules of the text

- **Encoding:** UTF-8, JSON, four-space indentation, a newline at the end.
- **Key order:** Qt's `QJsonObject` keeps keys in alphabetical order, so every object is written alphabetically. The order of keys does not matter when reading.
- **Numbers:** every number in the model is a 32-bit float. It is written as the shortest decimal that reads back to exactly the same float, so `0.03` and not `0.029999999329447746`. Reading gives the identical float. JSON has no negative zero, infinity or NaN; those are written as the strings `"-0"`, `"inf"`, `"-inf"` and `"nan"`, and any number may be given that way.
- **Whole numbers** (node counts, smoothing settings) must be written without a fraction.
- **Names of choices** are words, checked on reading: a wrong one is refused with the list of the right ones.
- **Unknown keys** are accepted and ignored, with a warning that names the place (`Warning: track 1, section 2 (curved): unknown key "colour" was ignored.`).
- **Refused texts:** a key that is missing, a value of the wrong type, an unknown section type, shape or style, a freeform transition, or text that is not JSON is refused with a message that names the place, for example `Error: track 1, section 3 (force), roll, transition 1: "shape" is "wobbly", which is not one of ...`.
- **Recalculated values:** some values in the model are worked out again whenever a project is loaded. The text still shows them, so that a reader can see them and the round trip stays exact, but under a key `recalculatedOnLoad` inside the object they belong to. **Editing anything under `recalculatedOnLoad` changes nothing**; the reader skips it. To change a section's length, change the `to` of its transitions.

## Recalculated on load

Each was checked against the code and is proved by the byte-identity test (a value placed here wrongly would make it fail).

| Value | Why it cannot be an input |
|---|---|
| A transition's `startValue` | Every section's `updateSection()` sets the first transition's start from the node before it (the force, roll rate, pitch or yaw rate it arrives with) and each later one from the end of the one before (`func::translateValues()`). |
| A transition's `from`, after the first | A later transition starts where the one before it ends (`func::appendSubFunction()`). |
| A `tozero` transition's `change` | It is always minus its start value (`subfunc::translateValues()`). |
| A straight's `length` | It is the end of its roll function (`secstraight::updateSection()`). |
| A curve's `angle` | It is the end of its roll function (`seccurved::updateSection()`). |
| A force or geometric section's `timeNodes`, when the argument is time | It is the shortest unlocked function's end times 1000 (`secforced::updateSection()`, `secgeometric::updateSection()`). With the argument distance the program does not touch it, so it is an ordinary key there. |
| A smoothing entry's `name`, `fromNode` and `toNode`, for the whole track and for each section | They follow the track's name, the section's name and where the section lies (`smoothHandler::update()`). Custom regions keep all three. |

## The project

| Key | Type | What |
|---|---|---|
| `format` | text | always `"fvd-project-text"` |
| `formatVersion` | whole number | always `1`; a text of another version is refused |
| `binaryVersion` | text | the binary format this text maps to, always `"v0.77"` |
| `groundTexture` | text | the ground texture's path (a `:/` name is built into the program) |
| `tracks` | list of tracks | in the order of the project |

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

### A colour

| Key | Type | What |
|---|---|---|
| `color` | text | `"#rrggbb"` |
| `alpha` | whole number | 0 to 255 |
| `exactBytes` | text, optional | 32 hexadecimal digits: the 16 bytes of the Qt colour object as the binary file stores them (in memory order). Written only when they are not the plain 8-bit colour (16-bit channels, another colour model, or bytes left over in the object's padding), so that the binary file comes out the same. If it does not agree with `color` and `alpha`, those two win and a warning says so. |

### The anchor

| Key | Type | Unit | What |
|---|---|---|---|
| `position` | [x, y, z] | m | start position (y is up) |
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
| `from` | number | where the first transition starts (only the first; usually 0) |
| `to` | number | where it ends |
| `change` | number | how much the value changes over the transition, in the function's unit (not for `tozero`) |
| `shapeParameter` | number | the shape's own setting (quadratic, quartic, quintic, plateau) |
| `center` | number | moves the middle of the transition (time-warp, "center") |
| `tension` | number | steepens or flattens the transition (time-warp, "tension") |
| `locked` | true/false | the transition stretches to the end of the section |
| `recalculatedOnLoad` | object | `startValue`; `from` after the first transition; `change` for `tozero` |

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

The reader turns the text into plain data first, checking every key. Only then does it call the client and build each track as `track::loadTrack()` and `readProject()` do: the anchor's fields and the same anchor calls; per section `track::appendSectionFromFile()`, the fields, the transitions (the first one filled in, each further one appended with `func::appendSubFunction()` and filled in), then `updateSection()`; the smoothing entries by position (0 the whole track, 1 to n the sections, then new custom regions); `updateTrack(0, 0)`; then `bAutoSmooth`, `updateSmoothEntries()`, `projectTrackSmoothingUpdated()` and `applyRollSmooth()`. A NoLimits CSV section keeps its points private, so they pass through its own stream save and load functions (the ones the undo list uses).
