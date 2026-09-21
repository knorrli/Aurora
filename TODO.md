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
- **The parametric generator** on PC 10, with the redesigned colour layer
  in the firmware as of 2026-09-21. See `docs/generator.md`.
- **The bench panel** — `tools/index.html`, drawn as the signal flow,
  with patch save/recall, the morph control and a row of colour looks.
- **The wall on screen** — `tools/preview.js`, five strips and four PARs
  rendered from a port of the firmware, so a look can be dialled with
  nothing plugged in. The colour layer was designed here before it was
  flashed.
- **A strip-order rigging aid** on PC 11 — each strip a flat colour in
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

**The colour layer has never been seen on a wall.** It was designed and
dialled in `tools/preview.js` and ported unjudged. Everything below it
here waits on that first look.

- [ ] **Judge the colour layer on the wall.** Flat first — every control
      centred should give exactly the faders' colour — then `Rainbow` on
      Fill, then `Comet tail` on Comet. A screen has been wrong before
      about desaturation, the dark floor and red's resolution. See
      `docs/bench-facts.md`.
- [ ] **Average the placed field across each pixel**, as the shape branch
      already does. It is read once at the pixel centre, so it aliases
      above a region count the grid can carry. Same fault and same fix as
      `docs/bench-facts.md` § "Point-sampling a pattern aliases".
- [ ] **Turn a region inside out.** One boolean, and it is what "base
      colour on the centre strip, outer ones departing" needs. See
      `docs/generator.md` § Open, item 8.
- [ ] **Thread the placed field's parameters as an argument.** A second
      placed field is a small refactor until this is done, and was
      designed for on the assumption it would be free. See
      `docs/generator.md` § Open, item 10.
- [ ] **Jitter in colour** — a starfield in hue rather than in
      brightness. The only randomness the colour layer has no way to
      make. See `docs/generator.md` § "The colour layer has no jitter".
- [ ] **Give the PARs their own saturation.** CC 62; the wash block has
      room. A scale down from the strips' saturation rather than a setting
      of its own, per `DESIGN.md` § "The PAR cans".
- [ ] **Reach the PARs with more than a hue offset.** A white flash
      between strip strobes, and the PARs following a slide with the
      strips. Both asked for, neither reachable. See `DESIGN.md` § "The
      PAR cans".
- [ ] **Fan shape and jitter scale — one piece of work.** Fan gains a
      centre and a random setting; jitter gains a scale from pixel to
      cell, and a rate that is not the pulse's. Together they are the
      chaotic strobe, and the fan half is also why Rain and Comet are the
      same look. See `docs/generator.md` § Open, "Fan is a linear
      staircase" and "Jitter has one scale", and `docs/visual-design.md`.
- [ ] **Width as a third pulse destination.** One multiply. See
      `docs/generator.md` § "The pulse drives brightness only".
- [ ] **Travel easing** — a Shape curve in Travel, beside Speed and Fan.
      See `docs/generator.md` § "Travel easing is a curve".
- [ ] **Make Count double rather than add across a morph.** See
      `docs/generator.md` § Open, "Morph moves every parameter in
      lockstep".
- [ ] **Decide whether a wrapping strip is a loop or a line.** Settled
      for bounce, open for wrap. See `docs/generator.md` § Open, "Is a
      strip a loop or a line?".
- [ ] **Decide the colour layer's usable ranges.** The test is a set, not
      a bench. See `docs/generator.md` § Open, item 6.
- [ ] **Test the touchpad window from the laptop, before any rewire.**
      See `DESIGN.md` § "Which strips — a window, not a selection".

## Known defects

- [ ] **Glitch's white pixels ignore the V fader.** The 30 % white share
      cannot be tuned until this is fixed. See `docs/bench-facts.md`.
- [ ] **`PRESET_OFF` alone no longer darkens the washes.** The
      controller's "off" key must send `PC 0` and `CC_WASH_LEVEL` 0
      together, or a blackout leaves the PARs lit. The contract lives
      only in `shared/aurora_protocol.h`.
- [ ] **Anchor the pulse's phase to the bar.** It is in time but not on
      time. See `docs/bench-facts.md` § "A phase derived from absolute
      time teleports".
- [ ] **Drop `-D AURORA_DEBUG`** from `brain/platformio.ini` once the
      strips are what gets read instead of the console.

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
