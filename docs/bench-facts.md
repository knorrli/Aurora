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

- **The Grove wire color is not what you would guess.** Teensy pin 17
  goes to **white / `TXD`**, not yellow. Proven end to end against a
  BCC145. Re-testable with `bench/dmx_bringup/`.
- **8-channel mode verified** 2026-09-09 with one fixture at `A001`: an
  orange held its shade down to 5 % dimmer, which 4-channel mode cannot
  do, since its only way to dim is to scale RGBW down.
- **The macro channel at +6 must stay below 50** or the fixture starts
  an auto sequence that overrides color entirely.
- Channel maps for both personalities are in `docs/wiring.md` § "Fixture
  profile — BeamZ BCC145", confirmed with `bench/dmx_channel_map/`.
- **The washes do not follow the trigger flash.** Observed 2026-09-09:
  `dmx_out::tick()` reads the preset's base color while the trigger
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
Bars read as visibly smoother while traveling more than twice as fast.

Whether the glide is smooth enough *at real viewing distance* is
untested. A bench flatters it.

## Glitch's white pixels ignored the V fader

Found 2026-09-09, fixed 2026-09-22. Glitch's white pixels were a flat
`CRGB::White` while its colored pixels were `CHSV(hue, sat, value)`, and
`FastLED.setBrightness()` is pinned at `MAX_BRIGHTNESS` and never follows
the fader. At a fifth of full V the whites were already about twenty times
the colored pixels and the preset collapsed into white noise.

**Every step of that was re-read before it was fixed, and it holds.** The
V fader maps to 0–255 and lands in `presetColor.value`. A colored pixel is
converted from that, and `hsv2rgb_rainbow` squares the value on the way
past, so a fifth of full V leaves a channel at 10 of 255 against a literal
white's 255 — a factor of 25, which is the "about twenty times" above.
`brightness` is assigned `MAX_BRIGHTNESS` once in `setup()` and appears
nowhere else in the firmware, so the master really is pinned.

The fix is to make white out of the fader's own brightness at zero
saturation, which rides the same squared curve as every colored pixel.
The 30 % white share is a judgeable constant now, and has not been judged.

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

**What the fix cost, and how it was paid, 2026-09-22.** Once the phase is
carried across rate changes, the cycle's zero sits wherever the rate was
last touched — which is never a bar line. The period was right and the
landing arbitrary, so the pulse was in time but not on time, and a deep
slow swell peaked wherever it happened to.

Two halves, and neither works alone:

- **Ease the offset back to a whole number of cycles.** A whole cycle of
  offset is invisible, so only the fraction has to go. Spread over about
  two cycles it is a correction and not a jump — measured in the preview,
  a rate moved mid-flight runs the phase at most about 1.4× its settled
  speed for a moment, and a morph sweeping the whole rate fader never
  steps more than a tenth above nominal in a frame. The tracker keeps
  doing the job it was built for.
- **Step the rate.** Anchoring only puts the cycle's zero on the music's
  zero. A period of 2.64 beats walks through the bar for ever whatever the
  phase is anchored to, and most of a continuous fader is periods like
  that. Thirteen positions — halves and their dotted values, 16 down to a
  quarter beat — are the periods a bar can hold a whole number of.

What is anchored is the **peak**, since the complaint was about where a
swell peaks. A square is that swell clipped around its own midpoint and
is therefore symmetric about the peak, so its flash is centered on the
beat rather than starting there. Skew is what moves the flash inside the
cycle. Whether the leading edge is the better anchor is a question for a
click track.

## Point-sampling a pattern aliases; averaging it does not

The generator originally read one value at each pixel's center. Once a
repeating shape got down to a pixel or two across, it strobed as it moved
instead of fading out — the classic result of sampling detail finer than
the grid can carry.

Averaging four samples across each pixel's own width fixed it completely.
Detail finer than the strip can resolve now washes out smoothly into an
even glow, which is the honest thing for it to do. The cost is four shape
evaluations per pixel — 900 per frame — which is nothing on this part.

**The color layer's placed field had the same fault, fixed 2026-09-22.**
It was read once at each pixel's center while the shape around it was
averaged. Measured in the preview on a full still fill painted with hard
regions drifting a cell a beat: the worst one-frame change on a single
pixel falls from 176 of 255 to 99 at every count from five regions
upward, and above about eleven regions the difference between neighboring
pixels stops climbing and starts falling — detail washing out instead of
being carved up by the grid.

Two things it leaves alone, both checked pixel for pixel over 120 frames.
A gradient measured across the strips or along one is unchanged, because
a straight ramp averaged over a pixel is its own value at that pixel's
center; the shape ruler is not straight, so a gradient on it moves by up
to 14 of 255. And the wander and the light level are untouched by
construction — the first is sines and smooth already, the second reads
the averaged profile.

## Converting a color at low brightness collapses its hue

A dim yellow rendered as dim red. The cause is handing a low value
straight to `CHSV`: the conversion computes each channel at that value and
one truncates to zero before its neighbor does, so the ratio between them
breaks and the hue moves.

Converting at full brightness and scaling the resulting RGB with
`nscale8_video` keeps the ratio, and the video floor keeps a non-zero
channel from vanishing. The perceptual dimming curve is unaffected.

Worth knowing anywhere color is scaled, not just in the generator.

## Brightness reads far weaker than saturation, and needs an edge either way

Observed 2026-09-19, sweeping a color field over a full still fill.

**The same numeric depth on brightness and on saturation are nowhere near
the same change.** A field spanning 255 down to 135 — a real halving of
light — was reported as unnoticeable. The equivalent move on saturation
took a saturated red to a pale rose and read immediately. Halving
luminance is roughly a quarter less *perceived* brightness, and the
change was spread as a smooth gradient with no boundary anywhere; a
saturation shift of the same size crosses what reads as a change of
color, which the eye is enormously more sensitive to.

**And a smooth gradient of brightness reads as almost nothing whatever
its depth.** A field running from full down to 2 % — a 50:1 range — was
still described as "not extremely visible" while its edges stayed soft.
Steepening the same field toward a hard boundary, with no change to its
depth at all, made it "extremely pronounced". Vision detects edges; a
ramp with no edge in it has nothing to detect.

This is the same finding as the pulse needing a shape control, one level
up: no amount of depth on a sine produces a boundary.

**Hue depth at full destroys the base color.** A swing of +-128 is the
whole wheel, so the hue fader stops meaning anything and the wall becomes
a spectrum rather than one color with depth in it. Usable settings
looked to be roughly a fifth to a half of that. Not a measurement, but
consistent across several sittings.

## A hue change is also a brightness change, by up to 5.4 to 1

Computed 2026-09-22 from `tools/preview.js`. Not metered on the wall.

`hsv2rgb_rainbow` hands out roughly equal eight-bit channel sums across
the wheel, which is what keeps a hue sweep even on a screen. The dies
behind those numbers are not equal: a WS2812B's green puts out around
three times the light of its red and around five times its blue.
Weighting the rendered RGB by a typical part's luminous intensities —
450, 1400 and 250 mcd — gives, for a flat wall at full saturation and
full value:

| base hue fader | rendered RGB | relative light |
|----|----|----|
| 0 | 255, 0, 0 | 55 |
| 24 | 171, 125, 0 | 120 |
| 48 | 11, 250, 0 | 169 |
| 72 | 0, 102, 154 | 86 |
| 84 | 13, 0, 242 | 32 |
| 108 | 138, 0, 118 | 44 |

Brightest to dimmest is 5.4 to 1 — yellow-green against blue — with red
a little under a third of the brightest.

Every hue push in the color layer therefore moves brightness as well as
color, and where the base hue sits decides which way. The ratio is
arithmetic over a datasheet, so its shape is certain; the figure itself
waits on a meter, and the diffuser goes with it.

## How far a pixel can be darkened before its color jitters

Measured 2026-09-19, extending the hue-collapse finding below.

Converting at full brightness and scaling the RGB is necessary but not
sufficient. At very low output the eight linear bits run out: near the
bottom one step is a third of the light, and the three channels cross
their steps at different moments. A pixel whose **hue is also moving**
therefore lurches between colors instead of sliding.

What was observed:

- **A single-channel color is immune.** Pure red at full saturation is
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
usable brightness below which anything quantizes this way, which lands on
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
about the center gives it comparable authority.

**It returns its midpoint exactly on the integer lattice**, and FastLED's
cells are 256 units wide. Five strips stepped one whole cell apart
therefore all sat on the same lattice line and came out sharing features
— reported as three of the five having the same pronounced blob in the
same place, at maximum separation. A step that is not a whole number of
cells fixes it.

## Sines and noise are not tellable apart at this resolution

Judged 2026-09-19, after the sine path was fixed to be fairly comparable.

A control crossfading a color field between stacked sines and Perlin
noise produced no perceptible change of character — reported as "I could
probably achieve the very same effect by just changing the base-hue
slider". Regular versus irregular needs enough repeats across a strip to
read as regular, and 45 pixels does not supply them at any grain coarse
enough to look like anything.

**The control was cut on this measurement**, and the redesigned color
layer never grew a noise path. Where it needs variation that does not
repeat, it beats two sines against each other instead.

## A field built as along-plus-across is not two-dimensional

Found 2026-09-19.

A field summing a wave along the strip with a wave across the strips is
separable: the along term is identical on every strip, so it alone
decides where the features are, and the across term only shifts their
level. The wall shows the same blobs at the same pixels on all five
strips, differing only in color — which is what was observed, and what
the hand-written Plasma had always done.

Making the across offset **displace the field along the strip** rather
than shift its level puts the features at different pixels per strip.
Fanning them from the middle strip rather than from the first also
matters: from the first, strip 1 never moves and the last does all the
traveling, which reads as a one-sided ramp rather than the wall opening.

The first half outlived the machine it was measured on: the color
layer's wander adds its across term inside the sine rather than to its
output.

**The second half carries much less force for a moving field**, which is
worth knowing before applying it anywhere else. Shifting where the zero
sits only relabels which strip sees which part of the pattern, so a
wander that is drifting sweeps the same family of walls either way. What
anchoring at the middle strip actually buys is the behavior of the
control that sets feature size: the wall opens outward from the center
instead of hinging on strip 1. Visible at slow rates, invisible at fast
ones. It was the static fan this was measured on that made it a fault.

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
  flashes, showing a continuous glow instead. The color also fell from
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

## The color layer's flat state is flat

Confirmed on the wall 2026-09-21, the first time any of the redesigned
color layer was seen on hardware.

With every color control centered, all five strips show exactly the
color on the three faders — no tint, no drift, nothing creeping in from
the placed field, the wander or the light level.

That is the state the whole layer is measured from, since every control
is a push away from it, and it is what makes the dialing order work: set
the color flat, then open one push and watch it depart from something
you chose. The previous field could not do this. It never reached its own
floor, so a color anchored there appeared nowhere on the wall — asked
for orange, the wall came back green through cyan to blue.

## The screen and the wall agree, give or take the diffuser

Judged 2026-09-21, playing the color looks side by side with
`tools/preview.js` open next to the strips.

**The preview matches the wall almost exactly.** One consistent
difference: the strips read slightly whiter than the screen, because they
are behind a diffuser and the screen is not. Judged small enough to leave
alone rather than compensate for — so when dialing on screen, expect the
wall to come back a touch paler than what you set.

This retires the question the preview was built under. It was trusted for
geometry and dialing and explicitly *not* trusted for color, on the
strength of three entries in this file where a screen would have got
color wrong. Those three were measured at the extremes of the old
field's controls; at the settings a look actually sits at, the screen is
good enough to design color on.

What that does not license is settling a question at the extremes on
screen — the dark floor and red's resolution are still hardware facts,
and the diffuser bias is one more reason the wall is the judge of a
finished look.

## The bounce rework holds on the wall, 2026-09-22

Five checks on the strips, all from the bench panel's own buttons, after
the preview had already agreed:

- **Alternate and bounce arrive on their own CCs.** Bars turns at the
  strip's ends; alternate sends the 2nd and 4th columns from the left the
  other way. So alternate and bounce land on CCs of their own — 60 and 61
  since the regroup — and the packed CC that held them is gone.
- **Bounce is per cell.** Four shapes each turn inside their own quarter.
  Nothing slides through into a neighbor and nothing re-enters at the
  far end, which is what it did while the journey was measured along the
  whole strip.
- **Fan staggers the swing.** With fan up the five strips no longer turn
  on the same frame; some are outbound while others are already coming
  back.
- **Full width is full.** Width at maximum with fan at maximum lights
  every pixel of all five strips. The fault this replaces lit 45, 36, 27,
  27 and 36 of 45.
- **The tail is a history.** At a turn the head reverses and travels back
  out through its own trail, which stays where it lies and fades. It no
  longer changes sides in one frame.

**Judged the same evening, and kept.** Per-cell bounce and the folding
tail are new behavior rather than repairs, so working and wanted were
separate questions; both were answered on the wall. Per-cell bounce in
particular was read as opening looks rather than repairing one — a row of
blocks each turning in its own compartment is a shape the machine could
not previously make, and it suggested more. The remaining untested thing
is a set with music, which is where a look earns its place in a song
rather than on a bench.
