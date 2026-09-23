# The CC regroup — a proposal

**Nothing here has been applied.** This is a layout to mark up. Drafted
2026-09-23, after the fan took the last numbers at the end of the map and
showed that the map is not short of numbers but short of room in the right
places.

The rule this exists to serve is at the top of `shared/aurora_protocol.h`:
*never assign a CC outside its stated range unless you are explicitly
extending that category.* Every category below is aligned and contiguous, so
that rule stays followable instead of being bent one control at a time.

---

## What is already decided

- **CC 40 retires.** The packed bitmap goes. The controller has only ever
  sent `flags = 0`, so `faderAltModeEnabled` and `presetAltModeEnabled` are
  already permanently false; the v1 paths that read them stay and simply lose
  a setter.
- **CC 41–44 lose their meanings, not their slots.** The switches are still
  on the box. They report position; what a position *does* is decided by the
  patch, the way a modwheel reports a modwheel.
- **The touchpad sends three independent values** — X, Y and pressure — plus
  an engage signal. No mode, no presumed target. Any sender with two knobs
  can drive the axes without knowing a pad exists.
- **Hold comes back.** v1's `R_Touchpad.cpp` held the last position when the
  finger lifted, and it was useful. So the axes are *positions that persist*,
  and engage is a separate signal rather than "a finger is down".
- **CC 76 (jitter) retires** once the scatter reaches the firmware.

## What this does not settle

**What the touchpad is for.** `DESIGN.md` § Open says no job has been named
for it, and that is settle-by-looking. Numbering does not need the answer:
raw sensor values are the right shape under every answer.

---

## Numbers to stay off

Aurora is driven by a DAW in two of its three modes — `DESIGN.md` § "Three
ways the rig gets driven" — so a channel can carry traffic nobody aimed at
Aurora.

| | Why |
|---|---|
| **120–127** | Channel mode messages. All Sound Off, Reset All Controllers, All Notes Off. Never parameters. |
| **0, 32** | Bank select MSB/LSB, and Mainstage changes patches with Program Changes, which bank select travels with. |
| **1** | Modwheel. The most likely stray CC on any channel a keyboard can reach. |
| **7, 11** | Volume and expression. A DAW track writes these without being asked. |
| **10** | Pan. **A live collision — CC 10 is `CC_TEMPO_DIVISION` today.** |

Two lower risks, accepted rather than dodged, because dodging them fragments
the blocks for little gain: **64** (sustain) and **96–101** (NRPN/RPN select)
both sit inside blocks below. They only fire if a keyboard or an
NRPN-speaking device is routed onto Aurora's channel. Say so if you would
rather spend the fragmentation.

That leaves **113 usable numbers**: 2–6, 8–9, and 12–119 without 32.

---

## The budget, which is the real finding

Aurora needs **85 numbers** once CC 40 and jitter retire and the fourth
rocker gets a slot. There are 113 safe ones. The map is three-quarters full,
and a regroup can give every category contiguous, correctly-sized space — but
it cannot give every category room to double. The layout below spends 112 of
the 113 and buys, in total:

- one more pulse destination, which is exactly what the modulator aimed at
  the fan's rate amount needs
- eleven spare in the box, for a controller being rebuilt
- two to four spare in each of color, the generator and the washes
- nothing at all spare in the per-preset slots

**The ceiling is real and worth naming now.** When 113 runs out the options
are NRPN, or a second MIDI channel — the brain ignores the channel byte
entirely today (`handleControlChange` takes it and drops it), so a second
channel is free for the taking but nothing is built to tell them apart. Not a
decision for this regroup; a decision that should not arrive as a surprise
the next time a lane wants three numbers.

---

## The proposed layout

| Range | Slots | Category | Used | Spare |
|---|---|---|---|---|
| 2–6 | 5 | Transport / meta | 1 | 4 |
| 12–31 | 20 | The box — positions, meaning decided elsewhere | 9 | 11 |
| 33–42 | 10 | Per-preset parameter slots | 10 | 0 |
| 43–47 | 5 | Washes / DMX | 3 | 2 |
| 48–69 | 22 | Color | 20 | 2 |
| 70–91 | 22 | Generator — shape, fan, pulse source | 18 | 4 |
| 92–101 | 10 | Scatter / texture | 9 | 1 |
| 102–119 | 18 | Where the pulse reaches — three apiece | 15 | 3 |

112 numbers. 0, 1, 7, 10, 11 and 32 skipped; 8 and 9 left free.

### 2–6 · Transport / meta

Tempo division, moved off CC 10 and away from pan. Four spare.

### 12–31 · The box

Everything the box physically has, reported as a value. The patch decides
what a value means; nothing here names a target.

- Touchpad X, touchpad Y, touchpad pressure — positions that persist
- Touchpad engage — separate from the axes, so hold works and a knob-only
  sender can ignore it
- Five switch positions: the 3-way rocker on A6, the 2-way rockers on D4, D5
  and A7, and the fourth rocker `DESIGN.md` § Open is still deciding a job for

Eleven spare, which is the largest allowance here and deliberately so: this
is the block a box being rebuilt around a Teensy will grow into.

**The foot pedal is not here.** Four momentary switches are events, not
positions, so they belong in the note map beside the trigger and preset
events. **The 12-position rotary is not here either** — it is the tempo
switch, and its value is the tempo division at 2–6. It has never been in the
*pin* map, which is a `docs/wiring.md` problem, not a CC one.

### 33–42 · Per-preset parameter slots

Unchanged, ten slots, no spare. They are deliberately generic and the count
was always arbitrary; ten is what the patch format already carries.

### 43–47 · Washes / DMX

Level, hue offset, saturation. Two spare. The old 60–69 reservation was seven
spare for three controls, which is more than the PARs have ever wanted.

### 48–69 · Color

The one category that was split across the map — 20–29 and 90–99 — because it
did not fit either. Twenty controls in one block: the three faders, the placed
field with its two switches, the wander, the lit reach.

### 70–91 · Generator — shape, fan, pulse source

Shape and its two switches, the fan's six, the pulse's own four. **The fan is
whole again**: its pulse amount comes home from CC 99, which is the debt this
regroup was called for. Jitter does not reappear.

### 92–101 · Scatter / texture

Nine as built, one spare. The scatter is settled and rendered in
`tools/preview.js`; the firmware does not have it yet, which is the reason
jitter is still alive and holding a number in the block above.

### 102–119 · Where the pulse reaches

Three apiece — amount, shape, skew — so the block reads as a table. Five
destinations today and room for exactly one more, which is the one the fan's
rate amount has been waiting on. A seventh needs the ceiling decision.

---

## Migration

1. `shared/aurora_protocol.h` — the enum, the range comments, the rule.
2. `brain/src/midi_in.cpp` — the switch is by symbol, so it follows for free.
3. `controller/src/` — eleven references, all by symbol. Follows for free.
4. `tools/patch.js` and `tools/index.html` — both carry a literal CC map.
5. **The editor's stored library.** `localStorage` under
   `aurora.editor.library`, written CC-indexed by `libToWire`. It needs a
   format version and an old→new remap on load, or every saved patch comes
   back scrambled. **This is the only thing that breaks.**
6. Re-flash both devices. Nothing else holds a number.

**No patch library has ever reached hardware** — "Run the patch sync against
the brain" is still unchecked on the wall list — so there is no device state
to migrate. That is why now is cheaper than later, and later is only ever
more expensive.

---

## To mark up

- **The block sizes.** The box gets eleven spare and the destinations get
  three. If that is backwards, say so — it is the one judgement here that is
  about where Aurora grows next rather than about arithmetic.
- **Whether the per-preset slots still want ten.** They serve the
  hand-written presets, which are parked. Four of them back would give the
  destinations a seventh.
- **Whether to spend fragmentation dodging 64 and 96–101.**
- **Whether the fourth rocker gets a slot before its job is decided.**
- **Whether 8 and 9 stay empty** or join the transport block.
