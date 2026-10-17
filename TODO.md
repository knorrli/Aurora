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
- Strip routes: a quarter-turn phase as a chevron (does it get Rain back?), staggered bars strobing in unison, a random draw across the strips. Does the random draw want a seed?
- White and Dark amounts from 0 up with S full and V at the top; Field and Flow White together; does Core Dark earn its place?
- Morph between patches of different tempo divisions: does the jump read as a glitch? Should it carry position across instead?
- Snap a press to the nearest beat with a click track: does a press just after the beat read as on it?
- Morph between real looks: does the morph need per-parameter timing rather than every parameter in lockstep? Should a patch choose to land its visible switches on the press rather than the release?
- Afterglow: tail stays behind through a swing, shrinks as it slows, gone at rest; brain frame time; no streak across a patch change.
- LFO routes: PARs swelling under still strips, a white flash on the PARs between strip strobes, shapes breathing on width.
- Anchor against a click track: should the peak or the leading edge land on the beat?
- Play a set to find where the color layer's controls should stop.
- Flow on darkened regions, strips against the preview: the preview drew it far stronger before it drew dim colors through the screen's curve.
- Dial the starting looks by eye and save them as library patches; nothing about the morph is worth judging before. The old shape, fan, swing and color looks are in `git show 21a435c:tools/patch.js`.

### PARs

- Set the remaining two BCC145 to `A017` and `A025`, watching the personality (`docs/hardware.md`).
- Give each PAR a different color and check the preview shows them in the same order as the wall.
- Does a PAR want FastLED's squared value curve? Compare a strip and a PAR side by side at evenly spaced values.

### Controller

- Flash the rebuilt controller and walk every control: keys and hook, rotary banks, faders and Cue, pad and its four rockers, pedal switches, TAP, TRIG and the mic, the twelve pixels.
- Re-measure the touchpad's raw X and Y range at 3.3 V; the controller still uses the Nano's.
- Set the box pixels' brightness: lit 25 %, dim 5 %, under the finger full.
- Press two keys at once on the old box and watch the wall: does 2+3 black it out?
- On the rebuilt controller's keypad: is tap versus hold, judged 200 ms after the later of the press and its beat, the right length?
- The acceptance test: play a full DJ set to Justice, "Women Worldwide", on the rebuilt controller.

## At rehearsal

- Calibrate the fixtures: RGB trims and the brightness curve, stepping evenly spaced values.
- Set each fixture's master scale by eye at soundcheck.
- Aim each PAR at the wall between two strips, addressed `A001` to `A025` in stage order.
- Read what the drum triggers send: note on with velocity, and which note is the kick.
- Record the band and check its kicks against a sixteenth grid: how tight the double hits are, and whether any section moves to a pulse that is not sixteenths.
- Tune tempo following (`docs/design.md` § Tempo following): how close to a sixteenth a kick must land to count, and how much of the error each counted kick corrects.

## Away from the wall

### Cells

`docs/design.md` § Cells and § Brightness; the reasons in `docs/cells-reasoning.md`, deleted once cells are built and tested on the wall.

- Dial cells in the preview. On this branch the controls list is 192 long and routes sit at 128 and up, eight fields each; the brain builds but its patch sync and storage wait for the CC numbers.
- Confirm or change what the first build settled on its own:
  - Rate's first step, born once, is born when the patch starts, so its attack plays then.
  - Gate grows toward the top of the fader, unlike LFO Rate.
  - Width runs 0–45 px and Edge adds 0–11 px of glow on each side; Position's middle is the strip's center.
  - Chord runs 1–16.
  - Only Position, Width, Edge, Brightness, Hue and Saturation take a value per cell. An envelope aimed elsewhere does nothing; Held aimed elsewhere plays live; a held pad route plays live.
  - Envelope stages are straight lines, with times from 1⁄16 to 16 beats.
  - A strip draws at most 64 cells, the newest; a fast Rate with a long life drops the oldest.
  - Cells overlapping take the brighter one, color included.
  - A gone cell's tail fades out after it.
  - An envelope set to step moves only on Rate's grid; with Rate on once it glides.
  - Polarity is its own route field; a route's destination holds the target, or another route's amount or phase.
  - A group route's pattern is picked on the route's Wave field, shown as a Pattern row.
  - A negative group amount on a one-sided control reverses the pattern and still pushes up: dimming every other bar means dialing Brightness low and pushing the others up.
  - A route aimed at an amount moves it by twice its own amount, so a full Strip route scales a journey from −1 to +1. Aimed at a phase, a full route spreads the group over one whole cycle.
  - Birth's Random draws a value per birth rather than shuffling. Births per LFO cycle count whole births.
  - Routes on Delay read at the moment the birth would have landed undelayed; Note and Birth routes aimed at controls that are not per cell do nothing.
  - The travel's ends include Edge's glow, and follow the cell's width as it changes, so a growing cell at an end stays flush.
  - Born once with Inside, a cell leaving the travel jumps to the other end; only Outside draws it across.
- Envelopes on any control: what starts one outside a cell? On a oneshot, the fire, with its length as the gate, which could replace Cycle once there. On a patch, each birth? Once also loops a wave once per LFO cycle on patch routes.
- The brightness curve overlay draws Vertical and Horizontal; what do Cell and Cell position show?
- Do envelopes need curves or steps inside a stage? A raindrop speeds up as it falls.
- A random source for routes; held, it gives each cell its own value.
- Do LFO routes also run per cell, on each cell's age, the way scatter routes run on each spot's clock?
- Two groups of cells, one painting over the other? Raindrops: born at random places, falling with a trail.
- Scatter limited by the cells: a switch for spots only inside cells, only outside them, or everywhere.
- Scatter's Value is not squared like a cell's brightness, so a spot at 50% looks brighter than a cell at 50%. Square it too?
- Build advanced patches together in the preview: cells dividing together and taking turns, five bars alternating bright and faint, the stepped trail.
- Should Strip's and Note's Random draw a new order on each birth, and on each start over, instead of one fixed shuffle?
- "Keep as… Motion" on the wall seemed to overwrite the base layer while dialing the alternating diagonals. Reproduce, then discuss how saving a wall look into a layer should work.
- Editing the base while a layer fader is up in the Mix bar: rows show the played value, so a control that layer overrides snaps back while dragged. What should its slider show and move?
- Can a oneshot play one cell?
- Does Field's Dark overlap too much with the brightness curve?
- Test looks: bars born at the center that grow, divide and move out both ways, together or taking turns; slots of 5 pixels where a cell is born each beat one slot lower and fades over the next three; a faint bar pattern with Scatter's white spots flashing over it.

### Hardware

- Move the brain off the breadboard onto perfboard.
- Build DIN MIDI in on the brain: 6N138 on `Serial1`.
- Cut the brain's enclosure around the finished perfboard.
- Socket both Teensys rather than soldering them down.
- Rebuild the controller on the second Teensy (`docs/hardware.md` § Controller — Teensy 4.0).
- Find out whether the touchpad reads pressure usefully.
- Replace the peak-follower board with an input stage that brings the handset mic to one analog pin at 3.3 V.
- Once the mic's input stage exists, set the mic trigger by ear (`controller/src/mic.cpp`): how fast the level falls, the 5 ms peak window, the 80 ms rest, and how the level scales to velocity.
- Relabel the rotary's twelve positions A to L.
- Cap the faders Color red, Motion green, Extent blue.
- Fit a 3-way rocker as touchpad rocker 2 (Width).
- Build the eight-switch foot pedal on a TRS cable (`docs/design.md` § Foot pedal).
- When the keypad is off the box: meter the lines before rewiring (D8 common, contacts passive), and confirm the idle code and what produces `0b00111111` and `0b00111101`. The controller's pin map reads the keypad on four lines and leaves D8 unread; if D8 is needed, a panel switch moves to the underside.

### Controller

- Where is the rig's kick note set: in the editor, or on the box?
- Does TRIG fire the oneshot the mic switch picks, or always Oneshot 1?

## Once Aurora v2 works

- Compile in a default set (`docs/design.md` § Patch storage).
