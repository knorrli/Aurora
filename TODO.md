# Aurora TODO

**What this file is for.** The design docs hold the thinking — settled or
open, with the reasoning. This file holds only the doing: one line per
item, an action and a pointer, no argument. An open question with no next
action does not appear here at all; it lives in its doc until someone
decides to act on it.

The phase plan that used to live here was built on design decisions
cleared on 2026-09-18; it is recoverable at commit `33f2d1f`.

Everything currently lives on the **`preset-redesign`** branch. `main` is
untouched.

---

## What is built and proven

- **Brain on Teensy 4.0.** Builds clean, 76 KB of 2 MB.
- **All five strips lit**, each on its own supply, through a 74AHCT125.
- **USB MIDI path validated** — program change, CC, clock, notes,
  transport, tempo division, free-run on clock loss.
- **DMX out working** end to end. Two PARs addressed at `A001` and
  `A009`, 2026-09-21.
- **The nine hand-written patterns and all nine variants** render, on
  PC 1–9. See `docs/visual-design.md`.
- **The parametric generator** on PC 10, with the redesigned color layer
  in the firmware as of 2026-09-21. See `docs/generator.md`.
- **The pulse's destinations**, 2026-09-22. One oscillator with one rate
  reaching the strips' brightness, width and hue and the washes' level, hue
  offset and saturation, each with its own amount and its own wave. Its
  phase is anchored to the bar and its rate is stepped to the periods a bar
  can hold. Flashed but **not yet seen on the wall**. See
  `docs/generator.md` § "Where the pulse reaches".
- **The bench panel** — `tools/index.html`, drawn as the signal flow,
  with patch save/recall, the morph control and a row of color looks. The
  pulse is a third branch on it, since it reaches both the others and the
  washes.
- **The wall on screen** — `tools/preview.js`, five strips and four PARs
  rendered from a port of the firmware, so a look can be dialed with
  nothing plugged in. The color layer was designed here before it was
  flashed.
- **A strip-order rigging aid** on PC 11 — each strip a flat color in
  data-chain order.

Measurements in `docs/bench-facts.md`.

## What is not built

- **The brain's perfboard.** Everything is still on a breadboard.
- **DIN MIDI in on the brain** (6N138 circuit on `Serial1`).
- **The brain enclosure.** Blocked on the perfboard — the box should be
  cut around the finished board, not before it.
- **The controller.** Still the old Nano box. The second Teensy is
  untouched and its pin map has never been drawn.
- **The foot pedal.**

## Parts

Checked items are confirmed in the drawer.

- [x] **Teensy 4.0 × 2** — brain on hand and running, controller
      ordered 2026-09-05.
    - [ ] *Deliberately skipped:* a third as a gig-bag spare. At CHF 51
          in Swiss retail it was judged too expensive; revisit via
          pjrc.com, Play-Zone or Pi-Shop. Note this leaves the
          "one spare board covers either failure" argument for the
          two-Teensy choice unfunded.
- [ ] **Header pins.** Teensy ships bare. Male 1×40 2.54 mm ordered
      2026-09-05; female sockets on hand. **Socket both Teensys rather
      than soldering them down** — a spare board only helps if a dead
      one can be swapped without a soldering iron at the venue.
- [x] **74AHCT125** × 2 — one fitted on the brain, one for the
      controller's indicator pixels. Must be HCT or AHCT; plain HC or
      AHC has the same threshold as the pixels and fixes nothing while
      looking identical on the shelf.
- [x] **6N138 opto-coupler** × 2, for DIN MIDI in on both nodes.
- [x] **5-pin DIN MIDI jacks**, panel-mount, more than three on hand.
- [x] **5-pin DIN MIDI cable** — confirm it spans the real stage
      distance plus slack.
- [x] **Resistors** — assorted pack covers every value needed.
- [x] **1N4148 / 1N914A signal diode** × 2 for MIDI in protection.
      Interchangeable here: same 200 mA fast small-signal part, 75 V
      reverse instead of 100 V, against a few volts of MIDI.
- [x] **Perfboard** for both assemblies.
- [x] **Tap tempo momentary switch** — reuse the existing one, it has a
      built-in LED.
- [x] **M5Stack DMX Unit (U183)** — isolated transceiver, isolated
      DC-DC, surge protection, switchable termination and the XLR-3
      socket in one part.
- [x] **Grove→Dupont cable** — on hand and proven. Strip the Dupont ends
      and solder to the perfboard; the module still unplugs at its own
      Grove socket, which is the end that matters at a venue. Leave
      slack and strain relief.
- [x] **DMX cable, XLR 3-pin** — on hand and working. Buy real 110 Ω
      cable for stage use.
- [x] **120 Ω terminating resistor** — belongs at the last fixture, not
      on the brain board.
- **Foot pedal parts.** Switches and guitar cables on hand. Ladder
  resistors (1 k / 2.2 k / 5.1 k / 10 k / 20 k, 1 %) on hand.
    - [ ] **The connector is open again.** Settle whether it is mono or
          stereo before drilling anything — see `docs/wiring.md` §
          "Foot pedal".

## Hardware work that does not wait on the design

- [ ] **Set the remaining two BCC145 to `A017` and `A025`.** Watch the
      personality as well as the number — see `docs/wiring.md` §
      "Fixture profile".
- [ ] **Calibrate the fixtures** at rehearsal — RGB trims, and whether
      FastLED's brightness curve suits the fixture. Step **evenly
      spaced** values. See `docs/bench-facts.md`.
- [ ] **Set the per-fixture master scale by eye** at soundcheck. See
      `docs/visual-design.md` § "The washes".
- [ ] **Aim each PAR at the wall between two strips**, not at a strip.
      See `DESIGN.md` § "The PAR cans".
- [ ] **Look at whether a pool bridges a gap.** One slow sweep and one
      fast one, narrow window. See `DESIGN.md` § "The PAR cans".
- [ ] **Press two keys at once and watch the wall.** Needs no rewiring.
      See `DESIGN.md` § "What the keypad actually is".
- [ ] *Deferred to the controller rebuild:* confirming what idle reads
      and what produces `0b00111111` and `0b00111101`. Getting at the
      lines means desoldering brittle keypad wiring, so they wait until
      the pad is off the box anyway.

## Generator, next

None of this is committed to — it is an experiment that earned a second
session. Ordered by what blocks what. Reasoning in `docs/generator.md`.

- [ ] **Dial in five or six endpoints by eye and save them.** Nothing
      else about the morph is worth judging until these exist. See
      `docs/generator.md` § "The panel's roster settings are guesses".

**The color layer has been seen on a wall and holds up**, and the
preview it was designed in matches the strips closely enough to keep
designing there. See `docs/bench-facts.md`.

- [x] **Judge the color layer on the wall.** Done 2026-09-21. Flat is
      flat, and the preview matches the strips almost exactly — a touch
      paler on the wall, which is the diffuser. See `docs/bench-facts.md`
      § "The color layer's flat state is flat" and § "The screen and the
      wall agree".
- [ ] **Average the placed field across each pixel**, as the shape branch
      already does. It is read once at the pixel center, so it aliases
      above a region count the grid can carry. Same fault and same fix as
      `docs/bench-facts.md` § "Point-sampling a pattern aliases".
- [ ] **Turn a region inside out.** One boolean, and it is what "base
      color on the center strip, outer ones departing" needs. See
      `docs/generator.md` § Open, item 8.
- [ ] **Thread the placed field's parameters as an argument.** A second
      placed field is a small refactor until this is done, and was
      designed for on the assumption it would be free. See
      `docs/generator.md` § Open, item 10.
- [ ] **Jitter in color** — a starfield in hue rather than in
      brightness. The only randomness the color layer has no way to
      make. See `docs/generator.md` § "The color layer has no jitter".
- [ ] **Give the PARs their own saturation.** CC 62; the wash block has
      room. A scale down from the strips' saturation rather than a setting
      of its own, per `DESIGN.md` § "The PAR cans". The pulse already
      pushes their saturation, off the S fader for want of anywhere else
      to measure from, so this would give that push its own origin — and
      give the read-only "the pulse reaches this" marker somewhere to sit,
      which is the one destination currently without one.
- [ ] **Reach the PARs with more than a hue offset.** The PARs following
      a gradient with the strips, which needs the color layer sampled at
      each PAR's position — today all four are one color. The white flash
      between strip strobes, the other half of this, became reachable on
      2026-09-22 when the pulse gained their saturation. See `DESIGN.md`
      § "The PAR cans".
- [ ] **Fan shape and jitter scale — one piece of work.** Fan gains a
      center and a random setting; jitter gains a scale from pixel to
      cell, and a rate that is not the pulse's. Together they are the
      chaotic strobe, and the fan half is also why Rain and Comet are the
      same look. See `docs/generator.md` § Open, "Fan is a linear
      staircase" and "Jitter has one scale", and `docs/visual-design.md`.
- [ ] **Judge the pulse's destinations on the wall.** Everything else about
      them is guesswork until this happens. Three looks are what it was
      built for, and each is one patch: the washes swelling under still
      strips, a white flash on the washes between strip strobes, and the
      shapes breathing on width while the light holds. See
      `docs/generator.md` § "Where the pulse reaches".
- [ ] **Judge the anchor against a click.** The *peak* is what lands on the
      beat, because "a deep slow swell peaks wherever it happens to" was
      the complaint. A square is that swell clipped around its own
      midpoint, so its flash is centered on the beat rather than starting
      there — at a two-beat period the light comes on half a beat early.
      Skew moves the flash inside the cycle and is the control to reach
      for. Whether the leading edge is the better thing to anchor is a
      question a click track answers and a bench cannot. See
      `docs/bench-facts.md` § "A phase derived from absolute time
      teleports".
- [ ] **Judge the stepped rate.** Thirteen positions where there were 128.
      The dotted values — 12, 6, 3, 1½, ¾, ⅜ beats — come back to the
      downbeat every three bars rather than every one, which is a musical
      relationship and may still read as adrift. Cutting them would leave
      seven positions, all powers of two.
- [ ] **Travel easing** — a Shape curve in Travel, beside Speed and Fan.
      See `docs/generator.md` § "Travel easing is a curve".
- [ ] **Preserve position when a switch flips.** Turning bounce on moves
      the core 12 px of 45 on every strip; alternate moves the odd strips
      16 px. Solve for the phase that leaves the shape where it stands.
      See `DESIGN.md` § "Switches belong to the patch".
- [ ] **Refuse to save a morph target whose switches differ** from its
      patch's, in `tools/index.html`. Same section.
- [ ] **Decide whether a wrapping strip is a loop or a line.** Settled
      for bounce, open for wrap. See `docs/generator.md` § Open, "Is a
      strip a loop or a line?".
- [ ] **Decide the color layer's usable ranges.** The test is a set, not
      a bench. See `docs/generator.md` § Open, item 6.
- [ ] **Test the touchpad window from the laptop, before any rewire.**
      See `DESIGN.md` § "Which strips — a window, not a selection".

## Open discussions

Raised in conversation and not yet settled. Each needs a decision before
it becomes a build item.

- [ ] **Editor layout.** `tools/index.html` grew the preview and the color
      controls without a regroup, and it is cramped. Two boxes earn no
      space: the one holding only the "send 120 BPM clock" button, never
      used, and "Strips", which is empty. The question is what the
      groupings should be, not where today's boxes go.
      The pulse left the shape branch on 2026-09-22 and became a third
      branch of its own, because it reaches the color layer and the washes
      as well and could no longer sit inside one of the two things it
      pushes on. Whether the join beneath still reads — it says "the two
      multiply", which is true of shape and color and says nothing about
      the third box now above it — is part of this question.
- [ ] **The wander's rate is the odd speed control.** Travel speed and the
      placed field's speed are both bipolar and squared: 64 is still,
      either side moves, and the slow end gets most of the fader because
      that is where a color reading as depth rather than as an effect
      lives. The wander's is unipolar and linear — 0 is frozen, the throw
      is even. The squared taper's reasoning applies to it unchanged, so
      at least that should match. Whether it should go bipolar too is a
      real question, since a noise field running backwards looks much like
      one running forwards; if it stays unipolar the label should say so.
      Compare `setWanderRate` against `setPlacedSpeed` in
      `brain/src/P_Generator.cpp`.
- [ ] **"How fast" and "How big" are named as questions.** Every other
      control in both branches is named for the thing it sets — Width,
      Count, Edge, Tail, Speed, Fan, Depth, Rate, Skew, Shape. These two
      are not. "How fast" is Rate or Speed depending on the item above.
      "How big" is the wander's spatial scale, the whole wall moving as
      one down to individual pixels; Scale is the obvious noun but has
      never been held against what the control actually does.
- [ ] **The color panel does not say what its switches govern, and four
      controls are dead in the default state.** Gradient / region and the
      three rulers drive the placed field and nothing else —
      `placedIsRegion` and `placedRuler` are read nowhere outside that
      path in `brain/src/P_Generator.cpp` — but they sit above three
      subsections and read as though they govern all three.
      Worse, `placedAt` returns on its first line under gradient, so Count,
      Width, Edge and Speed — four of the seven controls under "What you
      place" — do nothing at all whenever that switch is on gradient, which
      is where it starts. The preview agrees, so they really are inert
      rather than merely subtle. Found by playing the panel and wondering
      why the faders did nothing.
      Two ways out, and the panel rework has to pick one. Indicate it —
      dim the four while gradient is selected, the way a bypassed branch
      already dims, so a control that cannot do anything stops looking
      like it should. Or give them a meaning there, which only one of the
      four can take: Count has nothing to repeat and Width nothing to
      size, since a gradient spans the ruler once by definition, and Speed
      cannot move it without wrapping a monotone ramp and putting a hard
      jump where the two ends meet. Edge is the one that could — bending
      the ramp from straight to eased is the same idea as pulse shape and
      travel easing, which would make it a fourth use of one word rather
      than a new control.
      Two more open parts. The subsection names "What you place", "What
      lives" and "From the light level" are the design's own words and are
      longer than the controls under them. And the ruler buttons name the
      strips where they could name the wall: "across the strips" is
      horizontal and "along a strip" is vertical, since the five stand
      spaced across the stage against the back wall. Whether that swap is
      an improvement is not obvious — "within a shape" is not a direction
      at all, so the three would stop being one vocabulary.
- [ ] **Travel easing — is the look wanted?** Built shape is settled in
      `docs/generator.md` § "Travel easing is a curve, not a modulation
      route", and it is a build item above. What has never been discussed
      is whether a band backdrop asks for a thrown-ball traversal.
- [ ] **Gate length on the pulse, as the far end of Shape.** A short stab
      rather than an even square. Wanted from both sides independently, so
      the want is real. Not a control of its own: fold it into the bottom
      of the Shape fader, so the one axis runs stab -> square -> swell ->
      sine. That is what makes it work — a gate only has a length where the
      wave is square, and putting it anywhere else on the fader would mean
      shifting the threshold at the sine end, which flat-bottoms the swell
      and stops it reaching full. Costs: the sine half loses a quarter of
      the fader, so the midpoint drifts from CC 93 back to about 102, and
      Strobe, Stutter and Glitch all move off 0.
      **Bounded by the frame rate, not by taste.** At 7-8 ms per frame
      (`docs/bench-facts.md`) and the fastest rate of 0.25 beats, a 6 %
      stab at 120 BPM is 8 ms, which is one frame — below that it lands
      between frames and flickers instead of shortening. A gate measured
      in percent of the cycle is therefore reliable at slow rates and not
      at fast ones; a floor derived from frame time would fix that.
      Build it only if a shorter stab is still wanted after playing with
      the linear taper, since the dead bottom of the fader that made this
      attractive is largely what the taper reclaimed.

- [ ] **Fan's full shape, and a rate offset beside it.** `docs/generator.md`
      § Open, item 1 argues the two are one piece of work, and that a rate
      ratio of -1 on the odd strips reproduces alternate exactly. Whether
      that should dissolve the alternate switch is already settled as no,
      in `DESIGN.md` § "Switches belong to the patch"; the look itself is
      undiscussed.

## Known defects

- [ ] **Glitch's white pixels ignore the V fader.** The 30 % white share
      cannot be tuned until this is fixed. See `docs/bench-facts.md`.

## Housekeeping

- [ ] **Regroup the CC table.** The blocks were laid out before most of
      what uses them existed, and have only been added to since. Go
      through the whole list and realign it. The pulse now reads as two
      blocks — 77 to 80 for the strips' brightness and 100 to 114 for
      everything else — which is the clearest case in the file for a
      regroup and the reason not to do one piecemeal. See
      `shared/aurora_protocol.h`.

## Key files

| When you want to… | Open |
|---|---|
| Know what we measured | `docs/bench-facts.md` |
| Know how the system is built | `docs/architecture.md` |
| Know what the lights do | `docs/visual-design.md` |
| Know how the generator works | `docs/generator.md` |
| Look up a pin or a circuit | `docs/wiring.md` |
| Look up a CC / PC / note number | `shared/aurora_protocol.h` |
| Work on how it is played | `DESIGN.md` |

## Key commands

```bash
cd brain      && pio run -e teensy40 -t upload   # flash the brain
cd controller && pio run -t upload               # future: controller node
pio device monitor                               # serial console
```
