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
- Program Changes 109–127 are songs, never patches. The brain ignores them.
- The controller works that Program Change out from the bank and the key. There is no keymap.
- Recall writes the patch's `[patch]` and `[switch]` CCs through the same handlers a live CC goes through, then clears the tails.
- A patch change lands on the next beat.

## Patch transition and the accent

- One mechanism: from the live values toward the patch of the last key pressed, at whatever rate the driver sets.
- The start is a snapshot of what is on the wall at the press, never a patch number. Re-targeting mid-transition never lurches.
- A source that can hold sends note 57 on before the Program Change and note 57 off on release. The keypad, the foot pedal and a DAW can hold.
- A Program Change without a hold morphs over the patch's transition time and never reaches the accent. A transition time of none is a cut on the beat.
- Tap: a cut, on the beat.
- Hold: a transition toward the patch over its transition time.
- Release before arrival: the remaining distance is re-timed to land exactly on the next beat. No jump.
- Hold past arrival: the morph pushes on toward that patch's Accent layer over its accent time.
- Release during the accent: hold course to the next beat, then drop to the patch in one step.
- Holding the key of the patch already playing goes straight to the accent.
- Every keypad effect lands on a beat. A press is snapped to the nearest beat, not the next; tap versus hold is judged a short fixed time after that beat.
- Each patch carries a transition time and an accent time, in beats, stepped through `AURORA_LFO_PERIODS`. Transition time also has a step for none.
- A morph interpolates the raw CC bytes, all together, linearly. Circular controls (hue, PAR hue offset, fan phase) take the short way around.
- Switches (and route destinations) never interpolate. The destination patch's land on the release, not on arrival.
- An engine that shows nothing at one end of a morph takes the other end's values at once, its switches included; only the controls that make it visible blend (`hiddenEngines` in `shared/render/engines.cpp`).
- During an accent the source patch's switches are still in force.
- A route whose destination differs between two patches holds whole (destination, amount, ratio, wave, phase) and lands with the switches.
- A code arriving within a few tens of milliseconds of another key's is a fumbled two-key press and is ignored.

## The faders

- CC 12, 13 and 14 carry positions. Each morphs the patch toward one of its layers: Color, Extent and Motion.
- A layer is the patch's look pushed further. What further means is up to the patch.
- A layer is per patch and covers every control, the PARs included.
- A fader never arrives: it never moves a switch, whatever its position.
- A fader's position is its push. A patch change does not reset it: the new patch arrives pushed by wherever the faders sit. The controller sends every fader's position when it starts.
- A patch and its layers share switches.
- Several layers at once add: each contributes its fader position times the distance from the patch to that layer, and the sum is clamped per byte; a circular control takes the short way and wraps. The brain matches the editor's rule.
- Faders move energy; the keypad moves character. Mid-song lifts are fader moves; sideways changes at the same energy are patch changes.

## Songs

- A song is an ordered list of labeled sections (intro, verse, chorus). Each label names one patch; a section that repeats plays the same patch every time.
- Up to 19 songs, on Program Changes 109–127. Songs are prepared per gig: the editor holds every song and pushes the gig's set.
- Songs live on the controller. The controller catches a song's Program Change, does not forward it, and sends the first section's patch Program Change in its place.
- The foot pedal steps through the song's sections, one switch forward and one back. Each step sends that section's patch Program Change, held or tapped like a key. Two quick taps forward skip a section.
- The controller always knows the current song and section.
- The editor pushes the songs to the controller and the patches to the brain together, never one without the other.

## Oneshots

- A oneshot is a single triggered effect with a length in beats. At the end of its length the wall snaps back to what is playing; any fade is part of the oneshot.
- A oneshot marks the controls it sets, explicitly, even where the value equals the default. While it plays, a marked control overrides the wall, ignores the faders, and silences the patch's routes aimed at it; an unmarked control keeps playing the wall, fader push included.
- A oneshot carries its own routes, run alongside the patch's. They start at the trigger, and their period is the oneshot's length, except a route aimed at a Scatter control, which runs on each spot's Scatter clock as in a patch. The Fan LFO staggers them across the strips only when the oneshot marks Fan LFO itself.
- A oneshot starts the moment its note arrives. If presses are ever snapped to the beat, the controller snaps them before sending.
- The last oneshot fired wins: a new trigger replaces the one playing, or restarts it.
- Oneshots form one kit of up to 20, shared by every patch, on notes 60–79, one note each.
- Each patch picks two oneshots from the kit, stored in the patch on the brain. Notes 58 and 59 fire the playing patch's first and second pick, or the default pair when the patch has none.
- The pedal's Oneshot 1 and 2 send the song's pick for that switch as its kit note; with no song or no pick, they send note 58 or 59. The controller holds no patch data.
- The mic trigger fires whatever Oneshot 1 fires.

## Routes

- A route loops or plays once. A once route plays one cycle at the start of each period, at its ratio's speed, then holds its end value until the next period. Its phase delays its start within the period.
- Once and loop share the ratio control: its lower half is the eight looping ratios, its upper half the same eight played once.


## Foot pedal

- Eight switches over a TRS cable: tip and ring each carry the four-switch ladder in `docs/hardware.md`.
- Every switch has a fixed role, the same for every patch, the way each fader has its layer. A switch with nothing to do on a patch does nothing.
- Roles: Prev, Next, Color, Extent, Motion, Tap, Oneshot 1, Oneshot 2.
- Prev and Next step through the song's sections; with no song loaded, through the keys of the current bank.
- Color, Extent and Motion push their layer the way its fader does. Pressing starts the push at once; a quick release latches it at full and the next tap drops it; a long hold pushes only while held.
- Tap is tap tempo.
- Two rows of four, the back row offset half a switch, Next under the right foot:

```
 back    [Oneshot 1] [Oneshot 2]      [ Color ]       [ Prev ]
 front          [  Tap  ]        [Extent] [Motion]    [ Next ]
```

- Switch centers about 10 cm apart, rows about 12 cm apart, wider gaps between the three groups.

## Blackout

- Key 0 is the blackout (PC 0): not a patch, no slot, in every bank. It holds until the next key press and never returns on its own.
- Tapping 0 cuts to black on the next beat.
- Holding 0 fades the brightness to black over the playing patch's transition time, leaving the look itself untouched. Release before black: the rest of the fade is re-timed to land on the next beat. Holding past black stays black; the blackout has no accent.
- The phone's hook switch is the master kill. It works on any patch and is pressed by a finger; the handset almost never rests on it.
- The hook is a mute, not a latch: the wall goes dark the moment it is pressed, not on a beat, and is back on the playing patch the moment it is released.
- The hook sends its own note: note-on while held, note-off on release. It is never a Program Change or a CC.

## Touchpad and rockers

- The pad sends X, Y, pressure and engage as raw values on CC 15–18. X and Y are positions that persist when the finger lifts; engage is separate from a finger being down.
- The rockers report where they stand on CC 2–6. What that does belongs to the patch.
- The pad never sets a value and never arrives on its own. Switches never move during a pad gesture.
- Nothing depends on pressure until it has been measured on this pad.

## The controller

- A second Teensy 4.0: pure input to MIDI. Keypad, three faders, touchpad, rockers, foot pedal, tap tempo, mic trigger. Every control is in `docs/hardware.md`.
- It drives its own 12 indicator pixels from what it sends. Nothing is streamed back from the brain.
- The foot pedal's switches emit messages that already exist, the way the tap tempo button does.

## Out of scope

- Wireless between the boxes, DMX in, MIDI out to other gear, and anything that needs haze.
