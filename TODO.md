# Aurora TODO

## At the wall

### Brain

- Reflash the brain. Its flash predates the scatter rework and the last CC renumbers, so wall and preview disagree until then.
- Run the patch sync against the brain from `tools/protocol.html`: empty on a fresh flash; push eight and read back; a half library is refused and the old one survives; round-trip a file; pull the mains mid-sync; time a full 108.
- From the editor, push a library with a kit and pull it back: the patches' picks, the kit and the default pair come back.
- Play a song from the editor's songs view with the clock on: the wall follows the preview, tap, hold, accent and blackout included.
- Dial with the brain attached: the wall matches the preview on a layer tab, mid-scrub and while editing a oneshot's controls; a key, a fader or a oneshot hands playing back to the brain.

### Looks

Seen only in the preview so far. One look each, driven from the editor.

- Palettes: dark ends of Cyberpunk, Space, Nature and TV; Art's pastel at S full; mirrored Sky 35; the PARs' hue offset inside a three-color palette.
- PARs over cyan strips flowing green to blue: all swinging together, a still gradient, a gradient moving on a ripple, ripple in random mode, a steps strobe over each of those, random per pulse on steps. Do the PARs want different speeds, for an oscillation that looks random?
- The five strobe looks: strips in unison with the PARs chasing left to right; strips chasing with the PARs in unison; one side strobing while the other sits full on or off; strips and PARs on different waves; the same at different rates.
- Arp modes on steps, one each: sequence, bounce, evens / odds, pairs, mirror, random; reverse on the ones with a direction.
- Try to defeat the blackout: transport stopped, mic trigger firing, PARs at full. Then any key brings the wall back.
- Region inside out across the strips at count 1, then the dark case with V at the top.
- Swipe Position with a rise wave: climbs, drops back on the bar, tail drops at the fall. Try Bend on it.
- Bipolar routes: Bend at wobbling the bend point, the PARs' hue offset swinging either side of the strips, a square on a hue at full amount as a flip to the opposite color.
- The fan: a quarter-turn phase as a chevron (does it get Rain back?), staggered bars strobing in unison, strips at different rates (alive or coming apart?), Randomize. Does a fan whose strips drift apart want a per-patch drift reset, and does its random draw want a seed?
- White and Dark amounts from 0 up with S full and V at the top; Field and Flow White together; does Core Dark earn its place?
- Swing a rate: a sine on the fan's rate spread; the hypno look on speed and rate spread together.
- Morph between patches of different tempo divisions: does the jump read as a glitch? Should it carry position across instead?
- Snap a press to the nearest beat with a click track: does a press just after the beat read as on it?
- Morph between real looks: does the morph need per-parameter timing rather than every parameter in lockstep? Should a patch choose to land its visible switches on the press rather than the release?
- Afterglow: tail stays behind through a swing, shrinks as it slows, gone at rest; brain frame time; no streak across a patch change.
- LFO routes: PARs swelling under still strips, a white flash on the PARs between strip strobes, shapes breathing on width.
- Anchor against a click track: should the peak or the leading edge land on the beat?
- The scatter on the strips. Do spots need a lifetime so they can travel past their cell (raindrops)? It costs eight to ten CCs.
- Play a set to find where the color layer's controls should stop.
- Flow on darkened regions, strips against the preview: the preview drew it far stronger before it drew dim colors through the screen's curve.
- Dial the starting looks by eye and save them as library patches; nothing about the morph is worth judging before. The old shape, fan, swing and color looks are in `git show 21a435c:tools/patch.js`.

### PARs

- Set the remaining two BCC145 to `A017` and `A025`, watching the personality (`docs/hardware.md`).
- Give each PAR a different color and check the preview shows them in the same order as the wall.
- Does a PAR want FastLED's squared value curve? Compare a strip and a PAR side by side at evenly spaced values.

### Controller

- Press two keys at once on the old box and watch the wall: does 2+3 black it out?
- On the rebuilt controller's keypad: is tap versus hold, judged 200 ms after the later of the press and its beat, the right length?
- The acceptance test: play a full DJ set to Justice, "Women Worldwide", on the rebuilt controller.

## At rehearsal

- Calibrate the fixtures: RGB trims and the brightness curve, stepping evenly spaced values.
- Set each fixture's master scale by eye at soundcheck.
- Aim each PAR at the wall between two strips, addressed `A001` to `A025` in stage order.

## Away from the wall

### Hardware

- Move the brain off the breadboard onto perfboard.
- Build DIN MIDI in on the brain: 6N138 on `Serial1`.
- Cut the brain's enclosure around the finished perfboard.
- Socket both Teensys rather than soldering them down.
- Rebuild the controller on the second Teensy and draw its pin map (`docs/hardware.md`). Give the peak follower's on/off toggle a pin if one is free.
- Relabel the rotary's twelve positions A to L.
- Fit a 3-way rocker as touchpad rocker 2 (Width).
- Build the eight-switch foot pedal on a TRS cable (`docs/design.md` § Foot pedal).
- When the keypad is off the box: meter the lines before rewiring (D8 common, contacts passive), and confirm the idle code and what produces `0b00111111` and `0b00111101`.

### Controller

- Write the firmware for the second Teensy: keypad as a static code, two-key rejection, hook note, faders, pad, rockers, rotary, foot pedal, tap tempo, fresh clock, indicator pixels, DIN out.
- Store songs on the controller, received from the editor over USB, and step through their sections on the pedal (`docs/design.md` § Songs).
- Send a key as note 57 on, the Program Change of its bank and key, then note 57 off on release (`docs/design.md` § Recalling a patch).
- Find out whether the touchpad reads pressure usefully.
- Per-patch pad mode: any two controls, or a fixed list of pairings?
- Which four corner looks, and which five effects in which order?
- Focus, taking energy away from part of the wall: one of the effects, or something that works in every mode?
- What does the fader rocker do?
- What do the indicator pixels show? Lead: the current song section.

### Songs in the editor

- Push the gig's songs to the controller together with the patches to the brain.

### Patches on the brain

- Give the hook a note number, and read it on the brain: dark at once, back on release.

## Once Aurora v2 works

- Compile in a default set (`docs/design.md` § Patch storage).
