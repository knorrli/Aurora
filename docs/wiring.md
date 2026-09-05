# Aurora wiring reference

Target hardware for the split architecture: **Teensy 4.0** (brain) +
**Arduino Nano ATmega328** (controller), connected by a single DIN MIDI
cable. This document is the source of truth for pin assignments and the
MIDI circuits. Before moving a wire, move the line here first.

## System block diagram

```
  ┌─────────┐        MIDI          ┌─────────┐        MIDI
  │   DAW   │─────▶               │  Foot   │─────▶
  └─────────┘                      │  Ctl    │
                                   └────┬────┘
                                        │ (optional chain:
                                        │  DAW → FootCtl IN →
                                        │  FootCtl THRU →
                                        │  AuroraCtl IN)
                                        ▼
                          ┌──────────────────────────┐
                          │   Aurora Controller      │
                          │   (Nano ATmega328)       │
                          │                          │
                          │   keypad · 3 faders      │
                          │   touchpad · switches    │
                          │   tap button · mic trig  │
                          │                          │
                          │   MIDI IN  on D0/RX      │
                          │   MIDI OUT on D1/TX      │
                          └────────────┬─────────────┘
                                       │ DIN MIDI
                                       │ (up to ~15 m)
                                       ▼
                          ┌──────────────────────────┐
                          │   Aurora Brain           │
                          │   (Teensy 4.0)           │
                          │                          │
                          │   WS2812 out on pin 2    │
                          │   MIDI IN on pin 0 (RX1) │
                          │   DMX OUT on pin 17      │
                          │   (reserves pins for     │
                          │    up to 7 more strips)  │
                          └────────────┬─────────────┘
                                       │ WS2812 data
                                       │ (< 2 m)
                                       ▼
                          ┌──────────────────────────┐
                          │ 5 × 45-pixel LED strips  │
                          │ (225 pixels)             │
                          └──────────────────────────┘

The 12 UI pixels — 10 touchpad feedback + 2 colour indicators — used to
sit at the head of this same chain. They live in the controller box, which
the brain can no longer reach once it stands at the LEDs, so they are the
controller's to drive. It has no pin free for them yet; see "Reserved
room, by category" below.
```

---

## Brain pin map — Teensy 4.0

Teensy 4.0 has 40 GPIO. We use a handful and **reserve specific pins**
for OctoWS2811 expansion, since that library demands a fixed pin order.

| Pin  | Role                                   | Notes                                            |
|------|----------------------------------------|--------------------------------------------------|
|  0   | `Serial1` RX — DIN MIDI IN             | Opto-coupler output connects here                |
|  1   | `Serial1` TX — reserved (MIDI THRU)    | Leave unterminated for now; 30 min to add later  |
|  2   | WS2812 data out — main Aurora strips   | OctoWS2811 "strip 1"                             |
|  5   | RESERVED — OctoWS2811 strip 8          | Don't repurpose; adding an 8th strip should be cheap |
|  6   | RESERVED — OctoWS2811 strip 5          | Same                                             |
|  7   | RESERVED — OctoWS2811 strip 3          | Same                                             |
|  8   | RESERVED — OctoWS2811 strip 4          | Same                                             |
| 13   | Onboard LED                            | Flashes on each tempo pulse                      |
| 14   | RESERVED — OctoWS2811 strip 2          | Same                                             |
| 20   | RESERVED — OctoWS2811 strip 6          | Same                                             |
| 21   | RESERVED — OctoWS2811 strip 7          | Same                                             |
| 17   | `Serial4` TX — DMX OUT                 | To RS-485 transceiver `DI`; see DMX OUT section  |
| 3–4, 9–12, 15–16, 18–19, 22–23, 24+ | FREE    | Any future brain-side I/O                        |

The brain has **no local inputs**. Everything the old single-box Aurora
read from pins — numpad, faders, touchpad, mode switches, tempo, trigger —
now arrives as MIDI. The only local signal is the emergency fallback
switch, if that ever gets built.

Power: run the Teensy from 5 V into the `VIN` pin (or USB during dev).
Cut the `VIN`/USB jumper on the Teensy if powering from both USB and an
external 5 V supply simultaneously.

Ground the Teensy **and** the LED power supply together — WS2812 data
needs a common ground reference with its supply, not with the Teensy
alone.

---

## Controller pin map — Arduino Nano

Mostly inherited from the current Aurora wiring. The only moves are:

- **D2** — was WS2812 data out; now the **tap tempo button** (LEDs move
  to the brain).
- **D3** — was external tempo pulse input; now the **mic trigger**
  (envelope follower's digital output; the pulse input is replaced by
  MIDI clock on D0/RX).

| Pin  | Role                                       | Direction | Notes                                 |
|------|--------------------------------------------|-----------|---------------------------------------|
|  D0  | Hardware UART RX — MIDI IN                 | in        | Opto-coupler output                    |
|  D1  | Hardware UART TX — MIDI OUT                | out       | Drives DIN out circuit to brain        |
|  D2  | Tap tempo button                           | in        | External pull-up or INPUT_PULLUP       |
|  D3  | Mic trigger (digital, post envelope)       | in        | Envelope follower circuit transplanted from timing Arduino |
|  D4  | Touchpad effect mode switch (paint/sculpt) | in        | unchanged                              |
|  D5  | Hold mode switch                           | in        | unchanged                              |
|  D6  | Touchpad XP                                | I/O       | 4-wire resistive, unchanged            |
|  D7  | Touchpad YM                                | I/O       | 4-wire resistive, unchanged            |
|  D8  | Keypad line 0 — probably switch common     | in        | PINB bit 0; high in every code, incl. no-press |
|  D9  | Keypad line 1                              | in        | PINB bit 1                             |
|  D10 | Keypad line 2                              | in        | PINB bit 2                             |
|  D11 | Keypad line 3                              | in        | PINB bit 3                             |
|  D12 | Keypad line 4                              | in        | PINB bit 4                             |
|  D13 | Tempo LED                                  | out       | Onboard LED; flashes on beat           |
|  A0  | Saturation fader                           | analog    | 0–1023 → 0–127 MIDI                    |
|  A1  | Hue fader                                  | analog    | same                                   |
|  A2  | Value (brightness) fader                   | analog    | same                                   |
|  A3  | Foot pedal — 4 switches on a resistor ladder | analog  | See DESIGN.md § "Foot pedal wiring: four buttons on one analog pin" |
|  A4  | Touchpad YP (also I²C SDA)                 | analog    | 4-wire resistive, unchanged            |
|  A5  | Touchpad XM (also I²C SCL)                 | analog    | 4-wire resistive, unchanged            |
|  A6  | Touchpad strip mode switch                 | analog    | 3-state analog rotary                  |
|  A7  | A/B bank switch (preset vs. palette)       | analog    | Formerly fader-alt + preset-alt; to be simplified to 2-state per DESIGN.md |

**The keypad is not a scanned matrix.** Despite the five lines, nothing
drives columns low and reads rows back: the salvaged telephone keypad
presents a **static parallel code**, and the firmware took a single `PINB`
read and matched it against a table of thirteen values. D8 reads high in
every one of those codes including no-press, so it carries no information
and is most likely the switch common. All the data is on D9–D12.

That shape is what makes the keypad a candidate for the same
resistor-ladder treatment as the foot pedal, which would free D8–D12 and
give the controller a pin for its indicator pixels. Meter the lines before
building anything: confirm D8 really is common, and that the lines are
passive contacts rather than driven outputs.

---

## DIN MIDI IN — input circuit (both sides)

Standard opto-isolated MIDI input. Identical on the brain (on `Serial1`
RX = Teensy pin 0) and the controller (on hardware UART RX = Nano pin
D0). 6N138 is the canonical opto for MIDI speed.

```
                       +5 V
                        │
                        ┣━━━━━━━━━━━━━━━━━━━━━━┓
                        │ 220 Ω                │
        MIDI IN         │                      │  270 Ω
       5-pin DIN        │         ┌────────┐   │
     ╭────5───╮         │         │ 6N138  │   │
     │  ┌─4┐  │         │         │        │   │
     │  │   │─┼─────────┴────┬───▶│1     8 │   │
     │  │   │ │              │    │        │   │
     │  │ 2 │ │              ▼    │        │   │
     │  │ ○ │ │              ◣    │        │───┴──────── to MCU RX
     │  └───┘ │   1N4148     ┃    │        │        (Serial1.0 or D0)
     │   ┌─1┐ │              │    │        │
     │  ╱ 3 ╲ │              │    │2     7 │
     ╰───5───╯               │    │        │
         │                   │    │3     6 │── 4.7 kΩ → +5 V
         │                   │    │        │
         GND ────────────────┴────│4     5 │── GND
                                  └────────┘

Parts:
    1 × 6N138 opto-coupler (or 6N137 with ~220 Ω pull-up changes)
    1 × 1N4148 or similar small-signal diode (protection)
    2 × 220 Ω resistors (current-limit on LED side)
    1 × 270 Ω resistor (pull-up on collector side — tune if needed)
    1 × 4.7 kΩ resistor (base pull-up)
    1 × 5-pin DIN jack
```

Pin 4 of the DIN = MIDI signal (+), pin 5 = MIDI signal (−), pin 2 =
shield (tie to chassis ground, NOT signal ground, if possible).

On the **brain**, use the same circuit on pin 0 (Serial1 RX). Teensy 4.0
is 3.3 V tolerant only — the 6N138's open-collector output is fine since
it only pulls low and we use a pull-up to +3.3 V instead of +5 V on the
MCU side of the opto. The rest of the circuit stays on +5 V.

---

## DIN MIDI OUT — output circuit (controller only for now)

Two resistors and a DIN jack. No opto isolation needed on the sending
side.

```
                 MCU TX (D1 on Nano)
                        │
                        │
                  220 Ω ┣━━━━━━━━━━━━━┓
                        │             │
                       +5 V           ▼
                        │        ┌────4────┐
                  220 Ω ┃        │         │
                        ┣━━━━━━━─┤ 5-pin   │
                        │        │  DIN    │
                        ▼        │         │
                   (pin 4 on     │   ○ 2   │  2 = shield / unused
                    DIN jack)    │         │
                                 │   ╱ 3 ╲ │  3 = unused
                                 │         │
                                 │  1     5│
                                 │         │
                                 │ ○       │
                                 └─────────┘

On a Teensy 4.0 (3.3 V logic), swap the MCU-side 220 Ω for a 33 Ω — the
lower voltage needs lower resistance to hit MIDI's ~5 mA target current.
```

---

## DMX OUT — venue fixture colour echo (brain only)

Scope is colour echo, nothing else: every frame the brain writes the
active palette's centre colour × V to 1–2 hardcoded fixture addresses.
See DESIGN.md § "DMX: not the LED protocol, but useful for venue
fixtures" for why the scope stops there.

DMX is RS-485 at 250 kbaud — a balanced differential pair, so a Teensy
UART pin cannot drive it directly. An **M5Stack DMX Unit (U183)** does
the whole job in one part: a CA-IS3092W isolated transceiver (5 kVrms),
an on-board isolated DC-DC, surge protection, a switchable 120 Ω, and
the XLR-3 female socket. Nothing on the brain's perfboard but the Grove
connector.

**Port: `Serial4`, TX = pin 17.** Serial2, Serial3 and Serial5 all have
their TX pin inside the OctoWS2811 reservation, so they are unavailable.
Serial4, Serial6 and Serial7 are clear; 17 is the pick.

```
   Teensy 4.0                   M5Stack DMX Unit (U183)
                               ┌──────────────────────┐
   pin 17 ───── white ────────▶│ TXD                  │
   (Serial4 TX, 3.3 V)         │                      │    XLR-3 female
   VIN (5 V) ── red ──────────▶│ 5V                   │──▶ on the module:
                               │                      │      pin 3  data +
   GND ──────── black ────────▶│ GND                  │      pin 2  data −
                               │                      │      pin 1  common
                               │ RXD         (unused) │
                               └──────────────────────┘

Grove / PH2.0 4-pin: black = GND, red = 5 V, yellow = pin 1,
white = pin 2. Only three of the four wires are needed.
```

**The module's `RXD` / `TXD` silkscreen is written from the host's point
of view, not the module's**, so the Teensy transmits on `TXD` / white.
Verified on the bench: yellow leaves the fixture dark, white lights it.
The labels follow M5Stack's Port C convention, where Grove pin 1 is the
host's RXD and pin 2 the host's TXD — a unit labelling its own receiver
`RXD` on pin 1 would face the host's receiver and could never work
plugged into a Port C.

Teensy's `VIN` carries 5 V straight from USB during bench work, so the
module needs no separate supply. Its logic side expects 3.3 V TTL, which
Teensy drives directly.

**There is no DE/RE pin, and that is fine.** The module switches bus
direction on its own. That normally rules a board out for DMX, because
every frame opens with an 88 µs BREAK — a long low with no edges — and
a naive auto-direction circuit releases the driver partway through it,
truncating the break so fixtures reject every frame. This module is
built for DMX specifically, and M5Stack's own transmit example passes
`enablePin = -1`, i.e. no direction control from the host. Treat that as
the evidence it works; there is no pin for us to strap either way.

**Termination.** The module's own 120 Ω sits at the head of the chain
and is switch-selectable. What conventionally matters is the *far* end —
across data+/data− at the last fixture — but on short runs with a
handful of fixtures an unterminated far end is usually fine. Symptom if
it is not: intermittent flicker or fixtures jumping to wrong values.
The fix is a male XLR-3 plug with 120 Ω across pins 2 and 3, pushed into
the last fixture's DMX OUT.

**Cable and gender.** Aurora's OUT is female, a fixture's IN is male, and
a DMX cable is male on one end and female on the other — so it plugs in
one way round only. A 3-pin XLR microphone cable is pin-for-pin
identical and fine for bench work; use real 110 Ω DMX cable for anything
long or permanent.

**Software:** the `TeensyDMX` library on `Serial4`. It handles break /
mark-after-break / 44 Hz refresh timing over DMA, so it does not fight
OctoWS2811 for interrupts.

**A bring-up gotcha.** If nothing happens, check the fixture before
suspecting the circuit: PAR cans normally boot into auto or
sound-active mode and ignore DMX entirely until set to DMX mode with a
start address. On the BeamZ BCC145 that means a display reading `D001`
(4-channel mode) rather than `Au` or `SU01`.

### Fixture profile — BeamZ BCC145

The BCC145 has two personalities: `D001`–`D512` is 4-channel,
`A001`–`A512` is 8-channel. Both are in the manual, and both offsets
below were also confirmed on the bench with `bench/dmx_channel_map/`.

**4-channel (`Dxxx`) — use this one.**

| Offset | Function |
|--------|----------|
| +0     | Red      |
| +1     | Green    |
| +2     | Blue     |
| +3     | White    |

**8-channel (`Axxx`).**

| Offset | Function     | Notes                                      |
|--------|--------------|--------------------------------------------|
| +0     | Dimmer       | Master 0–100 %                             |
| +1     | Strobe       | 0–7 off, 8–255 slow to fast                |
| +2     | Red          |                                            |
| +3     | Green        |                                            |
| +4     | Blue         |                                            |
| +5     | White        |                                            |
| +6     | Macro        | ≤50 off; above that fixed colour, jump, pulse, gradient, voice-activated |
| +7     | Speed        | Macro speed                                |

At `D001` the 4-channel block is channels 1–4, so a second fixture
starts at 5.

4-channel is right for colour echo: fewer channels, and no macro channel
to accidentally write a nonzero value into — anything above 50 there
starts an auto sequence that overrides colour entirely.

**Two reasons 8-channel may earn its place later**, neither of them
urgent. Its dimmer is a real one, so brightness could come from the
dimmer channel while RGBW stays at full scale — in 4-channel mode the
only way to dim is to scale RGBW down, which throws away colour
resolution exactly where the palette is dimmest and will band on slow
fades. And its strobe channel is what the deferred tempo-synced fixture
effects would need; there is no way to strobe from the 4-channel block
except by toggling values frame to frame.

Full scale is far brighter than the strips — 255 on all four is hard to
look at directly, and the per-fixture master scale in DESIGN.md's
`Fixture` struct exists for this.

### Bench tools

Two standalone PlatformIO projects, independent of `brain/`, kept for
re-testing the link after any change:

- `bench/dmx_bringup/` — blinks the fixture on and off once a second
  with the onboard LED in sync. If the PAR follows the LED, the whole
  path works.
- `bench/dmx_channel_map/` — walks one channel at a time, announcing
  each with that many blinks of the onboard LED.

---

## What to build first (hardware-only checklist)

When you've got the Teensy in hand:

1. **Breadboard**, not perfboard. Pin 2 to a WS2812 data lead and a
   common ground with the LED supply. Run the LED supply straight to the
   strip — never through the breadboard, which has no business carrying
   the ~14 A that 225 pixels draw at full white. Turn `MAX_BRIGHTNESS`
   down for the first power-up. Verify: `FastLED.show()` lights the
   strips from the Teensy.
2. Perfboard comes at the start of Phase 5, when the DIN MIDI IN circuit
   goes on at the same time and the Grove→Lötpin adapter for the DMX unit
   has arrived. Building it earlier means soldering it twice.
3. DIN MIDI IN circuit, fed by a USB-MIDI or DAW-MIDI source during bench
   bring-up. Verify: clock parses on the brain. (USB MIDI already works
   and needs none of this — see `bench/midi_monitor/`.)
4. Mount in enclosure. Add a labeled 5-pin DIN jack.

Controller modifications (later, after brain is validated):

1. Desolder/disconnect the WS2812 data lead that currently leaves the
   controller box — the brain owns that now.
2. Repurpose D2 for a tap tempo button + LED.
3. Repurpose D3 for the mic trigger input (move the envelope follower
   from the timing Arduino).
4. Add DIN MIDI OUT circuit (2 resistors + jack).
5. Wire D0/RX to a DIN MIDI IN circuit (transplant from timing Arduino
   if it had one, otherwise new).

---

## Reserved room, by category

When this list shrinks, extend the ranges in `shared/aurora_protocol.h`
rather than squeezing new items into existing ranges.

| Resource                  | In use | Reserved | Notes                          |
|---------------------------|--------|----------|--------------------------------|
| Brain GPIO                |    3   |   7      | Pins 5/6/7/8/14/20/21 for OctoWS2811 strips 2–8 |
| Controller GPIO (digital) |   14   |   0      | D0–D13 all allocated           |
| Controller GPIO (analog)  |    8   |   0      | A3 now the foot-pedal ladder   |
| Aurora Program Change     |   19   |  108     | 0–9 presets + 64–72 palettes   |
| Aurora Control Change     |   23   |  ~90     | In the ~70 reserved slots      |
| Aurora Note On            |    6   |  ~50     | In the reserved trigger/preset-event slots |
