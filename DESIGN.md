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

### A/B switch: numpad controls songs *or* raw patterns

Reuse the current 4-state A7 analog rotary as a simple 2-position
toggle:

| A7 position  | Numpad 1–9         | Numpad 0       |
|--------------|--------------------|----------------|
| A (perform)  | load song preset   | off / mute     |
| B (freeform) | select raw pattern | off / mute     |

**A = song mode** is the performance path: a song preset bundles a
baseline look + named scenes + per-button pedal bindings (see the
"Song presets, scenes, and foot-pedal events" section below).
Reloading a song returns to its baseline scene.

**B = pattern mode** is the rehearsal / improv / fallback path: the
numpad picks a raw pattern, faders drive H/S/V directly — basically
today's behaviour. If nothing is prepared for a song or something
goes wrong mid-set, this is always available.

An earlier draft of this doc had B as "palette mode" (numpad picks
one of 9 palettes). That idea is retired — palettes stop being live
performer choices and become primitives that scenes compose from.
The S fader still provides live monochrome↔spread control.

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

### Song presets, scenes, and foot-pedal events

The redesign beyond palette and sculpt is about giving each of our
songs its own structured look — not one pattern+color frozen for
three minutes, but a *graph* of looks navigated with a foot pedal
while hands stay on faders and touchpad.

Our songs tend to be long-form (part A / B / C / buildup / D) rather
than verse-chorus-verse, so the abstraction is explicitly "song as a
sequence of sections," not "verse with decorations."

**Terminology:**

- **Scene** — atomic snapshot of the full Aurora look:
  `(pattern, palette, H, S, V, sculpt Y, per-preset params)`.
- **Song preset** — a baseline scene + up to 4 named scenes
  (e.g. intro / A / B / buildup / outro) + per-button pedal bindings.
- **Event** — what happens when a pedal button fires. Two flavors:
  - **Scene-change event** — jump/blend to one of the song's named
    scenes. Persists until another scene-change or song reload.
  - **Accent event** — fires a one-shot overlay from a small fixed
    library (e.g. white flash, bars-flow-up-once, blank-while-held,
    strip-wide pulse). Current scene state is untouched.

**Per-button configuration.** Each button's behaviour is set
per-song, not globally. A button can be:

- **Momentary** — held = active, released = return. Push-for-intensity.
- **Latching / toggle** — flips state each press. Stable section
  switches (A → B → A).
- **One-shot** — fires the bound event once and auto-returns. For
  accents or briefly-held scene changes.
- **Cycle** — each press advances through a list of bound events.
  Example: four successive accent fires for four drum hits.

**Numpad vs pedal division of labour:**

- Numpad (A mode) selects a song. Hands, used between songs.
- Pedal navigates *within* a song — scene changes and accents.
  Always accessible without taking hands off faders/touchpad.
- **No global "prev/next song" pedal buttons in v1.** Keep the pedal
  100% scene/accent. Reserving two of four buttons for navigation
  spends half the pedal on something the numpad already covers, and
  song changes happen in gaps where hands are free. It also needs a
  setlist concept that does not exist: songs are indexed by numpad
  key, so "next" would mean slot order, which is only useful if the
  set happens to run 1→9.

  Extensible later, in one of two ways. Two dedicated buttons on a
  larger pedal, or a *modifier convention* — hold one designated
  button while pressing another, so the second button's meaning
  changes, the way a synth's Shift button works. The resistor ladder
  reads every combination of the four switches, so the modifier route
  costs no extra pins or switches.

  It is not free in firmware, though, and that is the part to check
  before committing. Recognising two buttons pressed together means
  waiting after any single press to see whether a second one lands,
  and that delay then applies to *every* press, accents included. An
  accent meant to land on a snare hit does not have tens of
  milliseconds to spare. The way out, if we want this: allow the
  modifier only on buttons whose plain press is a scene change, since
  a scene change tolerates latency that an accent does not. That is a
  constraint on which buttons can participate, not a free upgrade.

**Pedal hardware shape:** start at 4 momentary footswitches, all
configurable — no reserved-role buttons in v1. Extensible to more
(6–8) if usage proves it necessary.

**Authoring songs — three stages:**

- **v1: hardcoded in firmware.** Songs as C++ structs in a
  `songs.cpp` file. Rebuild + reflash to change. Cheap and enough
  for first shows, decouples the song model from a controller-side
  editing UI we haven't designed yet.
- **v2: capture mode.** Hold pedal button N for ~2 s while Aurora
  is displaying the look you want → current state gets bound to
  that button for the active song. Scene captured; mode defaults to
  "scene-change, instant." Tweak and recapture as needed. Scenes
  persist to EEPROM or LittleFS on the Teensy.
- **v3 (deferred):** laptop companion tool over USB MIDI for
  named-songs, named-scenes, ramp curves, accent-library selection.

**Accent library (initial, small, fixed):**

- `ACCENT_WHITE_FLASH` — full-bright white across all strips, decays.
- `ACCENT_BARS_UP_ONCE` — single sweep of bars flowing up.
- `ACCENT_BLANK_HOLD` — all strips black while button is held
  (kill switch).
- `ACCENT_PULSE` — strip-wide brightness pulse on the current scene's
  color.

Room to add more over time. Accents run *on top of* whatever scene
is active; the scene state itself isn't perturbed.

**Data model sketch:**

```cpp
struct Scene {
    PatternId pattern;
    PaletteId palette;
    uint8_t h, s, v;
    uint8_t sculptY;
    // plus whatever per-preset params we decide matter
};

enum ButtonMode { MOMENTARY, LATCHING, ONE_SHOT, CYCLE };
enum EventKind  { SCENE_CHANGE, ACCENT };

struct Button {
    ButtonMode mode;
    EventKind  kind;
    uint8_t    target;         // scene index or accent id
    uint8_t    targetList[4];  // for CYCLE: up to 4 items
    uint16_t   rampMs;         // 0 = instant
};

struct Song {
    Scene  baseline;
    Scene  scenes[4];
    Button buttons[4];
};

Song songs[9];  // one per numpad key
```

Sizing: ~60 B per scene × 5 scenes × 9 songs ≈ 2.7 KB. Plus ~40 B of
button config per song. Trivial on Teensy 4.0.

**Open questions specific to this section:**

- Starting count: 4 scenes + 4 buttons per song, or push to 6/6 if
  it proves tight at rehearsal?
- Ramp behaviour: instant-only first pass, or bake in tempo-synced
  jumps ("scene changes on next downbeat") from day one? Instant is
  simpler; tempo-synced is nicer musically.
- Accent interaction with palette animation + brightness LFO:
  probably "accent overrides everything below it for its duration"
  is simplest, confirm when implementing.
- Pedal → brain transport: MIDI Note-on per button (cleanest) or CC?
  Likely Note, with the note number identifying the button and
  velocity carrying optional intensity.

### Foot pedal wiring: four buttons on one analog pin

The pedal is a controller-side peripheral — four bare momentary
switches, no microcontroller. The controller reads them and turns them
into MIDI for the brain.

**The constraint.** After the split, the Nano has exactly one free pin:
A3 (see the controller pin map in `docs/wiring.md`). Everything else is
taken by faders, touchpad, keypad, mode switches, tap tempo, and the
two UARTs. Four buttons therefore cannot have four inputs, so they
share A3 through a resistor ladder.

**The connector is 1/4" TS**, i.e. a guitar cable — rugged, and any
guitarist on stage carries a spare. Deliberately *not* 5-pin DIN, even
though the conductor count fits and we have the jacks: the controller
will already carry two DIN jacks that really are MIDI, and a third
identical one carrying switch contacts is something that gets a MIDI
cable plugged into it in a dark venue.

**Reading combinations.** Each switch pulls A3 toward ground through
its own resistor, under a common pull-up:

```
   +5 V ──[ R_top ]──┬── A3
                     │
            ┌────────┼────────┬────────┐
           SW1      SW2      SW3      SW4
            │        │        │        │
          [ R1 ]   [ R2 ]   [ R3 ]   [ R4 ]
            │        │        │        │
           GND      GND      GND      GND
```

Parallel resistors add in *conductance*, not resistance. Each closed
switch contributes its own 1/R to the total independently of the
others, so every subset of pressed buttons produces a different total
conductance, and therefore a different voltage at A3. Sixteen
combinations, sixteen distinct levels — the divider is acting as a
crude 4-bit ADC of which buttons are down.

Binary-weighting the conductances (1 : 2 : 4 : 8) is the textbook
choice, but the divider is non-linear in conductance, which bunches the
many-buttons-pressed end together. Searching instead for the widest
*minimum* separation, over the values stocked in the 1% kit on hand,
gives:

| Resistor | Value  |
|----------|--------|
| R_top    | 1 kΩ   |
| R1       | 2.2 kΩ |
| R2       | 5.1 kΩ |
| R3       | 10 kΩ  |
| R4       | 20 kΩ  |

That spreads the sixteen levels between 2.777 V (all four down) and
5.000 V (none), with the two closest neighbours 79 mV apart — about 16
counts on the Nano's 10-bit ADC. Note the levels are not ordered by
binary code: `SW3+SW4` sits above `SW2` alone. Decode by nearest match
against a table of the sixteen levels, never by binary-ordered
thresholds.

A 1 kΩ pull-up also keeps the source impedance the ADC sees low — it
peaks at R_top with no button pressed — which helps the sample-and-hold
settle inside one `analogRead()`.

**These values are forgiving of tolerance, which not every set is.**
Simulating twenty thousand builds, 1% parts never bring two levels
closer than 78 mV, and even 5% parts hold 55 mV — no build puts two
codes inside the noise. That is a property of this particular set, not
of ladders generally: the otherwise-similar 3.3 k / 3.9 k / 5.6 k /
10 k / 22 k set has a wider nominal gap yet collapses at 5%, with about
one build in nine landing under 30 mV. If the values are ever
re-picked, re-run the tolerance check rather than trusting the nominal
spacing. A calibration pass — press each combination once, store the
observed ADC readings — removes tolerance from the picture entirely and
is worth doing regardless.

**Two firmware gotchas.** A press is not instantaneous: while a contact
is bouncing or a second foot lands, A3 sweeps through voltages that are
themselves valid codes, so a naive reader emits phantom button events.
Require several consecutive agreeing samples before accepting a code
change. And a reading that matches nothing within tolerance should be
discarded rather than snapped to the nearest level — that is what an
unplugged cable or a dirty contact looks like, and on stage it should
do nothing rather than fire a random scene change.

**This tops out at four buttons.** Adding a fifth roughly halves the
spacing and pushes it under what 10-bit sampling can separate reliably.
If the pedal ever grows, the ladder is the wrong tool — put a shift
register or an I²C expander in the pedal enclosure and spend a real
digital pin on it, or accept single-press-only detection, which has far
wider margins because it only needs five levels instead of sixteen.

## Open questions before implementing

1. **Preset alt-mode disposition.** Leaning toward "absorb into
   sculpt-mode Y-axis" (option 2 above). Confirmed?
2. **Curated palettes vs. all-procedural.** Tentatively curated — 9
   hand-picked palettes in PROGMEM. Memory cost ~432 bytes of flash,
   zero SRAM. FastLED has stock palettes (`HeatColors_p`,
   `CloudColors_p`, `OceanColors_p`, `ForestColors_p`, etc.) that can
   serve as starting points. *Note: palettes are no longer live-
   selected — they're scene primitives now — but we still need the
   library of palettes to compose scenes from.*
3. **Touchpad indicator pixels (the 10-pixel strip at pixels 1–10)**
   — currently mirrors the five strips. Keep as strip preview in
   song mode too? Or repurpose to visualise scene/baseline state,
   pedal button bindings, etc.?
4. **Sculpt-mode Y-axis resolution.** Should Y be continuous (finer
   control, possibly finicky on stage) or quantized to, say, 4
   positions (reliable thumb placement without looking)?
5. **Song-preset open questions** — see dedicated list in the
   "Song presets, scenes, and foot-pedal events" section above.

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
5. Rewire A7 decoder in `helpers.cpp` to the two-position A/B scheme
   (A = songs, B = raw patterns).
6. Redo each preset to sample from the palette (start with Row 1).
7. Implement sculpt-mode touchpad — one preset at a time. This is
   where the actual live-feel lives; worth taking time on.
8. Add idle-richness (per-strip micro-offset + brightness LFO).
9. Physical hardware: swap A7 rotary for a 2-position toggle.
10. **Song-preset data model.** Implement `Scene` / `Song` / `Button`
    structs, active-song + active-scene state, scene-to-scene blend
    helper (instant first, ramped later).
11. **Hardcode 2–3 songs** in `songs.cpp` and play them back via
    USB-MIDI-simulated pedal input. Validate the scene-change path.
12. **Accent library.** Build the initial 3–4 overlays. Integrate
    with active-scene rendering (accent runs *on top* of scene).
13. **Physical foot pedal.** 4 momentary switches → controller-side
    → MIDI notes → brain. Map incoming notes to active song's
    button config.
14. **Capture mode (v2).** Hold pedal button N for ~2 s → bind
    current Aurora state to that button for the active song.
    Persist to EEPROM / LittleFS.

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

## DMX: not the LED protocol, but useful for venue fixtures

### Why DMX isn't the way we drive the LED strips

- 235 pixels × 3 channels = 705 DMX channels, exceeds one universe.
  Art-Net / sACN solves this but adds a networked device to the rig.
- DMX desks expect fixtures with ~5–15 channels, not 705 addressable
  pixels. We'd end up exposing Aurora at the parameter level anyway.
- No one in the band runs a DMX console, and no current venue
  requires one.

### DMX OUT for venue fixtures — color echo

We *do* want a minimal DMX **output** path from the brain, though,
so venues with 1–2 conventional fixtures (RGB/RGBW PAR cans, wash
lights) can participate in the look without sitting dark or visually
clashing. Scope is deliberately tiny: **color echo only**, no
choreography. Every frame, each configured fixture gets the active
palette's center color × current V. If Aurora is green, the fixtures
are green.

What we explicitly do NOT build here:

- Per-preset fixture choreography
- DMX IN (console-driven scenes)
- Art-Net / sACN
- Multi-universe
- Moving-head channels (pan/tilt)

**Hardware:** an M5Stack DMX Unit (U183) — one part carrying the
RS-485 transceiver, the isolation, and the XLR socket. It connects by a
4-pin Grove/PH2.0 cable to one of Teensy 4.0's spare hardware UARTs:
`Serial4`, TX = pin 17, since Serial2/3/5 have their TX inside the
OctoWS2811 reservation. Wiring in `docs/wiring.md`.

**Why an isolated part and not a bare transceiver.** XLR pin 1 ties the
brain's ground to the venue's lighting ground. Those are usually
separate mains circuits, sometimes separate phases, and the difference
between them lands across the transceiver — which tolerates about −7 to
+12 V before it dies. Isolation puts a barrier there instead, so the
two grounds are never joined by our cable. It also happens to be the
cheaper route: a module costs less than a bare chip plus a separate XLR
jack, and needs no surface-mount soldering.

**The socket is XLR-3, not the XLR-5 the standard specifies.** Three-pin
is what is actually fitted to the PAR cans and washes we expect to meet,
including the band's own. A 3-pin-male-to-5-pin-female adapter covers
the venues that go by the book; it is a cable-bag item, not a design
change.

**Firmware:** `TeensyDMX` library — mature, DMA-driven, non-blocking,
handles correct DMX timing (break / MAB / 44 Hz refresh). Zero
interrupt conflict with OctoWS2811.

**Fixture config** is hardcoded in firmware:

```cpp
struct Fixture { uint16_t addr; FixtureType type; RgbTrim trim; };
Fixture fixtures[2] = {
    { 1, FIXTURE_RGBW, {1.0, 0.9, 0.85, 1.0} },
    { 8, FIXTURE_RGB,  {1.0, 1.0, 1.0} },
};
// each frame: dmx[addr..] = paletteCenter(H) * V * trim
```

Change the venue? Edit two lines, reflash. For a band that plays
mostly the same rooms this is fine; if it becomes annoying we can
add a USB-MIDI or LittleFS config path later.

**Three things that actually bite when implementing:**

1. **Color calibration.** WS2812s and DMX fixtures have different
   color response. "Warm amber" on the strips can read as sickly
   yellow on a PAR can. Per-fixture RGB trims (the `trim` field
   above) fix it. One-time per fixture model.
2. **Brightness scaling.** A 50 W PAR at full output dwarfs the
   strips. Per-fixture master scale handles it.
3. **Palette spread > 0.** When the S fader is high, the strips
   show a gradient, not one color. Palette *center* is the honest
   answer — that's what all the strip colors orbit. Just commit to
   it and don't overthink.

If a venue later needs a DMX merger or scene recall, that's a whole
different conversation — not the same project.

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

### What the USB MIDI bench proved

`bench/midi_monitor/` is a standalone Teensy sketch that prints incoming
MIDI and turns clock into the tempo pulse, reporting for each pulse how
many ticks it counted and the tightest / widest spacing between them.
Run on 2026-09-05, driving it from `sendmidi` on the laptop:

- **No ticks are lost.** Every pulse counted its full quota, at both
  quarter and triplet division.
- **USB adds under a millisecond of jitter.** Ticks nominally 17.86 ms
  apart (140 BPM) arrived 17.0–18.1 ms apart. USB does not clump MIDI
  badly enough to matter here — which was the open worry, since USB
  moves data in scheduled frames rather than preserving the spacing a
  DIN cable would.
- **Triplets are exact.** See the tempo division section below.
- **Program change, CC, notes and transport all arrive** and decode
  against `shared/aurora_protocol.h`.
- **Free-running on clock loss works**, and holds the last known tempo
  rather than snapping back to a default.

One caution for future bench work: `sendmidi`'s `clock` command is not a
precision reference. It dumps a burst of 24 ticks the instant it starts,
and when looped its 2-beat chunks double a tick at each seam. Both show
up in the monitor as a `0.0 ms` minimum tick gap. Reach for a DAW when
timing accuracy is itself what's under test.

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

### Tempo division: the clock stays honest, the brain divides

The controller carries a rotary switch for subdivisions — half time,
triplets, sixteenths — inherited from the timing Arduino it replaces.
The tempting way to honour it is to have the controller emit clock at
the divided rate. That is wrong: the brain would then believe a 120 BPM
song was running at 60, and anything that later cares about real musical
time inherits the lie.

So the controller always emits true 24-PPQN clock, and reports the rotary
position separately on `CC_TEMPO_DIVISION` (CC 10). The brain counts ticks
and fires its tempo pulse every N of them.

This works cleanly because 24 divides exactly by 1, 2, 3, 4, 6, 8, 12 and
24 — which is why 24 PPQN was picked in the first place. Triplets land on
whole tick counts with no rounding, confirmed on the bench: at 140 BPM,
triplet-eighths measured 142.9–143.1 ms against an ideal 142.857, with no
drift across the run. Had the controller pre-divided instead, triplets
would have been the case that broke.

The division list lives in `shared/aurora_protocol.h`. It currently holds
bar / half / quarter / eighth / eighth-triplet / sixteenth, which is a
guess at what the rotary actually offers — match it to the real switch
positions once the controller is to hand. `TEMPO_DIV_QUARTER` is numbered
0 deliberately, so a controller that has not sent the CC yet, or that
sends 0 on connect, lands on ordinary one-pulse-per-beat behaviour rather
than something exotic mid-song.

### Tempo sanity: clamp what arrives

A burst of clock ticks — a misbehaving sender, a transport start, a USB
reconnect — makes a naive beat measurement report an absurd tempo. On the
bench a 24-tick burst produced readings of 148.8 BPM and 0.5 BPM.

The brain therefore clamps a measured tempo to the same 20–300 BPM range
the controller already enforces in `controller/src/tempo.cpp`, and ignores
anything outside it rather than acting on it.

A burst also fires several tempo pulses in the same instant. Presets that
advance a step counter per pulse jump visibly when that happens; presets
that compute position from elapsed time — the continuous-phase style Bars
already uses — glide through untouched. That is an argument for the
Phase 3 port beyond it merely looking smoother.

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
7. **Add DMX OUT to the brain** (M5Stack DMX Unit on `Serial4`,
   TeensyDMX library). Hardcode 1–2 fixture profiles. Verify color echo
   on the band's own PAR cans before a gig.
8. Stage test with the intended cable length between controller and
   brain, plus at least one DMX fixture downstream.
9. Only after all this is rock solid — consider additional LED
   fixtures, mic-reactive FFT, a setlist file for pedal prev/next,
   etc.

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
