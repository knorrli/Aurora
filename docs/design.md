# Aurora design: decided, not yet built

## Patch storage

- Up to 108 patches in LittleFS on the brain's program flash, in slots 1–108. A slot is the Program Change that plays its patch.
- Slot 0 holds no patch: Program Change 0 is the blackout. A key set to slot 0 is unset and does nothing.
- Patches survive a power cycle and are lost on a firmware upload.
- The editor holds the master library and pushes the whole library; the brain's copy is a mirror.
- A small default set is compiled into the firmware so an empty brain still lights the wall.
- The defaults are never written to storage: an empty library is how the editor recognizes a fresh flash.
- Booting on the defaults leaves the brain on a patch, never on blackout.

## Controls and MIDI

- Playing and patch design are separate. The box and a DAW play Aurora with the same messages: program changes for patches, songs and blackout, notes for oneshots and the held key, CCs for the three layer faders and the pad, and MIDI clock. These CC numbers are fixed.
- Patch controls are numbered by their place in the patch, not by CC, and are unlimited in count. The editor dials them on the brain with a SysEx message that sets a control to a value. No CC reaches inside a patch.
- Every patch control holds one thing. A setting is its own control, never folded into a value's range.
- A route's Source, Cycle, Ratio and Wave are settings, and so is the Pattern of a strip, note or birth route. Its Destination names any control, including another route's Amount or Phase; the arp's unison, steps or ripple is its own setting.
- A layer flips settings halfway up its fader, and back at the same point.
- When a layer changes any of a route's settings, the route plays twice: the base's version at its Amount × (1 − fader), the layer's version at its Amount × fader.

## The faders

- A layer is the patch's look pushed further. What further means is up to the patch.
- Faders move energy; the keypad moves character. Mid-song lifts are fader moves; sideways changes at the same energy are patch changes.

## Cells

- Shapes are built from cells. A cell is one lit thing with a life: it is born, lives, and ends. One cell is one note.
- A cell is the thing, not the lit region it draws: it can be dark for a moment, overlap another cell, or look merged with one.
- A cell's life is its gate: a length on the LFO's steps. Its envelopes wait out their delay, run attack, hold and decay, sit at sustain until the gate ends, then release.
- Cells replace Shape. Scatter stays, painting over the cells with its own color.
- A cell has three envelopes, set with knobs: delay, attack, hold, decay, sustain and release. Delay holds the envelope at zero after birth; on start over it waits again each time. They run on the cell's age from birth, and each cell runs its own.
- Envelopes are route sources, like the LFO: a route aims one at position, width, brightness or another control, with an amount. One envelope can drive several controls. A control no route aims at stays as dialed.
- A route on position moves a cell away from its birthplace.
- One setting decides what happens when the gate ends. Gone: the envelopes release, and the cell disappears once the last one has finished. Start over: the envelopes start again from attack, and release never plays. Stay: the gate never ends, and the envelopes hold at sustain.
- Going dark never ends a cell. Routes change cells and never end them.
- The clock starts cells and never ends one. Rate, Chord and Spread decide the births.
- Rate is how often cells are born, on the LFO's steps, landing on the beat grid. Its first step is born once.
- Delay is how late each birth lands, as a share of one Rate step; born once, of one LFO cycle. The whole life moves with it: birth, gate, release and end. Live LFO routes do not. Routes aim at Delay like any control: a Strip route ripples births across the wall, a Note route strums a chord's cells, a Birth route on Evens / odds swings. Knob and routes together reach two steps at most.
- Chord is how many cells each birth starts. Spread is how far apart they are: full spreads them evenly over the travel, zero puts them in one place.
- Cells are born at the dialed Position.
- Ends sets what Position's extremes mean, for the whole patch. Inside: at either extreme the cell's edge sits flush with the strip's end, the whole cell in view. Outside: at either extreme the cell sits just past the end, out of view. That span is the cell's travel; Spread and routes on Position are measured along it.
- A route plays live or is held per cell. Live moves every living cell at once. Held is read once when a cell is born and kept for its life: a held route on Position spreads births, a held route on brightness is an accent.
- A route's source is the LFO, the pad, an envelope, or a group: Strip, Note or Birth. A group source gives each member a number from 0 to 1 by one of the PAR arp's patterns, stretched to fit any number of members: Strip across the strips, Note across a chord's cells, Birth across successive births. Sequence is a slope, Mirror sets the center apart from the outsides, Evens / odds alternates, Pairs steps by two, Random shuffles. A negative amount reverses it.
- On Birth, Sequence, Mirror and Pairs stretch over the births of one LFO cycle and start over with the next, like the arp's passes; a cycle with one birth gives it the middle. Evens / odds and Random count births straight through.
- A route can aim at another route's amount or phase, setting it per strip, per cell or per birth. A route aimed at a route cannot itself be aimed at.
- There is no Fan. Its Position is a Strip route on Position; its LFO offset is a Strip route on another route's phase.
- The band's MIDI and audio never start cells; live triggers stay oneshots.
- One cell's life is set per patch; the births and the per-strip settings make the wall from it.
- Width is a cell's size on the strip, independent of Chord. The editor shows it in pixels.
- Edge, Tail, Bend and Bend at work on cells as they do on Shape today. There is no Count and no Bounce: Chord and Spread place cells, and the LFO's sine on Position, bipolar at full amount, swings each cell of a fully spread chord around its birthplace, the outer ones reaching the ends.
- There is no Speed. A cell moves by routes on Position: an envelope for its own journey, the LFO for every cell at once. A route on Position moves a cell by its amount times the travel divided by Chord, the distance between neighboring cells at full Spread; at Chord 1 that is the whole travel. Nothing confines a cell to that distance: it can pass its neighbors and leave the strip.
- Cells born at a Rate never wrap: a cell traveling off the strip is out of view until its gate ends. Cells born once wrap: leaving one end of the travel, they come back at the other. Outside, a cell is fully out of view when it wraps.
- Births come from the beat timeline: when a patch starts, the cells born on earlier beats are already in flight. Held and random values come from the birth's number.
- The plainest cell has no envelope routed. Still bars are a chord born once that stays. Moving bars are cells born at one end at Rate, carried across by an envelope on Position; the bars on the strip at once are how long each stays in view divided by Rate. Gliding bars are a chord born once, moved by the LFO's falling wave on Position at full amount.

## Brightness

- Brightness is set with the cells: the cell's Brightness, routes to it, live or held, and the brightness curve. Brightness is today's Value, moved out of the color settings and renamed; the code follows the name. Hue and Saturation stay in the color settings.
- The brightness curve works like Bend: an amount, positive or negative, and where along the strip it peaks. Direction picks what the peak's place is measured along: Vertical (a gradient pixel by pixel), Horizontal (strip by strip), Cell (inside each cell, front to back), or Cell position (one even value per cell, read at its center).
- Field's Direction has the same four options. Cell position gives each cell one even color, read at its center; its tail takes the cell's color.
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
