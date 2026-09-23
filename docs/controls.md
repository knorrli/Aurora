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
| Pushbutton with integrated LED — **TRIG** | gate indicator and manual trigger | The LED shows the audio-in gate. Pressing the button issues a TRIG ON by hand. |
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
| Phone cradle switch | under where the earpiece hangs, **wired into the numpad's own lines** | Pressed, switch to preset 0 and go black; released, back to the previous preset. Rarely used, because it was not instant. |
| Phone keypad | **10 keys** | 1–9 chose the hard-coded presets; 0 was "off", every strip black. |
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
| Ten pixels | 5 columns × 2 rows, evenly spread under the pad | Fitted, behind a black diffuser, and off in both photos — which is why the panel looks bare. |

---

## The keypad has ten buttons, not twelve

Worth stating because the count is easy to get wrong twice. The two positions
where a phone carries `*` and `#` are **fixed black inlays with nothing behind
them** — no switch, no wire.

`docs/wiring.md` says the v1 firmware matched `PINB` against "a table of
thirteen values", which invites the inference that there are twelve keys plus
no-press. There are not. Reading `readKeypad()` at `aurora-nano-final`,
`brain/src/IR_Preset.cpp:162`, the thirteen are:

- **Eleven case labels for ten keys.** Key 1 answers to two codes,
  `0b00100001` and `0b00110111`; every other key has one.
- **`0b00111101`, which returns the previous preset** — the resting state, so
  that reading the pad while nothing is held changes nothing.
- **`0b00111111`, which sets `muted`**, and `muted` is what
  `Aurora.ino:98` gates the whole render on. **That is the cradle switch**,
  confirmed 2026-09-23: it was hardwired into the same five lines, so it
  costs no pin of its own, and the code above is exactly the press-to-black,
  release-to-previous-preset behavior.

### Why it was not instant, and why that is already fixed

The blackout went through the preset mechanism, and a preset change is
beat-quantized. So the wall went dark on the next beat rather than under the
hand. The end-of-frame gate built 2026-09-23 fixes this for key 0 —
`DESIGN.md` § "Blackout has two forms" — by reading `selectedPreset` rather
than the quantized `currentPreset`, and by throwing the finished frame away
after every renderer instead of branching upstream. Anything the cradle drives
through that gate is instant by construction.

## Seven toggles, four inputs — and it may not matter

Counted from the photos: peak-follower on/off, ON/OFF, the fader-mode rocker,
and four at the touchpad. Two are not the main board's to read — ON/OFF is
hardwired to the 9 V, and the peak-follower switch feeds the tempo board — so
five wanted reading against four pins, D4, D5, A6 and A7. `docs/wiring.md`
calls A7 "formerly fader-alt + preset-alt", two jobs on one input, which is
where the shortfall was absorbed.

**That is a v1 count and the rebuild changes it.** With no secondary board,
the peak-follower switch either lands on the controller Teensy or stays purely
in-circuit and needs no pin at all. Its own discussion, not this file's.

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

## The cradle switch is spoken for, and by an argument worth re-reading

`DESIGN.md` § "Blackout has two forms" settles it: **the telephone hook is the
master kill**, and the reason is that a hook is a *maintained state* rather
than an event. Hang up and the wall is out until the handset is lifted. It
works regardless of patch, it is unmistakable by feel, and it cannot be left
wrong without noticing — which is what "a blackout reachable blind" always
wanted and never got.

**An accent trigger is the opposite kind of control.** A white flash is an
event: it happens and it is over, and afterwards the switch tells you nothing.
Putting both on the hook spends the one property that made it the master kill.

**And the design assumes a handset that neither photo shows.** The cradle is
empty in `top.jpeg` and `front.jpeg` — two brass prongs and nothing resting on
them. Without a handset the hook can only be held down by a finger, which
makes it momentary in practice and takes the "cannot be left wrong" argument
with it. Whether a handset exists, and whether it is tethered to the box,
decides which of the two designs is even available.

## Still not written down anywhere

- **Which fader is Color, Extent and Motion.** The pin map labels A0, A1 and
  A2 saturation, hue and value, which is the v1 meaning of the same three
  sticks. The mapping to the three routes has never been decided.
- **Which of the four touchpad rockers does what**, and which are 2-way and
  which 3-way.
- **Where the rotary lands on the Teensy.** It has never been in a pin map,
  because in v1 it sat on the second Arduino.
