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

### The nine palettes

**A palette is a shape, not a colour.** The H fader already covers the
colour wheel, so a library of "a blue one, a green one, an orange one"
would be nine ways of duplicating a control we already have. What a
palette carries is everything H cannot: how wide the spread is, what
shape it travels, whether brightness and saturation move along with the
hue, and whether it is a smooth gradient or hard steps.

They are therefore stored as *offsets* from the centre — hue offset,
saturation and value per entry — rather than as absolute colours, so
rotating one costs nothing at sample time.

Names below describe behaviour rather than scenery. Calling one "Lava"
would be a lie the moment the H fader turns it blue.

| # | Name | What it does |
|---|------|--------------|
| 1 | **Flat** | No variation at all. Today's monochrome, as a palette a scene can commit to. |
| 2 | **Narrow** | About ±15° around the centre, full saturation, even brightness. One colour with depth. The everyday one. |
| 3 | **Wide** | About ±60°. A real gradient, still one family — blue through purple into magenta, wherever it is placed. |
| 4 | **Two-pole** | The centre hue and its opposite, transitioning fast rather than blending through the muddy middle. |
| 5 | **Ember** | Hue barely moves; brightness and saturation do. Dark and deep at one end, bright and near-white at the other. Gives comet tails and rain trails real colour instead of just dimming. |
| 6 | **Haze** | Saturation falls away toward white while brightness stays up. Airy and pale — made for the ambient row. |
| 7 | **Deep** | Full saturation throughout, brightness falling to near-dark at one end. The opposite move to Ember. |
| 8 | **Banded** | Four hard steps instead of a smooth ramp. Reads as stripes and blocks — for Bars, chase and moving blocks, where a gradient turns to mush at speed. |
| 9 | **Spark** | Mostly the base colour with a small hot accent of the opposite hue. Pops and glints without becoming a rainbow. |

Two rules, both chosen for simplicity and both easy to revisit:

- **S scales everything**, not just the hue spread — hue, saturation
  and value deviation together. One rule: S is "how far from flat".
  The consequence to watch is that Ember and Deep lose their dark ends
  at low S, which is either correct or annoying depending on how they
  read on the wall.
- **All nine rotate with H.** None are anchored to a fixed hue. That is
  what makes them shapes rather than colours; the cost is that Ember
  placed on blue is a cold thing that no longer reads as fire. Add an
  anchor flag only if a palette turns out to need one.

*Considered and left out:* a **Triad** (centre plus ±120°) as a
deliberately loud option for a peak. It sits closest to the
rainbow-across-the-stage look we are trying to avoid, and it covers
similar ground to Wide more aggressively. It is the obvious tenth if
one is wanted.

### Fader alt-mode → palette animation

Retire the current hue-oscillation alt-mode. Replace with "palette
slowly rotates through the color wheel" as a modifier on everything.
Subtle by default, doesn't call attention to itself, fixes the
"looping feels dead" problem.

### Touchpad: one rule for X, one rule for Y

The pad has two jobs, selected by `PIN_TOUCHPAD_EFFECT_MODE` (D4).
Across both of them the axes keep the same meaning, which is the whole
point — there is one sentence to hold in your head in the dark:

- **X — which strips you are affecting.** Always. The single /
  mirrored / all switch qualifies it, exactly as it does today.
- **Y — what you are doing to them.** Colour in paint mode; distance
  toward the preset's second variant in sculpt mode.

**Paint mode** is today's behaviour: Y modulates the painted colour and
a touch fills or inverts the selected strips.

**Sculpt mode** is new. Y is how far the selected strips have moved
from the preset's default toward its variant. Bottom of the pad is the
first pattern, top is the second, anywhere between is a real mixture.

Because X still selects strips, the blend is **per strip, not global**:
four strips breathing together while one waves, three solid and two
twinkling, strips 2 and 4 stuttering at half height while the rest
strobe full. The uniform case is still there — put the strip-mode
switch on "all" first.

#### Y absorbs what used to be preset alt-mode

This replaces the alt switch outright. Instead of flipping all nine
presets to their variant at once, each strip sits wherever you put it.
Seven of the nine pairs blend as a single number moving rather than as
a crossfade between two renderers:

| Preset | What Y physically changes |
|--------|---------------------------|
| Fill → Starfield | Gaps open between lit pixels; twinkle depth rises from zero |
| Breathe → Wave | How far the phase spreads along the strip. At zero the wall breathes in unison; wound up, the breath becomes a travelling wave. Literally one number. |
| Plasma → Aurora | Mixes the two hue sources — stacked sines into Perlin noise |
| Pulse → Bars | A crossfade. The one pair with nothing structural in common |
| Sweep → Rain | How far the strips run out of step, and the tail fade with it. At zero, hard-edged blocks scrolling in lockstep; wound up, staggered comets. |
| CrossSweep → ? | Undecided. Alternating direction is the default; the variant has to vary something continuous. |
| Chase → Comet | Tail length shrinks from a whole strip down to a comet's tail |
| Strobe → Stutter | How much of the wall each flash covers, from all of it down to half — the alternating half only becomes visible as you wind it in |
| Chaos → Glitch | Blocks shrink, update faster, and white creeps in |

One of these still describes a relationship *between* strips rather
than something each strip does on its own — Chase → Comet moves across
strips in sequence. A per-strip blend is computable, but blending one
strip alone may read as a fault rather than an effect. Build it and look
before deciding. Sweep → CrossSweep used to be the other; the Row 2
restructure dissolved it.

**"Mirrored exclusive" in sculpt mode is a guess.** In paint mode it
means the selected pair keeps the colour and everything else inverts,
the intent being to make that pair stand out hard. The proposed sculpt
reading is that the touched strips take your Y and every other strip
snaps to the opposite end, giving a hard split down the wall. Nobody
has seen this yet.

#### The pad springs back

The sculpt value returns to the active scene's value when you lift off.
The pad is something you lean on while your thumb is down, not a
setting you leave behind.

That is not a stylistic preference. An absolute control holding a value
while nothing touches it has to answer what happens when your thumb
lands somewhere else next time, and every answer is bad: jump to the
new position and accept a lurch mid-song, or refuse to move until your
thumb crosses the old value, which makes the first part of every
gesture do nothing. Springing back removes the question — there is no
stored position left to disagree with.

The **hold switch is the deliberate exception** and keeps the meaning
it has today: it latches the last touch. Because flipping it is a
conscious act, the mismatch stops being a surprise.

Consequence: no thumb-position indicator is needed anywhere. While you
are touching the pad you can feel where your thumb is and watch the
wall respond; when you are not touching it there is nothing to show.

#### X is fully spent

There is no free axis left for a second per-preset knob — noise scale,
fall speed and strobe duty as knobs in their own right. That is the
trade, and it is the right way round: one axis with one clear meaning
is playable, two is something to grow into. Several of those parameters
survive inside the blends anyway — Strobe's duty *is* what its blend is
made of, and Rain's fall speed sits close to the Rain → Storm axis.

If a second knob is ever genuinely wanted it needs a control we have
not spent yet, not an axis of the pad. The Teensy controller will have
pins going spare.

### A/B switch: numpad controls songs *or* raw patterns

Reuse the current 4-state analog rotary as a simple 2-position
toggle:

| Switch position | Numpad 1–9      | Numpad 0       |
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

Hardware change: swap the current multi-state rotary for a simple
two-position toggle. Software change: trivial — one boolean from one
pin. The pin itself is assigned when the Teensy controller map is
drawn.

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

**Accent library (confirmed 2026-09-05 — initial, small, fixed):**

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

**Timing: scene changes wait, accents do not.**

A scene change lands on the next beat rather than the instant the
switch closes, so section changes arrive in time with the music. On the
next *beat*, not the next bar — a change can therefore arrive on beat 4
of a bar, which is accepted as the price of staying responsive.

Accents are the opposite and fire immediately. An accent bound to a
snare hit does not have half a beat to spare. This is not a tunable;
it is what separates the two kinds of event.

*Deferred:* which beat a scene waits for could become a per-scene
property — this one on the next beat, that one on the next bar. A
couple of bytes in `Scene` and one comparison. Add it the day a song
wants it, not before.

**Room to grow, and what each half costs.**

Four scenes and four switches per song is the starting count. The two
halves are not symmetric:

- **Four scenes → six scenes is pure software.** One number in a
  struct, ~60 bytes per scene, on a chip with megabytes free.
- **Four switches → six switches is not.** The ladder reads all sixteen
  combinations of four switches, and a fifth roughly halves the spacing
  between levels, pushing it under what 10-bit sampling separates
  reliably.

The trade, whenever it comes up: **if only one switch is ever read at a
time, the same single wire carries six to eight switches comfortably** —
five or nine levels to separate instead of sixteen, so far more margin.
Chords cost switch count; switch count costs chords. One guitar cable
cannot have both.

**Still to decide here:**

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

## What the first LED bench proved

2026-09-05, the first time the strips and the Teensy were in the same
room. Brain on a breadboard, USB-powered from a laptop running on
battery, all five strips on their own supplies and driven through the
existing strip boxes. Presets driven by `sendmidi` over USB MIDI, tempo
free-running at 120 BPM.

### The data link works at 3.3 V, but only just

The Teensy drives 3.3 V into WS2812s that want about 3.5 V. The result
renders correctly much of the time and corrupts under any disturbance.
The symptoms, in the order they became clear:

- Random red, green and blue pixels, occasionally white, appearing and
  clearing continuously.
- Worse with **busier data**. A solid fill is nearly clean; Starfield and
  Plasma speckle heavily. Varied bytes mean more bit transitions, and
  every transition is an edge a marginal threshold can misjudge.
- Worse with **dimmer pixels**, worst against **black**. A 1 bit is a
  long pulse and a 0 bit a short one, and short pulses are hardest to
  catch when the edges are already slowed by the 470 Ω and the cable. A
  dark strip is 135 consecutive zero bytes.
- **Proximity-dependent.** Moving a hand toward the laptop starts the
  flicker; moving away stops it. On battery the laptop floats free of
  earth, a body near it couples that reference toward earth, and the
  strips' mains-referenced supplies do not move with it. A two-prong
  charger made it worse still — unusable.

Corruption at the first pixel poisons the entire wall, because all 225
pixels' data enters through strip 1 and is re-transmitted from there.
Every link downstream is electrically clean and logically wrong.

The fix is the level shifter in `docs/wiring.md` § "3.3 V data and the
level shifter", and it turned out to be the entire fix. Fitted
2026-09-09 with an `SN74AHCT125N`: solid fill and Starfield against
black both render cleanly, and the proximity flicker is gone — including
on the two-prong charger that had been unusable. The marginal threshold
was therefore causing the grounding symptom too, rather than sitting
alongside a second independent fault.

The dedicated ground bond from the Teensy to strip 1's supply negative
stays unbuilt, and remains the first thing to try if flicker ever
reappears. Bench mains are not venue mains, and the original symptom was
about how the laptop's reference floats relative to the strips'
supplies. Building it means soldering inside a strip box.

### Two firmware findings

**Plasma had a wrap discontinuity, now fixed.** Its half-speed phase was
derived as `t >> 1` from an already-truncated `uint8_t` time base, so it
ramped 0–127 and snapped back rather than wrapping cleanly at 256 — half
a sine cycle, jumped every two seconds. Strip 1 was the one strip where
it was invisible, because its offset sits exactly on the sine's midpoint
where the discontinuity cancels; that asymmetry is what made it findable
by eye. The rule it teaches: derive each phase from `millis()` at the
shift you want, never by shifting a value already truncated to 8 bits.
No other preset does this.

**Bars' continuous-phase rewrite is half confirmed.** Its turning points
land on the beat with no jump-and-return, which is exactly what the old
`lastGateMillis` ordering bug produced, so that fix is good. The
sub-pixel edge rendering also demonstrably works: shown side by side
against Sweep on adjacent strips, Bars read as visibly smoother while
travelling more than twice as fast. Whether the glide is smooth enough
at real viewing distance is still open — a bench flatters it.

### What the pass could not settle

All nine presets and all nine variants rendered. The decisions that came
out of the pass are in "Row 2's third slot"; the questions it opened are
in "Still open".

Three limits are worth recording, because they bound how much the
session is worth:

- **Nothing was judged against music.** The tempo free-ran at 120 BPM in
  a quiet room, so "does this lock convincingly to a song" — the whole
  point of Row 2 — remains untested.
- **Bench distance flatters everything.** Sub-pixel smoothness, the
  Plasma/Aurora distinction and the per-strip Stutter idea all need
  seeing from across a room.
- **Glitch cannot be reviewed on this link at all.** A preset that is
  random noise by construction is indistinguishable from a corrupted
  data stream.

## Row 2's third slot: Sweep, Rain, and retiring Storm

Decided 2026-09-05 at the bench, the first time all five strips were lit.
Supersedes the `Sweep/CrossSweep` and `RainFall/Storm` pairing listed in
"Where the branch stands".

**Rain is mechanically the fanned-out version of Sweep.** Both scroll a
block along every strip, same direction, same speed, same four steps per
beat. Sweep holds every strip at the same position; Rain holds them at
fixed per-strip offsets of 20, 28, 34, 28, 20 and gives each block a
fading tail. Nothing else separates them.

That makes Sweep → Rain a single number — how far out of step the strips
run — which is the shape sculpt-Y wants, and the same shape as
Breathe → Wave. The axis moves the fan-out and the tail fade together: at
zero the wall scrolls in lockstep with hard-edged blocks, wound up it
becomes staggered comets. Rain's present offsets become the maximum fan
and the axis scales toward zero.

**That frees CrossSweep from a pairing it could never satisfy.** What
makes it interesting is that direction alternates strip by strip, and
direction has no midpoint — half of "runs opposite" is not a state.
Phase offset has a midpoint; direction does not. So CrossSweep becomes a
default in its own right, and its variant is free to vary anything
continuous: tail length, block length, a speed difference between the
two groups. Which one is still open.

**Storm is retired.** Its rain half is identical to RainFall — both call
the shared helper in wrapping mode — so its only distinguishing feature
is a full-wall white flash on a 12.5%-per-beat dice roll. That
duplicates `ACCENT_WHITE_FLASH` from the accent library, fired at random
instead of from the pedal. It is also wrong for the slot: Rain is for
downtempo and calm sections, and random flashes across a calm wall read
as jarring rather than atmospheric.

Auto-firing lightning is still a reasonable thing to want, since a
performer with both hands busy cannot play it from the pedal. If it
returns it belongs in the accent system as a rate parameter on one of
the reserved `CC_PRESET_PARAM_*` slots, not as a preset slot of its own.

RainBounce stays parked in `P_Retired.cpp`. It is the only caller that
passes the shared rain helper's `changeDirectionOnEnds` branch.

## Decisions settled — 2026-09-05

These were the Phase 0 blockers. They are decided; the reasoning sits
in the sections above and below.

1. **Preset alt-mode is retired.** The alt switch goes and the variant
   becomes the top of the touchpad's Y axis, per strip. See "Touchpad:
   one rule for X, one rule for Y".
2. **Sculpt Y is continuous**, not quantized to thumb positions.
3. **X keeps meaning "which strips" in both modes**, qualified by the
   single / mirrored / all switch — so the blend is per strip.
4. **The sculpt value springs back** to the active scene's value on
   release; the hold switch is the deliberate exception.
5. **Four scenes and four pedal switches per song.** Growing the scenes
   is pure software; growing the switches is not. See "Room to grow,
   and what each half costs".
6. **Scene changes land on the next beat.** A change may therefore
   arrive on beat 4 of a bar; accepted.
7. **Accents do not quantize** — they fire the instant the switch
   closes. The quantize rule applies to scene changes only.
8. **The indicator pixels keep showing the wall**, by a simplified
   mechanism, and both single indicators keep their existing jobs. See
   "The controller's indicator pixels".
9. **The controller becomes a second Teensy 4.0.** See "The controller
   is a second Teensy, not the Nano".
10. **The nine palettes are shapes, not colours**, curated rather than
    procedural, all rotating with H, with S scaling every kind of
    deviation. See "The nine palettes".
11. **The accent library starts at four** — white flash, bars-up-once,
    blank-while-held, strip-wide pulse.
12. **Row 2's third slot is restructured.** Sweep → Rain becomes the
    pair, CrossSweep takes a slot of its own, and Storm is retired. See
    "Row 2's third slot: Sweep, Rain, and retiring Storm".

## Still open

1. **The first song.** One, not three — drafting scenes without being
   able to look at them is guessing. Split in two: the *structure*
   (how many sections, which switch jumps where, momentary versus
   latching) is desk work needing no light, and it is what will tell
   us whether four scenes and four switches is enough for a real song.
   The *looks* for each scene need the strips up on the bench.
2. **Where the 12-position rotary lands** on the Teensy controller once
   the timing Arduino is retired. It carries the tempo subdivisions,
   three tap-tempo positions (half / regular / double) and
   mic-as-stepper, and it has never appeared in any pin map. A resistor
   chain on one analog pin is the obvious answer — one contact closes
   at a time, so the twelve levels sit roughly 400 mV apart, which is
   comfortable.
3. **What "mirrored exclusive" means in sculpt mode.**
4. **Whether a per-strip blend reads as an effect or as a fault** on
   Chase → Comet, now the only pair describing a relationship between
   strips. Watching it on the wall made this harder rather than easier:
   the two ends differ in tail length (45 pixels vs 15), speed (45 vs 6
   pixels per beat) and strip order (3,4,5,1,2 vs 1,3,5,2,4). Tail and
   speed would blend together on one knob, the way Sweep → Rain now
   does, but the two orders have no midpoint — one of them would have to
   be used at both ends. Comet earns its place on looks alone; whether
   the *pair* survives is the open part.
5. **What CrossSweep's variant should be**, now that it holds a slot of
   its own and no longer has to be reachable from Sweep.
6. **Whether Stutter's halves should alternate per strip.** Tried on the
   bench 2026-09-05 against the current all-strips-in-sync version and
   preferred: odd strips take the opposite half from even ones, so the
   wall reads as a checkerboard inverting on the beat rather than one
   horizontal line sliding up and down. The in-sync version was judged
   too static to hold interest.

   Sweeping coverage from full down to half confirmed the axis behaves:
   at full coverage the alternation is invisible, and as it winds down,
   dark wedges enter from opposite ends on neighbouring strips until the
   permanently-lit middle band vanishes and the hard checkerboard is all
   that remains. Needs a second look at proper viewing distance, and on
   a clean data link, before it replaces the current behaviour.
7. **Glitch, reviewed 2026-09-09** on a clean data link. It holds up:
   12 pixels per frame reads as a dense, fast shimmer rather than
   countable dots, and it works across the whole colour wheel, best
   between cyan and magenta — but that is taste, not a reason to
   restrict the range. Running free of tempo was not felt as wrong in
   the intensity row.

   One real defect found. Its white pixels are a flat `CRGB::White`
   while its coloured pixels are `CHSV(hue, sat, value)`, and
   `FastLED.setBrightness()` is fixed at `MAX_BRIGHTNESS` and never
   follows the V fader. So white ignores brightness entirely: at a
   fifth of full V the whites are already about twenty times the
   coloured pixels and the preset collapses into white noise. The
   documented 30 % white share is therefore only true at full V, which
   is why the constant could never be judged. Fix before the share
   itself is worth tuning.
8. **StrobeStrips' duty cycle.** Currently a flash of one quarter of the
   beat — `currentTempo / 4`, clamped to 20–200 ms, so 125 ms at 120
   BPM. Flagged on the bench as wanting adjustment; a shorter flash
   reads as more percussive, a longer one as more of a pulse. It is one
   constant and only the wall can settle it.

## Where Aurora gets used — Band Mode and DJ Mode

Two uses, and they are different instruments sharing one box. Naming
them matters because several controls want different assignments in
each, and because a feature can be essential in one and pointless in
the other.

**Band Mode** is stage lighting for the band's own set — roughly 70%
lighting, 30% decoration. Hands are almost never free, so the design
target is that a whole song is playable with the foot pedal alone.
Songs are queued over MIDI where the band's rig can send it, rather
than by numpad, which leaves the numpad nearly idle and makes it the
fallback path rather than the primary one. The pedal is loaded per song
with the things that fit that song.

**DJ Mode** is Aurora at a party or a concert with nothing else to do.
Hands are on everything. There are no prepared songs because the music
is not known in advance, so the work is reacting to what is playing and
anticipating what is about to happen. The pedal cannot be preloaded
with song knowledge; it carries generic dynamics instead — things that
complement or contrast each other.

| | Band Mode | DJ Mode |
|---|---|---|
| Song / scene source | prepared per song | none |
| Primary control | foot pedal | hands, all of it |
| Numpad | fallback, mostly idle | primary pattern select |
| Tempo | real MIDI clock | tap, or the mic |
| Faders | trim on top of a scene | the whole colour performance |

Two consequences worth stating plainly.

**DJ Mode's verbs are transitions, not scenes** — build, drop, hold,
break. A drop is a build followed by a *release*, which means a
momentary pedal switch needs a release action and not only a hold
action. That is where the impact lands, and nothing in the pedal model
above provides it.

**DJ Mode's weakest link is tempo.** Every tempo-locked pattern in Row 2
is only as convincing as a tap. Beat detection from the mic is
therefore a DJ Mode feature specifically rather than a general
nice-to-have, and it is worth more than the FFT work parked behind it
in Phase 8.

## The energy axis

**The problem.** Aurora expresses intensity by *switching pattern* — the
three rows are a discrete ladder climbed by picking a slot. Music does
not build in three steps. There is no number anywhere in the system
meaning "the wall is at 30% right now", and no way to move it to 80%
over eight bars.

"Idle richness" above was the earlier answer to "nothing evolves". It
solves *looping*. It does not solve *dynamics*, because it is
autonomous: the wall varies on its own but the performer still cannot
play it. Both are wanted; they are not the same thing.

**Energy is one value, 0–255, that every pattern reads and interprets in
its own terms** — density, height, speed, tail length, flash
probability, brightness. Nine patterns each covering a range rather
than sitting at a point.

**It lives on the third fader.** Brightness comes off the faders and
becomes a soundcheck trim on the rotary or a dedicated pot, because
absolute brightness is set once per room and energy scales it anyway.
Brightness is the crudest version of the thing actually wanted.

Three sources feed it, and they compose:

- **Hand** — the fader, which sets the base value.
- **Foot** — a pedal switch that ramps it over N beats while held and
  releases back. A build played without hands.
- **Ear** — the mic envelope, as an offset. The discipline is that audio
  drives *only* this, never colour and never pattern. That is the
  difference between lights running during a song and lights playing
  it, without the flickering-visualiser look.

**The pad supplies a per-strip offset, not the value.**
`strip_energy[i] = global_energy + pad_offset[i]`, with the offset
springing back to zero on release and pad centre meaning no change — so
up boosts a strip and down ducks it, and one gesture covers both
"highlight the soloist" and "drop that strip out".

**Why energy is not the pad's primary home.** The pad springs back, and
that is settled and right. Energy must hold: a build that collapses
when a thumb lifts is worthless, and a foot or the mic has to be able
to keep moving it while the hands are elsewhere. Sculpt is the exact
opposite and was deliberately designed as a lean-on-it control. Putting
energy on the pad and the sculpt blend on a fader swaps both onto the
wrong control.

**Why energy on a "colour" fader is not a category error.** The left
region of the box is level and colour, and energy is the grown-up
version of V, which already lives there. Sculpt on a fader would be the
real break, because sculpt is a shape control and shape lives in the
centre and right of the surface.

**Protocol implication.** A `CC_ENERGY` is needed — the 20–29 colour
block has room — and `CC_VALUE` becomes a rarely-sent trim rather than a
live fader. Not yet added to `shared/aurora_protocol.h`.

## The washes stop being an echo

Decided 2026-09-06. Supersedes the "colour echo only, no choreography"
scope in "DMX OUT for venue fixtures"; the reasons that section gives
for *not* driving the strips over DMX are unaffected.

**8-channel mode (`Axxx`) from the start**, not as a deferred upgrade.
The strategy below keeps the washes habitually low, and 4-channel
mode's only way to dim is to scale RGBW down — losing colour resolution
at exactly the levels the design now lives at, and banding on slow
fades. The real dimmer is at offset +0 of the 8-channel block and the
strobe channel comes with it. The cost is the macro channel at +6,
which must be held below 50 or it starts an auto sequence that
overrides colour entirely. Channel map in `docs/wiring.md` § "Fixture
profile — BeamZ BCC145".

**They are first-class as a class, agnostic about the count.** The
firmware always models a wash group; four, two or zero connected is
config, and an absent fixture simply ignores its channels.

**Hard constraint: the strips must carry the look alone.** A scene whose
character depends on the washes is a scene that cannot be played in a
room where they could not be rigged, or when one dies mid-set. Every
wash contribution is additive. This is what keeps the songs portable
across venues, and it is a rule rather than a preference.

**Each pattern says what the washes do.** A small enum plus a level, per
pattern — follow, antiphase, step across on the beat, hold dark, flash
only. This is what "per-preset fixture choreography" was ruled out as,
but that argument was made by analogy with driving 705 addressable
channels over DMX. A wash has two meaningful parameters, level and
colour; choreographing two numbers is a different-sized problem.

**Scenes override it** — level offset, hue offset, behaviour, about
three bytes in `Scene`. This is where a song says "washes dark through
the whole verse".

**Two of these controls exist as of 2026-09-09**, built on the bench
because a demo needed them: `CC_WASH_LEVEL` is a wash master
independent of the strips and of `PRESET_OFF`, and `CC_WASH_HUE_OFFSET`
rotates the washes off the strips' hue. Together they cover the wash
blackout and the complementary-colour case. Holding the washes a half
turn off the strips is the single change that most stops the rig
reading as one light source — worth more than it looks on paper. The
cost of the first is that `PRESET_OFF` alone no longer darkens the
washes: the controller's "off" key has to send `PC 0` and
`CC_WASH_LEVEL` 0 together, and that contract lives only in the
protocol header and here.

**One pedal switch is washes-only.** A wash blackout under a running
pattern is the cheapest drop in the rig.

### Keeping the washes from overpowering the strips

In order of effect:

1. **Aim them off the strips.** A wash falling on the surface the strips
   are mounted on lights the background of the graphic and collapses
   its contrast. Point them at the band, across the stage, or at the
   audience. Free, and most of the problem.
2. **Set a ceiling by eye, once, in the room.** Per-fixture master scale
   set at soundcheck so washes-at-full read as equal *weight* to
   strips-at-full, and never exceeded afterwards. A separate number
   from the RGBW trims: those correct hue, this one caps authority.
3. **Different colour, not the same colour.** Two fixtures on one hue
   means the brighter wins and the dimmer disappears. Strips saturated,
   washes low and desaturated or complementary, and they read as two
   layers rather than one thing plus glare. One byte of hue offset buys
   this, and it is the real departure from pure echo.
4. **Split the energy range between them.** Washes own the bottom, strips
   own the top: low energy is a warm room with the strips ticking over,
   and as energy climbs the strips take over while the washes retreat to
   accents. They never both peak, so overpowering cannot happen by
   construction — and the rig changes character as it builds rather than
   just getting brighter.
5. **Dark is a state.** The most effective thing a wash does is usually
   be off until it matters.

## The touchpad, enumerated

Written down because the combinatorics had never been listed in one
place, and because the gaps are the interesting part. Four switches
qualify the pad: **effect** (3-way, `CC_TOUCHPAD_EFFECT`), **strip
mode** (3-way, `CC_TOUCHPAD_STRIP_MODE`), **hold** (2-way), **vertical**
(2-way, boot-time only).

Note the protocol already defines effect as three positions — 0
paint/fill, 64 paint/invert, 127 sculpt — while `controls.cpp` still
reads it as a digital two-state. The switch itself has not been
replaced yet.

| Effect mode | X means | Y means | On touch | On release |
|---|---|---|---|---|
| paint / fill | which strips | painted colour: saturation or hue, per vertical mode | selected strips filled with `touchColor` | paint stops; hold latches the last touch |
| paint / invert | which strips | same | each LED on the selected strips toggles black ↔ `touchColor` | same |
| sculpt | which strips | blend from pattern default toward variant, **per strip** | selected strips move along their blend axis | springs back to the scene's value; hold latches |

Strip mode qualifies X in all three:

| Strip mode | Selected | Everyone else |
|---|---|---|
| mirrored | strip N and strip 4−N | untouched, keeps running the pattern |
| all | all five | — |
| mirrored-exclusive | strip N and 4−N | forced to the opposite state — inverted to `touchColor` or black |

Vertical mode applies only to the two paint modes and is chosen by
holding key `0` at boot. That gives 3 × 3 × 2 = 18 live states, ×2 for
vertical inside the paint modes, so 30 distinct behaviours. **None of
them touches the washes.**

Three things the enumeration exposes:

- **Vertical mode is a boot-time setting**, so it cannot be changed
  during a gig. That makes it either a constant nobody has committed to
  or a control wanting a real switch.
- **The effect switch mixes two levels of hierarchy.** paint-versus-
  sculpt changes what the pad fundamentally *is*; fill-versus-invert is
  a small look variation. They sit at the same level on the same
  switch, which is why the middle position feels arbitrary.
- **Strip mode is secretly two controls** — how wide the selection is,
  and whether the unselected strips get forced to the opposite state.
  That second job, "what happens to everything you did not touch", is
  the natural slot for any future scope question.

## Considered and rejected — 2026-09-06

**Touchpad X as stage position rather than strip index.** Each fixture
would carry a normalised 0–1 position and a gesture would affect
whatever stands near that point, washes included. Mirroring survives
this — it mirrors about the centre line rather than about strip index
2, which under uneven spacing is more symmetric, not less. Rejected
because it gives the washes *co-located* behaviour and never
independent behaviour: a softer tie to the strips, but still a tie, and
the question was how to do more than mirroring. Worth revisiting for
its other benefit, which is that patterns written against position
survive uneven strip spacing — the one-strip-per-player arrangement
stops costing the geometric patterns. Note this is not the
resolution-independent rewrite rejected in "The controller's indicator
pixels"; that was about vertical resolution, this is five horizontal
constants.

**Splitting the pad vertically**, upper half strips and lower half
washes. X carries identity, Y carries quantity. Splitting Y spends the
only continuous axis on an identity question, halves the resolution of
both halves, and puts the boundary in the middle of the pad where it
cannot be felt in the dark. Identity questions belong on switches.

**The pad as energy's primary home.** See "The energy axis".

**Any dependency on haze.** Most venues forbid it and the practice room
cannot take it. A hazer stays worth owning for the rooms that allow it,
but nothing in the design may assume air. Its two benefits are
partially recoverable without it: beams pointed at people land without
visible air, which is why blinders work in haze-free rooms, and depth
comes from fixtures at different distances rather than all in one
plane.

If the pad ever must reach the washes, the cheap version that breaks
nothing: **washes follow the pad's Y and ignore its X**, taking the
average or the peak of the gesture. One meaning for the pad, a share
for the washes, and no 5-into-4 mapping to invent.

## Decisions settled — 2026-09-06

Continuing the numbering from 2026-09-05.

13. **Two named use modes, Band and DJ.** They differ in control
    assignment and in what is worth building, and features are judged
    against a named mode rather than against "Aurora" in general.
14. **The energy axis exists**, is read by every pattern, and lives on
    the third fader. Brightness becomes a soundcheck trim.
15. **Energy is base plus a springy per-strip pad offset**, not a pad
    value.
16. **Audio drives energy only** — never colour, never pattern.
17. **Momentary pedal switches get a release action**, not only a hold
    action, because that is where a drop lands.
18. **The washes move to 8-channel mode from the start**, for the real
    dimmer and the strobe channel.
19. **The washes are first-class as a class**, and the strips must carry
    every look alone.
20. **Wash behaviour is per pattern**, overridable per scene, with one
    wash-only pedal switch.
21. **The wash restraint rules** — aim off the strips, ceiling by eye,
    contrasting colour, split energy range.

## Still open — added 2026-09-06

9. **Vertical mode: keep it or delete it.** A setting that cannot be
   reached during a gig is either a constant or a missing switch. Pick
   one; deleting it frees a boot key, a CC and a branch.
10. **Whether the effect switch should stop mixing hierarchy levels** —
    fill-versus-invert is a much smaller distinction than
    paint-versus-sculpt and probably does not deserve equal billing.
11. **Which of the 30 touchpad states actually get used.** The ones
    never visited are the honest answer to what the pad should be. This
    needs a gig, not a bench.
12. **The wash behaviour vocabulary** — what the enum's members actually
    are, and which one each of the nine patterns gets. Needs the
    fixtures and the strips lit together.
13. **How energy is divided between fader, pedal and mic in each
    mode.** Band Mode probably wants the pedal ramp to dominate; DJ
    Mode probably wants the fader. Untested either way.
14. **Beat detection from the mic** — algorithm, and whether the
    existing envelope follower is enough or whether it needs
    re-designing. DJ Mode depends on it.
15. **The DJ Mode pedal loadout.** Four generic switches with no song
    knowledge: build, kill, accent, and one more. What the fourth is,
    and whether any of them should be a cycle.
16. **Whether the washes belong under the touchpad** — raised
    2026-09-09, after the bench session where the washes first got their
    own level and hue and stopped merely echoing the strips. They read
    as a second instrument rather than stage backlight, and leaving them
    out of the pad wastes that. The problem is that X already means
    "which strips", so there is no obvious room: are the washes a sixth
    zone on the same axis, a separate region of the pad, something the
    pad reaches only in one of its modes, or something the pad never
    touches because their two parameters belong on the pedal instead?
    Answering this decides whether the wash behaviour enum in item 12 is
    the whole story or only the per-pattern default that a pad then
    overrides live.

## Scope and the foot pedal — 2026-09-09, evening

A resumption point rather than a set of decisions. Almost everything
below is still open; the pedal vocabulary in particular is being slept
on before anything is settled.

### Scope, corrected

**The band backdrop is the target and stays the target.** The session
took a detour through "what would it take for a promoter to ask who
that guy with the lighting system was", and that framing is retired.
It produced one observation worth keeping — a rig that can drive
whatever is already hanging in a room reads as production, while a rig
that only brings its own five strips reads as a band member with LEDs —
but it is not what Aurora is for. The Teensy's headroom and the DMX
output are capability, not direction, and mistaking the two is how the
last few sessions grew a feature list nobody asked for.

The goals for the next phases, stated plainly:

1. **Patterns that look good**, replacing the ones that did not pull
   their weight. Done.
2. **A better touch mode.** No dead controls: in almost any state of the
   system, every fader, switch and pad press should do something.
3. **Song mode.** Patterns, colours, accents and transitions saved
   together as a song, with things assignable to the four pedal buttons
   so a whole song is playable without hands.
4. **Songs switchable over MIDI Program Change**, so the band's rig can
   drive them.

DJ mode stays secondary, with two goals that pull harder than band mode
does: a touch surface intuitive enough to add and remove intensity and
cue a drop without thinking, and enough variety that an hour does not
become the same nine patterns on a loop. The washes matter more there
than anywhere — as first-class material for the patterns, and as
something the hands can reach.

### Goal 2 is really two goals

They pull in opposite directions and want different answers:

- **No dead controls** — there is never a state where a fader or a
  switch does nothing. Cheap, unambiguously good, band mode included.
- **Poking randomly produces something good** — a forgiving, generative
  surface. That is a DJ mode goal. In band mode you are not exploring,
  you are hitting a known look at a known moment, and the property you
  want is the opposite: nothing you touch can wreck the running look.

Same hardware, two different promises. The generative behaviour is what
the surface does when no song is loaded.

### The washes, at band scale

The first run of strips and one BCC145 together produced three effects
worth naming, because all three are *relationships between two light
sources* rather than one source adding to another:

- a soft, dim, complementary wash against saturated strips
- alternating quickly between broad wash and strip accents
- the strips holding a fixed colour or white while the wash changes
  underneath them

That last one is the interesting one: the colour performance moves off
the strips entirely and they become pure shape. It is also the cheapest
source of variety over a long set, because a wash changing colour under
a running pattern re-reads the whole wall without touching the pattern.

Note this cuts against the earlier rule that every wash contribution
must be additive and the strips must carry every look alone. Under that
rule none of the three effects above are buildable. At band scale the
rule is also solving the wrong problem: the fixtures are ours and the
rooms are the same rooms, so the realistic failure is *one died* or
*no time to rig them tonight* — degrade by count, not degrade to zero.

**Minimal version that makes the washes a tool rather than a setting**,
and nothing more than this in the near phases:

1. Wash colour independent of strip colour. Exists — `CC_WASH_HUE_OFFSET`.
2. Something on the box that actually moves wash level and hue. Nothing
   emits `CC_WASH_LEVEL` or `CC_WASH_HUE_OFFSET` today; the brain
   consumes both but they only move from a laptop. One knob and one
   fader, not a touchpad redesign.
3. A song stores its wash state along with everything else.

Deferred, deliberately: per-pattern wash choreography, the washes under
the touchpad, wash-only pedal moves.

### The foot pedal

Where the model got to. Four buttons, and the shape is a small
vocabulary of actions assignable per song rather than fixed roles in
firmware:

- **advance** — next scene in the song's sequence
- **reverse** — previous scene
- **jump** — go to a named scene
- **hold** — show a target while the button is down
- **toggle** — latch a target on and off

Any four of those, on any of the four buttons, per song. Fixed roles in
firmware were argued for and dropped: the discipline belongs with the
performer, not baked into a one-person instrument.

**New songs inherit a default loadout.** This is what makes per-song
assignment safe rather than a trap — a personal convention such as
"button 1 is always advance" then holds on every song for free, and a
song deviates only deliberately.

**Navigation and override are two layers, not one variable.** advance,
reverse and jump move *where you are* in the song. hold and toggle show
something else *without* moving where you are. Sharing one "current
scene" value produces a whole class of wrong behaviour — release an
accent after advancing and you land on the pre-advance scene; latch an
accent and advance and nothing says whether it survives. Keeping a
position and an override on top of it costs one variable, and it
answers "what does releasing return to" without a history stack: always
whatever the position currently says.

**Open: is an accent a scene, or a layer over one?** A whole-scene swap
reads as a cut — the running pattern stops, which is right for "kill to
a strobe" and wrong for flicking in a highlight. A layer keeps the
pattern running underneath and adds to it. Both are probably wanted, and
the cheap shape is that the five verbs stay as they are while the
*target* of hold, toggle and jump can be either a scene or an accent.
Deciding this decides what a "fixed scene" target means.

Smaller things to pin when the vocabulary settles:

- **End of sequence.** advance past the last scene should probably stop
  dead rather than wrap; wrapping mid-song because the singer talked
  longer than expected is worse than nothing happening.
- **reverse is "go to scene N−1", not "undo".** If scenes do anything on
  arrival, arriving backwards re-fires it.
- **Scenes per song** becomes a memory question rather than a UX one,
  since four buttons no longer cap it.

**Consequence: tape on the pedal stops being sufficient.** Per-song
loadouts make the labels right for most songs and silently wrong for
the ones that deviate — which are exactly the songs that will go wrong
under pressure. The cheap fix uses hardware already planned: show the
four current bindings on the controller's pixels for a few seconds when
a song loads, then return to being the wall monitor. Check it during
the count-in.

### Transitions as a pedal action — idea, 2026-09-09

Raised at the very end of the session and not yet worked through. Two
shapes, both for the same button:

- **Held** — the button advances toward the next section and the
  transition *stays active while the foot is down*; releasing lands it
  and the next section starts. The transition becomes somewhere you can
  stay rather than an event that happens to you, which is what riding a
  build actually is.
- **Timed** — the button advances via a named transition over a fixed
  span, say two bars, and runs on its own.

Three things this exposes, all open:

1. **Only some transitions are holdable.** A held transition needs a
   renderable middle: the system has to sit at part of the way from A to
   B indefinitely and have it look deliberate. A crossfade has one; a
   wipe has one, with the edge frozen partway; a cut on the downbeat has
   none. So transitions divide into those with a scrubbable middle and
   those that are instantaneous, and only the first family can go on a
   held button.
2. **A transition with a middle is the same shape as the sculpt blend** —
   two looks and a position between them. That suggests transitions are
   not a new subsystem but that blend with different things driving the
   position: a foot by hand, the clock over N bars, or a jump for a cut.
   One mechanism, three sources, which is the version worth aiming at.
3. **Release has two musical meanings.** Release-completes lands the
   next section; release-aborts falls back to where it started, which is
   the fake drop and one of the better tricks available. Both are real,
   so release probably cannot have one fixed rule — either it is part of
   the button's configuration or a short release is distinguished from a
   long one. Related and unsettled: releasing mid-bar either lands
   immediately, which is sloppy, or waits for the next downbeat, which
   is tight but feels late under the foot. Only the wall can settle it.

Transitions want their own session rather than a sub-item of song mode.
They matter as much in DJ mode as in band mode — arguably more, since a
DJ set is transitions rather than sections.

### Program Change songs — two traps

- **Song IDs must be stable and explicit**, not slot order. Once the
  band's backing track has PC 7 saved for a song, reordering the song
  list silently breaks a gig.
- **A PC arriving mid-song should not take effect mid-song.** Cheapest
  version: the PC loads and arms the song, and it does not jump to that
  song's home scene until the next advance.

Also unresolved: songs need their own PC range, since 0–9 is preset
select today, and if the message arrives at the controller's MIDI IN
then the controller is the node that has to translate it.

### Open when we resume

Roughly in the order they block work:

1. **The accent fork above** — scene swap, layer, or both.
2. **Transitions.** Their own session, not a sub-item of song mode — see the idea recorded above. Named as part of song mode and never designed. When
   a pedal press changes the look, does it cut, crossfade, or wait for
   the next downbeat? Snapping on the beat is most of the difference
   between a change that reads as played and one that reads as a switch
   being flipped, and the clock to do it is already there. Likely a
   property of the change, per song, defaulting to quantised to the bar.
3. **A blackout reachable blind** — one known-safe state that works
   regardless of mode, song, or what the pad last did. Under the pedal
   vocabulary this is just a jump at a blackout scene, provided every
   song has one.
4. **What the rig does between songs** — banter, tuning, a broken
   string. A real fraction of a set with nothing specified for it.
5. **Where wash level and hue live physically**, given the box was built
   for five strips and the fader row is already spoken for.

## Press vs hold, and what a scene actually is — 2026-09-10

Started as a narrow question — can the four momentary footswitches tell
a press from a hold without losing timing accuracy — and turned into the
foundational one underneath it. The discussion is unfinished; this
records where it got to.

### Detection is not the bottleneck

Foot down to light changing, on the hardware as it stands:

| Stage | Cost |
|---|---|
| Switch bounce plus several agreeing ladder samples | ~2–10 ms |
| Three-byte MIDI note, controller → brain at 31250 baud | ~1 ms |
| Brain frame — 225 px of WS2812 output plus render | ~7–8 ms |

Ten to twenty milliseconds against a sixteenth note of 125 ms at
120 BPM. Nothing in the reading chain is worth optimising.

**The hold threshold is the only real cost.** A foot needs roughly
250–350 ms before "still down" reliably means "deliberately held" —
feet are slower than fingers, and under pressure stomps get shorter,
not longer. So the question is what has to be delayed to tell the two
apart:

- **Press and hold as different actions** — the press action must wait
  the full threshold, because you cannot fire it and then discover the
  foot stayed down. Visibly late on an accent.
- **Hold as a continuation of press** — fire on contact close, and if
  the foot is still down at the threshold the hold behaviour starts on
  top. Costs nothing.

The same escape that applies to the two-button modifier applies here,
and more cheaply: a scene change already waits for the next beat, so on
a scene-change button even the exclusive form is free — the decision
fits inside the beat the change was going to wait for anyway. Unlike
the modifier it needs no second foot and no sixteen-level decoding.
Caveat: at 160 BPM a beat is 375 ms and a 300 ms threshold nearly fills
it, so the change can slip a beat. Quantising to the bar removes that.

Release needs debouncing too, since a hold *ends* on release. About
20 ms, inaudible.

### The word "scene" is doing two jobs

This is where the confusion lives. It is being used for both:

- **a destination** — somewhere the wall goes and stays, left only by
  going somewhere else
- **something that happens** — it starts, it runs, it is over

The one-line test that separates them is not scene-versus-effect-versus-
transition. It is **does this thing terminate?** Plasma never says done.
"Black out, white fills in from both ends, they touch" does, and that
"done" is the whole difference.

It matters because **the moment something terminates, holding it needs a
definition, and a destination never needed one.** "The effect continues
until I release" is ambiguous once the effect has already finished.
Three different answers, all real:

- **freeze** — stall part-way, two white bars closing in and stuck
- **loop** — run it again, and again
- **sustain the end** — arrive at all-white and stay there until release

For that fill-from-the-ends effect, freeze is a build you can ride and
sustain-the-end is a flash you are stretching. They look nothing alike
and neither is more correct; it depends on the song.

### Most "effects" are not a new kind of object

Running the accent library through "is this a look plus a number
moving":

| Accent | As a number moving |
|---|---|
| White flash | current look → white, fast up, slow down |
| Blank while held | current look → black, pinned at 1 while the foot is down |
| Strip pulse | current look → brighter version, driven by an LFO |
| White fills from the ends | black → white, the pattern's own shape being "fill inward" |
| Bars flow up once | **does not fit** |

Four of five are the mechanism sculpt Y already needs: two looks and a
position between them. The one that breaks is not a position between
two looks at all — it is the bars pattern's own animation played
through once instead of looping.

### The foundational split: two numbers, three sources

There are **two different values between 0 and 1**, and calling both of
them "transition" is what made the model feel unfinished:

1. **Blend position** — how far between look A and look B. Sculpt Y is
   this. Crossfades are this. The white-fill is this.
2. **Playhead** — how far through one look's own animation. Tempo drives
   this today, for every pattern, all the time.

Either can be driven by the same three sources: the clock (free-running,
or once over N bars), a foot (while held), or a jump (instant).

**Two numbers, three sources** is the proposed foundation. Under it
there is exactly one kind of saved object — a look — and everything else
is a binding saying which number a button drives and from where. A
transition is therefore not a shorter scene; it is two looks and a
number. So is most of the accent library.

### What this does to press versus hold

It stops being per-button configuration and becomes a choice of source:

- **press** → the clock drives the number, once, over N beats
- **hold** → the foot drives the number; release hands it back to the clock

The button's meaning does not change between the two, and nothing has to
be configured anywhere to get the behaviour that started this
discussion.

### The gap: a footswitch has no position

The touchpad can scrub — a thumb is somewhere, so the number is
somewhere. A stomp switch reports only elapsed time, so "hold to ride
the build" is a ramp at a fixed rate that you start and stop, not a
scrub.

That forces a question the model does not answer yet: **what happens if
the foot stays down past the end of the ramp?** Reaching 1.0 while the
foot is still down means the arrival already happened and releasing does
nothing musically. The alternative is a ramp that stalls short of the
end — say 85% — so the landing is always still ahead of the foot no
matter how long the singer talks.

The stall is the better bet, with the ramp rate tempo-relative (two
bars, not 1500 ms) so it feels the same in every song. Only the wall can
confirm it.

### The entanglement, and which way it runs

This felt tangled with "what does a song actually save", and it is — but
the dependency runs the opposite way to how it feels. Settling what a
song saves is not a prerequisite for the model above. The model above
decides what a song saves: if it holds, a song stores looks and
bindings, one kind of object and one kind of pointer. If effects and
transitions really are separate nouns, a song stores three kinds of
object and needs an editor that understands all three.

### Open when we resume

- **Which of freeze / loop / sustain-the-end** a held terminating look
  does, and whether that is a property of the look or of the binding.
- **The stall point and ramp rate** for a foot-driven blend.
- **Whether the playhead is worth having at all in v1**, or whether the
  one accent that needs it can be dropped until the blend model is
  standing.
- The whole presets-versus-songs discussion, deferred: does a song store
  a frozen snapshot that overrides the faders on load, or a set of
  choices the hands still ride on top of?


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

1. Answer what is left in "Still open" — the palettes, the accent
   library and the first songs are the ones that gate code.
2. Test the Bars continuous-phase prototype at practice; decide
   whether to port the other tempo-based presets (PulseFill, Sweep,
   CrossSweep, MovingBlocks, Comet, Rain) to the same helper.
3. If Bars felt good → extract `phaseInBar()` + sub-pixel draw helper
   into a shared module; port the rest.
4. Implement palette infrastructure (CRGBPalette16 in PROGMEM, active
   palette + animation state, palette-aware color sampling helpers).
5. Rewire the A/B switch decoder in `helpers.cpp` to the two-position
   scheme (A = songs, B = raw patterns).
6. Redo each preset to sample from the palette (start with Row 1).
7. Implement sculpt-mode touchpad — one preset at a time. This is
   where the actual live-feel lives; worth taking time on.
8. Add idle-richness (per-strip micro-offset + brightness LFO).
9. Physical hardware: draw the Teensy controller pin map, then swap
   the alt rotary for a 2-position toggle and find a home for the
   12-position rotary.
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

Amended 2026-09-06. The changes are additions and one reordering:

- **Energy rides along with step 6.** Each preset gains a palette sample
  and an energy reading in the same pass — doing them separately means
  touching all nine patterns twice.
- **Accents come before scenes**, so step 12 moves ahead of steps 10 and
  11. Accents are immediate, hands-busy and testable at a single
  rehearsal. Scenes, capture mode and persistence are the largest block
  of work in this document and their payoff scales with how much state
  the rig has — which today is five strips and four washes. Build the
  half that pays now.
- **The wash work is its own track**, Phase 4.75 in `TODO.md`. Only its
  scene override and its pedal switch depend on step 10.

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

## The controller is a second Teensy, not the Nano

Decided 2026-09-05, superseding the Nano assumption in the earlier half
of this document and in `docs/wiring.md`.

A Nano solution does exist and was worked out in full. The salvaged
phone keypad is a static parallel code on D9–D12 rather than a scanned
matrix, so it collapses onto one analog pin, freeing D8–D12; moving the
two mode switches off A6/A7 onto freed digital pins then buys back the
analog pins the ladders need:

| Pin | Role |
|-----|------|
| A6 | keypad ladder |
| A7 | 12-position rotary ladder |
| D8, D9 | touchpad strip mode (3-way, as two lines) |
| D10 | A/B switch |
| D11 | WS2812 data — the 12 indicator pixels |
| D12 | spare |

It closes, with exactly one pin spare and no headroom after.

**What tipped it to the Teensy is the rendering load, not the pin
count.** Deciding that the indicator pixels keep showing the wall (see
below) puts pattern work, palette maths and FastLED on the controller,
on top of MIDI in and out, a touchpad, three faders, two ladders and
debouncing — inside 2 KB of SRAM and 30 KB of flash. It would probably
fit. The failure mode is discovering that it does not, late. Secondary
reasons: the budget above ends at one spare pin, the box needs
substantial rewiring either way, and one part with one toolchain across
both nodes means `shared/` compiles for both and a single spare board
in the gig bag covers either failure.

What the move actually costs:

- **Everything passive comes across unchanged.** The resistor ladders
  are voltage dividers, so they are ratios — levels scale with the
  supply and the ADC reference together, and the decode table is
  untouched at 3.3 V.
- **Two real items.** The mic envelope follower is an analog circuit
  built for 5 V and needs re-referencing or re-powering. Driving 5 V
  WS2812 data from a 3.3 V pin is marginal and wants a level shifter —
  a problem the brain already has with 225 pixels, so it gets solved
  once and reused.
- **The keypad ladder leaves the plan.** Five pins is nothing on a
  40-pin part, so the keypad keeps its existing wiring and the
  breadboard session that was going to prove the ladder is no longer
  needed. The **foot-pedal** ladder survives regardless — it exists
  because a 1/4" TS cable carries two conductors, not because of pin
  count.
- Not 5 V tolerant, so a wiring mistake is fatal to the board.

The Teensy controller pin map is not written yet. It is a fresh
assignment rather than a translation of the Nano map, and it wants
doing at the bench with the box open.

## The controller's indicator pixels

The 12 pixels — one touch-colour indicator, a 5×2 grid under the
touchpad, one preset-colour indicator — sit physically in the
controller box. After the split the brain cannot reach them, so the
controller drives them.

**They keep showing the wall.** An earlier working note concluded the
strip preview had to be dropped. It weighed one way of keeping it —
streaming strip data back over the MIDI link — and rejected it
correctly, as exactly the traffic the split exists to avoid. What it
did not consider is that the controller can *recompute* the picture
rather than receive it.

Why that matters more than it sounds: **the performer cannot see the
wall.** The strips stand behind the band or face away, spread out. You
can see one strip's colour; you cannot see the pattern. The grid is not
a redundant copy of something already in your eyeline — it is the only
place the whole wall exists at once. It is a monitor. It is also the
best-looking thing on the box, which on stage gear is a real
requirement and not a tiebreaker.

**What it renders is character, not shape.** Not the real pattern
renderers: a small table maps each preset to one of about five cheap
motion primitives — still, pulse, chase across strips, strobe, vertical
travel — driven by the clock the controller already generates and the
colour it already computes.

Running the real renderers was considered and rejected for two reasons:

- Every pattern would have to become resolution-independent, written as
  brightness against normalised position rather than against pixel N,
  so both boxes could sample one function at different densities. That
  is a permanent tax on how patterns get written, and it does not work
  at all for Starfield's individual stars or Glitch's random pixels.
- The controller would have to duplicate the brain's whole performance
  state — scenes, the next-beat quantize rule, accent overlays running
  on top. Miss any of it and the box shows a confident lie, which is
  worse than showing nothing. Two boxes that must agree, with no
  mechanism to resync when they drift.

What the primitives lose is smaller than it sounds. On the five slots
that are already per-strip — chase, strobe, stutter, chaos, sweep — the
primitive is not an approximation, it is exact. On the vertical
patterns, upper-then-lower carries the motion, which is all the old
45-into-2 averaging was really showing anyway. Rain survives well: its
streak is nearly half a strip, so the two cells genuinely alternate,
and the stagger between the five droplets shows across the five
columns.

Three patterns are genuinely unrepresentable, for one shared reason:
**their average brightness barely changes, and everything interesting
is in hue or in individual pixels.** Plasma and Aurora are lit
flat-out, so averaging any chunk gives "on" while the hue variation
averages back to the centre colour. Starfield's stars pulse hard, but
eleven per half-strip all out of phase average to a steady middle grey.
For those three the honest primitive is a soft glow in the current
colour — "something slow is happening", which is true.

**Interaction feedback costs nothing.** The controller is the *source*
of every interaction: it reads the touchpad and it reads the pedal. So
touch feedback and foot-event feedback are local and need no knowledge
of the brain at all — the grid lights at the moment the message is
sent, not in response to anything coming back.

**Per-strip sculpt shows exactly.** Five columns, five per-strip blend
values. A per-strip sculpt gesture is represented precisely rather than
approximately.

**Both single indicators keep their jobs.** The surface is organised by
hand, not by function — colour on the left with the faders and the
colour dot, pattern in the centre with the phone, touch on the right
with the pad and its dot. Each dot previews what its own hand is doing,
which is why the box is legible in the dark: you look at the region
your hand is already in, rather than hunting for a light. Repurposing
one to show scene state breaks that rule, and scene state does not need
a label anyway — a scene *is* a look, and the look is already moving in
the grid. Labelling which pedal switch does what is a text problem
rather than a light problem: tape on the pedal until proven
insufficient.

**The song table moves to `shared/`.** Compiled into both nodes, so the
controller knows which preset is active at every moment without a byte
coming back — it is the thing that selected it. Reflash both from the
one repo.

**Timing.** Drawing 12 pixels on the old Nano blocked interrupts for
~360 µs against a UART tolerance of ~640 µs, which is the measurement
that made any of this viable; driving all 237 would have blocked
~7.1 ms and been hopeless. Moot on a Teensy, recorded because it is
what ruled the alternatives out.

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

**Four things that actually bite when implementing:**

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
4. **The brightness curve comes along for the ride.** FastLED's
   `hsv2rgb_rainbow` squares the value before scaling — `val =
   scale8_video(val, val)` — so a CHSV value of 80 leaves as RGB 26.
   That is a perceptual dimming curve for LEDs, and the echo inherits
   it because it converts the same CHSV the strips do. Whether a PAR
   wants it is unknown: if the fixture already bends its own response,
   we would be applying the curve twice and the bottom of the fade
   collapses into nothing. Settle it at calibration with a strip and a
   PAR lit side by side, stepping **evenly spaced** values — an uneven
   ramp makes any curve look uneven and proves nothing. If it turns out
   to be doubled, take the colour before FastLED's conversion instead
   of after.

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

### Musical position, not tempo pulses

The old brain learned about tempo through a single wire carrying one edge
per beat, so `tempoGate` / `currentTempo` / `lastGateMillis` was all it
could know: *a beat just happened, and the last one was this long ago*.
Every animation reconstructed motion from that, and nine of them carry the
same block of boilerplate — chop the beat into N slices, advance one pixel
per slice.

That model has four problems, all of them visible on stage. Position is
accumulated, so a frame that runs long leaves the animation permanently
behind the music with nothing to pull it back. A tempo change alters the
slice length but not the position already accumulated, so motion jumps.
The `- elapsedLoopTime / 2` term in every one of those blocks is a fudge
factor for loop latency, tuned by feel. And motion is quantised to whole
slices, which is the stutter that prompted the Bars rewrite.

MIDI clock supplies what the wire could not: a steadily advancing count.
So `tempo::` exposes a monotonic musical position in fractional beats,
interpolated between ticks, and presets ask where the music *is*:

```c
void Bars(CHSV color) {
    float cycle = tempo::cyclePosition(8);              // 0..1 over 8 beats
    float travel = cycle < 0.5f ? cycle * 2 : (1 - cycle) * 2;
    drawBlock(travel * (PIXELS_PER_STRIP - BARS_BAR_LENGTH), BARS_BAR_LENGTH, color);
}
```

Position is recomputed from elapsed time every frame rather than
accumulated, so a late frame lands where the music actually is. Tempo
changes need no handling. Clock loss is the position continuing at the
last known rate, and Stop is it not advancing — neither of which any
preset has to know about. Presets that want an event rather than a
position (strobe on the beat, Chaos reshuffling) still get `pulsed()`,
derived from the position rather than being the foundation.

Fractional maths would have been painful on the ATmega328; the Teensy has
hardware for it.

**Migration.** The three old variables are still published, by the main
loop, from the position the module already computes — three assignments,
not a second implementation. Every existing preset therefore runs
unchanged, so the first time the strips light up they are driven by code
already proven on stage, and anything wrong is the port rather than one of
twelve fresh guesses. Phase 3 converts the twelve tempo-dependent presets
one at a time with the wall in view, since "is this cycle two beats or
four" is a judgement you can only make by looking. The three assignments
go with the last of them.

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
