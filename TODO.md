# Aurora TODO

## Hardware

- Move the brain off the breadboard onto perfboard.
- Build DIN MIDI in on the brain: 6N138 on `Serial1`.
- Cut the brain's enclosure around the finished perfboard.
- Socket both Teensys rather than soldering them down.
- Rebuild the controller on the second Teensy and draw its pin map (`docs/hardware.md`).
- Build the foot pedal.
- When the keypad is off the box: confirm the idle code and what produces `0b00111111` and `0b00111101`.
- Press two keys at once on the old box and watch the wall: does 2+3 black it out?
- Buy real 110 Ω DMX cable for stage.

## PARs

- Set the remaining two BCC145 to `A017` and `A025`, watching the personality (`docs/hardware.md`).
- Calibrate the fixtures at rehearsal: RGB trims and the brightness curve, stepping evenly spaced values.
- Set each fixture's master scale by eye at soundcheck.
- Aim each PAR at the wall between two strips, addressed `A001` to `A025` in stage order.

## On the wall

Seen only in the preview so far. One look each, driven from the editor.

- Flash the brain; if the wall ignores a sender, check it is on MIDI channel 1.
- Time a frame at eight samples per pixel; if well past 7–8 ms, go back to four.
- Palettes: dark ends of Cyberpunk, Space, Nature and TV; Art's pastel at S full; mirrored Sky 35; the PARs' hue offset inside a three-color palette.
- PARs alone, strips dark: stepped hue spread, 25% LFO spread chase, the same shuffled, steady lamps with hue shuffle every bar, a swell with the shuffle up. Cut the hue shuffle (frees CCs 3 and 4) if it reads as nothing.
- Pull the PARs' Saturation down, then open the LFO on PAR saturation: the flash toward white should start from pale.
- Try to defeat the blackout: transport stopped, mic trigger firing, PARs at full. Then any key brings the wall back.
- Region inside out across the strips at count 1, then the dark case with V at the top.
- Swipe Position with a build wave: climbs, snaps back on the bar, tail drops at the snap. Try Bend on it.
- Bounce on and off at count 1 and with the fan up: every shape stands still across the flip.
- The fan: a quarter-turn phase as a chevron (does it get Rain back?), staggered bars strobing in unison, strips at different rates (alive or coming apart?), Randomize (wants a seed?).
- Frequency fader at the top, sweep the phase: does the fan going quiet read as a fault?
- Field region count wound up in the vertical direction with a hard edge: washes out, does not strobe.
- V fader under a scattered look: white pixels dim with the colored ones.
- White and Dark amounts from 0 up with S full and V at the top; Field and Flow White together; does Light Dark earn its place?
- Swing a rate: a sine on the fan's rate spread; the hypno look on speed and rate spread together.
- Morph between patches of different tempo divisions: does the jump read as a glitch?
- Afterglow: tail stays behind through a swing, shrinks as it slows, gone at rest; is 8 beats the right top; brain frame time; no streak across a patch change.
- LFO routes: PARs swelling under still strips, a white flash on the PARs between strip strobes, shapes breathing on width.
- Anchor against a click track: should the peak or the leading edge land on the beat?
- Stepped LFO rate: do the dotted values read as adrift? Cutting them leaves seven.
- Bend at full: do shapes squashed at the slow end shimmer?
- The scatter on the strips, and whether spots need to outlive their cell.
- A wrapping strip: loop, clip, or boundary fade?
- Play a set to find where the color layer's controls should stop.
- Dial five or six endpoints by eye and save them; nothing about the morph is worth judging before.

## Patches on the brain

Deferred until the patch model stops changing. The editor drives the wall live over CCs until then.

- Run the patch sync against the brain from `tools/protocol.html`: empty on a fresh flash; push eight and read back; a half library is refused and the old one survives; round-trip a file; pull the mains mid-sync; time a full 128.
- Recall a patch: keypad lookup, Program Change to slot, write through the live handlers, clear the tails.
- Compile in a default set (`docs/design.md` § Patch storage).
- Build the patch transition, the accent and their times in the brain.
- Read the faders and mix their targets the way the editor does.
- Read the key-held CC for tap versus hold.

## Controller firmware

- Write it for the second Teensy: keypad as a static code, two-key rejection, faders, pad, rockers, rotary, tap tempo, fresh clock, indicator pixels, DIN out.
- Find out whether the touchpad reads pressure usefully.
- Then the acceptance test: play a full DJ set to Justice, "Women Worldwide", on the rebuilt controller.

## Editor

- Use it at the bench: is driving a morph's CCs at frame rate too much USB traffic?
