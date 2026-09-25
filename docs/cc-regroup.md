# The CC map, regrouped

**Applied 2026-09-23, regrouped again 2026-09-25.** This is the map as it
now stands in `shared/aurora_protocol.h`, which the browser side reads
through `tools/cc.js` rather than keeping its own copy. The first regroup
came after the fan took the last numbers at the end of the map and showed
that the map is not short of numbers but short of room in the right places.
The second came when a route grew a fifth byte, its phase, and eight routes
needed one run of 40 — see `docs/modulation.md` § "A route has a phase".

The rule this exists to serve is at the top of `shared/aurora_protocol.h`:
*never assign a CC outside its stated range unless you are explicitly
extending that category.* Every category below is aligned and contiguous, so
that rule stays followable instead of being bent one control at a time.

---

## What the first regroup decided

The CC numbers in this section are the ones from before 2026-09-23.

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
slammed to full. Aurora holds route 4's destination and amount on 100 and
101 today, so that sequence would aim route 4 at CC 127, which no control
reads, and turn its amount up — the route goes silent rather than wrong.

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

Aurora needs **104 numbers**: 64 controls and eight routes of five bytes.
There are 114 safe ones, so the map spends them all but ten.

**The ten are 3–6, 8, 9, 30, 31, 53 and 70.** Only the generator's two are
in a block anything is waiting to grow into, and nothing claims either: 53
was Alternate, cut 2026-09-25, and 70 was held for a tail switch that
became a single afterglow instead. After those, the next generator control
means a regroup.

**The scatter's lifetime fork no longer fits.** Spots with a birth and a
death, which is what raindrops and shooting stars need, is priced in
`docs/generator.md` at eight to ten new controls. The first regroup kept room
for it; the route's phase byte took that room. Building it now means the
ceiling decision below, or fewer routes.

**The ceiling is real and worth naming now.** When 114 runs out the options
are NRPN, or a second MIDI channel — the brain ignores the channel byte
entirely today (`handleControlChange` takes it and drops it), so a second
channel is free for the taking but nothing is built to tell them apart.

---

## The map, in blocks

| Range | Slots | Category | Assigned | Spare |
|---|---|---|---|---|
| 2–9 | 7 | Transport / meta | 1 | 6 |
| 12–26 | 15 | The controller | 15 | 0 |
| 27–31 | 5 | Washes / DMX | 3 | 2 |
| 33–52 | 20 | Color | 20 | 0 |
| 53–70 | 18 | Generator — shape, fan, the LFO, the bend | 17 | 1 |
| 71–79 | 9 | Scatter / texture | 9 | 0 |
| 80–119 | 40 | Modulation routes | 40 | 0 |

0, 1, 7, 10, 11 and 32 are skipped, each for a reason above. Everything else
between 2 and 119 is in a block, and no block is split.

**How it got here.** Five-byte routes needed 40 numbers, and the first map
had 36 where a route could sit without leaving its own block. The washes
moved beside the controller, into what had been its spare, and color, the
generator and the scatter slid down to close the gaps. That leaves the
controller with no spare, which is right for a box whose controls are all
counted in `docs/controls.md`.

## The map

Every number, assigned. 105 spoken for, 9 spare.

### 2–9 · Transport / meta

| CC | | | |
|---|---|---|---|
| **2** | `TEMPO_DIVISION` | [patch] | note value one tempo pulse stands for |

Spare: 3, 4, 5, 6, 8, 9.

### 12–26 · The controller

| CC | | | |
|---|---|---|---|
| **12** | `FADER_COLOR` | [ambient] | fader 1 position — the Color route |
| **13** | `FADER_EXTENT` | [ambient] | fader 2 position — the Extent route |
| **14** | `FADER_MOTION` | [ambient] | fader 3 position — the Motion route |
| **15** | `PAD_X` | [gesture] | touchpad X, a position that persists |
| **16** | `PAD_Y` | [gesture] | touchpad Y, a position that persists |
| **17** | `PAD_PRESSURE` | [gesture] | touchpad pressure, 0-127 |
| **18** | `PAD_ENGAGE` | [gesture] | is the pad's effect live; separate from a finger being down |
| **19** | `ROCKER_PAD_A` | [ambient] | rocker below-left of the pad |
| **20** | `ROCKER_PAD_B` | [ambient] | rocker above the pad, left |
| **21** | `ROCKER_PAD_C` | [ambient] | rocker above the pad, center |
| **22** | `ROCKER_PAD_D` | [ambient] | rocker above the pad, right |
| **23** | `ROCKER_FADERS` | [ambient] | rocker below the fader panel |
| **24** | `AUDIO_FOLLOWER` | [ambient] | peak-follower on/off, if it lands on the Teensy |
| **25** | `AUDIO_THRESHOLD` | [ambient] | audio-in gate threshold, if it lands on the Teensy |
| **26** | `KEY_HELD` | [gesture] | is the key the last Program Change named still down |

No spare.

### 27–31 · Washes / DMX

| CC | | | |
|---|---|---|---|
| **27** | `WASH_LEVEL` | [patch][plain] | wash master |
| **28** | `WASH_HUE_OFFSET` | [patch][circular][plain] | rotates the washes off the strips' hue |
| **29** | `WASH_SATURATION` | [patch][plain] | scales the washes down from the strips' saturation |

Spare: 30, 31.

### 33–52 · Color

| CC | | | |
|---|---|---|---|
| **33** | `HUE` | [patch][circular] | hue center |
| **34** | `SATURATION` | [patch] | saturation |
| **35** | `VALUE` | [patch] | brightness |
| **36** | `COLOR_REGION` | [switch] | one gradient across the ruler / regions |
| **37** | `COLOR_RULER` | [switch] | across the strips / along a strip / within a shape |
| **38** | `PLACED_HUE` | [patch] | how far one end of the ruler departs |
| **39** | `PLACED_WHITE` | [patch] | toward white, or toward a pure hue |
| **40** | `PLACED_DARK` | [patch] | toward dark, or toward full |
| **41** | `PLACED_COUNT` | [patch] | regions along the ruler |
| **42** | `PLACED_WIDTH` | [patch] | region width |
| **43** | `PLACED_EDGE` | [patch] | region softness |
| **44** | `PLACED_SPEED` | [patch][rate] | bipolar; the field drifting along its ruler |
| **45** | `WANDER_HUE` | [patch] | bipolar; how far the hue wanders |
| **46** | `WANDER_WHITE` | [patch] | bipolar |
| **47** | `WANDER_DARK` | [patch] | bipolar |
| **48** | `WANDER_RATE` | [patch][rate] | 0 = frozen |
| **49** | `WANDER_SCALE` | [patch] | the whole wall as one, through to fine grain |
| **50** | `LIT_HUE` | [patch] | bipolar; hue at the core of a shape |
| **51** | `LIT_WHITE` | [patch] | white at the core |
| **52** | `LIT_DARK` | [patch] | bipolar; the core toward dark or toward full |

No spare.

### 53–70 · Generator — shape, fan, the LFO

| CC | | | |
|---|---|---|---|
| 53 | — | | free |
| **54** | `GEN_BOUNCE` | [switch] | turn at the cell's edge instead of wrapping |
| **55** | `GEN_WIDTH` | [patch] | the solid core, as a proportion of one cell |
| **56** | `GEN_COUNT` | [patch][plain] | shapes along the strip, 1-20 |
| **57** | `GEN_EDGE` | [patch] | glow into the gap, both sides |
| **58** | `GEN_TAIL` | [patch] | beats a passed pixel glows, 0–8, squared |
| **59** | `GEN_POSITION` | [patch][circular] | bipolar; where a still pattern stands in its cell |
| **60** | `GEN_SPEED` | [patch][rate] | bipolar; center is still |
| **61** | `GEN_FAN_FREQ` | [patch][plain] | stepped; 0 to two turns across the wall |
| **62** | `GEN_FAN_PHASE` | [patch][circular][plain] | where the wave sits on the strips |
| **63** | `GEN_FAN_RANDOM` | [patch][plain] | the wave, through to a fixed draw per strip |
| **64** | `GEN_FAN` | [patch][plain] | bipolar; how far apart the strips stand in their cells |
| **65** | `GEN_FAN_RATE` | [patch][rate][plain] | bipolar; how far apart their speeds stand |
| **66** | `GEN_FAN_LFO` | [patch][plain] | bipolar; how far apart they stand in the LFO's cycle |
| **67** | `GEN_LFO_RATE` | [patch] | stepped; beats per LFO cycle. No route may aim at it |
| **68** | `GEN_BEND` | [patch][plain] | bipolar; travel slowed and sped by where a shape is |
| **69** | `GEN_BEND_AT` | [patch][plain] | where the bend peaks, bottom to top |

Spare: 70.

### 71–79 · Scatter / texture

| CC | | | |
|---|---|---|---|
| **71** | `SCATTER_RATE` | [patch][rate] | how often a cell relights |
| **72** | `SCATTER_COUNT` | [patch] | cells along a strip, 1-20 |
| **73** | `SCATTER_WIDTH` | [patch] | the spot's core, in space and in time at once |
| **74** | `SCATTER_EDGE` | [patch] | hard through to a fade, both axes |
| **75** | `SCATTER_STAGGER` | [patch] | one clock for every cell, through to spread |
| **76** | `SCATTER_DRIFT` | [patch] | bipolar; how far a spot slides across its cell |
| **77** | `SCATTER_LIGHT` | [patch] | bipolar; amount toward full light or toward dark |
| **78** | `SCATTER_HUE` | [patch] | bipolar; amount, up to half the wheel |
| **79** | `SCATTER_WHITE` | [patch] | bipolar; toward white or toward a pure hue |

No spare.

### 80–119 · Modulation routes

Five bytes apiece — destination, amount, ratio, wave, phase — laid out by
`AURORA_ROUTE_BASE`. The destination names the control it pushes by that
control's own CC number, and it is a switch: a morph lands it on arrival
rather than sliding it there.

| Route | CCs |
|---|---|
| 0 | 80 destination, 81 amount, 82 ratio, 83 wave, 84 phase |
| 1 | 85–89 |
| 2 | 90–94 |
| 3 | 95–99 |
| 4 | 100–104 |
| 5 | 105–109 |
| 6 | 110–114 |
| 7 | 115–119 |

No spare: raising the count means finding five more numbers per route.

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

## What applying the first regroup took

The CC numbers in this section are the 2026-09-23 map's.

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

## What the second regroup took

Every consumer reads the numbers by symbol, so the enum and the range
comments in `shared/aurora_protocol.h` were the change, `tools/cc.js` was
regenerated from it, and both firmwares rebuilt. The prose CC numbers across
`docs/` and `TODO.md` were renumbered by hand. The editor's stored library
holds patches by control name, not by number, and there is none worth
keeping either way.

---

## Settled while drafting

- **64 and 96–101 stay inside their blocks.** Filtering the receive channel
  is the fix; fragmenting the map is not.
- **The controller has no spare**, since every control on the box has a
  number and `docs/controls.md` counts them.

## To mark up

Nothing. Every control on the box and every parameter in a patch has a
number.

**None of it is final.** The model will move again — the scatter has an
unbuilt fork, the touchpad has no job yet, and the fan has been rebuilt once
already. This map is sized for the next stretch, not for ever.
