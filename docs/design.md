# Aurora design: decided, not yet built

## The link

- DIN MIDI is the live link: controller to brain, one cable, one way. USB MIDI stays for the editor.
- The brain treats whatever arrives as the truth. A DAW drives it exactly as the controller does.
- The rig is driven three ways: the controller alone; a DAW into the controller, soft-thru to the brain; a computer straight into the brain.
- The controller sends gestures (a fader's position, a key pressed), never what a patch contains. The brain owns meaning.
- The brain never answers over DIN. A controller forwarding a DAW's patch change updates its own state from what it forwards.
- The controller has no design mode. Patches are built in the editor.

## Clock

- The controller is the clock source. It tracks tempo from incoming clock or from tap tempo and always emits its own fresh 24 PPQN.
- External clock is never passed through. Tapping switches source with no discontinuity at the brain; an upstream dropout keeps the last tempo running.
- The source picks itself: the controller follows incoming clock whenever it arrives, and switches to tap tempo as soon as the button is tapped. No control selects it.
- The controller's tempo LED flashes from that clock.
- Tempo division is a patch switch on CC 33. The clock itself is never divided.

## Patch storage

- Up to 127 patches in LittleFS on the brain's program flash, in slots 1–127. A slot is the Program Change that plays its patch.
- Slot 0 holds no patch: Program Change 0 is the blackout. A key set to slot 0 is unset and does nothing.
- Patches survive a power cycle and are lost on a firmware upload.
- The editor holds the master library and pushes the whole library; the brain's copy is a mirror.
- A small default set is compiled into the firmware so an empty brain still lights the wall.
- The defaults are never written to storage: an empty library is how the editor recognizes a fresh flash.
- Booting on the defaults leaves the brain on a patch, never on blackout.

## Recalling a patch

- The keypad's nine keys map to library slots through the keymap the editor syncs. The controller sends the key; the brain looks it up.
- A DAW names a slot directly with a Program Change.
- The 12-position rotary is a patch bank, turned between songs. Nine patch keys per bank (0 is the blackout) reach 108 of the 127 slots; the rest are Program Change only.
- Recall writes the patch's `[patch]` and `[switch]` CCs through the same handlers a live CC goes through, then clears the tails.
- A patch change lands on the next beat.

## Patch transition and the accent

- One mechanism: from the live values toward the patch of the last key pressed, at whatever rate the driver sets.
- The start is a snapshot of what is on the wall at the press, never a patch number. Re-targeting mid-transition never lurches.
- Tap: a cut, on the beat.
- Hold: a transition toward the patch over its transition time.
- Release before arrival: the remaining distance is re-timed to land exactly on the next beat. No jump.
- Hold past arrival: the morph pushes on toward that patch's Accent target over its accent time.
- Release during the accent: hold course to the next beat, then drop to the patch in one step.
- Holding the key of the patch already playing goes straight to the accent.
- CC 19 reads 127 while the key the last Program Change named is down, and 0 on release.
- Every keypad effect lands on a beat. A press is snapped to the nearest beat, not the next; tap versus hold is judged a short fixed time after that beat.
- Each patch carries a transition time and an accent time, in beats, stepped through `AURORA_LFO_PERIODS`.
- A morph interpolates the raw CC bytes, all together, linearly. Circular controls (hue, PAR hue offset, fan phase) take the short way around.
- Switches (and route destinations) never interpolate. The destination patch's land on the release, not on arrival.
- A layer that shows nothing at one end of a morph takes the other end's values at once, its switches included; only the controls that make it visible blend (`hiddenLayers` in `shared/render/layers.cpp`).
- During an accent the source patch's switches are still in force.
- A route whose destination differs between two patches holds whole (destination, amount, ratio, wave, phase) and lands with the switches.
- A code arriving within a few tens of milliseconds of another key's is a fumbled two-key press and is ignored.

## The faders

- CC 12, 13 and 14 carry positions. Each morphs the patch toward one of its targets:
  - **Color**: hotter, toward white.
  - **Extent**: more of the wall lit.
  - **Motion**: faster, harder, more agitated.
- A target is per patch and covers every control, the PARs included.
- A fader never arrives: it never moves a switch, whatever its position.
- A fader's position is its push. A patch change does not reset it: the new patch arrives pushed by wherever the faders sit. The controller sends every fader's position when it starts.
- A patch and its targets share switches.
- Several targets at once add: each contributes its fader position times the distance from the patch to that target, and the sum is clamped per byte; a circular control takes the short way and wraps. The brain matches the editor's rule.
- Faders move energy; the keypad moves character. Mid-song lifts are fader moves; sideways changes at the same energy are patch changes.

## Blackout

- Key 0 is the blackout (PC 0): not a patch, no slot, never in the keymap. It holds until the next key press.
- The phone's hook switch is the master kill. It works on any patch and is pressed by a finger; the handset almost never rests on it.
- The hook is a mute, not a latch: the wall is dark while it is held and back on the playing patch when it is released.
- The hook shares the keypad's lines; its code is the blackout.

## Touchpad and rockers

- The pad sends X, Y, pressure and engage as raw values on CC 15–18. X and Y are positions that persist when the finger lifts; engage is separate from a finger being down.
- The rockers report where they stand on CC 2–6. What that does belongs to the patch.
- The pad never sets a value and never arrives on its own. Switches never move during a pad gesture.
- Nothing depends on pressure until it has been measured on this pad.

## The controller

- A second Teensy 4.0: pure input to MIDI. Keypad, three faders, touchpad, rockers, foot pedal, tap tempo, mic trigger. Every control is in `docs/hardware.md`.
- It drives its own 12 indicator pixels from what it sends. Nothing is streamed back from the brain.
- The foot pedal's four switches emit messages that already exist, the way the tap tempo button does.

## Out of scope

- Wireless between the boxes, DMX in, MIDI out to other gear, and anything that needs haze.
