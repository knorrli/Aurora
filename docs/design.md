# Aurora design: decided, not yet built

## The link

- DIN MIDI is the live link: controller to brain, one cable, one way. USB MIDI stays for the editor.
- The brain treats whatever arrives as the truth. A DAW drives it exactly as the controller does.
- The rig is driven three ways: the controller alone; a DAW into the controller, soft-thru to the brain; a computer straight into the brain.
- The brain has one MIDI input. Any source playing alongside the controller goes through the controller.
- The controller sends gestures (a fader's position, a key held) and the slot a key or song section names, never what a patch contains.
- The brain never answers over DIN. A controller forwarding a DAW's patch change updates its own state from what it forwards.
- The controller has no design mode. Patches and songs are built in the editor.

## Clock

- The controller is the clock source. It tracks tempo from incoming clock or from tap tempo and always emits its own fresh 24 PPQN.
- External clock is never passed through. An upstream dropout keeps the last tempo running.
- The source picks itself: the controller follows incoming clock whenever it arrives. Tapping does nothing while clock arrives; with none arriving, a tap takes over with no discontinuity at the brain. No control selects it.
- The controller's tempo LED flashes from that clock.
- Tempo division is a patch switch on CC 29. The clock itself is never divided.

## Patch storage

- Up to 108 patches in LittleFS on the brain's program flash, in slots 1–108. A slot is the Program Change that plays its patch.
- Slot 0 holds no patch: Program Change 0 is the blackout. A key set to slot 0 is unset and does nothing.
- Patches survive a power cycle and are lost on a firmware upload.
- The editor holds the master library and pushes the whole library; the brain's copy is a mirror.
- A small default set is compiled into the firmware so an empty brain still lights the wall.
- The defaults are never written to storage: an empty library is how the editor recognizes a fresh flash.
- Booting on the defaults leaves the brain on a patch, never on blackout.

## Recalling a patch

- Every source changes patch the same way: a Program Change naming a slot. The drum pad and MainStage send nothing else.
- The 12-position rotary is a patch bank, labeled A to L. Bank b, key k plays slot (b − 1) × 9 + k, counting A as 1: A1 is slot 1, C5 is slot 23, L9 is slot 108.
- Program Changes 109–127 are songs, never patches.
- The controller works that Program Change out from the bank and the key. There is no keymap.

## Patch transition and the accent

- A source that can hold sends note 57 on before the Program Change and note 57 off on release. The keypad, the foot pedal and a DAW can hold.
- A code arriving within a few tens of milliseconds of another key's is a fumbled two-key press and is ignored.

## The faders

- A layer is the patch's look pushed further. What further means is up to the patch.
- The controller sends every fader's position when it starts.
- The fader rocker is Cue. While it is on, the controller holds fader moves back; the wall keeps the positions last sent, across patch changes too. Turning it off sends every fader's position at once. Cue never holds back the pedal.
- Faders move energy; the keypad moves character. Mid-song lifts are fader moves; sideways changes at the same energy are patch changes.

## Songs

- A song is an ordered list of labeled sections (intro, verse, chorus). Each label names one patch; a section that repeats plays the same patch every time.
- Up to 19 songs, on Program Changes 109–127. Songs are prepared per gig: the editor holds every song and pushes the gig's set.
- Songs live on the controller. The controller catches a song's Program Change, does not forward it, and sends the first section's patch Program Change in its place.
- The foot pedal steps through the song's sections, one switch forward and one back. Each step sends that section's patch Program Change, held or tapped like a key. Two quick taps forward skip a section.
- The controller always knows the current song and section.
- The editor pushes the songs to the controller and the patches to the brain together, never one without the other.

## Oneshots

- If presses are ever snapped to the beat, the controller snaps oneshot presses before sending.
- The pedal's Oneshot 1 and 2 send the song's pick for that switch as its kit note; with no song or no pick, they send note 58 or 59. The controller holds no patch data.
- The mic trigger fires whatever Oneshot 1 fires.

## Routes

- A route loops or plays once. A once route plays one cycle at the start of each period, at its ratio's speed, then holds its end value until the next period. Its phase delays its start within the period.
- Once and loop share the ratio control: its lower half is the eight looping ratios, its upper half the same eight played once.


## Foot pedal

- Eight switches over a TRS cable: tip and ring each carry the four-switch ladder in `docs/hardware.md`.
- Every switch has a fixed role, the same for every patch, the way each fader has its layer. A switch with nothing to do on a patch does nothing.
- Roles: Prev, Next, Color, Motion, Extent, Tap, Oneshot 1, Oneshot 2.
- Prev and Next step through the song's sections; with no song loaded, through the keys of the current bank.
- Color, Motion and Extent push their layer the way its fader does. Pressing starts the push at once; a quick release latches it at full and the next tap drops it; a long hold pushes only while held.
- Tap is tap tempo.
- Two rows of four, the back row offset half a switch, Next under the right foot:

```
 back    [Oneshot 1] [Oneshot 2]      [ Color ]       [ Prev ]
 front          [  Tap  ]        [Motion] [Extent]    [ Next ]
```

- Switch centers about 10 cm apart, rows about 12 cm apart, wider gaps between the three groups.

## Blackout

- Key 0 sends Program Change 0, the blackout, in every bank.
- The phone's hook switch is the master kill. It works on any patch and is pressed by a finger; the handset almost never rests on it.
- The hook is a mute, not a latch: the wall goes dark the moment it is pressed, not on a beat, and is back on the playing patch the moment it is released.
- The hook sends its own note: note-on while held, note-off on release. It is never a Program Change or a CC.

## Touchpad and rockers

- The pad sends X, Y and touch as raw values: touch on at the press, off at the lift, also when latching. On a press, X and Y go out before touch. CC 17 is kept for pressure.
- The main touchpad rocker is Hold (momentary or latching), touchpad rocker 1 the pad mode (per patch, morph, effects), rocker 2 Width (strip 3, strips 2–4, all), rocker 3 Gaps (solid or every other). Each reports where it stands.
- The fader rocker sends nothing of its own.
- Nothing depends on pressure until it has been measured on this pad.

## The controller

- A second Teensy 4.0: pure input to MIDI. Keypad, three faders, touchpad, rockers, foot pedal, tap tempo, mic trigger. Every control is in `docs/hardware.md`.
- It drives its own 12 indicator pixels from what it sends. Nothing is streamed back from the brain.
- The left indicator mixes the faders as light: Color is red, Motion green, Extent blue, matching the fader caps; all three up is white. While Cue is on, it alternates between the mix the wall plays and where the faders stand, or with dim white when the two match.
- The right indicator shows the pad mode's color, and blinks while the pad is latching.
- The pad's 5×2 grid shows the pad mode in that mode's own color; in Effects, each column shows its effect's color. Columns of strips outside Width and Gaps are dim. The pixels under the finger light up where the pad is pressed.
- The three mode colors and five effect colors are eight different colors, fixed in the controller's firmware.
- The foot pedal's switches emit messages that already exist, the way the tap tempo button does.

## Out of scope

- Wireless between the boxes, DMX in, MIDI out to other gear, and anything that needs haze.
