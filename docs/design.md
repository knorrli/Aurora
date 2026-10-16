# Aurora design: decided, not yet built

## Patch storage

- Up to 108 patches in LittleFS on the brain's program flash, in slots 1–108. A slot is the Program Change that plays its patch.
- Slot 0 holds no patch: Program Change 0 is the blackout. A key set to slot 0 is unset and does nothing.
- Patches survive a power cycle and are lost on a firmware upload.
- The editor holds the master library and pushes the whole library; the brain's copy is a mirror.
- A small default set is compiled into the firmware so an empty brain still lights the wall.
- The defaults are never written to storage: an empty library is how the editor recognizes a fresh flash.
- Booting on the defaults leaves the brain on a patch, never on blackout.

## The faders

- A layer is the patch's look pushed further. What further means is up to the patch.
- Faders move energy; the keypad moves character. Mid-song lifts are fader moves; sideways changes at the same energy are patch changes.

## Cells

- Shapes are built from cells. A cell is one lit thing with a life: it is born, lives, and ends. One cell is one note.
- A cell is the thing, not the lit region it draws: it can be dark for a moment, overlap another cell, or look merged with one.
- A cell's life is its gate: a length on the LFO's steps. Its envelopes run attack, hold and decay, sit at sustain until the gate ends, then release.
- Cells replace Shape. Scatter stays, painting over the cells with its own color.
- A cell has three envelopes, set with knobs: attack, hold, decay, sustain and release. They run on the cell's age from birth, and each cell runs its own.
- Envelopes are route sources, like the LFO: a route aims one at position, width, brightness or another control, with an amount. One envelope can drive several controls. A control no route aims at stays as dialed.
- A route on position moves a cell away from its birthplace.
- One setting decides what happens when the gate ends. Gone: the envelopes release, and the cell disappears once the last one has finished. Start over: the envelopes start again from attack, and release never plays. Stay: the gate never ends, and the envelopes hold at sustain.
- Going dark never ends a cell. Routes change cells and never end them.
- The player starts cells on the clock and never ends one. It has three knobs: Rate, Chord and Spread.
- Rate is how often cells are born, on the LFO's steps, landing on the beat grid. Its last step is born once.
- Chord is how many cells each birth starts. Spread is how far apart they are: full spreads them evenly over the strip, zero puts them in one place.
- Cells are born at the dialed Position.
- A route plays live or is held per cell. Live moves every living cell at once. Held is read once when a cell is born and kept for its life: a held route on Position spreads births, a held route on brightness is an accent.
- The band's MIDI and audio never start cells; live triggers stay oneshots.
- One cell's life is set per patch; the player and the per-strip settings make the wall from it.
- Width is a cell's size on the strip, independent of Chord. The editor shows it in pixels.
- Edge, Tail, Speed, Bend and Bend at work on cells as they do on Shape today. There is no Count and no Bounce: Chord and Spread place cells, and a looping envelope on Position swings a cell around its birthplace.
- Nothing wraps. A cell traveling off the strip is out of view until its gate ends.
- Births come from the beat timeline: when a patch starts, the cells born on earlier beats are already in flight. Held and random values come from the birth's number.
- The plainest cell has no envelope routed. Still bars are a chord born once that stays. Moving bars are cells born at one end at Rate, drifting at Speed; their spacing is how far a cell travels between births.

## Brightness

- Brightness is set with the cells: the cell's Brightness, routes to it, live or held, and the brightness curve. Brightness is today's Value, moved out of the color settings and renamed; the code follows the name. Hue and Saturation stay in the color settings.
- The brightness curve works like Bend: an amount, positive or negative, and where along the strip it peaks. Across picks what the peak's place is measured along: Strip (a gradient pixel by pixel), Wall (strip by strip), Cell body (inside each cell, front to back), or Cell place (one even value per cell, read at its center).
- Dark in Field, Flow and Core stays, as shading toward black with the color pattern.
- Shape brightness and color Dark multiply.

## Oneshots

- If presses are ever snapped to the beat, the controller snaps oneshot presses before sending.

## Foot pedal

- Two rows of four, the back row offset half a switch, Next under the right foot:

```
 back    [Oneshot 1] [Oneshot 2]      [ Color ]       [ Prev ]
 front          [  Tap  ]        [Motion] [Extent]    [ Next ]
```

- Switch centers about 10 cm apart, rows about 12 cm apart, wider gaps between the three groups.

## Touchpad

- CC 17 is kept for pressure. Nothing depends on pressure until it has been measured on this pad.

## Tempo following

- The drum triggers' MIDI goes into the controller's DIN in.
- Tap sets the tempo. While kick notes arrive, the clock follows them; when they stop, it holds the last tempo. There is no switch for it.
- Which notes count as the kick is one setting for the rig.
- The grid is sixteenths, one setting for the rig.
- A kick counts only when it lands close to a sixteenth of the running clock; any other kick is ignored. Each counted kick corrects part of the clock's error.
- Half-time, double-time and triplet feel are never detected: the pedal's Next moves to the section's patch, whose division sets the feel.

## Mic

- The mic switch picks the oneshot the mic fires: Oneshot 1 or Oneshot 2. The mic has no off; nothing plugged in is off.

## Out of scope

- Wireless between the boxes, DMX in, MIDI out to other gear, and anything that needs haze.
