# Aurora TODO

Hardware state, parts, and work that does not depend on the interaction
model. The phase plan that used to live here was built on design
decisions cleared on 2026-09-18; it is recoverable at commit `33f2d1f`.

Everything currently lives on the **`preset-redesign`** branch. `main` is
untouched.

---

## What is built and proven

- **Brain on Teensy 4.0.** Builds clean, 65 KB of 2 MB. All nine
  patterns and all nine variants render on the wall.
- **All five strips lit** from pin 2 through the strip boxes, each strip
  on its own supply, buffered by a 74AHCT125.
- **USB MIDI path validated** — program change, CC, clock, notes,
  transport. Tempo division live, triplets exact, free-run on clock loss.
- **DMX out working** end to end against one BCC145 in 8-channel mode,
  with wash level and wash hue offset as independent controls.
- **The parametric generator**, on PC 10, with a slider panel and a morph
  control in `tools/index.html`. Proven on the wall 2026-09-18: it reaches
  most of the roster, the space between settings is playable, and morphing
  between two patches works. See `docs/generator.md`.
- **A strip-order rigging aid** on PC 11 — each strip a flat colour in
  data-chain order.

Details and measurements in `docs/bench-facts.md`.

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
    - [ ] **The connector is open again.** A stereo jack would add a
          second analog line without putting power in the pedal, which
          changes how many switches are possible. Settle this before
          drilling anything — see `docs/wiring.md` § "Foot pedal".

## Hardware work that does not wait on the design

- [ ] **Set the other three BCC145 to `A009`, `A017` and `A025`.** Only
      one has ever been connected.
- [ ] **Calibrate the fixtures** at rehearsal — RGB trims, and whether
      FastLED's brightness curve suits the fixture. Step **evenly
      spaced** values; an uneven ramp proves nothing. See
      `docs/bench-facts.md`.
- [ ] **Per-fixture master scale by eye** at soundcheck, so
      washes-at-full read as equal weight to strips-at-full.

## Generator, next

None of this is committed to — it is an experiment that earned a second
session. Ordered by what blocks what. Reasoning in `docs/generator.md`.

- [ ] **Build the endpoints by eye and save them.** The roster settings in
      the panel are guesses, several of which were wrong; a morph between
      two wrong destinations tells you nothing. Five or six looks worth
      morphing between, dialled in on the wall and saved as patches.
- [ ] **Decide whether a strip is a loop or a line.** Open question 2. It
      determines what happens when a tail crosses the strip end, and it
      wants deciding rather than patching case by case.
- [ ] **Fan shape** — diagonal through to symmetric, so the chevron
      arrangement the roster's Rain uses becomes reachable.
- [ ] **Count should double rather than add** across a morph.
- [ ] Decide whether Plasma and Aurora get a second generator or stay
      hand-written.

## Known defects

- [ ] **Glitch's white pixels ignore the V fader.** See
      `docs/bench-facts.md`. The 30 % white share cannot be tuned until
      this is fixed.
- [ ] **`PRESET_OFF` alone no longer darkens the washes.** The
      controller's "off" key must send `PC 0` and `CC_WASH_LEVEL` 0
      together, or a blackout leaves the PARs lit. This contract lives
      only in the protocol header.
- [ ] **Drop `-D AURORA_DEBUG`** from `brain/platformio.ini` once the
      strips are what gets read instead of the console.

## Key files

| When you want to… | Open |
|---|---|
| Know what we measured | `docs/bench-facts.md` |
| Know how the system is built | `docs/architecture.md` |
| Know what the lights do | `docs/visual-design.md` |
| Look up a pin or a circuit | `docs/wiring.md` |
| Look up a CC / PC / note number | `shared/aurora_protocol.h` |
| Work on how it is played | `DESIGN.md` |

## Key commands

```bash
cd brain      && pio run -e teensy40 -t upload   # flash the brain
cd controller && pio run -t upload               # future: controller node
pio device monitor                               # serial console
```
