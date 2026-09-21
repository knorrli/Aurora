# Aurora — what the bench proved

Measured and observed facts only. Nothing here is a plan or a
preference. If a line in this file turns out to be wrong, it is wrong
because reality changed or the measurement was bad — not because we
changed our minds.

Anything that reads as "we decided" belongs in `docs/architecture.md`
or `DESIGN.md`, not here.

---

## WS2812 data at 3.3 V was marginal, and the level shifter fixed it

Found 2026-09-05, fixed 2026-09-09.

A WS2812 on a 5 V supply wants about 3.5 V for a reliable "high". The
Teensy 4.0 drives 3.3 V. It rendered correctly much of the time and
corrupted under any disturbance. Symptoms, in the order they became
clear:

- Random red, green and blue pixels, occasionally white, appearing and
  clearing continuously.
- Worse with **busier data** — a solid fill was nearly clean, Starfield
  and Plasma speckled heavily. More bit transitions means more edges a
  marginal threshold can misjudge.
- Worse with **dimmer pixels**, worst against **black**. A 0 bit is a
  short pulse and short pulses are hardest to catch when edges are
  already slowed by the series resistor and the cable. A dark strip is
  135 consecutive zero bytes.
- **Proximity-dependent.** Moving a hand toward the battery-powered
  laptop started the flicker; moving away stopped it. A two-prong
  charger made it unusable.

Corruption at the first pixel poisons all 225, because the whole frame
enters through strip 1 and is re-transmitted from there. Every link
downstream is electrically clean and logically wrong.

**An `SN74AHCT125N` between pin 2 and the strip-1 data cable was the
entire fix.** Solid fill and Starfield-against-black both render
cleanly, and the proximity flicker is gone, two-prong charger included.
So the marginal threshold was also causing the grounding symptom, rather
than sitting alongside a second independent fault.

The dedicated ground bond from the Teensy to strip 1's supply negative
was never built. It remains the first thing to try if flicker ever
returns, since bench mains are not venue mains. Building it means
soldering inside a strip box.

Wiring and the HCT-versus-HC trap are in `docs/wiring.md` § "3.3 V data
and the level shifter".

## Frame timing

- **7–8 ms per frame**, steady, for every preset — almost all of it
  FastLED pushing 225 pixels. Measured 2026-09-05 on the Teensy 4.0.
- Brain firmware is **65 KB of 2 MB flash**. No memory pressure of any
  kind on this part.
- On the old Nano, drawing the 12 indicator pixels blocked interrupts
  for **~360 µs** against a UART tolerance of **~640 µs**; driving all
  237 would have blocked **~7.1 ms**. Recorded because it is the
  measurement that ruled out the alternatives, and it is moot on Teensy.

## USB MIDI carries clock well enough

`bench/midi_monitor/` on 2026-09-05, driven from `sendmidi`:

- **No ticks lost**, at both quarter and triplet division.
- **Under 1 ms of jitter.** Ticks nominally 17.86 ms apart (140 BPM)
  arrived 17.0–18.1 ms apart. This was the open worry, since USB moves
  data in scheduled frames rather than preserving DIN's spacing.
- **Triplets are exact.** At 140 BPM, triplet-eighths measured
  142.9–143.1 ms against an ideal 142.857, with no drift across the run.
  24 PPQN divides exactly by 1, 2, 3, 4, 6, 8, 12 and 24, so there is no
  rounding to accumulate.
- **Program change, CC, notes and transport all arrive** and decode
  against `shared/aurora_protocol.h`.
- **Free-running on clock loss works**, holding the last known tempo
  rather than snapping to a default.

**Caution for future bench work:** `sendmidi`'s `clock` command is not a
precision reference. It dumps 24 ticks the instant it starts, and when
looped its 2-beat chunks double a tick at each seam. Both appear in the
monitor as a 0.0 ms minimum gap. Use a DAW when timing accuracy is
itself under test.

## A clock burst reports an absurd tempo

A 24-tick burst on the bench produced readings of 148.8 BPM and 0.5 BPM
from a naive beat measurement. The brain therefore clamps measured tempo
to 20–300 BPM and ignores anything outside it.

A burst also fires several tempo pulses in the same instant. Presets
that advance a step counter per pulse jump visibly; presets that compute
position from elapsed time glide through untouched.

## DMX

- **The Grove wire colour is not what you would guess.** Teensy pin 17
  goes to **white / `TXD`**, not yellow. Proven end to end against a
  BCC145. Re-testable with `bench/dmx_bringup/`.
- **8-channel mode verified** 2026-09-09 with one fixture at `A001`: an
  orange held its shade down to 5 % dimmer, which 4-channel mode cannot
  do, since its only way to dim is to scale RGBW down.
- **The macro channel at +6 must stay below 50** or the fixture starts
  an auto sequence that overrides colour entirely.
- Channel maps for both personalities are in `docs/wiring.md` § "Fixture
  profile — BeamZ BCC145", confirmed with `bench/dmx_channel_map/`.
- **The washes do not follow the trigger flash.** Observed 2026-09-09:
  `dmx_out::tick()` reads the preset's base colour while the trigger
  renders from its own, so a kick-driven flash fires on the strips while
  the PARs hold steady.

## FastLED squares the value before scaling

`hsv2rgb_rainbow` does `val = scale8_video(val, val)`, so a CHSV value of
80 leaves as RGB 26. That is a perceptual dimming curve for LEDs, and
anything converting the same CHSV inherits it — including the DMX echo.

Whether a PAR wants it is **unknown and untested**. If the fixture bends
its own response the curve is applied twice and the bottom of a fade
collapses. Settle it with a strip and a PAR side by side, stepping
**evenly spaced** values (127, 109, 91, 73, 54, 36, 18, 0). A first pass
on 2026-09-05 used an uneven ramp and therefore proved nothing.

## Two firmware findings

**Plasma had a wrap discontinuity, fixed 2026-09-05.** Its half-speed
phase was derived as `t >> 1` from an already-truncated `uint8_t` time
base, so it ramped 0–127 and snapped back rather than wrapping at 256 —
half a sine cycle, jumped every two seconds. Strip 1 was the one strip
where it was invisible, because its offset sits exactly on the sine's
midpoint where the discontinuity cancels; that asymmetry is what made it
findable by eye.

The rule it teaches: **derive each phase from `millis()` at the shift you
want, never by shifting a value already truncated to 8 bits.** No other
preset does this.

**Bars' turning points land on the beat.** The old `lastGateMillis`
ordering bug made it render a full beat ahead and snap back every gate
frame; computing the gate before `render()` fixed it. Sub-pixel edge
rendering demonstrably works — shown beside Sweep on adjacent strips,
Bars read as visibly smoother while travelling more than twice as fast.

Whether the glide is smooth enough *at real viewing distance* is
untested. A bench flatters it.

## Known defect: Glitch's white pixels ignore the V fader

Found 2026-09-09, unfixed. Glitch's white pixels are a flat `CRGB::White`
while its coloured pixels are `CHSV(hue, sat, value)`, and
`FastLED.setBrightness()` is pinned at `MAX_BRIGHTNESS` and never follows
the fader. At a fifth of full V the whites are already about twenty times
the coloured pixels and the preset collapses into white noise.

The documented 30 % white share is therefore only true at full V, which
is why that constant has never been judgeable.

## Limits of every bench session so far

Worth stating, because it bounds what any of the above is worth:

- **Nothing has been judged against music.** Tempo free-ran at 120 BPM in
  a quiet room, so "does this lock convincingly to a song" is untested.
- **Bench distance flatters everything.** Sub-pixel smoothness, the
  Plasma/Aurora distinction and the per-strip Stutter idea all need
  seeing from across a room.
- **Only one wash fixture has ever been connected at once.** Everything
  about four fixtures together is projection.

---

## A phase derived from absolute time teleports when its rate changes

Found 2026-09-18, and this is the third time this project has been bitten
by the same shape of mistake. It is worth stating as a rule.

**Never compute a position as `elapsed × rate` and then change the rate.**
Elapsed time is large and only grows, so a small change of rate is a large
change of product, and the position jumps.

The generator's brightness cycle was computed by dividing the running
musical position by the cycle length. At beat 100 with a 6-beat cycle that
is 16.67 cycles — 67 % of the way through the current one. Change the
cycle length to 5.9 and it becomes 16.95 cycles: the brightness teleports
to 95 % instantly. A morph that steps the rate seventy times on its way
across produced seventy visible brightness jumps.

The fix is to carry an offset across each rate change, chosen so the
position is unchanged and only the speed of advance differs. Roughly ten
lines, and it turned the morph from unusable into the thing that made the
whole approach work.

The two earlier instances of the same family:

- **Plasma** derived a half-speed phase by shifting an already-truncated
  8-bit time base, so it ramped 0–127 and snapped rather than wrapping at
  256.
- **The old preset model** accumulated position a step at a time, so a
  tempo change altered the step size but not the position already
  accumulated, and motion jumped. That is what `tempo::` exists to fix.

## Point-sampling a pattern aliases; averaging it does not

The generator originally read one value at each pixel's centre. Once a
repeating shape got down to a pixel or two across, it strobed as it moved
instead of fading out — the classic result of sampling detail finer than
the grid can carry.

Averaging four samples across each pixel's own width fixed it completely.
Detail finer than the strip can resolve now washes out smoothly into an
even glow, which is the honest thing for it to do. The cost is four shape
evaluations per pixel — 900 per frame — which is nothing on this part.

## Converting a colour at low brightness collapses its hue

A dim yellow rendered as dim red. The cause is handing a low value
straight to `CHSV`: the conversion computes each channel at that value and
one truncates to zero before its neighbour does, so the ratio between them
breaks and the hue moves.

Converting at full brightness and scaling the resulting RGB with
`nscale8_video` keeps the ratio, and the video floor keeps a non-zero
channel from vanishing. The perceptual dimming curve is unaffected.

Worth knowing anywhere colour is scaled, not just in the generator.

## Brightness reads far weaker than saturation, and needs an edge either way

Observed 2026-09-19, sweeping a colour field over a full still fill.

**The same numeric depth on brightness and on saturation are nowhere near
the same change.** A field spanning 255 down to 135 — a real halving of
light — was reported as unnoticeable. The equivalent move on saturation
took a saturated red to a pale rose and read immediately. Halving
luminance is roughly a quarter less *perceived* brightness, and the
change was spread as a smooth gradient with no boundary anywhere; a
saturation shift of the same size crosses what reads as a change of
colour, which the eye is enormously more sensitive to.

**And a smooth gradient of brightness reads as almost nothing whatever
its depth.** A field running from full down to 2 % — a 50:1 range — was
still described as "not extremely visible" while its edges stayed soft.
Steepening the same field toward a hard boundary, with no change to its
depth at all, made it "extremely pronounced". Vision detects edges; a
ramp with no edge in it has nothing to detect.

This is the same finding as the pulse needing a shape control, one level
up: no amount of depth on a sine produces a boundary.

**Hue depth at full destroys the base colour.** A swing of +-128 is the
whole wheel, so the hue fader stops meaning anything and the wall becomes
a spectrum rather than one colour with depth in it. Usable settings
looked to be roughly a fifth to a half of that. Not a measurement, but
consistent across several sittings.

## How far a pixel can be darkened before its colour jitters

Measured 2026-09-19, extending the hue-collapse finding below.

Converting at full brightness and scaling the RGB is necessary but not
sufficient. At very low output the eight linear bits run out: near the
bottom one step is a third of the light, and the three channels cross
their steps at different moments. A pixel whose **hue is also moving**
therefore lurches between colours instead of sliding.

What was observed:

- **A single-channel colour is immune.** Pure red at full saturation is
  RGB (255, 0, 0); scaled it stays (N, 0, 0), and one channel can only
  step in brightness. No jitter at any depth. This makes red a useless
  test case for the problem.
- **Down to 2 % is clean** with hue depth at about a third of full, on
  orange and on cyan — both multi-channel — with saturation also being
  pulled toward white.
- **Hue depth at full jitters heavily** at the same depth, and pixels
  crossing to zero pop out entirely.

So the limit is set by how far hue moves at low brightness, not by the
darkening alone. A floor a little above zero plus a moderate hue swing
stays inside it.

**The knock-on is bigger than the knob.** These strips have a minimum
usable brightness below which anything quantises this way, which lands on
any proposal to express intensity by scaling brightness: a dim wall is a
wall in the region that falls apart. Untested at rig scale.

## FastLED's inoise8 does not fill its range, and repeats on its lattice

Two properties of `inoise8`, both found 2026-09-19 by reading `noise.cpp`
after the wall showed the symptoms.

**It never reaches either end.** The implementation is:

```c
int8_t n = inoise8_raw(x, y, z);  // -64..+64
n += 64;                          //   0..128
uint8_t ans = qadd8(n, n);        //   0..255
```

That correction is calibrated for the +-64 a *single* gradient can
theoretically reach. What is returned is a trilinear blend of eight of
them, and blending pulls any result toward the middle, so the output
clusters around 128 and the ends never arrive. Against stacked sines over
the same sweep, which fill the range with a gain of 1.04, noise read as
"a less intense version" with the extreme hues missing. Doubling again
about the centre gives it comparable authority.

**It returns its midpoint exactly on the integer lattice**, and FastLED's
cells are 256 units wide. Five strips stepped one whole cell apart
therefore all sat on the same lattice line and came out sharing features
— reported as three of the five having the same pronounced blob in the
same place, at maximum separation. A step that is not a whole number of
cells fixes it.

## Sines and noise are not tellable apart at this resolution

Judged 2026-09-19, after the sine path was fixed to be fairly comparable.

A control crossfading a colour field between stacked sines and Perlin
noise produced no perceptible change of character — reported as "I could
probably achieve the very same effect by just changing the base-hue
slider". Regular versus irregular needs enough repeats across a strip to
read as regular, and 45 pixels does not supply them at any grain coarse
enough to look like anything.

## A field built as along-plus-across is not two-dimensional

Found 2026-09-19.

A field summing a wave along the strip with a wave across the strips is
separable: the along term is identical on every strip, so it alone
decides where the features are, and the across term only shifts their
level. The wall shows the same blobs at the same pixels on all five
strips, differing only in colour — which is what was observed, and what
the hand-written Plasma had always done.

Making the across offset **displace the field along the strip** rather
than shift its level puts the features at different pixels per strip.
Fanning them from the middle strip rather than from the first also
matters: from the first, strip 1 never moves and the last does all the
travelling, which reads as a one-sided ramp rather than the wall opening.

## A PAR smooths below about 25 ms, and does not lag

Measured 2026-09-21 with two BeamZ BCC145 at `A001` and `A009`, flashed
against strip 3 on the same clock by `bench/par_flash_speed/`. Flash
lengths of 200, 100, 50, 25 and 12 ms, each run for three seconds.

- **No latency at any rate.** Flashes landed with the strip at every
  step, and the two fixtures stayed in step with each other. The strip is
  the slower path in that sketch — it blocks some 7 ms transmitting while
  a DMX frame arrives in about 1.2 — so the PAR is given a head start and
  would have had to lag by more than that to look late. It did not.
- **Clean to 25 ms**, perhaps marginally dimmer there.
- **Broken at 12 ms**: the fixture no longer returns to black between
  flashes, showing a continuous glow instead. The colour also fell from
  orange to red, which is the green emitter — at 85 of 255 — dropping
  below the fixture's resolution as the effective level collapses.

Not reaching black is smoothing rather than latency: a late flash still
reaches black, just late.

So the floor sits between 12 and 25 ms, and 25 is the number to build
against. Judged by eye in a lit room, which is enough for a floor and not
enough for a figure — the sharper version is to watch the pool on the
wall rather than the fixture's lens, which saturates the eye and hides
exactly the brightness differences being looked for.

**What it settles:** 25 ms is a 64th note at 120 BPM, so a PAR can play
any rhythm a band plays. The limit only bites on travel — nine positions
at 25 ms each puts a window crossing the whole wall in about 225 ms, or
just under half a beat at 120. Faster than that and the pools smear.

