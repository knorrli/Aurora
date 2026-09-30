# Aurora TODO

## Hardware

- Move the brain off the breadboard onto perfboard.
- Build DIN MIDI in on the brain: 6N138 on `Serial1`.
- Cut the brain's enclosure around the finished perfboard.
- Socket both Teensys rather than soldering them down.
- Rebuild the controller on the second Teensy and draw its pin map (`docs/hardware.md`). Give the peak follower's on/off toggle a pin if one is free.
- Build the foot pedal.
- When the keypad is off the box: meter the lines before rewiring (D8 common, contacts passive), and confirm the idle code and what produces `0b00111111` and `0b00111101`.
- Press two keys at once on the old box and watch the wall: does 2+3 black it out?
- Buy real 110 Ω DMX cable for stage.

## PARs

- Set the remaining two BCC145 to `A017` and `A025`, watching the personality (`docs/hardware.md`).
- Calibrate the fixtures at rehearsal: RGB trims and the brightness curve, stepping evenly spaced values.
- Set each fixture's master scale by eye at soundcheck.
- Aim each PAR at the wall between two strips, addressed `A001` to `A025` in stage order.

## On the wall

Seen only in the preview so far. One look each, driven from the editor.

- Palettes: dark ends of Cyberpunk, Space, Nature and TV; Art's pastel at S full; mirrored Sky 35; the PARs' hue offset inside a three-color palette.
- PARs over cyan strips flowing green to blue: all swinging together, a still gradient, a gradient moving on a ripple, ripple in random mode, a steps strobe over each of those, random per pulse on steps.
- Arp modes on steps, one each: sequence, bounce, evens / odds, pairs, mirror, random; reverse on the ones with a direction.
- Try to defeat the blackout: transport stopped, mic trigger firing, PARs at full. Then any key brings the wall back.
- Region inside out across the strips at count 1, then the dark case with V at the top.
- Swipe Position with a rise wave: climbs, drops back on the bar, tail drops at the fall. Try Bend on it.
- The fan: a quarter-turn phase as a chevron (does it get Rain back?), staggered bars strobing in unison, strips at different rates (alive or coming apart?), Randomize (wants a seed?).
- White and Dark amounts from 0 up with S full and V at the top; Field and Flow White together; does Light Dark earn its place?
- Swing a rate: a sine on the fan's rate spread; the hypno look on speed and rate spread together.
- Morph between patches of different tempo divisions: does the jump read as a glitch?
- Afterglow: tail stays behind through a swing, shrinks as it slows, gone at rest; brain frame time; no streak across a patch change.
- LFO routes: PARs swelling under still strips, a white flash on the PARs between strip strobes, shapes breathing on width.
- Anchor against a click track: should the peak or the leading edge land on the beat?
- The scatter on the strips, and whether spots need to outlive their cell.
- Play a set to find where the color layer's controls should stop.
- Flow on darkened regions, strips against the preview: the preview drew it far stronger before it drew dim colors through the screen's curve.
- Dial five or six endpoints by eye and save them; nothing about the morph is worth judging before.

## Generator

- Hold every PAR pulse to at least 25 ms (`docs/hardware.md`). The renderer knows beats, not milliseconds.
- Measure the fan's delay against each route's own cycle. It sits on the master LFO, so a route at 2× doubles every strip's delay.

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

- One pass over every control: does the readout and its unit make sense; would it, alone or with others, help as a wall overlay; does it belong in its panel or in one of its own; is its color, and its overlay's, the right one.
- Controls that modulate only upward (Hue offset, Bend at): make them bipolar, or help center the swing by marking the fader value mirrored across center, or snapping the amount to it.
- Let a morph target set a "from the patch" switch by writing it into the base patch: setting arp mode while routing PAR hue range on the accent should not need a trip to base.
- Save what the wall shows as any part, base or a target, of this patch or a new one, then keep refining and save it again anywhere. Replaces copy from / move onto / swap with.
- Shrink `AURORA_PATCH_NAME_LENGTH` to 13 at the next patch format change; the editor already stops at 13.
- Rebuild starting-point looks on routes, the LFO and the scatter. The old tables (shapes, fan looks, swing looks, color looks) are in `git show 21a435c:tools/patch.js`.
