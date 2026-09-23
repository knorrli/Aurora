# Every control on the box

**Reconstructed 2026-09-23 and not yet confirmed.** A full inventory was made
in conversation two or three days earlier and never written down, so it was
lost. This file exists so that cannot happen twice: it is the list of physical
things a hand can move, and what each one puts on the wire.

**Correct it rather than trusting it.** Every row is assembled from a source
that is partial or stale, and each row says which:

- `docs/wiring.md` § "Controller pin map" is the **v1 Nano** map. The doc
  itself says the Teensy assignment will be "a fresh assignment rather than a
  translation", and it does not list the 12-position rotary at all, because
  that sat on a *second* Nano doing tempo.
- `controller/src/pins.h` calls itself "the single source of truth" and
  already disagrees with the wiring doc: it says A3 is free, the doc says A3
  is the foot-pedal ladder.
- `docs/architecture.md` has the only complete-sounding sentence anywhere —
  "keypad, three faders, touchpad, switches, foot pedal, tap tempo, mic
  trigger" — and it is a sentence, not a list.
- `docs/wiring.md` § "Reserved room, by category" says 23 Control Changes are
  in use. It is 86.

---

## What a hand can move

| Control | Where | What it sends today | Home |
|---|---|---|---|
| Keypad, 13 codes | D8–D12, static parallel code | Program Change — the patch you are heading toward | yes |
| Keypad **hold and release** | same | nothing | **none** |
| Fader 1 — Color | A0 | CC 21, saturation | **none for the route** |
| Fader 2 — Extent | A1 | CC 20, hue | **none for the route** |
| Fader 3 — Motion | A2 | CC 22, brightness | **none for the route** |
| Touchpad X | D6 / A5 | CC 31 | yes |
| Touchpad Y | D7 / A4 | CC 30 | yes |
| Touchpad pressure | same | CC 32, never measured | yes |
| Touchpad engage | — | CC 33 | yes |
| Rocker — touchpad effect, 2-way | D4 | CC 42 | yes |
| Rocker — hold mode, 2-way | D5 | CC 43 | yes |
| Rocker — strip mode, 3-way | A6 | CC 41 | yes |
| Rocker — A/B bank, 2-way | A7 | CC 40's bits, retiring | needs its own |
| Rotary, 12 positions — tempo | **no pin**, was a second Nano | CC 10, tempo division | yes, CC only |
| Tap tempo button | D2 | Note 61 | yes |
| Foot pedal, 4 switches | A3, resistor ladder | nothing | **none** |

Not controls, listed so the inventory is complete: the **mic trigger** on D3
is a sensor and sends Note 60; the **tempo LED** on D13, the **two indicator
pixels** and the **ten pixels under the pad** are outputs, recomputed on the
controller and never streamed.

## The three faders are the interesting gap

`DESIGN.md` § "The three faders are three routes to 'more'" makes each fader a
per-patch morph route — Color, Extent, Motion — and the patch format carries
their far ends as three of its five sets. The brain holds the patches, so the
brain does the morphing, so it needs to know where each fader stands.

Nothing carries that. CC 20–22 are `[patch]` base hue, saturation and value:
saved, recalled, and slid by a morph. The header still calls them "H fader /
S fader / V fader", which is the v1 meaning, and the controller still sends
the faders straight to them. **Three `[ambient]` numbers are wanted**, and
`docs/cc-regroup.md` reserves room for them.

## What is missing besides

- **The keypad's hold and release.** "A morph you stretch by holding" needs
  the brain to know a key is down and then let go. A Program Change cannot
  say that and no note carries it.
- **The foot pedal.** Four momentary switches are events, so they want notes.
  62–69 and 74–79 are reserved and empty.

Room exists for all three. Neither is assigned.

## To confirm

- **Is this everything?** Four rockers matches `DESIGN.md` § Open, "What the
  fourth rocker does", which says three of four are spoken for. If the list
  made in conversation had more, this is where it goes.
- **Which fader is which route.** The pin map labels A0/A1/A2 saturation, hue
  and value, which is the v1 meaning. Which physical fader becomes Color,
  Extent and Motion has never been written down.
- **Where the rotary lands on the Teensy.** It has never been in a pin map.
