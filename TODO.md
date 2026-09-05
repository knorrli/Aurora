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

- [x] **Bench-test the USB MIDI path** with `bench/midi_monitor/`.
      Proved clock, program change, CC, notes and transport all arrive
      and decode; no ticks dropped at quarter or triplet division; USB
      jitter under 1 ms; triplets exact; free-run holds the last known
      tempo. Full findings in DESIGN.md § "What the USB MIDI bench
      proved". Re-run any time with
      `cd bench/midi_monitor && pio run -t upload`.
- [x] **Stay on the breadboard for the whole of Phase 2.** The only new
      hardware this phase needs is pin 2 → WS2812 data plus a common
      ground; perfboard buys nothing yet and is blocked on the
      Grove→Lötpin adapter. Keep LED power off the breadboard entirely
      — supply straight to the strip, only data and one ground jumper
      touch the Teensy. Move to perfboard at the start of Phase 5, when
      the DIN MIDI circuit goes on at the same time.
- [x] **Port the brain firmware to Teensy.** Builds clean; flash 65 KB of
      2 MB. Untested on LEDs — no strips on the bench yet. Reasoning for
      each decision is in DESIGN.md.
    - [x] Tag the current commit (e.g. `aurora-nano-final`) before
          deleting anything, so the old single-box firmware stays
          reachable — `git show aurora-nano-final:brain/src/R_Touchpad.cpp`
          and friends.
    - [x] Drop the `nano` env from `brain/platformio.ini`. The brain is
          always a Teensy; that env is the *old* un-split firmware, not
          a brain target.
    - [x] Delete `readKeypad()` and `readPreset()` outright — the numpad
          moves to the controller. `setBootSettings()` and the `muted`
          global go with them (preset 0 is the mute, and nothing else
          reads `muted`).
    - [x] Delete the touchpad input half of `R_Touchpad.cpp` — the
          controller owns the pad. The render half comes back in
          Phase 5, by which point Phase 4's sculpt work will have
          reshaped it anyway. `touchpadStripMode`, `touchpadEffectMode`,
          `holdModeEnabled`, `touchpadVerticalMode` and `touchColor` go
          with it; grep confirms nothing else reads them.
    - [x] Collapse the framebuffer to the 225 strip pixels. The 12 UI
          pixels sit in the controller box and the brain cannot reach
          them once it lives at the LEDs; the controller drives its own
          indicators. `PIXEL_INDEX_STRIP_START` → 0, `pixels`/`strips`
          collapse into one, `renderColorIndicators()` goes.
    - [x] Adjust pin numbers in `Aurora.h` to Teensy 4.0 (see
          `docs/wiring.md`).
    - [x] Tempo module (`brain/src/tempo.cpp`): a monotonic musical
          position in fractional beats, not a pulse. Division from CC 10,
          tempo clamped to 20–300 BPM, freeze on Stop, free-run after
          500 ms of silence. The old `tempoGate` / `currentTempo` /
          `lastGateMillis` are published from it by the main loop, so no
          `P_*.cpp` file needed touching. See DESIGN.md § "Musical
          position, not tempo pulses".
    - [x] Fix the Bars phase glitch: `lastGateMillis` used to be updated
          *after* `render()`, so on every gate frame Bars rendered a full
          beat ahead and snapped back. The gate is now computed before
          `render()`. The one deliberate behaviour change in the port,
          and still unconfirmed by eye — check it when the strips arrive.
    - [x] MIDI Program Change → preset selection (keep the existing
          tempo-quantized swap: the preset changes on the next gate,
          not mid-bar).
    - [x] MIDI CC 20 / 21 / 22 → hue / saturation / value.
    - [x] Note 60 → trigger flash, replacing the `digitalRead` in
          `IR_Trigger.cpp`.
    - [ ] *Open:* whether to swap FastLED's WS2812 output for
          OctoWS2811. The stated reason — `.show()` blocking interrupts
          — is ATmega behaviour; on Teensy 4 FastLED defaults to
          re-enabling interrupts between pixels, so a MIDI byte cannot
          be lost to it. Not needed until a second LED fixture lands,
          and note the FastLED Octo controller reads 8 lanes' worth of
          pixels, so the framebuffer must be sized for 8 strips.
- [ ] **Light the strips.** Pin 2 → data, common ground, LED supply
      straight to the strip and *not* through the breadboard. Set
      `MAX_BRIGHTNESS` low for the first power-up — bare NeoPixels drawing
      from the Teensy's own supply will brown it out long before full
      white — and confirm on a handful of pixels before wiring the array.
- [ ] **Validate over USB MIDI** with a laptop. Everything checkable
      without strips is done; the rest needs them on the bench.
    - [x] PC 0–9 selects presets, and the swap lands on the next pulse
          rather than the instant the message arrives.
    - [x] CC 20/21/22 drive hue / saturation / value, correctly scaled
          from the 0–127 MIDI range.
    - [x] CC 10 switches tempo division live. Triplets measured at
          exactly three pulses per musical beat, with BPM unchanged.
    - [x] CC 40 alt flag reaches all nine alt variants.
    - [x] All 18 preset / alt combinations render without hanging.
    - [x] Frame time a steady 7–8 ms for every preset — almost all of it
          FastLED pushing 225 pixels. Ample headroom.
    - [x] Tempo changes and clock loss move the position smoothly; it
          only ever jumps at a deliberate division change.
    - [ ] Clock drives the phase-based presets correctly *to the eye*.
    - [ ] Trigger note 60 produces the flash.
    - [ ] Drop `-D AURORA_DEBUG` from `brain/platformio.ini` once the
          strips are the thing being read instead of the console.

---

## Phase 3 — port remaining presets to continuous-phase

Once the brain is on Teensy, generalise the Bars prototype:

- [ ] **Convert the twelve tempo-dependent presets** to
      `tempo::cyclePosition()`: PulseFill, MovingBlocks, CrossSweep, Bars,
      Rain, Storm, Comet, StripByStrip, StrobeStrips, StrobeUpDown,
      Stutter, Chaos. One at a time, each checked on the wall — deciding
      whether a cycle is two beats or four is a judgement you can only
      make by looking. A `drawBlockSubpixel()` helper lifted out of Bars
      covers most of the drawing.
- [ ] **Delete the compatibility layer** once the last preset is
      converted: the three assignments in `loop()` and the `tempoGate` /
      `currentTempo` / `lastGateMillis` / `elapsedLoopTime` globals.
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
- [x] **Integrate the TeensyDMX library** in `brain/platformio.ini` on
      `Serial4`, in `brain/src/dmx_out.cpp`.
- [x] **Fixture config in firmware** — four BCC145 of our own in
      4-channel mode at `D001` / `D005` / `D009` / `D013`, each with RGBW
      trims and a master scale. A fixture that is not connected simply
      ignores its channels, so the table is safe with fewer plugged in.
- [x] **Render loop hook**: each frame every fixture gets the colour the
      strips are showing, with the common component pulled into the white
      channel rather than mixed from RGB. Confirmed on one BCC145: the
      hue walk reads correctly and desaturating gives a clean
      `0/0/0/255`.
- [ ] **Calibrate per fixture model** at rehearsal (RGB trims + master
      scale), with a strip and a PAR lit side by side. Two things to
      settle:
    - [ ] RGB trims — green is the usual offender.
    - [ ] Whether FastLED's brightness curve suits the fixture. Step
          **evenly spaced** values (127, 109, 91, 73, 54, 36, 18, 0) and
          ask whether the perceived steps are even. A first pass on
          2026-09-05 used an uneven ramp and so proved nothing. See
          DESIGN.md § "DMX OUT for venue fixtures", point 4.
- [ ] **Test at a venue** before relying on it live. Note this means our
      own fixtures: DMX carries no channel semantics, so an unknown
      fixture cannot be driven correctly without knowing its layout —
      see the note under Phase 8 on profiles.

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
- [ ] **DMX fixture profiles + addressing without a reflash.** DMX
      carries no semantics — nothing says "channel 1 is red", and a
      dimmer-first fixture fed RGB produces nonsense rather than an
      approximation. RDM solves this properly by letting fixtures
      describe themselves, but cheap fixtures rarely implement it and
      our auto-direction M5Stack module is probably unsuitable for its
      bus turnaround. The cheap 90% is a handful of named profiles
      (RGB, RGBW, Dim+RGB, Dim+Strobe+RGBW) plus live profile and
      address selection, turning load-in into "walk the channels with
      `bench/dmx_channel_map`, pick a profile, set an address". Only
      worth it if we start driving fixtures we do not own.
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
