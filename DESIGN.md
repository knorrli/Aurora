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

## Suggested order when resuming

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
