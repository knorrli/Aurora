# Aurora wiring reference

Target hardware for the split architecture: **Teensy 4.0** (brain) +
**a second Teensy 4.0** (controller, replacing the Arduino Nano as of
2026-09-05), connected by a single DIN MIDI cable. This document is the source of truth for pin assignments and the
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

The 12 UI pixels — 10 touchpad feedback + 2 color indicators — used to
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

## WS2812 strips — strip boxes and the data chain

Five strips of 45 pixels. Each strip has a small hand-built junction box
at its head, holding the connectors and the standard NeoPixel protection
circuit. The boxes predate the split and were built around the 5 V Nano.

### Inside a box

Nothing active — two passive parts:

- **1000 µF** electrolytic across +5 V and GND, absorbing the current
  surge when the strip switches on.
- **470 Ω 1 %** in series with the data line, between the data-in
  connector and the first pixel's `DIN`. Damps ringing on the long
  cable and protects that input.

Do not add a second series resistor at the brain. 470 Ω already sits at
the top of the 300–500 Ω window this circuit wants, and stacking another
starts rounding off the signal edges.

### Connectors

Three barrel jacks per box. **The two data jacks use a wider center pin
than the power jack**, so a power plug cannot be forced into a data
socket.

| Jack       | Carries                                             |
|------------|-----------------------------------------------------|
| Power      | +5 V and GND, from that strip's own PSU             |
| Data in    | Data and GND, from the brain or the previous strip  |
| Data thru  | Data and GND, on to the next strip                  |

Every strip has **its own 5 V supply**. Ground is common across the
whole network: the data cables tie the separate supplies' grounds
together, and that is also how the brain picks up its ground reference
to the strips.

### The chain

```
   Teensy pin 2                 ~2.5 m           ~2.5 m
        │                          │                │
        ▼                          ▼                ▼
   ┌─────────┐              ┌─────────┐       ┌─────────┐
   │ strip 1 │─────thru────▶│ strip 2 │──────▶│ strip 3 │──▶ 4 ──▶ 5
   │  box    │              │  box    │       │  box    │
   └────┬────┘              └────┬────┘       └────┬────┘
        │                        │                 │
      5 V PSU                  5 V PSU           5 V PSU
```

**Only the first hop runs at the brain's logic level.** A WS2812 does
not pass data through — it reads its own 24 bits and re-transmits the
remainder from its own 5 V supply. Every 2.5 m link between strips is
therefore driven at 5 V by the previous strip's last pixel, whatever the
brain is. Those runs are proven in the field over hundreds of gigs, and
the split does not touch them.

What the split does change is that one first hop, which the Nano drove
at 5 V and the Teensy drives at 3.3 V.

### 3.3 V data and the level shifter

A WS2812 on a 5 V supply wants about 3.5 V (0.7 × VDD) to read a
reliable "high". Teensy 4.0 outputs 3.3 V, which is under that. It
often works in practice, especially on a short first cable, but it is
out of spec and drifts with temperature.

The failure is all-or-nothing rather than gradual: if the first pixel
misreads its bits it forwards garbage, so the entire wall goes wrong at
once. Fine to gamble with on the bench, not on stage.

**Fix: a 74AHCT125 quad buffer between pin 2 and the strip 1 data
cable.** It takes 3.3 V in and drives 5 V out, restoring exactly the
signal the Nano produced.

DIP-14 numbering runs counter-clockwise seen from above. With the notch
at the left end, pin 1 is **bottom**-left and the bottom row counts 1–7
left to right; the top row is 8–14 counting right to left, so pin 14
sits directly above pin 1. Some packages also carry a round divot, which
may be a molding mark rather than a pin-1 dot — trust the notch.

| Chip pin  | To                                                    |
|-----------|-------------------------------------------------------|
| 14 `VCC`  | 5 V — `VIN` on the bench, which carries USB 5 V        |
| 7 `GND`   | common ground                                         |
| 1 `1OE`   | GND — enables the channel                             |
| 2 `1A`    | Teensy pin 2                                          |
| 3 `1Y`    | data-in pigtail, center pin                           |
| 5, 9, 12  | GND — unused inputs, never leave them floating        |
| 4, 10, 13 | VCC — unused output-enables, floating just as badly   |
| 14 ↔ 7    | 0.1 µF ceramic, short leads, at the chip body          |

The 0.1 µF earns its place: AHCT edges are a couple of nanoseconds, and
the breadboard rail back to the Teensy has enough inductance that it
cannot supply a current spike that fast. Without a local reservoir the
chip's own supply rings — which is the exact defect the buffer is here
to remove.

The part must be **HCT** or **AHCT**. Plain HC or AHC has the same
3.5 V threshold as the pixels themselves and fixes nothing, while
looking identical on the shelf. A DC bench check will not separate them
either: an AHC part typically switches near 2.5 V despite guaranteeing
only 3.5 V, so it passes a multimeter and fails on stage. Read the
marking on the chip body — `AHCT` or `HCT` between the prefix and `125`.

Three of the four channels stay spare, which covers the OctoWS2811
expansion pins reserved above. The controller needs the same part for
its indicator pixels, so buy two.

The chip also sits between the Teensy and a connector that gets plugged
and unplugged in the dark. A barrel plug shorts center to sleeve as it
slides in, so whatever drives that line is briefly shorted to ground —
better a one-franc buffer than the Teensy.

**Built and verified 2026-09-09** on the breadboard with an
`SN74AHCT125N`. Solid fill and Starfield-against-black both render
cleanly, and the proximity flicker is gone, two-prong charger included.

### Where they stand on the wall

**The data chain runs right to left across the room.** Strip 5 stands at
the left-hand end and strip 1 at the right. Read off PC 11 on
2026-09-22, which paints each strip one flat color — red, orange,
green, cyan, blue in chain order — and came back blue on the left.

Nothing in the firmware knows this. It matters in two places:

- `tools/preview.js` draws the wall, and carries the order as
  `WALL_STRIP_ORDER`. Change it there if the rig is ever re-strung.
- **The fan's wave counts from strip 1**, so its phase starts at the
  wall's right and a staircase climbs leftward. Reversing the rigging
  would reverse it; so does turning the fan's amount through zero to the
  other side, which is what that control is bipolar for.

**Pixel 0 is at the bottom of every strip**, and always will be — stated
by the performer, 2026-09-25. So positive speed travels up, and the bend's
plus is fast at the top. PC 11 could never have told you this: every strip
is one flat color, so there is no end to tell apart. The editor keeps its
"pixel 0 at bottom" toggle as a check, not as an open question.

---

## Controller pin map — Arduino Nano

> **This is the wiring in the box today**, and it is the only record of
> it. The controller is being rebuilt around a second Teensy 4.0 — see
> `docs/architecture.md` § "Why Teensy 4.0" — and that map is a fresh
> assignment rather than a translation of this one. It must find a home
> for the 12-position rotary, which no map has ever listed. Keep this
> table until the Teensy map exists and the box is rewired.
>
> It was never listed because it was never on this board: it sat on a
> *second* Nano that did the tempo calculation and emitted a clock signal
> to the primary. See § "12-position rotary" below.

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
|  A3  | Foot pedal — 4 switches on a resistor ladder | analog  | See § "Foot pedal" in this file                                     |
|  A4  | Touchpad YP (also I²C SDA)                 | analog    | 4-wire resistive, unchanged            |
|  A5  | Touchpad XM (also I²C SCL)                 | analog    | 4-wire resistive, unchanged            |
|  A6  | Touchpad strip mode switch                 | analog    | 3-way rocker (left / center / right)   |
|  A7  | A/B bank switch (preset vs. palette)       | analog    | Formerly fader-alt + preset-alt; unread in firmware since palettes became CC 53 |

**There is exactly one rotary in the rig** — the 12-position tempo
switch. Everything else is a rocker: D4, D5 and A7 are 2-way, A6 is
3-way.

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

## Foot pedal — four switches on one analog line

The pedal is a controller-side peripheral: bare momentary switches, no
microcontroller. The controller reads them and turns them into MIDI.

**The connector carries two conductors today**, which is why the
switches share one analog line instead of taking a pin each. The
original reason was the Nano's single free pin; that reason is gone, and
**the connector itself is currently an open question** — a stereo jack
would add a second line without adding power to the pedal.

Each switch pulls the line toward ground through its own resistor, under
a common pull-up. The pull-up sits at the controller; the four ladder
resistors sit in the pedal.

```
   +5 V ──[ R_top ]──┬── analog in
                     │
            ┌────────┼────────┬────────┐
           SW1      SW2      SW3      SW4
            │        │        │        │
          [ R1 ]   [ R2 ]   [ R3 ]   [ R4 ]
            │        │        │        │
           GND      GND      GND      GND
```

| Resistor | Value  |
|----------|--------|
| R_top    | 1 kΩ   |
| R1       | 2.2 kΩ |
| R2       | 5.1 kΩ |
| R3       | 10 kΩ  |
| R4       | 20 kΩ  |

**Why every combination is readable.** Parallel resistors add in
*conductance*, not resistance. Each closed switch contributes its own
1/R independently of the others, so every subset produces a different
total conductance and therefore a different voltage — sixteen
combinations, sixteen levels. The divider is acting as a crude 4-bit ADC
of which buttons are down.

Binary-weighting the conductances is the textbook choice, but the
divider is non-linear in conductance, which bunches the
many-buttons-pressed end together. The values above were found by
searching for the widest *minimum* separation over the 1 % kit on hand.
They spread the sixteen levels between 2.777 V (all four down) and
5.000 V (none), with the closest neighbors 79 mV apart — about 16
counts on a 10-bit ADC.

**Decode by nearest match against a table of the sixteen levels, never
by binary-ordered thresholds.** The levels are not ordered by binary
code: `SW3+SW4` sits above `SW2` alone.

A 1 kΩ pull-up also keeps the source impedance low — it peaks at R_top
with nothing pressed — which helps the sample-and-hold settle inside one
read.

**These values are forgiving of tolerance, which not every set is.**
Simulating twenty thousand builds, 1 % parts never bring two levels
closer than 78 mV, and even 5 % parts hold 55 mV. That is a property of
this particular set, not of ladders generally: the otherwise-similar
3.3k / 3.9k / 5.6k / 10k / 22k set has a wider nominal gap yet collapses
at 5 %, with about one build in nine landing under 30 mV. **If the values
are ever re-picked, re-run the tolerance check rather than trusting
nominal spacing.**

A calibration pass — press each combination once, store the readings —
removes tolerance from the picture entirely and is worth doing
regardless.

**Two firmware gotchas.**

- A press is not instantaneous. While a contact bounces or a second foot
  lands, the line sweeps through voltages that are themselves valid
  codes, so a naive reader emits phantom events. Require several
  consecutive agreeing samples before accepting a change.
- A reading that matches nothing within tolerance should be **discarded,
  not snapped to the nearest level.** That is what an unplugged cable or
  a dirty contact looks like, and on stage it should do nothing rather
  than fire something random.

**One line tops out at four switches.** A fifth roughly halves the
spacing and pushes it under what 10-bit sampling separates reliably. The
escape, if more are wanted on one line: if only one switch is ever read
at a time, five levels instead of sixteen gives far more margin and the
same wire carries six to eight comfortably. Chords cost switch count;
switch count costs chords.

---

## 12-position rotary — tempo division on one analog line

Twelve poles into one analog pin, the same trick as the foot pedal and a
much easier version of it: a rotary closes exactly **one** position at a
time, so there are no combinations to separate. Equal resistors, evenly
spaced levels, no value search.

Chain twelve equal resistors across the supply and tap every junction:

```
  3V3 ──[1k]──┬──[1k]──┬── ... ──[1k]──┬── GND
              │        │               │
            pos 12   pos 11          pos 1

  wiper ──┬──→ analog in
          │
       [100k]
          │
         GND
```

| Part | Value | Count |
|------|-------|-------|
| Ladder resistors | 1 kΩ | 12 |
| Pull-down | 100 kΩ | 1 |

Twelve 1 kΩ draws about 275 µA and puts the taps 0.275 V apart — around
85 counts on a 10-bit ADC and 340 on the Teensy's 12-bit, roughly five
times the foot pedal's margin. Tolerance is a non-issue here.

**The pull-down is not optional.** Rotary switches are break-before-make:
between detents the wiper connects to nothing and the line floats at
whatever charge was left on it, which can read as a valid position on the
way past. 100 kΩ is large enough against the 1 kΩ taps to shift them by a
few percent against 8 % spacing, and it gives a floating wiper a defined
zero — a level no detent produces, so it is discardable rather than
ambiguous.

**Decode by nearest match and require agreement**, as for the pedal: two
or three consecutive samples on the same tap before accepting a change,
and a reading matching no tap gets discarded rather than snapped. A knob
turned by hand leaves milliseconds to spare.

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

## DMX OUT — venue fixture color echo (brain only)

Scope is color echo, nothing else: every frame the brain writes the
washes' color to 1–2 hardcoded fixture addresses.
See `docs/architecture.md` § "DMX is for the wash fixtures, never for
the strips" for why the strips are not driven this way.

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
host's RXD and pin 2 the host's TXD — a unit labeling its own receiver
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

**4-channel (`Dxxx`) — superseded, kept for reference.**

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
| +6     | Macro        | ≤50 off; above that fixed color, jump, pulse, gradient, voice-activated |
| +7     | Speed        | Macro speed                                |

At `D001` the 4-channel block is channels 1–4, so a second fixture
starts at 5.

**8-channel is what the brain drives**, as of 2026-09-09. Brightness
comes from the dimmer at +0 while RGBW stays at full scale; the strobe
channel also comes with it, which is what any tempo-synced fixture
effect would need. The cost is the macro channel at +6, which must be
held below 50 or the fixture starts an auto sequence that overrides
color entirely — `dmx_out.cpp` pins it and its speed channel at zero.

Verified on the bench 2026-09-09 with one fixture at `A001`. An orange
of red 255 / green 85 held its shade all the way down to 5 % dimmer,
with only a slight loss of yellow that is as easily the eye as the
fixture. Dimming the same color in 4-channel mode would have left
green with four levels of resolution — one step is a 25 % change in
green against an 8 % change in red, which is the hue drift and banding
the switch was made to avoid.

Full scale is far brighter than the strips — 255 on all four is hard to
look at directly, and the per-fixture master scale in the `Fixture`
struct exists for this. Bench comparison 2026-09-09 confirmed
the gap is wide: at matching settings the PAR clearly overpowers the
strips, so master is a number that has to be set in the room.

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
| Aurora Program Change     |   19   |  108     | 0–11 presets                   |
| Aurora Control Change     |   23   |  ~90     | In the ~70 reserved slots      |
| Aurora Note On            |    6   |  ~50     | In the reserved trigger/preset-event slots |
