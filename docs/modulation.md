# Modulation routing

Raised 2026-09-23, not settled. What sends a modulator at a control, how
many can be live at once, and what shape the wave is.

Nothing here is built. The rig today has one oscillator reaching six
hardwired destinations, and that is what this would replace.

## What started it

Three complaints from using the editor, in the performer's words:

- The targets are set in the modulator's own section, not at the control
  being modulated, which is a lot of scrolling and means remembering what
  the valid targets even are.
- Only a fixed set of controls can be reached at all. "I'd love to
  modulate this slider right now" runs into a slider that was never on
  the list.
- The rate is shared by every target, so fast strobing strips with slow
  strobing washes needs a workaround rather than a setting.

## What is settled

**One clock, not several oscillators.** Everything else about a
connection — how much, how fast relative to the clock, what wave —
belongs to the connection. The existing design already put amount, shape
and skew on the destination and shared only the rate; this finishes that
thought and makes the rate a per-route ratio. What is left in the middle
is not an oscillator any more, it is a clock, which is the same shape as
tempo division and `AURORA_PULSE_PERIODS` one level down.

**Ratios multiply, never divide**, so the base rate is the slowest thing
in a patch. Fast strips and slow washes is the clock at the slow swell
and the strips multiplied up. This is not a preference — see "Division
breaks the anchor" below.

**No free-running modulator.** The wander already is one: two sine terms
at the golden ratio, built so they never come back into step. The pulse
is anchored to the bar on purpose, its rate is stepped so it stays on the
grid, and there is a routine whose whole job is walking it back there. A
free-running oscillator throws that away and reads as a fault from the
floor rather than as depth.

**A destination is named by its own CC number.** No second numbering to
keep stable, no mapping table to drift, no reordering hazard once patches
exist. `shared/aurora_protocol.h` is already the list, and whether a
control may be a destination can be written next to it, where the
`[patch]` and `[switch]` tags already live.

**The destination byte is a switch, not a magnitude.** It must not be
interpolated when a patch morphs. The format already has this concept —
`tools/patch.js` keeps `SWITCHES` out of far-end interpolation entirely.

**Retire the pulse's eighteen; keep the fifteen singles.** See "Every
amount is a soldered route" below.

**The panel goes to the slider**, with a mark on any slider a route
reaches so it can be seen without being opened, and one place listing
every live route so "what is wired in this patch" is a glance rather than
an audit.

**Start at eight routes** — subject to the wave decision, which changes
what a route costs and therefore how many fit.

## Every amount is a soldered route

The rig already has a modulation matrix. It is hardwired.

| Source | Connections | CCs |
|----|----|----|
| The fan | position, rate, pulse | 3 |
| The placed field | hue, white, dark | 3 |
| The wander | hue, white, dark | 3 |
| The light level | hue, white, dark | 3 |
| The scatter | light, hue, white | 3 |
| The pulse | six destinations, each with shape and skew | 18 |

Twenty-one connections over 33 CCs, every one of them "how much does this
source reach that destination", with the destination frozen at design
time.

The pulse's eighteen are the ones to retire. They cost **three** CCs
apiece because each carries its own shape and skew, and a route replaces
them with something strictly more capable — any destination, its own
ratio, and the shared-rate defect gone.

The other fifteen stay. Each is **one byte**, always available, costing no
route slot, with a destination that is the point of the control rather
than an arbitrary choice. Converting one to a route would pay five bytes
and a scarce slot to buy the ability to re-aim something mostly aimed
correctly already. Two of them could not become routes anyway: the fan's
rate and pulse amounts are aimed at a rate and a phase, which is the
class that is off-limits.

**The rule this gives.** A connection whose destination would never change
stays soldered. A connection that exists only because the destination had
to be picked at design time becomes a route. The fifteen are the common
path and stay cheap; routes are the general path, for the aims nobody
anticipated — the wander pushing width instead of hue, the scatter
widening a shape rather than lighting it. Those are genuinely unreachable
today and they are what the mechanism buys.

There is no conflict in having both. The color layer already sums its
pushes, so a soldered amount and a route landing on the same destination
add, which needs no new rule.

**And it is the same argument that parked palettes.** No patches exist
yet, so how many simultaneous connections a real look needs is a guess.
Retiring fifteen working, already-dialed, one-byte amounts on a guess is
expensive in both implementations and in the editor, before a single
patch has been written to say whether it hurt.

## What it costs, measured

The answer to "is there a mechanical reason not to build this" is no, on
every axis but one.

- **Frame time.** 7–8 ms, steady, almost all of it FastLED pushing 225
  pixels — `docs/bench-facts.md` § "Frame timing". That part does not care
  what is modulated.
- **Computation.** Modulation is evaluated once per parameter per strip
  per frame: at worst 43 × 5 = 215 evaluations, against the 900 shape
  evaluations already recorded as "nothing on this part". Nothing lands in
  the per-pixel loop, because no destination is per-pixel — the pulse is
  already computed per strip.
- **RAM.** A parameter table of 43 floats plus accumulators is about 350
  bytes, against 411 KB free for locals. Flash is 93 KB of 2 MB.
- **Storage and wire.** Exactly unchanged. A patch is a flat 128-byte CC
  set and routes are CCs, so a patch stays 128 bytes, the library stays
  the same size, and the SysEx format does not move.

**The one real constraint is the CC map**, and the current design is the
expensive one. The pulse spends 18 CCs to reach six destinations; the same
triple for all 43 reachable controls would be 129 CCs against the 128 that
exist. That arithmetic is why the fixed list exists.

Routes cost per route instead of per destination. There are **51 CCs to
spend**: 33 spare after the regroup, plus the pulse's 18 reclaimed.

| Bytes per route | What a route carries | Routes that fit |
|----|----|----|
| 4 | destination, amount, ratio, one wave byte | 12 |
| 5 | destination, amount, ratio, shape, skew | 10 |
| 6 | destination, amount, ratio, shape, skew, duty | 8 |

**Raising the count later is safe.** An array size in two renderers and a
loop bound in the editor. Nothing in storage, nothing on the wire, and
nothing in patches already written, since an absent route's CCs arrive as
zero and a zero-amount route does nothing. There is no migration and no
version byte. What is *not* freely raisable is the ceiling: the table
above is the whole of it.

**A source byte is not needed yet.** Adding wander, scatter or fan as
route sources later costs one more CC per route, which fits inside what
eight routes leave spare. So leaving it out now paints nothing into a
corner. The catch when it comes: the wander and the scatter have a
position, so pointing one at a global control needs a rule for where on
the wall to sample it — the same unbuilt work `docs/generator.md` already
records against sampling the color layer at the PARs.

## Two details that would be found on the wall

**A ratio scales the distance from the peak, not the phase.** The anchor
puts the clock's peak on the bar line. A route reading `wave(phase × 2)`
sits in its trough there and peaks a quarter of a base period later.
Written as `0.5 + (phase − 0.5) × ratio`, every route peaks on the bar
together whatever its ratio.

**Division breaks the anchor; multiplication does not.** The anchor fixes
only the *fraction* of the tracker's offset, because a whole cycle of
offset is invisible — true for the clock and for any whole multiple of it.
A route at half rate takes two base cycles, and which of the two it peaks
on depends on the offset's integer part, which nothing controls. Nudging
the rate can shift that integer by one, which flips a halved route to the
opposite phase: a control that inverts a slow swell when a different
control is touched. Restricting ratios to whole multiples costs nothing
and removes it entirely.

## Rates as destinations

Every rate feeds a running total through a `PhaseTracker`, so a push on a
rate integrates and the wall drifts permanently instead of returning. The
argument that a bipolar push averages to zero over a cycle holds **only
while skew is centered** — skew slides the peak, which makes the average
non-zero, which makes a modulated rate drift for ever.

So a route aimed at a rate is a different mechanism from one aimed at a
level, and rates are off-limits as destinations until that is designed
rather than assumed.

## The waves

### What is wrong today

`pulseWave(phase, shape, skew)` sweeps a sine toward a square by gain and
clamp, and warps the phase to approximate a ramp.

**Skew cannot narrow a square, and never could.** The wave is above half
between `t = 0.5k` and `t = 0.5 + 0.5k`, where `k = 0.5 + 0.48 × skew`.
That window is **exactly 0.5 wide for every value of k**. At the hard end
of the shape sweep, skew slides the stab through the cycle and leaves its
length alone. There is no setting in the current two knobs that produces a
short stab — not a weak approximation, a hole.

**Triangle is the shape nobody can see.** On a light fixture a triangle
swell and a sine swell look the same; the difference is a slightly sharper
apex, on a fade, at stage distance. The fan uses a triangle for a
geometric reason — standing five strips at even offsets — not a visual
one. Not worth spending anything to reach.

**Duty is the real gap, and it is one line.** The clamp's center is
hardcoded: `shaped = (lfo − 0.5) / softness + 0.5`. Gain gives
square-ness; moving that center gives duty.

**Short has a floor, and it is the frame, not the maths.** At 7–8 ms a
frame and the fastest rate, a 6 % stab is about one frame long; shorter
lands between frames and flickers instead of shortening.

### The fork, still open

**A selector** — sine, triangle, square, saw up, saw down, narrow pulse —
is **one byte instead of three**, because a list containing the saws and a
narrow pulse absorbs what skew and duty were approximating. That is its
real argument, and it is a strong one: 12 routes instead of 8.

Against it: it gives up everything between the named shapes, which is the
direction this instrument is going — a continuous space rather than a
fixed roster, and a wave selector is that roster one level down. It is
also a switch, so two patches with different waves snap rather than
travel.

**A detented sweep** keeps the continuum and answers the selector's one
genuine virtue, which is that it is dialable. The rig has solved this
tension twice already the same way: the fan's frequency steps in eighths
so "still" is exactly reachable, and the pulse's rate steps to bar-holding
periods. Put the named shapes on detents — exact at both ends and the
center rather than merely near them. Costs three bytes and eight routes.

**A cheaper middle**, noted and not recommended: one "lean" byte instead
of skew and duty, warping the phase when the wave is soft and biasing the
clamp when it is hard. Reaches everything for two bytes, but a control
that means two things depending on another control is close to the
"parameter abuse" this whole discussion is trying to get away from, and it
would be met at the bench rather than here.

**One thing the selector silently drops.** Skew is doing two jobs: shape
when the wave is soft, and *placement* when it is hard, where sliding the
stab through the cycle decides where in the bar it lands. A list absorbs
the first job and loses the second, so the selector may hand back a
question about whether a route wants its own phase offset.

## Still open

1. **How a push applies, per destination.** They do not agree today:
   brightness multiplies (`swell = 1 − amount × …`), width pushes toward a
   limit, hue adds scaled to the wheel. A route pointing anywhere needs one
   rule, or a declared rule and unit per destination. This is the actual
   work, and it is where the firmware and `tools/preview.js` can silently
   drift apart.
2. **Two routes on one destination** — sum the pushes and apply once, or
   apply in turn. The color layer already sums; nothing else has had to
   answer it.
3. **Fanned clock or plain clock, per route.** Today the strips read
   `pulse + fanPulse × wave` and the washes read the plain phase, because a
   PAR is one position with no strip to be offset from. That distinction is
   implicit in where each destination is read in the frame. Once a route
   can point anywhere it has to be stated, or it gets decided by accident
   by whoever writes the loop.
4. **Which destinations are refused**, and whether the quantized ones —
   the fan's frequency in eighths, count as geometric whole numbers, the
   3-way ruler — are refused or allowed with the stepping treated as an
   effect.
5. **The wave fork**, above. It decides what a route costs and therefore
   how many fit, so it is the one that unblocks the others.
6. **Morphing between patches whose routes are aimed differently.** A
   destination is a switch, and switches land on arrival or on release at
   the end of a journey — so travelling from a patch where route 3 pushes
   width to one where route 3 pushes hue, the *amount* interpolates the
   whole way while the destination stays on width and snaps at the end.
   Halfway across, width is being pushed by a number dialed for hue.
   Soldered amounts cannot do this, because their destinations never
   disagree. Two ways out, both cheap: order routes canonically by
   destination so slot N means the same thing in every patch, or have the
   editor align slots when it builds a library. This is the one place the
   matrix is genuinely weaker than what it replaces.
7. **Sequencing** — whether the pulse's eighteen retire in the same change
   that brings routes, or after.

## Resuming this

**Read in this order.** This file; then `docs/generator.md` §§ "Where the
pulse reaches", "The fan is a wave" and "The scatter"; then the 101–119
block in `shared/aurora_protocol.h`; then the pulse machinery in
`brain/src/P_Generator.cpp` — `PulseSend`, `pulseWave`, `pulsePush`,
`anchoredPulsePhase`, `pushToward` — and its mirror in
`tools/preview.js`; then `docs/editor.md` for the panel model the new
one has to fit into.

**Answer the seven questions before building anything.** They are not
independent: the wave decides what a route costs, which decides how many
routes fit, which decides whether the fifteen soldered amounts are under
any pressure at all.

**Then build in this order.** The editor comes last on purpose — its shape
depends on what a route turns out to be.

1. The destination table in the firmware: every modulatable parameter
   becomes a base plus a modulation sum, with a declared application rule
   and unit, while the existing six destinations stay hardwired. Nothing
   should change on the wall.
2. The same in `tools/preview.js`, cross-checked function by function.
3. The wave, both sides, whatever question 5 decided.
4. The CC map: retire 101–115, add the route block, and check every
   consumer number by number the way the regroup did.
5. Routes replace the six hardwired sends, both sides.
6. The editor: the control list in `tools/patch.js`, the per-slider panel,
   the route marks, the route list, and `docs/editor.md`.
7. Fold what is settled here into `docs/generator.md` and leave this file
   as the record of why.

**Check it with `node tools/crosscheck.mjs`.** It lifts a function and
its dependencies out of both renderers, drives them over the same grid,
and reports where they disagree — the discipline that keeps two
implementations of the same maths identical. Add a check for every
function this work touches; a check is a few lines of table. Sweep on
binary-exact steps, halves and quarters rather than tenths, or the two
sides are handed different inputs before the function is even called.

Two things it cannot do yet, both worth building when they are needed
rather than now:

- **Compare a whole rendered frame** rather than one function. That is the
  check that would prove a refactor changed nothing, and it is blocked on
  the firmware's `Generator()` needing FastLED, `CHSV` and `tempo::beats()`
  to run on a host.
- **Compare against a stored baseline** instead of against the other
  implementation, which is what step 1 above actually wants: proof that a
  no-behavior-change refactor changed no behavior. A `--save` that writes
  the outputs and a run that diffs against them.
