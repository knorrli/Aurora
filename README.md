# Aurora

Arduino-based light effects machine driving five chainable LED strips (45 LEDs each, 225 total) from a custom hardware controller: three HSV faders, a telephone-style 0–9 numpad for preset selection, a resistive touchpad for live overrides, and a handful of mode switches.

## Hardware

### LED output

A single FastLED chain of **235 pixels** on digital pin 2, laid out as:

| Index   | Purpose                              |
|---------|--------------------------------------|
| 0       | Touch-color indicator                |
| 1–10    | Touchpad feedback grid (5×2)         |
| 11      | Preset-color indicator               |
| 12–234  | Five strips of 45 LEDs (5 × 45 = 225)|

The 5×2 touchpad feedback block mirrors the live colors of the five strips so you can see the effect of a touch without looking up.

### Controller inputs

| Pin  | Input                                                        |
|------|--------------------------------------------------------------|
| D2   | LED data out                                                 |
| D3   | Tempo gate (pulse input)                                     |
| D4   | Touchpad effect mode switch (fill / invert)                  |
| D5   | Hold mode switch (latch last touch)                          |
| D6/D7 + A4/A5 | 4-wire resistive touchpad (XP/YM + YP/XM)           |
| D8–D12 | Numpad matrix, read directly from `PINB`                   |
| D13  | Tempo indicator LED                                          |
| A0   | Saturation fader                                             |
| A1   | Hue fader                                                    |
| A2   | Value / brightness fader                                     |
| A3   | External trigger input (e.g. audio gate)                     |
| A6   | Touchpad strip mode (3-way: mirrored / all / mirrored-exclusive) |
| A7   | Fader & preset alt-mode switch (3-way, two independent flags) |

## Runtime model

Every iteration of `loop()` in `Aurora.ino`:

1. `perform()` — read all inputs: faders, numpad, tempo gate, switches, touchpad, external trigger.
2. `render()` — clear the framebuffer, render the current preset, overlay the touchpad action, overlay any trigger flash, draw the two color indicators, and push via `FastLED.show()`.

### Tempo

A hardware tempo gate on D3 drives every time-based effect. Aurora measures `currentTempo` as the ms between rising edges and uses it two ways:

- **Step advance:** patterns move one step per gate pulse (or N steps per pulse, depending on the effect).
- **Inter-beat interpolation:** `elapsedLoopTime` since the last gate is used to smoothly interpolate frames between beats.

A 100 ms read-window debounces the gate; D13 flashes once per beat for visual confirmation.

### Preset commit on beat

The numpad sets `selectedPreset` immediately (debounced 100 ms). The actual switch (`currentPreset = selectedPreset`) only happens on the next tempo gate, so preset changes land cleanly on the beat. `previousPreset` is tracked so the center key toggles back.

## File layout

The code follows a prefix convention:

| Prefix | Role                                                |
|--------|-----------------------------------------------------|
| `I_*`  | Input only (reads hardware into state)              |
| `IR_*` | Input + its own render pass                         |
| `P_*`  | Pattern / effect renderers                          |
| `R_*`  | Renderer with side-effects on the strip buffer      |

| File                | Contents                                                                 |
|---------------------|--------------------------------------------------------------------------|
| `Aurora.ino`        | `setup()` + `loop()` — dispatches `perform()` and `render()`             |
| `Aurora.h`          | Pin defs, pixel layout, global state, framebuffer + FastLED setup        |
| `helpers.cpp`       | Mode-switch reading, indicator LEDs, boot sequence, tempo LED            |
| `I_ColorFaders.cpp` | Hue / saturation / value faders, alt-mode hue oscillation                |
| `IR_Preset.cpp`     | Numpad matrix read, preset dispatch, per-preset state reset              |
| `IR_Trigger.cpp`    | External trigger input, white→color strobe overlay                       |
| `P_Fills.cpp`       | Static fills: `FillStrips`, `FillStars`, `PulseFill`, `XFill`            |
| `P_Movements.cpp`   | Scrolling / position-based: `MovingBlocks`, `Rain`, `Invert`, `Bars`     |
| `P_Strobes.cpp`     | Flashes and randomness: `StrobeStrips`, `StripByStrip`, `Chaos`          |
| `R_Touchpad.cpp`    | Touchpad read, grid mapping, color override, fill/invert rendering       |

## Presets (0–9)

The numpad selects one of ten presets. The **preset alt-mode switch** (A7) swaps half of them to variants:

| # | Default              | Alt mode             |
|---|----------------------|----------------------|
| 0 | Off                  | Off                  |
| 1 | FillStrips (solid)   | FillStars (spaced)   |
| 2 | RisingBlocks         | RisingStars          |
| 3 | FallingBlocks        | FallingStars         |
| 4 | PulseFill            | Bars                 |
| 5 | Invert               | XFill                |
| 6 | RainFall             | RainBounce           |
| 7 | StripByStripOrdered  | StripByStripRandom   |
| 8 | StrobeStrips         | Chaos                |
| 9 | (empty)              | (empty)              |

Pressing **all keys at once** mutes output. The **center key** (special bit pattern) toggles back to the previous preset.

## Color pipeline

1. **Fader read** produces a base color every frame:
   - *Normal:* `presetColor = CHSV(hue, saturation, value)` straight from the three faders.
   - *Alt (fader alt-mode):* saturation fader becomes oscillation *rate*, value fader becomes oscillation *range*; hue sweeps around the base hue with S and V pinned to 255.
2. **Preset renderer** draws into the strip framebuffer using `presetColor`.
3. **Touchpad overlay** (if touched, or if holding) derives `touchColor` from `presetColor.hue` modulated by the pad's Y-axis (either saturation or hue, depending on vertical mode), then fills or inverts the selected strips.
4. **Trigger overlay** fades a white→color strobe (~700 ms) over the entire strip array when A3 fires.
5. **Indicators** at pixels 0 and 11 show `touchColor` and `presetColor` at full saturation for visibility.

## Touchpad

4-wire resistive pad mapped to a **5×2 grid**: five horizontal positions (one per strip) and two vertical rows. Pressure threshold gates whether a touch is registered.

Modes:

- **Strip mode (A6, 3-way):**
  - *Mirrored:* touched strip N and strip (4−N) both respond.
  - *Mirrored exclusive:* only the mirrored pair stays lit; the others invert to `touchColor` or black.
  - *All:* touch affects every strip.
- **Effect mode (D4):** *fill* writes `touchColor`; *invert* toggles each LED between black and `touchColor`.
- **Hold mode (D5):** when on, the last touch position persists after you lift off.
- **Vertical mode** (set at boot): Y-axis modulates either saturation (default) or hue (hold key `0` at boot).

## Boot-time configuration

Whichever numpad key is held at power-on selects a startup option:

- **No key** → full brightness, vertical mode = saturation.
- **Key `0`** → vertical mode = hue.
- **Keys `1`–`9`** → brightness set to half max (dim mode).

## Global state (Aurora.h)

Worth knowing when reading or extending the code:

- **Timing:** `tempoGate`, `currentMillis`, `lastGateMillis`, `currentTempo`, `expectedNextGate`, `elapsedLoopTime`.
- **Color:** `presetColor`, `touchColor`, `brightness`.
- **Preset:** `currentPreset`, `selectedPreset`, `previousPreset`, `muted`.
- **Modes:** `faderAltModeEnabled`, `presetAltModeEnabled`, `touchpadVerticalMode`, `touchpadStripMode`, `touchpadEffectMode`, `holdModeEnabled`.
- **Framebuffer:** `pixels[NUM_PIXELS_TOTAL]` plus `CRGBSet` views (`strips`, `touchpad`) and per-strip pointers `strip[0..4]`.

Each preset owns its own animation counters (positions, directions, gate counts). `resetPreset()` zeroes them whenever the selected preset changes so transitions start clean.
