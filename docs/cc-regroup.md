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
  on the controller. They report what they stand at; what that *does* is
  decided by the patch, the way a modwheel reports a modwheel.
- **The touchpad sends three independent values** — X, Y and pressure — plus
  an engage signal. No mode, no presumed target. Any sender with two knobs
  can drive the axes without knowing a pad exists.
- **Hold comes back.** v1's `R_Touchpad.cpp` held the last position when the
  finger lifted, and it was useful. So the axes are *positions that persist*,
  and engage is a separate signal rather than "a finger is down".
- **CC 76 (jitter) retires** once the scatter reaches the firmware.
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

## The proposed layout

| Range | Slots | Category | Used | Spare |
|---|---|---|---|---|
| 2–6 | 5 | Transport / meta | 1 | 4 |
| 12–31 | 20 | The controller — what each control stands at | 9 | 11 |
| 33–37 | 5 | Washes / DMX | 3 | 2 |
| 38–59 | 22 | Color | 20 | 2 |
| 60–82 | 23 | Generator — shape, fan, pulse source | 18 | 5 |
| 83–100 | 18 | Scatter / texture | 9 | 9 |
| 101–119 | 19 | Where the pulse reaches — three apiece | 15 | 4 |

112 numbers, 75 of them spoken for. 0, 1, 7, 10, 11 and 32 skipped; 8 and 9
left free.

### 2–6 · Transport / meta

Tempo division, moved off CC 10 and away from pan. Four spare, and nothing
named for them — kept anyway, deliberately.

### 12–31 · The controller

Every control on the controller, reported as the value it stands at. The
patch decides what a value means; nothing here names a target.

- Touchpad X, touchpad Y, touchpad pressure — positions that persist
- Touchpad engage — separate from the axes, so hold works and a knob-only
  sender can ignore it
- Five switch positions: the 3-way rocker on A6, the 2-way rockers on D4, D5
  and A7, and the fourth rocker `DESIGN.md` § Open is still deciding a job for

Eleven spare, because the controller is being rebuilt around a Teensy and
already has more switches than jobs for them.

**The foot pedal is not here.** Four momentary switches are events, not
positions, so they belong in the note map beside the trigger and preset
events. **The 12-position rotary is not here either** — it is the tempo
switch, and its value is the tempo division at 2–6. It has never been in the
*pin* map, which is a `docs/wiring.md` problem, not a CC one.

### 33–37 · Washes / DMX

Level, hue offset, saturation. Two spare. The old 60–69 reservation was seven
spare for three controls, which is more than the PARs have ever wanted.

### 38–59 · Color

The one category that was split across the map — 20–29 and 90–99 — because it
did not fit either. Twenty controls in one block: the three faders, the placed
field with its two switches, the wander, the lit reach.

### 60–82 · Generator — shape, fan, pulse source

Shape and its two switches, the fan's six, the pulse's own four. **The fan is
whole again**: its pulse amount comes home from CC 99, which is the debt this
regroup was called for. Jitter does not reappear.

### 83–100 · Scatter / texture

Nine as built, nine spare — the largest growth allowance in the map, and the
only one backed by a costed plan rather than a guess. The scatter is settled
and rendered in `tools/preview.js`; the firmware does not have it yet, which
is the reason jitter is still alive and holding a number in the block above.

### 101–119 · Where the pulse reaches

Three apiece — amount, shape, skew — so the block reads as a table. Five
destinations today and room for exactly one more, which is the one the fan's
rate amount has been waiting on. A seventh needs the ceiling decision.

---

## Migration

1. `shared/aurora_protocol.h` — the enum, the range comments, the rule.
2. `brain/src/midi_in.cpp` — the switch is by symbol, so it follows for free.
3. `controller/src/` — eleven references, all by symbol. Follows for free.
4. `tools/patch.js` and `tools/index.html` — both carry a literal CC map.
   `patch.js` also drops its `SLOTS` block and the editor loses the collapsed
   per-pattern section that showed slots A–J.
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

## Settled since drafting

- **The scatter keeps nine spare and the destinations four.** Revisit it when
  the scatter's lifetime fork is actually built, not before.
- **64 and 96–101 stay inside their blocks.** Filtering the receive channel
  is the fix; fragmenting the map is not.
- **The fourth rocker gets its slot now**, before its job is decided. The
  controller block has eleven spare for exactly this reason.

## To mark up

- **Whether 8 and 9 stay empty** or join the transport block.
