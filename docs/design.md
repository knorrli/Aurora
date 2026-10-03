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

## Out of scope

- Wireless between the boxes, DMX in, MIDI out to other gear, and anything that needs haze.
