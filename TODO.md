# Aurora TODO

Cheatsheet for what's done, what's next, and the open decisions blocking
work. Check items off as you go. For the *why* of anything here, see
`DESIGN.md`. For wiring and pin maps, see `docs/wiring.md`.

All work currently lives on the **`preset-redesign`** branch. `main` is
untouched.

---

## Phase 0 — things you can do right now, no new hardware

- [ ] **Test the Bars continuous-phase prototype at rehearsal.**
      Flash the current `preset-redesign` firmware to the existing
      Aurora, pick preset 4 alt-mode (Bars), compare to how it felt
      before. Looking for: smoother glide, no 1-pixel stutter, tempo
      changes don't cause speed jumps.
      Build/flash: `cd brain && pio run -e nano -t upload`
- [ ] **Decide: retire preset alt-mode in favour of touchpad sculpt-Y?**
      This is the blocker for the UX rewrite. See "Open questions" in
      `DESIGN.md`. Short version: alt variants become the "far end"
      of the touchpad's Y-axis per preset, giving a continuous blend.
- [ ] **Curate 9 palettes for the palette system.** FastLED ships
      stock ones (`HeatColors_p`, `CloudColors_p`, `OceanColors_p`,
      `ForestColors_p`, `PartyColors_p`, `LavaColors_p` …). Pick what
      suits the band's aesthetic; custom palettes are also trivial to
      hand-roll in PROGMEM. Note: palettes are no longer numpad-
      selected — they're building blocks that scenes compose from.
- [ ] **Draft 2–3 songs** as the first hardcoded `Song` structs (see
      DESIGN.md § "Song presets, scenes, and foot-pedal events"). For
      each, sketch baseline + 2–4 named scenes + which buttons do what.
      Start with songs whose structure is clearest in the set.
- [ ] **Decide the initial accent library.** Propose: white flash,
      bars-up-once, blank-while-held, strip-wide pulse. Add / remove
      as the band's aesthetic demands.

---

## Phase 1 — parts to order

Checked items are confirmed in the components drawer.

- [x] **Teensy 4.0** (~$24 from pjrc.com or distributor).
- [x] **6N138 opto-coupler** (for DIN MIDI IN on the Teensy — 2 pcs
      recommended, one for each node).
- [x] **5-pin DIN MIDI jacks** (panel-mount, at least 3: one for
      controller IN, one for controller OUT, one for brain IN).
      On hand: >3, all panel-mount.
- [x] **5-pin DIN MIDI cable** (the link between controller and brain;
      confirm it actually spans the stage distance + slack).
- [x] **Resistors**: 220 Ω × 6, 270 Ω × 2, 4.7 kΩ × 2 (standard
      quarter-watt is fine). 33 Ω × 2 if using MIDI out on Teensy.
      Assorted pack on hand covers every value, 120 Ω included.
- [x] **1N4148 signal diode** × 2 (protection on MIDI IN). 1N914A on
      hand, interchangeable here — same 200 mA fast small-signal part,
      75 V reverse instead of 100 V, against a few volts of MIDI.
- [x] **Small perfboard** for the Teensy brain assembly.
- [x] **Tap tempo momentary switch** — the existing controller already
      has one with a built-in LED; reuse it on D2 rather than buying.
- [x] **Foot pedal with 4 momentary switches.** Switches, 1/4" TS
      jacks and guitar cables all on hand. Pedal is a controller-side
      peripheral, not a separate MIDI node — the four switches share
      the Nano's one free pin (A3) via a resistor ladder. See
      DESIGN.md § "Foot pedal wiring: four buttons on one analog pin".
    - [x] Ladder resistors, **1% tolerance**: 1 k (pull-up), 2.2 k,
          5.1 k, 10 k, 20 k. All five are in the 1% kit on hand — the
          values were picked by re-running the separation search over
          what the kit stocks.
- [ ] **DMX OUT parts for the brain** (venue fixture color echo):
    - [x] **M5Stack DMX Unit (U183)**, ~CHF 13. Isolated transceiver,
          isolated DC-DC, surge protection, switchable 120 Ω and the
          XLR-3 female socket in one part. Replaces the bare
          transceiver, the XLR panel jack and the isolation parts.
    - [x] Grove→Dupont cable (breadboard) — on hand and proven. The
          unit's own Grove-to-Grove cable reaches neither breadboard nor
          Teensy.
    - [ ] Grove→Lötpin adapter, for the perfboard build. Not needed
          while the brain is on a breadboard.
    - [x] DMX cable, XLR 3-pin — on hand and working on the bench.
          Buy real 110 Ω DMX cable for stage use.
    - [x] Terminating resistor 120 Ω × 1. Belongs at the last fixture
          in the chain, not on the brain board — and on short runs it
          is usually unnecessary. Deferred until something misbehaves;
          needs a male XLR-3 plug to build.
    - [ ] *Deferred:* 3-pin-male → 5-pin-female adapter, for venues
          with 5-pin fixtures. The band's own BeamZ BCC145 PARs are
          3-pin.
- [ ] *Deferred until the breadboard and perfboard stages are done:*
      project enclosure for the brain node, enclosure for the foot
      pedal, M3 mounting hardware.

---

## Phase 2 — brain bring-up on the bench (USB MIDI, no DIN hardware yet)

Goal: validate every preset + every MIDI message on the desk before
touching the existing Aurora or the controller.

- [ ] **Teensy on perfboard**, pin 2 broken out to a WS2812 data
      header, common ground with the LED power supply.
- [ ] **Port the brain firmware to Teensy**. On `brain/platformio.ini`
      the `teensy40` env is already defined. Work needed:
    - [ ] Swap FastLED's WS2812 output for OctoWS2811 (faster, DMA-
          driven, doesn't block interrupts during `.show()`).
    - [ ] Rewrite `readKeypad()` — current code uses `PINB` register
          tricks specific to the ATmega328. Or skip entirely, since the
          numpad is moving to the controller.
    - [ ] Adjust pin numbers in `Aurora.h` to Teensy 4.0 pin
          assignments (see `docs/wiring.md`).
    - [ ] Replace the tempo-pulse pin read with a MIDI clock handler.
    - [ ] Add MIDI Program Change handler → preset selection.
    - [ ] Add MIDI CC handlers → fader / sculpt / mode values.
- [ ] **Validate over USB MIDI** with a laptop and a DAW / MIDI Monitor:
    - [ ] Clock drives the phase-based presets correctly.
    - [ ] PC 0–9 selects presets.
    - [ ] CC 20/21/22 drive hue/saturation/value as expected.
    - [ ] `FastLED.show()` no longer blocks interrupts (verify by
          sending clock continuously and watching for drops — there
          shouldn't be any).

---

## Phase 3 — port remaining presets to continuous-phase

Once the brain is on Teensy, generalise the Bars prototype:

- [ ] **Extract a shared tempo-phase helper.** Currently the logic is
      inlined in `Bars`; lift to something like `phaseInBar()` plus a
      `drawBlockSubpixel()` draw helper.
- [ ] **Port PulseFill, Sweep, CrossSweep, MovingBlocks, Comet,
      Rain** to use the helper. Kill the discrete-step boilerplate in
      each.
- [ ] **Confirm on hardware** that each preset still feels right.

---

## Phase 4 — palette system (UX redesign)

Depends on Phase 2 and the Phase 0 decisions.

- [ ] **Implement palette infrastructure**: `CRGBPalette16` array in
      PROGMEM, active-palette state, palette-aware color sampling
      helpers. See `DESIGN.md` § "Faders pick a palette, not a point".
- [ ] **Rewire faders**: H = palette hue center, S = palette spread,
      V = brightness. (Already wired via CC in the controller; this is
      brain-side interpretation.)
- [ ] **Rewire A7 on the controller**: swap the multi-state rotary for
      a simple 2-position toggle — preset vs. palette select.
      Firmware-side the logic is already in `controls.cpp`.
- [ ] **Re-do each preset to sample the palette** instead of using a
      single color. Start with Row 1 (ambient).
- [ ] **Implement sculpt-mode touchpad Y-axis**, one preset at a time:
      each preset gets a per-preset parameter that continuously blends
      between default and alt variant (or tunes another axis: fall
      speed, noise scale, etc.).
- [ ] **Idle-richness**: add per-strip micro-offset (±5° hue) and a
      slow brightness LFO layered over every preset.

---

## Phase 4.5 — song presets, scenes, and foot-pedal events

Depends on Phase 4 (palette + sculpt in place, since scenes are
composed from them). See DESIGN.md § "Song presets, scenes, and
foot-pedal events" for the full model.

- [ ] **Implement the `Scene` / `Song` / `Button` data model** in
      the brain. Active-song + active-scene state. Scene-change
      handler that swaps the active scene on pedal events.
- [ ] **Hardcode 2–3 songs** in `brain/src/songs.cpp` as the v1
      authoring path. Validate each song's scenes and button
      bindings by driving pedal input from a laptop via USB MIDI
      notes (no hardware pedal required for bring-up).
- [ ] **Accent overlay system**: library of 3–4 accents that render
      on top of the active scene without mutating it.
- [ ] **Wire up the physical foot pedal** (controller-side). 4
      momentary switches → MIDI notes → brain. Map incoming notes
      to the active song's per-button config.
- [ ] **Capture mode (v2 authoring path)**. Hold a pedal button for
      ~2 s while Aurora is displaying the desired look → bind
      current state to that button for the active song. Persist to
      EEPROM or LittleFS.
- [ ] **Update A/B switch semantics** on the controller: A = song
      mode (numpad selects `Song`), B = pattern mode (numpad
      selects raw pattern, today's behaviour). The earlier "B =
      palette mode" idea is retired.

---

## Phase 4.75 — DMX OUT color echo

Depends on Phase 2 (Teensy brain bring-up). Order-wise it can slot
in before Phase 5 if you want fixtures at the next gig; functionally
it's independent of the DIN MIDI input work.

- [x] **Wire the M5Stack DMX Unit** to `Serial4` TX (pin 17) over
      Grove — three wires, and the XLR is on the module. Wiring in
      `docs/wiring.md` § "DMX OUT". Done on the breadboard and proven
      end to end against a BCC145: pin 17 goes to **white / `TXD`**, not
      yellow. Re-test any time with `bench/dmx_bringup/`.
- [ ] **Integrate the TeensyDMX library** in `brain/platformio.ini` on
      `Serial4`.
      Allocate a 513-byte DMX universe buffer.
- [ ] **Fixture config in firmware** — hardcoded struct for 1–2
      fixtures (address, channel layout, RGB trim, master scale). The
      BCC145 layout is settled: 4-channel mode (`D001`), RGBW, no
      master dimmer. See `docs/wiring.md` § "Fixture profile".
- [ ] **Render loop hook**: each frame, write `paletteCenter × V ×
      trim` to each fixture's DMX channels.
- [ ] **Calibrate per fixture model** at rehearsal (RGB trims +
      master scale). Commit the tuned values.
- [ ] **Test at a venue** with one of the house PAR cans before
      relying on it live.

---

## Phase 5 — DIN MIDI IN on the brain, controller firmware ready

- [ ] **Build the 6N138 DIN MIDI IN circuit** on the brain's
      perfboard, wire to Teensy pin 0 (Serial1 RX). Circuit in
      `docs/wiring.md`.
- [ ] **Verify the brain behaves identically to USB MIDI** when fed by
      a DIN MIDI source.
- [ ] **Fully flesh out controller `controls.cpp`**:
    - [ ] Port the touchpad reader (Adafruit TouchScreen) → emit
          `CC_SCULPT_X/Y`, `CC_TOUCH_PRESSURE`, `CC_TOUCH_ACTIVE`.
    - [ ] Wire the new A/B mode switch decoder once the physical
          switch is in.
- [ ] **Wire the foot-pedal input** on the controller (4 momentary
      switches, debounced, emit `NOTE_PEDAL_1..4` to the brain).
- [ ] **Flash the new controller firmware to a spare Nano** (don't
      overwrite the old Aurora Nano until you're ready to lose
      the old single-box setup).

---

## Phase 6 — controller hardware modifications

Only do this when you're ready to retire the old single-box Aurora.

- [ ] **Move WS2812 data lead out of the controller box** — LEDs
      now go directly to the brain, which sits near them.
- [ ] **Wire tap tempo button + LED** to D2.
- [ ] **Transplant the mic envelope follower** from the timing
      Arduino onto D3. Verify trigger still fires.
- [ ] **Add DIN MIDI IN** to the controller on D0/RX. Circuit in
      `docs/wiring.md`.
- [ ] **Add DIN MIDI OUT** to the controller on D1/TX (2 resistors +
      DIN jack).
- [ ] **Retire the timing Arduino**. Its tempo-divider logic is now
      in the controller's `tempo.cpp`.
- [ ] **Replace A7 rotary with a 2-position toggle** for A/B mode
      select.
- [ ] **Flash the controller firmware** to the modified box.

---

## Phase 7 — integration + stage testing

- [ ] **Connect controller to brain over DIN MIDI**, cable length that
      matches real stage use.
- [ ] **Play a full set at rehearsal.** Watch for:
    - Any dropped MIDI messages.
    - Tempo lock stability when the DAW starts/stops/changes tempo.
    - Controller tempo LED in sync with the music.
    - Touchpad latency compared to the old direct-wired version.
- [ ] **Iterate** until it feels solid.

---

## Phase 8 — nice-to-haves (only when everything above is rock solid)

- [ ] Second LED fixture (backdrop, front-of-stage row, etc.) — cheap
      on Teensy with OctoWS2811.
- [ ] Audio-reactive FFT from the mic trigger input.
- [ ] **Setlist file + prev/next-song pedal buttons**, if the numpad
      proves annoying for hands-free song transitions. Requires 2
      extra pedal switches OR a modifier ("Shift") scheme.
- [ ] **Laptop companion editor** (v3 song authoring) over USB MIDI
      SysEx — named songs, named scenes, ramp curves, accent
      selection, visual timeline. Only once capture mode proves
      insufficient.
- [ ] **DMX tempo-synced effects on fixtures** (strobe-on-beat,
      fade-on-drop) — extend beyond pure color echo. Only if the
      simple echo proves too quiet to matter. Note this moves the
      BCC145 to its 8-channel mode (`Axxx`), which is also where its
      master dimmer lives; see `docs/wiring.md` § "Fixture profile".
- [ ] DMX **input** (console drives Aurora) — separate conversation
      entirely; not on the roadmap unless a venue demands it.
- [ ] Import `Aurora_Tempo` sources into `legacy/` once found, for
      archaeological reference.

---

## Key files to consult

| When you want to…                                 | Open                       |
|---------------------------------------------------|----------------------------|
| Remember what the system is supposed to do        | `README.md`                |
| Understand design decisions or rationale          | `DESIGN.md`                |
| Look up a pin or a MIDI circuit                   | `docs/wiring.md`           |
| Look up what a CC / PC / Note number means        | `shared/aurora_protocol.h` |
| See the Bars continuous-phase prototype           | `brain/src/P_Movements.cpp` (around line 92) |
| Check the controller firmware scaffold            | `controller/src/*`         |
| Read the song-preset / pedal-event design         | `DESIGN.md` § "Song presets, scenes, and foot-pedal events" |
| Read the DMX-out color-echo design                | `DESIGN.md` § "DMX: not the LED protocol, but useful for venue fixtures" |

## Key commands

```bash
# On preset-redesign branch (always verify with: git branch)
cd brain      && pio run -e nano -t upload        # flash current Aurora
cd brain      && pio run -e teensy40 -t upload    # future: brain on Teensy
cd controller && pio run -t upload                # future: controller node
pio device monitor                                # serial console
git log --oneline                                 # see what's landed
git checkout main                                 # rollback everything
```
