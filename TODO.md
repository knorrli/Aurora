# Aurora TODO

Hardware state, parts, and work that does not depend on the interaction
model. The phase plan that used to live here was built on design
decisions cleared on 2026-09-18; it is recoverable at commit `33f2d1f`.

Everything currently lives on the **`preset-redesign`** branch. `main` is
untouched.

---

## What is built and proven

- **Brain on Teensy 4.0.** Builds clean, 76 KB of 2 MB. All nine
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
- **The colour field**, layered into the generator 2026-09-19. Plasma and
  Aurora turned out to be the same function with different constants, so
  those constants are parameters now. A morph from a field-driven look to
  a shape-driven one — Plasma-with-dark-sections to Bars — was judged to
  work flawlessly, which is the first time a morph has crossed both
  layers at once.
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
- [ ] **Aim each PAR at the wall between two strips**, not at a strip.
      They stand on the floor and uplight the back wall, so this is what
      keeps their pools out of the strips' background while filling the
      gaps. See `DESIGN.md` § "The PAR cans".
- [ ] **Measure how fast a PAR can be played.** The one number the PAR
      design leans on: the shortest dimmer flash that still reads as a
      flash, and whether the fixture lags or smooths between values.

      The wire is not the limit. Four fixtures are 32 channels, and
      `TeensyDMX`'s `setPacketSize()` puts a frame at roughly 1.5 ms —
      some 600 a second, against 44 for the full 512-slot frame it sends
      by default. Everything that matters is fixture-side, and none of it
      is published: a BCC145 has no datasheet behind its manual, so this
      is a measurement rather than a search.

      *The test:* flash the dimmer channel full to black with a strip
      beside it on the same clock, stepping the flash length down —
      200, 100, 50, 25, 12 ms.

      *Expect:* clean and simultaneous with the strip at 200 and 100 ms.
      Between 50 and 25, one of two things, and they mean different
      problems — flashes that go **dimmer but stay in time** are the
      fixture smoothing between values, and flashes that stay full but
      land **late by the same amount at every rate** are input latency.
      At 12 ms expect a continuous dim glow, or an irregular stutter if
      the fixture samples slower than it is sent.

      *Why the difference decides something:* latency is fixable and
      smoothing is not. Everything renders from musical position, which
      is predictable, so a fixed lag is corrected by sampling the PARs
      that far ahead. Smoothing is a hard floor on flash length that
      nothing in software gets under.
- [ ] **Look at whether a pool bridges a gap.** With the PARs interleaved
      between the strips, a window sliding across should hand over
      through a pool rather than jump from strip to strip. Whether a soft
      pool on a wall reads as continuous with a bar of pixels, or as a
      separate thing blinking in turn, cannot be argued. One slow sweep
      and one fast one, narrow window.
- [ ] **Find out what two keys at once do.** Needs no rewiring — press
      two keys on the box as it stands and watch the wall. The per-key
      codes were measured key by key and have run stably for over a year,
      so they are not in doubt; what has never been tried is a *pair*.
      The codes have the shape of a wired-OR, and if that is what it is,
      `2+3` selects key 0 and a fumble blacks the wall out, `3+4` reads
      as 6, and `3+7` as 9. If it turns out to be real, the firmware
      should reject a code arriving within a few tens of milliseconds of
      another.
- [ ] *Deferred to the controller rebuild:* confirming what idle reads
      and what produces `0b00111111` and `0b00111101`. Both are curiosity
      rather than risk, and getting at the lines means desoldering
      brittle keypad wiring, so they wait until the pad is off the box
      anyway.

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
- [ ] **Cut the field's Source control** and the sine path with it. No
      perceptible difference of character at this resolution, judged
      2026-09-19. That frees CC 27 and brings the field's Edge control
      back from the CC 90 overflow into the colour block.
- [ ] **Test the touchpad window from the laptop, before any rewire.**
      The brain renders and `tools/index.html` already drives it over USB
      MIDI, so the controller is not needed — an XY pad in the page, a
      width selector, a mirror toggle and a destination picker are enough
      for a real test on the real wall. Answers the three things the
      interaction model is guessing at: whether arbitrary patch pairs
      morph through anything worth seeing, whether a continuous window
      reads as a sweep or as a smear, and whether a strip caught halfway
      between two patches looks deliberate or broken. What it cannot
      answer is feel — thumb travel and spring-back need the hardware.
- [ ] **Travel easing.** Linear through to slow at the ends and fast
      through the middle, so a shape reads as a bouncing ball. Wanted by
      the Motion fader in `DESIGN.md`; does not exist.
- [ ] **Decide the field's usable ranges.** A hard edge with deep
      darkening and a wide hue swing is three strong things at once and
      easy to make ugly. Open question 6 in `docs/generator.md`; the
      test is a set, not a bench.

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
