# Every control on the box

**The inventory, written down 2026-09-23 from Simon's own walk across the
panel and cross-checked against `images/top.jpeg` and `images/front.jpeg`.**
It was made once before in conversation and never committed, so it was lost.
This is the record; keep it current when the box is rebuilt.

Layout is top-left to right, then the rows below — the way a hand finds them.
"v1" throughout means the single-box Arduino rig, tagged `aurora-nano-final`.

---

## Top row

### Audio-in section, top left

| | What it is | v1 job |
|---|---|---|
| Jack socket | audio in | Feeds a standalone peak-follower circuit, which feeds the secondary tempo Arduino. That board decided tempo source and division and sent a gate at the resulting frequency to the main Arduino. |
| 2-way toggle | peak-follower on/off | Off makes the peak-follower hold LOW to the tempo board. |
| Pushbutton with LED — **TRIG** | gate indicator and manual trigger | LED shows the audio-in gate; the button fires it by hand. |
| Potentiometer, likely 10 k linear | gate threshold | Sets the level the peak-follower fires at. |

### 12-step rotary

Tempo source *and* division in one switch. The hand-drawn ring on the panel
reads **MIDI** down one side and **TOUCH** / **TRIG** on the other: several
MIDI-clock divisions from sixteenths and dotted sixteenths through quarter to
half-time, quarter or eighth from TRIG, and half, quarter or eighth from the
tap tempo button.

### ON/OFF

Big 2-way rocker, top right. Hardwired — it cuts the 9 V to the Arduino and,
after a 5 V step-down, to the controller's pixels. **Not read by any
firmware and never will be.**

---

## Middle

| | What it is | v1 job |
|---|---|---|
| Left indicator pixel | single NeoPixel, left of the phone | Showed the color set by the faders. Blinks an RGB sequence at power-on to say the controller is ready. |
| Phone cradle button | momentary, under where the earpiece hangs | Every strip black, instantly. A performance control, rarely used. |
| Phone keypad | 12 keys | 1–9 chose the hard-coded presets; 0 was "off", every strip black. |
| Right indicator pixel | single NeoPixel, right of the phone | Showed the color on the strips the touchpad was modifying. Same ready-blink. |

---

## Bottom

### Left

| | What it is | v1 job |
|---|---|---|
| Three faders | green, red, black caps, left to right | One hue, one saturation, one brightness — which is which is not recorded. In an alternative mode one spread color along the strips and one moved the color; the third is not remembered. |
| Rocker, 2- or 3-way | below and right of the fader panel | Selected that alternative mode for the faders. |

### Center

| | What it is | v1 job |
|---|---|---|
| Pushbutton with LED | amber, below the phone | LED showed tempo after division; the button recorded tap tempo. |

### Right

| | What it is | v1 job |
|---|---|---|
| Four rockers | one below-left of the touchpad, three above it; a mix of 2- and 3-way | Hold last touchpad position · touchpad alternative mode · touchpad affects all strips / selected+mirror / selected+mirror with the underlying pattern blanked · preset alt mode, which switched to a variation of the hard-coded preset. **Which switch is which is not recorded.** |
| Touchpad | 4-wire resistive, in the cream panel | Modified the running preset. X usually chose a strip; Y shifted hue, or faded to white above center and to black below. |
| Ten pixels | 5 columns × 2 rows, at the pad | See the discrepancies below — not visible in either photo. |

---

## What the photos do not confirm

Recorded as open rather than silently resolved.

1. **The keypad has twelve keys, and only ten have a job.** Both photos show
   a fourth row wider than "0" alone, and `docs/wiring.md` says the firmware
   matched `PINB` against **a table of thirteen values** — twelve keys plus
   no-press. Simon's walk accounts for 1–9 and 0. **Two keys exist and have
   never been assigned anything**, which on a phone body would be the `*` and
   `#` positions.

2. **There are seven toggles on the box and the pin map reads four.** Counted
   from the photos: peak-follower on/off, ON/OFF, the fader-mode rocker, and
   four at the touchpad. Two of those are not the main Arduino's to read —
   ON/OFF is hardwired and the peak-follower switch feeds the tempo board — so
   **five want reading against four pins** (D4, D5, A6, A7). A7 is described
   in `docs/wiring.md` as "formerly fader-alt + preset-alt", which is two jobs
   on one input, and is probably where the shortfall was absorbed.

3. **The ten pixels at the pad are not visible in either photo.** Both
   indicator pixels are, clearly, either side of the phone. `DESIGN.md` § Open
   lists "the two indicator pixels, the ten pixels under the pad" among things
   still to be decided, so they are most likely planned for the Teensy rebuild
   rather than fitted today.

4. **The audio-in TRIG item may be an indicator only.** In the photos it is a
   small amber dot, noticeably smaller than the tap tempo button below the
   phone, which has an obvious bezel and cap. Whether it is an illuminated
   pushbutton or a bare LED needs a hand on it.

---

## What has nowhere to send

Three controls are live on the box and carry no message. Room exists for all
of them; see `docs/cc-regroup.md`.

- **The three faders.** `DESIGN.md` § "The three faders are three routes to
  'more'" makes each one a per-patch morph route, and the patch format carries
  their far ends as the Color, Extent and Motion sets. The brain holds the
  patches, so it does the morphing and needs to know where each fader stands.
  CC 20–22 are not that: they are `[patch]` base hue, saturation and value,
  saved and recalled and slid by a morph — the v1 meaning, which the header
  still uses and the controller still sends.
- **The keypad's hold and release.** "A morph you stretch by holding" needs
  the brain to know a key went down and then came up. A Program Change cannot
  say that and no note carries it.
- **The foot pedal's four switches.** Not on the panel — it is a separate
  pedal on a guitar cable, four bare switches on a resistor ladder into A3.
  No note, no CC.

## Still not written down anywhere

- **Which fader is Color, Extent and Motion.** The pin map labels A0, A1 and
  A2 saturation, hue and value, which is the v1 meaning of the same three
  sticks. The mapping to the three routes has never been decided.
- **Which of the four touchpad rockers does what**, and which are 2-way and
  which 3-way.
- **Where the rotary lands on the Teensy.** It has never been in a pin map,
  because in v1 it sat on the second Arduino.
