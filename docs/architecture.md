# Aurora — architecture

How the system is built and why. These decisions are settled and mostly
already running; they are not part of the interaction-model reset.

Measurements that back any of this are in `docs/bench-facts.md`. Pins
and circuits are in `docs/wiring.md`.

---

## The split: brain at the LEDs, controller at the performer

One DIN MIDI cable between them, reliable to about 15 m.

**Brain node**, standing at the LEDs:

- Teensy 4.0, no local inputs at all. Everything the old single-box
  Aurora read from pins now arrives as MIDI.
- WS2812 output through a level shifter.
- DIN MIDI in. It cannot tell the Aurora controller from a DAW or any
  other source, and does not need to — whatever arrives is the truth.
- DMX out for the wash fixtures.

**Controller node**, wherever the performer stands:

- Teensy 4.0. Pure I/O to MIDI encoding: keypad, three faders,
  touchpad, switches, foot pedal, tap tempo, mic trigger.
- Drives its own 12 indicator pixels.
- Single DIN MIDI out to the brain.

**Three reasons the split exists:**

1. **Cable length.** WS2812 data degrades over long runs, so the old
   single box had to sit within ~2 m of the first LED — which is not
   where the performer wants to stand. Brain-at-LEDs means short clean
   data runs no matter how many strips get added.
2. **Timing.** A second Arduino currently turns MIDI clock into a tempo
   pulse on one wire. Speaking MIDI directly retires it and makes the
   brain a full citizen of a MIDI rig.
3. **Scale.** More fixtures without rationing SRAM or worrying about
   FastLED blocking MIDI.

The cost is two firmwares, two power supplies and one more cable. The
`shared/` directory compiles for both nodes, and one spare board in the
gig bag covers either failure.

## Why Teensy 4.0

Both a Teensy and an ESP32-S3 were on hand.

- No WiFi or BT stack running in the background, so no latent timing
  jitter from radio interrupts.
- OctoWS2811 plus FastLED on Teensy is the most battle-tested WS2812
  stack there is. The ESP32's RMT peripheral is good; Teensy is boring.
- Multiple hardware UARTs map cleanly onto DIN in and out.

The ESP32 would win if we wanted WiFi or BLE-MIDI, and the DIN-only
decision removes that advantage.

**The controller is a second Teensy rather than the Nano**, decided
2026-09-05. A Nano solution existed and closed with exactly one pin
spare, but what tipped it was rendering load rather than pin count:
pattern work, palette maths and FastLED on the controller, on top of
MIDI in and out, a touchpad, three faders and debouncing, inside 2 KB of
SRAM. It would probably have fit. The failure mode is discovering late
that it does not.

What the move costs:

- **Everything passive comes across unchanged.** Resistor ladders are
  voltage dividers, so they are ratios — levels scale with the supply
  and the ADC reference together, and decode tables are untouched at
  3.3 V.
- **Two real items.** The mic envelope follower is built for 5 V and
  needs re-referencing. WS2812 data from a 3.3 V pin needs a level
  shifter — the same problem the brain has, solved once for both.
- **Not 5 V tolerant**, so a wiring mistake kills the board.

**What it has for storage**, measured 2026-09-22 against the installed
core rather than assumed. Flash is 1984 KB and the firmware uses well
under a tenth of it; RAM is 1 MB. The emulated EEPROM is 1080 bytes
(`E2END 0x437` in `cores/teensy4/avr/eeprom.h`) — an AVR compatibility
shim, not the chip's storage, and too small for even one patch. Patches
therefore go in LittleFS on the program flash, which survives a power
cycle but not a firmware upload. See `DESIGN.md` § "Patch storage".

The keypad ladder conversion left the plan with this decision: five pins
is nothing on a 40-pin part, so the keypad keeps its existing wiring.
The **foot-pedal** ladder survives, because it exists to fit a
two-conductor cable, not because of pin count.

## Clock routing: the controller is the source

The controller's tempo LED has to flash in sync with whatever tempo
source is live, so the controller must see the clock. It cannot go
straight from a DAW to the brain.

```
 DAW ──────┐
           ├──▶ Controller ── MIDI OUT ──▶ Brain
 Foot Ctl ─┘         │
                     ├─ parses clock → tempo LED
                     └─ emits its own fresh clock, always
```

**The controller does not pass external clock through verbatim.** It
tracks tempo internally and generates fresh 24 PPQN from it. When the
performer taps, the same logic switches source from external to local
tap and the brain sees no discontinuity.

Why, over a true MIDI THRU merge:

- "Tap overrides external clock" falls out for free.
- Upstream drop-outs — DAW paused, cable glitch — do not propagate. The
  controller keeps emitting at last-known tempo.
- One clock source arriving at the brain, always, which makes debugging
  tractable.

## Tempo division: the clock stays honest, the brain divides

The controller carries a rotary for subdivisions — half time, triplets,
sixteenths. The tempting implementation is to emit clock at the divided
rate. That is wrong: the brain would believe a 120 BPM song was running
at 60, and anything that later cares about real musical time inherits
the lie.

So the controller always emits true 24 PPQN and reports the rotary
position separately on `CC_TEMPO_DIVISION`. The brain counts ticks and
fires its pulse every N of them.

`TEMPO_DIV_QUARTER` is numbered 0 deliberately, so a controller that has
not sent the CC yet lands on ordinary one-pulse-per-beat rather than
something exotic mid-song.

The division list in `shared/aurora_protocol.h` is a guess at what the
rotary offers. Match it to the real switch positions once the controller
is to hand.

## Musical position, not tempo pulses

The old brain learned about tempo through one wire carrying one edge per
beat, so "a beat just happened, and the last one was this long ago" was
all it could know. Every animation reconstructed motion from that, and
nine carried the same boilerplate: chop the beat into N slices, advance
one pixel per slice.

Four problems, all visible on stage. Position was accumulated, so a long
frame left the animation permanently behind with nothing to pull it
back. A tempo change altered slice length but not accumulated position,
so motion jumped. Every block carried an `- elapsedLoopTime / 2` fudge
factor tuned by feel. And motion was quantized to whole slices.

MIDI clock supplies what one wire could not: a steadily advancing count.
So `tempo::` exposes a **monotonic musical position in fractional
beats**, interpolated between ticks, and presets ask where the music is:

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
last known rate; Stop is it not advancing. No preset has to know about
either. Presets wanting an event rather than a position still get
`pulsed()`, derived from the position rather than being the foundation.

Fractional maths would have hurt on an ATmega328. The Teensy has
hardware for it.

**Migration.** The three old globals are still published by the main
loop from the position the module computes — three assignments, not a
second implementation. Every existing preset runs unchanged, so the
first time the strips lit they were driven by code already proven on
stage, and anything wrong was the port rather than one of twelve fresh
guesses. The assignments go with the last converted preset.

## DMX is for the wash fixtures, never for the strips

**Why not the strips:** 235 pixels × 3 channels is 705 DMX channels,
over one universe. Art-Net or sACN solves that and adds a networked
device to the rig. DMX desks expect fixtures with 5–15 channels, so we
would end up exposing Aurora at the parameter level anyway. And nobody
in the band runs a console.

**Why an isolated transceiver.** XLR pin 1 ties the brain's ground to
the venue's lighting ground. Those are usually separate mains circuits,
sometimes separate phases, and the difference lands across the
transceiver, which tolerates about −7 to +12 V before it dies. The
M5Stack DMX Unit puts a barrier there instead, and costs less than a
bare chip plus a separate XLR jack.

**The socket is XLR-3, not the XLR-5 the standard specifies**, because
3-pin is what is fitted to the PAR cans we expect to meet, our own
included. An adapter covers venues that go by the book — a cable-bag
item, not a design change.

`TeensyDMX` on `Serial4` is DMA-driven and non-blocking, with no
interrupt conflict against OctoWS2811. `Serial2/3/5` have their TX
inside the OctoWS2811 pin reservation, which is why it is `Serial4`.

## The indicator pixels are recomputed, not streamed

The 12 pixels sit in the controller box, so after the split the brain
cannot reach them and the controller drives them.

An earlier note concluded the strip preview had to be dropped. It
weighed exactly one way of keeping it — streaming strip data back over
the MIDI link — and rejected that correctly, as precisely the traffic
the split exists to avoid. What it missed is that **the controller can
recompute the picture rather than receive it.** It already knows the
clock, the color, and which pattern is active, because it is the thing
that selected them.

Running the *real* renderers on the controller was considered and
rejected:

- Every pattern would have to become resolution-independent — written as
  brightness against normalized position rather than against pixel N —
  so both boxes could sample one function at different densities. That
  is a permanent tax on how patterns get written, and it does not work
  at all for Starfield's individual stars or Glitch's random pixels.
- The controller would have to duplicate the brain's entire performance
  state. Miss any of it and the box shows a confident lie, which is
  worse than showing nothing. Two boxes that must agree, with no
  mechanism to resync when they drift.

**Interaction feedback costs nothing**, because the controller is the
*source* of every interaction — it reads the touchpad and it reads the
pedal. Feedback is local and needs no knowledge of the brain; the grid
lights when the message is sent, not in response to anything coming
back.

**What the grid should show is deliberately reopened** as part of the
interaction-model reset. Only the mechanism above is settled.

**The song table lives in `shared/`**, compiled into both nodes, so the
controller knows what is active without a byte coming back.

## Development path: brain-first over USB MIDI

Teensy 4.0 is a class-compliant USB MIDI device out of the box, so the
whole brain firmware can be built and validated on a desk with a laptop
and a strip — no controller modifications, no DIN hardware, no
enclosure. Adding DIN input afterwards is a short solder job on the same
board.

**USB MIDI stays compiled in forever** as a dev path. The Teensy MIDI
library treats USB and serial MIDI almost identically, so it is the same
code either way. The live rig uses DIN.

## The file layout is v1's, by inheritance rather than by decision

Raised 2026-09-24, recorded rather than acted on.

`P_Fills.cpp`, `P_Movements.cpp`, `P_Strobes.cpp` and `IR_Preset.cpp` are
the nine-preset roster, `PRESET_*` 0–9. The generator is
`PRESET_GENERATOR = 10` — one preset among eleven, standing beside the
thing it is meant to replace. The setter-per-CC pattern, where a control
is converted to internal units the moment its CC arrives, fits "a knob
changes a variable", which is what v1 was. `presetColor` as a global read
by both the strips and the washes is the same inheritance.

None of that is wrong, and none of it was chosen for what Aurora is
becoming. The rule this records is only that the current shape is not an
argument for itself: where it no longer fits, redesign rather than work
around.

**One job serves three purposes, and it does not look like it.** Making
`Generator()` run on a host — stubbing FastLED, `CHSV` and
`tempo::beats()` — is written down in `docs/modulation.md` as what blocks
comparing a whole rendered frame. It also buys real testability, and it is
the first half of the only serious route to one renderer instead of two:
extract the maths as plain functions over a parameter block with no
Arduino in them, wrap it with FastLED for the brain and with WASM for the
editor. `tools/crosscheck.mjs` exists purely to stop the two
implementations drifting, so it is a symptom of the duplication, not an
asset.

**The counter-argument, which is real.** `tools/preview.js` states that
the color layer was designed there first and ported once it survived. A
fast, disposable JavaScript implementation is how this instrument gets
designed, and one shared renderer takes that away or puts a rebuild in
front of every experiment. Worth weighing rather than assuming the
duplication is pure cost.

## Explicitly not wanted

- **Wireless between controller and brain.** Stage reliability beats
  cable freedom.
- **USB MIDI as the live path.** Cable length is a non-starter on stage.
- **MIDI out on the controller for external gear.** Its MIDI out exists
  to talk to its own brain. We are not driving other instruments.
- **DMX in.** A console driving Aurora is a different project.
- **Any dependency on haze.** Most venues forbid it and the practice
  room cannot take it. A hazer stays worth owning for rooms that allow
  it, but nothing in the design may assume air.

## Building: the include case has to match exactly

Found 2026-09-18 after a program change silently did nothing.

Every `.cpp` in `brain/src/` included the main header as `"aurora.h"`,
while the file on disk is `Aurora.h`. macOS has a case-insensitive
filesystem, so this compiled perfectly. **SCons, which does PlatformIO's
dependency scanning, looks the name up case-sensitively, fails to resolve
it, and silently skips scanning it.**

The result: nothing reached *through* `Aurora.h` was ever tracked as a
dependency. Editing `Aurora.h` itself — the globals, the framebuffer
layout, the pin defines — rebuilt nothing. Editing
`shared/aurora_protocol.h` rebuilt only `tempo.cpp`, which happened to
include it directly.

The symptom that exposed it was a new program change being accepted by the
renderer and rejected by the MIDI handler, because the two files had been
compiled an hour apart against different versions of the same header. The
build reported success every time.

**Match the case exactly in every `#include`.** Nothing warns about this,
the build never fails, and the failure mode is a binary built from a
mixture of old and new headers.

If a change to a shared header ever appears not to take effect, the check
is to compare object-file timestamps against the header's:

```bash
stat -f '%Sm %N' -t '%H:%M:%S' shared/aurora_protocol.h \
  brain/.pio/build/teensy40/src/*.o | sort
```

A clean build is the workaround; matching the case is the fix.
