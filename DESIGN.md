# Aurora — design notes (in-progress)

Scratchpad for the UX redesign currently being worked out on the
`preset-redesign` branch. The README documents how the system works
*today*; this file captures the direction we're heading and the open
questions we still need to decide on before implementing.

## Where the branch stands

Already committed on `preset-redesign`:

1. **README rewrite** — full architecture / hardware / preset reference.
2. **Preset redesign** — nine slots reorganized by intensity row:
   - Row 1 (ambient): FillStrips/Starfield, Breathe/Wave, Plasma/Aurora
   - Row 2 (groove): PulseFill/Bars, Sweep/CrossSweep, RainFall/Storm
   - Row 3 (intensity): StripByStripOrdered/Comet, StrobeStrips/Stutter, Chaos/Glitch
   - Retired legacy patterns (XFill, RisingBlocks/Stars, FallingBlocks/Stars,
     Invert, RainBounce, StripByStripRandom, FillStars) parked in
     `P_Retired.cpp` under `#if 0` — zero flash/SRAM cost.
3. **Bars continuous-phase prototype** — smoother rendering, sub-pixel
   edges, bar-synced. Old discrete-step Bars kept inline under `#if 0`
   for A/B testing. Untested on hardware yet — listening for whether the
   glide is noticeably smoother at practice.

Compiles clean. Flash 57%, SRAM 53%, both well within budget.

## The UX problem we're solving

- Every preset locks to a single color (+ fader alt-mode's hue wobble,
  which is too strong to always leave on).
- Touchpad is a 5×2 stamp pad — X works hard, Y barely earns its
  keep.
- Once a preset is running, nothing evolves. Three minutes of the same
  song is three minutes of the same loop.
- Goal: "something extra" that makes the wall feel *alive* without
  drifting into rainbow-across-the-stage territory.

## The direction we're leaning toward

### Faders pick a palette, not a point

- **H fader** → palette hue center
- **S fader** → palette *spread*: 0 = monochrome (today's behaviour
  exactly), max = wide gradient
- **V fader** → brightness

At S=0 everything collapses to today's single-color behaviour — the
clean monochrome look is preserved as the left end of a knob. Turn S
up and every preset gets a family of colors instead of one. Each
preset decides how to sample the palette (by position, by time, by
strip, by noise — depending on what fits the preset).

### Fader alt-mode → palette animation

Retire the current hue-oscillation alt-mode. Replace with "palette
slowly rotates through the color wheel" as a modifier on everything.
Subtle by default, doesn't call attention to itself, fixes the
"looping feels dead" problem.

### Touchpad gets two contextual modes

Existing `PIN_TOUCHPAD_EFFECT_MODE` (D4) switches between:

- **Paint mode (today's behaviour):** X selects strip(s), Y modulates
  the painted color. Fill or invert on touch.
- **Sculpt mode (new):** X still selects strips. Y becomes a
  **per-preset parameter axis** — a live modulation knob whose meaning
  depends on the active preset. Suggested mappings:
  - Breathe/Wave: Y blends between breathe (Y=0) and wave (Y=max).
  - PulseFill/Bars: Y controls pulse amplitude / bar speed.
  - Plasma/Aurora: Y controls noise scale.
  - RainFall/Storm: Y controls fall speed and lightning chance.
  - StripByStrip/Comet: Y blends between sequencer (Y=0) and comet (Y=max).
  - Strobe/Stutter: Y controls strobe duty cycle.
  - Chaos/Glitch: Y blends between Chaos and Glitch density.

The sculpt-mode Y-axis is the natural home for **what used to be
preset alt-mode**: instead of a binary flip between default and
variant, you get a continuous blend you can sit at 30% of. This is
the payoff that makes losing the A7 alt-switch worthwhile.

### A/B switch: numpad controls presets *or* palettes

Reuse the current 4-state A7 analog rotary as a simple 2-position
toggle:

| A7 position | Numpad 1–9        | Numpad 0                   |
|-------------|-------------------|----------------------------|
| A           | select preset     | off / mute                 |
| B           | select palette    | monochrome (no palette)    |

9 presets × 9 palettes = 81 unique looks from 10 keys and one switch.
"Numpad 0 in palette mode = no palette, fader HSV only" preserves the
clean-single-color escape hatch.

Hardware change: swap the current multi-state rotary on A7 for a
simple two-position toggle. Software change: trivial (one boolean
derived from `analogRead(PIN_FADER_AND_PRESET_MODE)`).

### Idle richness — three cheap tricks layered over everything

None of these are individually noticeable, together they make "just
let it run" feel breathing rather than looping:

1. **Palette animation** (above) — very slow hue drift on the palette.
2. **Per-strip micro-offsets** — each of the five strips has a small
   hue offset (≈ ±5° on the color wheel). Reads as depth, not as
   different colors.
3. **Breath on brightness** — a ~8–16-beat LFO on overall `value`
   adds a barely-perceptible inhale/exhale.

## Open questions before implementing

1. **Preset alt-mode disposition.** Leaning toward "absorb into
   sculpt-mode Y-axis" (option 2 above). Confirmed?
2. **Curated palettes vs. all-procedural.** Tentatively curated — 9
   hand-picked palettes in PROGMEM. Memory cost ~432 bytes of flash,
   zero SRAM. FastLED has stock palettes (`HeatColors_p`,
   `CloudColors_p`, `OceanColors_p`, `ForestColors_p`, etc.) that can
   serve as starting points.
3. **Palette numpad layout.** Do we want palettes organized by mood
   to mirror the preset rows (warm / neutral / cool by row)? Or just
   nine palettes without a grid meaning?
4. **What does palette mode's "0" key do — monochrome as proposed, or
   something else (e.g. "last palette used, palette animation off")?**
5. **Touchpad indicator pixels (the 10-pixel strip at pixels 1–10)** —
   currently mirrors the five strips. Should it preview the active
   palette in palette-select mode? Or stay as strip preview always for
   consistency?
6. **Sculpt-mode Y-axis resolution.** Should Y be continuous (finer
   control, possibly finicky on stage) or quantized to, say, 4
   positions (reliable thumb placement without looking)?

## Memory budget check

After the preset redesign commit:

- Flash: 57% used → ~13 KB free
- SRAM: 53% used → ~950 B free (excluding stack)

Estimated added cost for the palette/sculpt-mode rewrite:

- 9 curated palettes in PROGMEM: ≈ 450 B flash, 0 SRAM.
- Palette animation state: ~4 B SRAM (rotation phase).
- Sculpt-mode-per-preset parameter logic: mostly in flash, maybe
  50–200 B per preset = 500 B – 2 KB flash total. Plenty of room.
- Per-strip micro-offset table: 5 B SRAM.

No budget concerns.

## Suggested order when resuming the UX redesign

1. Decide the open questions above (esp. #1 — preset alt-mode).
2. Test the Bars continuous-phase prototype at practice; decide
   whether to port the other tempo-based presets (PulseFill, Sweep,
   CrossSweep, MovingBlocks, Comet, Rain) to the same helper.
3. If Bars felt good → extract `phaseInBar()` + sub-pixel draw helper
   into a shared module; port the rest.
4. Implement palette infrastructure (CRGBPalette16 in PROGMEM, active
   palette + animation state, palette-aware color sampling helpers).
5. Rewire A7 decoder in `helpers.cpp` to the two-position A/B scheme.
6. Redo each preset to sample from the palette (start with Row 1).
7. Implement sculpt-mode touchpad — one preset at a time. This is
   where the actual live-feel lives; worth taking time on.
8. Add idle-richness (per-strip micro-offset + brightness LFO).
9. Physical hardware: swap A7 rotary for a 2-position toggle.

---

# Architecture / hardware redesign (parallel thread)

This is a separate, bigger conversation that has started alongside the
UX redesign. It's about moving Aurora off the ATmega328 Nano, making
it fully MIDI-capable, and splitting the brain from the controller.
None of this is committed to yet — notes for when we come back to it.

## Motivation

- **Timing:** a second Arduino currently translates MIDI clock into a
  simple tempo pulse. We want Aurora to speak MIDI directly (clock,
  program change, CC) so the divider Arduino can go away and the brain
  becomes a full citizen in a MIDI rig.
- **Cable length:** the current single box has to sit within ~2 m of
  the first LED because the WS2812 data line degrades over longer runs.
  That forces the performer's control surface to also be ~2 m from the
  LEDs, which isn't where the performer usually wants to be.
- **Scale:** want to be able to drive more LED fixtures (backdrop
  behind the drummer, a front-of-stage row) without rationing SRAM or
  worrying about FastLED blocking MIDI.

## The split

Brain at the LEDs, controller wherever the performer stands, one DIN
MIDI cable between them (reliable up to ~15 m):

**Brain node** (near LEDs)
- Teensy 4.0.
- WS2812 output(s). With OctoWS2811 we can drive up to 8 strips in
  parallel via DMA, so adding fixtures later costs nothing in
  interrupt budget.
- **DIN MIDI in** (Serial1/Serial2 + opto-coupler circuit). Accepts
  clock / program change / CC / note from either the Aurora controller
  or an external source (DAW, pedalboard, etc.) — they're
  indistinguishable on the wire. Passive MIDI merger if both at once.
- Minimal local emergency fallback: one physical switch for a
  "solid warm white" mode if the MIDI link dies mid-set. Optional but
  cheap peace of mind.
- No USB MIDI needed for the live rig (USB cable length is a
  deal-breaker on stage). USB-MIDI from the Teensy is fine as a
  programming/testing convenience only.

**Controller node** (near performer)
- Can stay on an existing Nano. The controller's job is pure I/O →
  MIDI encoding: no LED rendering, no interrupt pressure, no SRAM
  anxiety.
- Reads the current hardware (keypad, 3 faders, touchpad, switches),
  emits MIDI to the brain.
- Absorbs the current timing Arduino: tap tempo → internally computed
  tempo → MIDI clock out. Subdivision logic (half-time, sixteenths,
  etc.) becomes ~20 lines of firmware here.
- Mic trigger (currently on the timing Arduino) → MIDI note-on, brain
  renders it as the trigger flash.
- Single DIN MIDI out to the brain.

**The divider Arduino disappears entirely** once the controller
speaks MIDI clock natively.

## Why Teensy 4.0 over ESP32-S3

We have both parts on hand. For stage use Teensy wins:

- No WiFi/BT stack running in the background → no latent timing jitter
  from radio interrupts.
- Mature LED ecosystem: OctoWS2811 + FastLED on Teensy is the most
  battle-tested WS2812 stack around. ESP32-S3's RMT peripheral is
  good, but Teensy is boringly reliable.
- MIDI Library has first-class Teensy support; the multiple hardware
  UARTs map cleanly to DIN in/out.
- TeensyDuino dev loop is fast and rarely fights you.

ESP32-S3 would win if we wanted WiFi remote control or BLE-MIDI
later, but with the explicit DIN-only decision (USB cable length is
a non-starter on stage) that advantage evaporates.

## Why not DMX

Briefly considered and discarded for this use case:

- 235 pixels × 3 channels = 705 DMX channels, exceeds one universe.
  Art-Net / sACN solves this but adds a networked device to the rig.
- DMX desks expect fixtures with ~5–15 channels, not 705 addressable
  pixels. We'd end up exposing Aurora at the parameter level anyway.
- No one in the band runs a DMX console, and no current venue
  requires one.

Keep two unused Teensy pins reserved for a future MAX485 DMX in/out
just in case, but don't build it now. Revisit only if a venue asks.

## Why the split is worth the two-device cost

Yes, two devices means two firmwares, two power supplies, one more
cable. The payoff is large:

- Performer no longer tethered to the LEDs.
- DIN MIDI over 15 m is more reliable than 15 m of analog fader lines
  would be.
- Brain-at-LEDs means short, clean WS2812 data runs — no signal
  integrity problems even if we add many more strips.
- Any MIDI source (controller, DAW, pedalboard) can drive the brain,
  for free, because that's just how DIN MIDI works.
- Frees us from all current Nano constraints in one move: flash,
  SRAM, interrupt budget, MIDI parsing headroom.

## The thing we explicitly do NOT want

Some things I floated earlier that are off the table:

- **MIDI out on the controller for external gear.** We're not trying
  to use the Aurora controller to drive non-Aurora instruments. Its
  MIDI out exists only to talk to its own brain. Skip the extra
  hardware effort.
- **USB MIDI as the live path.** USB cable length restrictions make
  this a non-starter on stage. DIN only for live. USB is a dev tool.
- **Wireless between controller and brain.** Stage reliability
  trumps cable freedom.

## Development strategy: brain-first via USB MIDI

Teensy 4.0 is a class-compliant USB MIDI device out of the box. The
entire brain firmware can be built, debugged, and validated on a desk
with just a Teensy, a USB cable to a laptop, and the LED strip — no
controller modifications, no DIN MIDI hardware, no enclosure work.
Any DAW, MIDI Monitor, or custom script on the laptop can send:

- MIDI clock — exercises every phase-based preset's tempo lock.
- Program Change — exercises preset selection.
- CC — exercises palette hue / spread / brightness / sculpt-mode Y /
  switch-mode equivalents / etc.

When the brain feels right, adding DIN MIDI input is a ~30-minute
solder job on the same Teensy (6N138 opto + two resistors + DIN jack
on `Serial1`). **USB MIDI stays compiled in forever** as a dev/test
path — leave it active; the live rig just uses DIN. The Teensy MIDI
library treats USB and serial MIDI almost identically, so the code
path is the same either way.

Net effect: the brain can be fully validated before the controller
is touched at all.

## Clock routing: controller is the MIDI source to the brain

The controller's tempo LED must flash in sync with whatever tempo
source is live, which means the controller has to see the clock —
so clock can't go directly from the DAW to the brain. Topology:

```
 DAW ─┐
      ├─▶ Aurora Controller ── MIDI OUT ──▶ Brain
 Foot Ctl ─┘      │
                  └─ watches incoming clock → flashes tempo LED
                  └─ re-emits its own clock to the brain (always)
```

The controller becomes a **MIDI router + merger**:

- Receives external MIDI (DAW clock, foot-controller program changes,
  anything chained in).
- Parses clock to update its own tempo LED.
- Emits a single unified MIDI stream on its DIN out to the brain:
  clock (from whichever tempo source is active), PC from local numpad
  and/or forwarded from foot controller, CC from faders / touchpad /
  switches.

The brain stays source-agnostic — whatever arrives on its DIN in is
the truth.

### Decision: Option A — controller always re-emits clock

The controller does **not** pass external clock bytes through
verbatim. It uses them to track tempo internally, then generates
fresh 24-PPQN clock to the brain from that tempo. When the performer
taps the tempo button, the same tempo-tracking logic just switches
its source from external to local tap — the brain sees no
discontinuity at the changeover.

Why Option A over a true MIDI THRU merge:

- Simpler firmware on the Nano controller.
- "Tap overrides external clock" falls out for free.
- Upstream clock drop-outs (DAW paused, cable glitch) don't propagate
  to the brain — the controller keeps emitting at last-known tempo.
- Debugging is easier: one clock source arriving at the brain,
  always.

### Controller-side Nano load after the split

The controller no longer drives LEDs, so no FastLED interrupt
blocking and no SRAM pressure. Remaining responsibilities:

- Scan keypad, faders, touchpad, switches.
- Parse incoming MIDI (D0/RX) — clock bytes at 48/sec @ 120 BPM,
  occasional PC / CC.
- Emit outgoing MIDI (D1/TX) — merged clock + local events.
- Drive tempo LED.

Trivial load. The full-duplex hardware UART handles IN and OUT
simultaneously; no SoftwareSerial tricks needed on the main stream.

## Suggested order when resuming the architecture work

1. Confirm the split and Option A clock routing.
2. **Brain bring-up on the bench with Teensy + USB MIDI.** Port the
   `preset-redesign` firmware to Teensy 4.0. Replace FastLED's WS2812
   output with OctoWS2811. Adjust the `readKeypad()` PORTB tricks for
   Teensy pin mapping (or retire keypad entirely if the controller
   will own it — probably retire; the brain shouldn't need local
   numpad anymore).
3. Validate clock-locked rendering, program-change preset switching,
   CC-driven parameters — all over USB MIDI from a laptop.
4. Add the DIN MIDI input circuit to the brain (Serial1 + 6N138).
   Verify it behaves identically to the USB MIDI path.
5. Write the controller firmware: scan local controls, parse
   incoming MIDI (D0/RX), track tempo (external clock OR local tap
   OR internal fallback), emit unified MIDI (D1/TX) per Option A.
6. Modify the controller enclosure: add DIN MIDI OUT jack (2 resistors
   + jack + Nano TX). Transplant tap-tempo button and mic-trigger
   circuit from the divider Arduino into the controller. Retire the
   divider Arduino.
7. Stage test with the intended cable length between controller and
   brain.
8. Only after all this is rock solid — consider DMX, additional LED
   fixtures, mic-reactive FFT, etc.

## Memory / performance headroom after the move

Teensy 4.0 vs current Nano:
- Flash: 1 MB vs 32 KB (~32×)
- SRAM: 1 MB vs 2 KB (~500×)
- Clock: 600 MHz vs 16 MHz (~38×)
- LED output: DMA-driven, non-blocking vs bit-banged blocking-for-7 ms

Translation: every constraint we've been designing around disappears.
The "per-pixel state arrays would burn 12% of SRAM each" concern is
gone. The "FastLED.show() disables interrupts" concern is gone. We'd
have capacity for audio-reactive effects (FFT from a mic input),
multiple simultaneous LED fixtures, richer palettes with crossfades
in RAM, whatever we want.
