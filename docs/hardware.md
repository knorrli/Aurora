# Aurora hardware

## Brain — Teensy 4.0

| Pin | Role |
|-----|------|
| 0 | `Serial1` RX — DIN MIDI in (not built yet) |
| 1 | `Serial1` TX — spare, for a MIDI thru |
| 2 | WS2812 data, through the level shifter |
| 13 | Onboard LED, tempo pulse |
| 17 | `Serial4` TX — DMX unit |
| rest | free |

- Power: 5 V into `VIN`, or USB. Cut the `VIN`/USB jumper if both are connected at once.
- Not 5 V tolerant on any pin.
- Storage: flash 1984 KB, RAM 1 MB. The emulated EEPROM is only 1080 bytes. LittleFS on program flash survives a power cycle, not a firmware upload.

## WS2812 strips

- 5 strips × 45 pixels, one data chain, about 2.5 m between strip boxes.
- Wall order: strip 1 at the right, strip 5 at the left. Pixel 0 at the bottom of every strip.
- Every strip has its own 5 V supply. Ground is common through the data cables.
- 225 pixels draw about 14 A at full white. Never route that through a breadboard.
- Only the first hop (brain to strip 1) runs at brain logic level. Each strip re-drives the next at 5 V.

### Strip box (one per strip, passive)

- 1000 µF across +5 V and GND.
- 470 Ω 1 % in series with data before the first pixel. Do not add another series resistor at the brain.
- Three barrel jacks: power (+5 V, GND), data in (data, GND), data thru (data, GND). Data jacks have a wider center pin than the power jack.

### Level shifter — 74AHCT125 between pin 2 and strip 1

A 5 V WS2812 needs about 3.5 V for a high. Unshifted 3.3 V data gives random speckle, worst against black and near a laptop charger, and poisons the whole chain from pixel 1.

| Chip pin | To |
|----------|----|
| 14 `VCC` | 5 V (`VIN`) |
| 7 `GND` | GND |
| 1 `1OE` | GND |
| 2 `1A` | Teensy pin 2 |
| 3 `1Y` | strip 1 data-in, center pin |
| 5, 9, 12 | GND (unused inputs) |
| 4, 10, 13 | VCC (unused output enables) |
| 14 ↔ 7 | 0.1 µF ceramic at the chip |

- Must be **HCT or AHCT**. HC or AHC has the same threshold as the pixels, fixes nothing, and passes a DC multimeter check. Read the marking.
- DIP-14: notch at left, pin 1 bottom-left, 1–7 left to right along the bottom, 14 above pin 1.
- If flicker ever returns: first try a dedicated ground wire from the Teensy to strip 1's supply negative.

## DMX — M5Stack DMX Unit (U183) on `Serial4`

- Isolated RS-485 transceiver, 120 Ω switchable termination, XLR-3 female. Library: `TeensyDMX`.
- Grove wiring: pin 17 → **white** (`TXD`), `VIN` 5 V → red, GND → black. Yellow unused. The silkscreen names pins from the host's side; yellow leaves the fixture dark.
- No direction pin; the unit handles it and the DMX break survives.
- XLR: pin 1 common, 2 data −, 3 data +. A 3-pin mic cable works on the bench.
- Flicker or wrong values on a long run: put 120 Ω across pins 2–3 in a male XLR in the last fixture's DMX out.
- A DMX frame takes about 1.2 ms on the wire.

### PAR fixture — BeamZ BCC145

- The display must read `Axxx` (8-channel). Out of the box it sits in `Au` / `SU01` and ignores DMX. `Dxxx` is the 4-channel personality (R, G, B, W).
- Fixtures are spaced 8 channels apart.

| Offset | Function |
|--------|----------|
| +0 | Dimmer |
| +1 | Strobe (0–7 off, 8–255 slow to fast) |
| +2 … +5 | Red, green, blue, white |
| +6 | Macro — **must stay ≤ 50**, above that an auto program overrides color |
| +7 | Macro speed |

- Full scale is far brighter than the strips; the per-fixture `master` has to be set in the room.
- Dimming on +0 holds hue down to 5 %; scaling RGB down does not.
- The four PARs stand on the floor pointing up; their order is known, their position is not.

## Measured

- Frame: 7–8 ms, almost all FastLED pushing 225 pixels. Firmware is well under a tenth of flash.
- PAR flash: clean down to 25 ms on / off, no visible latency against the strips. At 12 ms it no longer returns to black. Build against 25 ms.
- USB MIDI clock at 140 BPM: no lost ticks, under 1 ms jitter, triplets exact. `sendmidi clock` bursts and doubles ticks; use a DAW to test timing.
- Pixel sampling: point-sampling aliases (strobes) once a shape is a pixel or two wide. A shape a third of a pixel wide sliding slowly swings total brightness 50 % at 4 samples per pixel, 34 % at 8. Cost of 8 samples not yet timed on the Teensy.
- Low brightness: converting HSV at low value shifts hue (dim yellow goes red). Convert at full value and scale the RGB with `nscale8_video`.
- Near black, 8-bit channels step unevenly: a pixel whose hue moves at under about 2 % brightness jitters between colors. Single-channel colors (pure red) never show it.
- A smooth brightness gradient is nearly invisible at any depth, even 50:1; the same depth with a hard edge is obvious. Saturation changes read far more than equal brightness changes.
- Hue alone changes light output up to 5.4:1 (yellow-green brightest, blue dimmest), computed from WS2812B datasheet intensities, not metered.
- The preview in `tools/preview.js` matches the wall; the wall reads slightly paler because of the diffuser.
- Whether a PAR wants FastLED's squared value curve is untested. Test with a strip and a PAR side by side at evenly spaced values.
- A phase computed as elapsed × rate jumps when the rate changes; carry an offset across rate changes.

## Controller box — as it physically exists

Wired to an Arduino Nano (5 V). Its firmware is at git tag `aurora-nano-final` (`brain/src/`, single-box build). The box is to be rebuilt on a Teensy 4.0; no Teensy pin map exists yet.

| Nano pin | Control | Read as |
|----------|---------|---------|
| D2 | WS2812 data out: box pixels, then the strips | output |
| D3 | Tempo gate from the second Arduino | digital |
| D4 | Touchpad effect rocker (2-way) | digital |
| D5 | Touchpad hold rocker (2-way) | digital |
| D6, D7 | Touchpad XP, YM | 4-wire resistive |
| D8–D12 | Phone keypad + cradle switch | static code, see below |
| D13 | Tap button LED | output |
| A0, A1, A2 | Faders (v1 labels: saturation, hue, value) | analog |
| A3 | TRIG / mic gate input | digital |
| A4, A5 | Touchpad YP, XM | 4-wire resistive |
| A6 | Touchpad strip-mode rocker (3-way) | analog |
| A7 | Two 2-way rockers on one analog line | analog |

- Tap tempo button and foot pedal: not in the Nano map; where they land physically is not recorded.
- Faders: read inverted (`1023 − analogRead`), usable span 10–1020.
- A6 levels: ≤ 400, 401–1000, > 1000.
- A7 levels (four combinations): < 450, 450–599, 600–999, ≥ 1000.
- Touchpad: Adafruit `TouchScreen`, X-plate 230 Ω, usable raw range X 90–930, Y 220–860 (10-bit at 5 V).
- Which fader, and which touchpad rocker, does what is not recorded.

### Keypad — a static code, not a matrix

Ten keys (no `*` / `#` — those are blank inlays). Each key presents a fixed 5-bit code on D12…D8; read them once, never scan. D8 is high in every code (probably the switch common). The cradle switch is wired into the same lines.

| D12 D11 D10 D9 D8 | Meaning |
|-------------------|---------|
| 11101 | nothing pressed |
| 11111 | cradle hook pressed |
| 00001, 10111 | 1 |
| 00011 | 2 |
| 10001 | 3 |
| 00101 | 4 |
| 00111 | 5 |
| 10101 | 6 |
| 01001 | 7 |
| 01011 | 8 |
| 11001 | 9 |
| 10011 | 0 |

- Meter the lines before rewiring: confirm D8 is common and the contacts are passive.

### Box pixels

One WS2812 chain inside the box, driven ahead of the strips in v1: index 0 right indicator, 1–10 touchpad grid (5 columns × 2 rows), 11 left indicator. On a 3.3 V Teensy it needs its own 74AHCT125.

### Other controls

- Audio-in jack → peak-follower circuit (built for 5 V; re-reference for 3.3 V) with its on/off toggle, threshold pot (about 10 kΩ linear) and the TRIG button with LED. The handset earpiece is wired as a microphone into this jack.
- 12-position rotary (tempo source and division): was on the second Arduino, not the Nano. Panel reads MIDI divisions (sixteenth, dotted sixteenth through quarter to half), TRIG (quarter, eighth) and tap (half, quarter, eighth).
- Fader-mode rocker below the faders; power rocker (hardwired to the 9 V, never read).

### Foot pedal — four switches on one line

Two-conductor cable. Pull-up `R_top` at the controller; each switch shorts the line to GND through its own resistor in the pedal.

| R_top | R1 | R2 | R3 | R4 |
|-------|----|----|----|----|
| 1 kΩ | 2.2 kΩ | 5.1 kΩ | 10 kΩ | 20 kΩ |

- All 16 combinations are distinct: 55.6 % (all down) to 100 % (none) of supply, closest pair 79 mV apart at 5 V, still ≥ 55 mV with 5 % parts. Re-run a tolerance check if values ever change.
- Levels are not in binary order (SW3+SW4 sits above SW2). Decode by nearest match against a calibrated table.
- Require several agreeing samples before accepting a change; discard readings that match nothing.
- Ratios, so the table holds at 3.3 V if pull-up and ADC reference share the supply.

### Rotary ladder (to build)

- Twelve 1 kΩ in series from 3.3 V to GND, one tap per position; wiper to the analog pin with 100 kΩ to GND.
- Taps are 0.275 V apart. The pull-down gives a floating wiper between detents a level no position produces — discard it.
- Require 2–3 agreeing samples.

## MIDI hardware to build

- **Brain DIN in on pin 0:** 6N138. DIN 4 → 220 Ω → opto pin 2; DIN 5 → opto pin 3; 1N4148 reversed across pins 2–3; pin 8 → 5 V; pin 5 → GND; pin 7 → 4.7 kΩ → GND; pin 6 → Teensy pin 0 with a 270 Ω–1 kΩ pull-up to **3.3 V**. DIN 2 unconnected.
- **Controller DIN out (3.3 V Teensy):** 3.3 V → 33 Ω → DIN 4; TX → 10 Ω → DIN 5; DIN 2 → GND.
- **Controller DIN in** (from a DAW or foot controller): same circuit as the brain's.
- USB MIDI stays compiled into the brain for bench work.

## Build gotcha

`#include` names must match the file's case exactly (`"Aurora.h"`, not `"aurora.h"`). macOS compiles either, but PlatformIO's dependency scan silently skips the mismatch and a header change stops rebuilding its dependents.
