# The CC map, regrouped

**Applied 2026-09-23.** This is the map as it now stands in
`shared/aurora_protocol.h`, which the browser side now reads through
`tools/cc.js` rather than keeping its own copy. Drafted
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
  on the controller. They report what they stand at; what that *does* is
  decided by the patch, the way a modwheel reports a modwheel.
- **The touchpad sends three independent values** — X, Y and pressure — plus
  an engage signal. No mode, no presumed target. Any sender with two knobs
  can drive the axes without knowing a pad exists.
- **Hold comes back.** v1's `R_Touchpad.cpp` held the last position when the
  finger lifted, and it was useful. So the axes are *positions that persist*,
  and engage is a separate signal rather than "a finger is down".
- **Jitter is gone**, out of the map and out of both renderers. Keeping it
  alive until the scatter ships would have been carrying the replaced
  mechanism through the rebuild that exists to stop exactly that. The cost is
  real and accepted: until the scatter is in the firmware the wall has no
  texture at all, and Starfield and Glitch come off the bench page's roster
  until it does.
- **The foot pedal needs nothing here.** It hangs off the controller, which
  reads its four switches and emits messages that already exist — a Program
  Change, a note, a CC. It is a second button for a control that has one, not
  a function of its own, the same way the tap tempo button is.
- **The per-preset parameter slots retire outright.** CC 50–59 held ten
  generic numbers a hand-written pattern could read however it liked.
  `CC_PRESET_PARAM_A` through `_J` appear in exactly one file — the header
  that defines them. No brain code, no controller code, no pattern has ever
  read one. They are also the extension point of the model the generator
  replaced: a patch is a position in one continuous space now, not a preset
  with bespoke knobs. Ten numbers back.

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

**64** (damper pedal) and **96–101** (data increment and decrement, NRPN
LSB/MSB, RPN LSB/MSB) are defined too, and are left inside blocks rather than
dodged. The reason is that dodging them treats a symptom.

CC 100 and 101 are the sharper of the two, because of the **RPN null**: after
any RPN operation — setting a keyboard's pitch bend range, which DAWs and
keyboards do on patch load — the convention is to send `CC 101 = 127,
CC 100 = 127` to close the RPN so later data entry lands nowhere. Two CCs
slammed to full. Aurora holds the pulse's width amount and its wave on 100
and 101 today, so that sequence would push the width to full and the wave to
a sine.

**But the real fault is that the brain listens to every channel.** The
convention at the top of `shared/aurora_protocol.h` already says all Aurora
traffic is on `AURORA_MIDI_CHANNEL`, "so a shared cable / merger can carry
other devices' traffic without confusion", and both senders honour it. Both
*receivers* take the channel byte and drop it — `(void)channel` in
`brain/src/midi_in.cpp` and again in `controller/src/midi_io.cpp`, whose own
comment calls it "permissive for now". So a pitch bend setup on any channel
in the rig reaches Aurora. See TODO.md § Known defects.

With the receivers filtered, only what a DAW writes on *Aurora's own* track
can collide, which is the short list above — and those cost nothing to dodge,
so the layout dodges them anyway.

That leaves **114 usable numbers**: 2–6, 8–9, and 12–119 without 32.

---

## The budget

Aurora needs **75 numbers** once CC 40, jitter and the per-preset slots retire
and the fourth rocker gets a slot. There are 114 safe ones, so the layout
below spends 112 and leaves 37 spare — about a third of the map.

**Where that spare goes is the one real judgement here**, and the evidence
says it is not the modulation matrix. The scatter's open fork — spots with a
birth and a death, which is what raindrops and shooting stars need — is
priced in `docs/generator.md` at **eight to ten new controls**. That is the
only growth in the rig that anyone has costed. So the ten numbers the preset
slots give back go to the scatter, not to the pulse.

The pulse destinations keep four spare, which is one more destination: the
modulator aimed at the fan's rate amount, and nothing beyond it. A seventh
destination and the scatter's lifetime fork cannot both happen without the
ceiling decision below.

**The ceiling is real and worth naming now.** When 114 runs out the options
are NRPN, or a second MIDI channel — the brain ignores the channel byte
entirely today (`handleControlChange` takes it and drops it), so a second
channel is free for the taking but nothing is built to tell them apart. Not a
decision for this regroup; a decision that should not arrive as a surprise
the next time a lane wants three numbers.

---

## The map, in blocks

| Range | Slots | Category | Assigned | Spare |
|---|---|---|---|---|
| 2–9 | 7 | Transport / meta | 1 | 6 |
| 12–31 | 20 | The controller | 15 | 5 |
| 33–37 | 5 | Washes / DMX | 3 | 2 |
| 38–59 | 22 | Color | 20 | 2 |
| 60–75 | 16 | Generator — shape, fan, the one clock | 16 | 0 |
| 76–79 | 4 | Modulation route 0 | 4 | 0 |
| 80–82 | 3 | Spare (the generator) | 0 | 3 |
| 83–91 | 9 | Scatter / texture | 9 | 0 |
| 92–119 | 28 | Modulation routes 1–7 | 28 | 0 |

0, 1, 7, 10, 11 and 32 are skipped, each for a reason above. Everything else
between 2 and 119 is in a block.

**One block is split, and it is the routes.** Eight routes at four bytes need
32 numbers; the longest free run is 28, and 119 is the ceiling because 120–127
are Channel Mode messages. So route 0 sits in the generator's old pulse
numbers and the other seven follow the scatter. `AURORA_ROUTE_BASE` in
`shared/aurora_protocol.h` is the only place that knows, and the map wants
regrouping again before the block grows.

## The map

Every number, assigned. 95 spoken for, 19 spare.

### 2–9 · Transport / meta

| CC | | | Was | |
|---|---|---|---|---|
| **2** | `TEMPO_DIVISION` | [patch] | — | note value one tempo pulse stands for |

Spare: 3, 4, 5, 6, 8, 9.

### 12–31 · The controller

| CC | | | Was | |
|---|---|---|---|---|
| **12** | `FADER_COLOR` | [ambient] | — | fader 1 position — the Color route |
| **13** | `FADER_EXTENT` | [ambient] | — | fader 2 position — the Extent route |
| **14** | `FADER_MOTION` | [ambient] | — | fader 3 position — the Motion route |
| **15** | `PAD_X` | [gesture] | — | touchpad X, a position that persists |
| **16** | `PAD_Y` | [gesture] | — | touchpad Y, a position that persists |
| **17** | `PAD_PRESSURE` | [gesture] | — | touchpad pressure, 0-127 |
| **18** | `PAD_ENGAGE` | [gesture] | — | is the pad's effect live; separate from a finger being down |
| **19** | `ROCKER_PAD_A` | [ambient] | — | rocker below-left of the pad |
| **20** | `ROCKER_PAD_B` | [ambient] | — | rocker above the pad, left |
| **21** | `ROCKER_PAD_C` | [ambient] | — | rocker above the pad, center |
| **22** | `ROCKER_PAD_D` | [ambient] | — | rocker above the pad, right |
| **23** | `ROCKER_FADERS` | [ambient] | — | rocker below the fader panel |
| **24** | `AUDIO_FOLLOWER` | [ambient] | — | peak-follower on/off, if it lands on the Teensy |
| **25** | `AUDIO_THRESHOLD` | [ambient] | — | audio-in gate threshold, if it lands on the Teensy |
| **26** | `KEY_HELD` | [gesture] | — | is the key the last Program Change named still down |

Spare: 27, 28, 29, 30, 31.

### 33–37 · Washes / DMX

| CC | | | Was | |
|---|---|---|---|---|
| **33** | `WASH_LEVEL` | [patch] | — | wash master |
| **34** | `WASH_HUE_OFFSET` | [patch] | — | rotates the washes off the strips' hue |
| **35** | `WASH_SATURATION` | [patch] | — | scales the washes down from the strips' saturation |

Spare: 36, 37.

### 38–59 · Color

| CC | | | Was | |
|---|---|---|---|---|
| **38** | `HUE` | [patch] | — | hue center |
| **39** | `SATURATION` | [patch] | — | saturation |
| **40** | `VALUE` | [patch] | — | brightness |
| **41** | `COLOR_REGION` | [switch] | — | one gradient across the ruler / regions |
| **42** | `COLOR_RULER` | [switch] | — | across the strips / along a strip / within a shape |
| **43** | `PLACED_HUE` | [patch] | — | how far one end of the ruler departs |
| **44** | `PLACED_WHITE` | [patch] | — | toward white, or toward a pure hue |
| **45** | `PLACED_DARK` | [patch] | — | toward dark, or toward full |
| **46** | `PLACED_COUNT` | [patch] | — | regions along the ruler |
| **47** | `PLACED_WIDTH` | [patch] | — | region width |
| **48** | `PLACED_EDGE` | [patch] | — | region softness |
| **49** | `PLACED_SPEED` | [patch] | — | bipolar; the field drifting along its ruler |
| **50** | `WANDER_HUE` | [patch] | — | bipolar; how far the hue wanders |
| **51** | `WANDER_WHITE` | [patch] | — | bipolar |
| **52** | `WANDER_DARK` | [patch] | — | bipolar |
| **53** | `WANDER_RATE` | [patch] | — | 0 = frozen |
| **54** | `WANDER_SCALE` | [patch] | — | the whole wall as one, through to fine grain |
| **55** | `LIT_HUE` | [patch] | — | bipolar; hue at the core of a shape |
| **56** | `LIT_WHITE` | [patch] | — | white at the core |
| **57** | `LIT_DARK` | [patch] | — | bipolar; the core toward dark or toward full |

Spare: 58, 59.

### 60–82 · Generator — shape, fan, pulse source

| CC | | | Was | |
|---|---|---|---|---|
| **60** | `GEN_ALTERNATE` | [switch] | — | odd strips run the journey backwards |
| **61** | `GEN_BOUNCE` | [switch] | — | turn at the cell's edge instead of wrapping |
| **62** | `GEN_WIDTH` | [patch] | — | the solid core, as a proportion of one cell |
| **63** | `GEN_COUNT` | [patch] | — | shapes along the strip, 1-20 |
| **64** | `GEN_EDGE` | [patch] | — | glow into the gap, both sides |
| **65** | `GEN_TAIL` | [patch] | — | trail behind, into the gap |
| **66** | `GEN_POSITION` | [patch] | — | bipolar; where a still pattern stands in its cell |
| **67** | `GEN_SPEED` | [patch] | — | bipolar; center is still |
| **68** | `GEN_FAN_FREQ` | [patch] | — | stepped; 0 to two turns across the wall |
| **69** | `GEN_FAN_PHASE` | [patch] | — | where the wave sits on the strips |
| **70** | `GEN_FAN_RANDOM` | [patch] | — | the wave, through to a fixed draw per strip |
| **71** | `GEN_FAN` | [patch] | — | bipolar; how far apart the strips stand in their cells |
| **72** | `GEN_FAN_RATE` | [patch] | — | bipolar; how far apart their speeds stand |
| **73** | `GEN_FAN_PULSE` | [patch] | — | bipolar; how far apart they stand in the swell |
| **75** | `GEN_PULSE_RATE` | [patch][rate] | — | stepped; beats per swell. The one clock |

Spare: 74, 80, 81, 82. 76–79 are route 0, below.

### 83–100 · Scatter / texture

| CC | | | Was | |
|---|---|---|---|---|
| **83** | `SCATTER_RATE` | [patch] | — | how often a cell relights |
| **84** | `SCATTER_COUNT` | [patch] | — | cells along a strip, 1-20 |
| **85** | `SCATTER_WIDTH` | [patch] | — | the spot's core, in space and in time at once |
| **86** | `SCATTER_EDGE` | [patch] | — | hard through to a fade, both axes |
| **87** | `SCATTER_STAGGER` | [patch] | — | one clock for every cell, through to spread |
| **88** | `SCATTER_DRIFT` | [patch] | — | bipolar; how far a spot slides across its cell |
| **89** | `SCATTER_LIGHT` | [patch] | — | bipolar; amount toward full light or toward dark |
| **90** | `SCATTER_HUE` | [patch] | — | bipolar; amount, up to half the wheel |
| **91** | `SCATTER_WHITE` | [patch] | — | bipolar; toward white or toward a pure hue |

No spare: 92 onward is the route block.

### 76–79 and 92–119 · Modulation routes

Four bytes apiece — destination, amount, ratio, wave — laid out by
`AURORA_ROUTE_BASE`. The destination names the control it pushes by that
control's own CC number, and it is a switch: a morph lands it on arrival
rather than sliding it there.

| Route | CCs |
|---|---|
| 0 | 76 destination, 77 amount, 78 ratio, 79 wave |
| 1 | 92–95 |
| 2 | 96–99 |
| 3 | 100–103 |
| 4 | 104–107 |
| 5 | 108–111 |
| 6 | 112–115 |
| 7 | 116–119 |

No spare: the block is full, and raising the count means finding four more
numbers in a regroup.
## What is not a CC, and why

- **Patch selection and the blackout** — Program Change. The keypad and the
  phone's cradle share five lines and the cradle's code is the blackout.
- **Tap tempo, the mic trigger, the TRIG button, the foot pedal's four
  switches** — notes. They are events, not positions. 62–69 and 74–79 are
  reserved and empty in the note map; the pedal and TRIG are unassigned.
- **Which key the keypad names** — Program Change. *Whether it is still
  held* is CC 26, because that is a state rather than an event: the morph
  stretches for as long as it reads 127 and lands when it reads 0. A note-on
  and note-off pair would carry the same fact and repeat the key's identity in
  a second place, where the Program Change has already said it and the model
  is "toward whatever key you last pressed" — one destination at a time, so
  one gate is enough.
- **ON/OFF** — hardwired to the 9 V, read by nothing, and never will be.
- **The 12-position rotary** — its value *is* the tempo division at CC 2.
- **The indicator pixels and the ten under the pad** — outputs, recomputed on
  the controller.

---

## What applying it took

1. `shared/aurora_protocol.h` — the enum, the range comments, the rule.
2. `brain/src/midi_in.cpp` — by symbol, so it followed for free; only the
   retired CCs needed their cases removing.
3. `controller/src/` — by symbol too. The three rocker sends were renamed to
   the panel positions they report, and the fifth rocker goes unsent because
   v1 has no pin for it.
4. `tools/patch.js` and `tools/index.html` — both carried a literal CC map.
   `patch.js` also dropped its `SLOTS` block and the editor lost the collapsed
   per-pattern section that showed slots A–J.
5. **The editor's stored library is cleared, not migrated.** It lives in
   `localStorage` under `aurora.editor.library`, CC-indexed by `libToWire`,
   and Simon will empty it and rebuild by hand — settled 2026-09-23. The
   model is still being built; there is no library worth carrying across, so
   no format version and no remap.
6. Re-flash both devices. Nothing else holds a number.

**Nothing breaks, because nothing is stored anywhere that matters.** No patch
library has ever reached hardware — "Run the patch sync against the brain" is
still unchecked on the wall list — and the editor's is disposable. That is why
now is cheaper than later, and later is only ever more expensive.

---

## Settled while drafting

- **The scatter keeps nine spare and the destinations four.** Revisit it when
  the scatter's lifetime fork is actually built, not before.
- **64 and 96–101 stay inside their blocks.** Filtering the receive channel
  is the fix; fragmenting the map is not.
- **The fourth rocker gets its slot now**, before its job is decided. The
  controller block has eleven spare for exactly this reason.

## To mark up

Nothing. Every control on the box and every parameter in a patch has a
number. The map is ready to build from.

**None of it is final.** The model will move again — the scatter has an
unbuilt fork, the touchpad has no job yet, and the fan has been rebuilt once
already. This map is sized for the next stretch, not for ever.
