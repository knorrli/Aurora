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

**The wave is one continuous byte**, running from a build through a swell
to a stab, with the named shapes on exactly reachable values. See "The
waves" below.

**A push is a fraction of the distance to a limit**, everywhere except
controls that wrap, which take a rotation instead. Two routes aimed at one
destination add their pushes and move it once, capped at the limit. A push
reaches one fixture family: the strips' three colour controls are CC 38–40
and the PARs' are CC 33–35. See "How a push lands".

**Which clock a route reads is decided by its destination**, not by the
route — a destination on a strip reads that strip's fanned reading, a
global one reads the plain clock. See "Which clock a route reads".

**The editor aligns route slots across a patch's five sets**, so a fader
morph never changes a destination mid-journey. Between patches, a route
whose destination differs at the two ends lands on arrival like any other
switch. See "What a morph does to a route".

**Eight routes, four bytes each** — destination, amount, ratio, wave. That
is 32 of the 51 CCs the retirement leaves free, so a source byte per route
stays affordable and the count can still be raised. See "What it costs,
measured".

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

**The eighteen go in one change, not two.** Running the old sends
alongside the routes would fit — the route block needs 32 of the 33
usable CCs even before the retirement frees anything — but it buys only
one thing, a rig that keeps working through the transition, and the rig
is not playable today regardless. What it costs is worse: a mechanism
half retired is the state most likely to be read as the design by whoever
picks this up next. So the eighteen retire in the same change that wires
the routes, and `shared/aurora_protocol.h` says so beside them.

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

Routes cost per route instead of per destination. Counted off
`shared/aurora_protocol.h` rather than off the blocks, which are not
final:

| | CCs |
|----|----|
| CC numbers that exist, 0–127 | 128 |
| Excluded, never to be assigned | −14 |
| Assigned today: 2, 12–26, 33–35, 38–57, 60–77, 83–91, 101–115 | −81 |
| Unassigned and usable | 33 |
| Reclaimed by retiring the pulse's sends | +18 |
| **Free** | **51** |
| Eight routes at four bytes | −32 |
| **Left over** | **19** |

The 18 reclaimed are CCs 74, 76, 77 and 101–115: the five destinations in
the 101 block, plus the strips' own brightness, whose amount, skew and
shape sit at 74, 76 and 77. **CC 75 stays**, because it is the rate, and
the rate becomes the clock.

### The fourteen excluded numbers

**0, 1, 7, 10, 11, 32 and 120–127 are excluded and must never be
assigned.** The test is not that the spec names them — it names CC 64,
74, 91 and 38 too, which are all in use here without trouble. The test is
whether something else on the chain sends the number *without being
asked*, and `DESIGN.md` makes that a live concern: one topology puts a DAW
into the controller with soft-thru to the brain, another aims a computer
straight at the brain.

| CC | What sends it unbidden |
|----|----|
| 0, 32 | Bank Select, emitted ahead of a Program Change by most DAWs — and a Program Change is how a patch is selected |
| 1 | a keyboard's mod wheel |
| 7, 10, 11 | a DAW track's own volume, pan and expression automation |
| 120–127 | Channel Mode messages — All Sound Off, Reset All Controllers, Local Control, All Notes Off, Omni/Mono/Poly — sent on transport stop, on panic and on track disarm, usually to every channel, so `AURORA_MIDI_CHANNEL` is no protection |

A parameter parked on one of these does not fail randomly. It fails on a
patch change or when the transport stops, which is precisely when nobody
can afford to look at the wall.

**The exclusion is a default, not a law.** If the map ever runs out, the
harmless end is CC 122, Local Control, which is essentially only ever sent
by hand, and then 124–127, which DAWs send far less often than the
120/121/123 panic trio. The dangerous end is 0 and 32, where every patch
change is another opportunity.

**CC 64 fails the same test and is already assigned.** It is Sustain, it
holds `CC_GEN_EDGE`, and a keyboard with a sustain pedal on channel 1
would send it unbidden. It is in use rather than up for choice, so it is
recorded here rather than acted on.

**Raising the count later is safe.** An array size in two renderers and a
loop bound in the editor. Nothing in storage, nothing on the wire, and
nothing in patches already written, since an absent route's CCs arrive as
zero and a zero-amount route does nothing. There is no migration and no
version byte. What is *not* freely raisable is the ceiling: 19 CCs left
over is the whole of it, and a source byte would claim eight of them.

**A source byte is not needed yet.** Adding wander, scatter or fan as
route sources later costs one more CC per route — eight at this count,
against the 19 left over. So leaving it out now paints nothing into a
corner. It is also why the count did not go higher: raising it later is
safe and lowering it is not, and twelve routes would have left three. The catch when it comes: the wander and the scatter have a
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

## How a push lands

Every destination takes one rule, with an exception for controls that have
no top or bottom.

**The default: a push is a fraction of the distance left.**

    result = base + |amount| × (limit − base)

The sign picks the limit — positive travels toward 127, negative toward 0.
At full amount the control arrives exactly at that limit; at half, it
covers half the remaining distance. A control dialed at 100 pushed +100%
reaches 127; the same control pushed −50% reaches 50.

This is `pushToward` in `brain/src/P_Generator.cpp:297` and
`brain/src/dmx_out.cpp:46`, already used for width, the PAR level, the PAR
saturation and the scatter's push on brightness. Adopting it everywhere is
less a decision than finishing one.

Two things fall out of it. Nothing can clip. And a control already sitting
at a limit has nowhere to go that way, so a route aimed at it does nothing
in that direction — which is intuitive, but it does mean the amount is not
the same size twice, because what a route can do depends on where the
control is parked.

**The exception: circular controls.** Hue is a wheel. 0 and 127 are the
same red, sitting next to each other, so there is no limit to travel
toward and the rule above has nothing to compute. For these the amount is
a rotation instead — how far around, and which way. The pulse's hue send
uses half the wheel at full amount (`GEN_PULSE_MAX_HUE`); whether that
span suits every circular control is not settled.

**Which controls are circular is a per-control fact**, and it belongs next
to the CC where the `[patch]` and `[switch]` tags already live, never in a
table somewhere else that can drift out of step. Three read as circular
today: hue, the pattern's standing position (CC 66, where the pattern
repeats once per cell), and the fan's phase (CC 69, which runs on 128ths
of a full turn). Tagging them is the same pass that answers which
destinations are refused.

**Two routes on one destination add up.** The pushes are summed and the
control moves once, rather than each route moving it in turn. Moving in
turn compounds — a control at 100 pushed half up and then half down lands
at 57, where summing leaves it at 100 — and the result then depends on
which slot each route sits in, which is nothing anyone chose musically.

Not an average, either. Averaging would mean a second quiet route
*weakens* the first, so layering a subtle stab over a big swell would
flatten the swell instead of adding to it, and an unused zero-amount slot
would drag the rest down with it.

**An amount is what a route contributes at the top of its wave**, not at
every moment — a route's push right now is `amount × wave`. So +50% and
+25% aimed at one control make +75% only where both waves peak together.
The corollary is a useful check on the model: two routes carrying the same
wave at the same ratio are redundant, because one route at the combined
amount does exactly the same thing. Two routes buy something only when the
waves or the ratios differ, which is the slow swell against the fast stab
that one shared rate made impossible.

**The sum is capped at the whole distance**, so +80% and +50% together
arrive exactly at the limit rather than sailing past it. Nothing reaches
this today, because each destination has exactly one send carrying one
amount — which is why the two implementations already disagree about it.
`brain/src/dmx_out.cpp:46` caps the amount; `brain/src/P_Generator.cpp:297`
and its mirror in `tools/preview.js` do not. Routes make it reachable.

**A push lands on one fixture family.** CC 38, 39 and 40 are the strips'
hue, saturation and brightness — the editor already draws the pulse as one
of the sources pushing them — and a push on them does not reach the PARs.
The PARs have their own three, CC 33, 34 and 35, expressed as a
relationship to the strips' *dialed* colour: an offset is measured from
where the fader sits, not from where the modulation has it at that
instant.

This is what happens today, but by accident rather than by decision —
`brain/src/dmx_out.cpp:71` reads `presetColor` straight, while the strips
get their pushes added in `colorAt`. It has to be written down, or someone
building "a base plus a modulation sum" will quite reasonably make the
PARs follow the modulated base, and then every hue swell drags them along.
The consequence is that swinging the strips and the PARs together takes
two routes, one on each side — affordable at eight, and probably what you
want anyway, since the two rarely ask for the same depth.

**Brightness stops being the odd one out.** It multiplies today
(`P_Generator.cpp:808`), and it is the only destination that rests at the
wave's high point while the rest rest at its low point. Under one rule it
rests at its dialed value like everything else, and a swell that pulls the
wall down is a negative amount. Both behaviors are already in that file —
the scatter pushes brightness the new way at `P_Generator.cpp:935` — so
this picks the one used more.

## Which clock a route reads

One clock ticks for the whole rig, and anything that pulses reads it to
know where it is in the cycle. It can be read two ways. **Plain**, where
everything reads the same time and the wall flashes as one. **Fanned**,
where each strip reads that same clock shifted a little, so the flash
rolls across the wall instead of landing at once. `CC_GEN_FAN_PULSE` sets
how far apart those five readings stand, and at its center they are
identical — so this is one clock read two ways, not two clocks.

**The route does not choose. Its destination does.** A destination that
lives on a strip reads that strip's shifted reading; a global one reads
the plain clock. This costs nothing, keeps a route at four bytes, and
reproduces exactly what happens today, where the choice is made only by
which line of the frame each send happens to sit on
(`brain/src/P_Generator.cpp:773` for the strips, `:959` for the PARs).

What the table records is not a fanned-or-plain flag but **where the
destination sits**, because the same column answers the unbuilt question
of where on the wall to sample the wander and the scatter when a route
points one of them at a global control.

The split is lopsided. Everything inside the strip loop at
`brain/src/P_Generator.cpp:762` is on a strip, and that is most of the
map:

| | CCs |
|----|----|
| On a strip | the shape 62–67, the fan 68–73, the placed field 43–49, the wander 50–54, the light level 55–57, the scatter 83–91, the three faders 38–40 |
| Global | the washes 33–35, tempo division 2 |

Roughly 36 of the 43 sit on a strip. The PARs have three controls of their
own.

**What this cannot do.** All strip routes are fanned together or none are,
because CC 73 is a single global amount — brightness rolling across the
wall while width pulses in unison is out of reach. A per-route byte would
buy it, eight CCs of the nineteen left over. Whether that stays cheap
depends on a CC layout nobody has chosen: routes laid out as blocks of
four consecutive numbers make raising the count free and a fifth byte
expensive, and giving each parameter its own block of eight does the
reverse. That belongs to the CC map step, not here.

## What a morph does to a route

One of a route's four bytes — the destination — is a switch, and
`DESIGN.md` § "Switches belong to the patch" allows a switch to move only
on arrival. That is new ground. Today the two ends of a journey always
agree about what pushes what, because the six sends are hardwired;
`tools/patch.js` states it outright: every destination is always connected
and its amount may be zero, so a morph interpolates an amount and can
never snap a connection on. Routes make the connection itself a value, so
the ends can disagree.

What that opens: patch A's route 3 pushes width at +60%, patch B's pushes
hue at −40%. The destination holds A's value the whole way while the
amount slides, so halfway across width is being pushed by +10% — a number
dialed for hue — and on arrival it jumps.

**Inside a patch, the editor aligns the slots.** A patch is five CC sets
and every fader move is a morph between two of them, so untreated this
would bite on every fader. The editor collects the destinations used
anywhere in the five, gives each a fixed slot across all five, and writes
zero where a set does not use it. An absent route and a route at zero
amount are the same thing, so a destination that differs between two sets
becomes one amount falling to zero while another rises — a crossfade, for
free, with the firmware still doing nothing but interpolating bytes.

The limit: the destinations used across the five sets have to fit in eight
slots. A far end is usually a variation of its base rather than a separate
patch, so they overlap heavily — but the editor has to notice when they do
not and say so, rather than silently dropping one.

**Between patches, a disagreeing route lands on arrival**, exactly like any
other switch. The editor cannot align here: any patch can follow any other
on the keypad, and making every pair agree would mean one slot assignment
across the whole library, which eight slots against 43 destinations cannot
carry. So a route whose destination differs at the two ends holds whole —
destination, amount, ratio and wave together — and changes on arrival. A
route whose destination matches at both ends interpolates its amount as
usual, because nothing about it is ambiguous.

**The UI decides whether this is usable**, and it is not designed. Slots
are bookkeeping the performer should never have to think about; what they
say is "the wander pushes width here". Whether alignment reads as the
editor helping or as the editor moving things behind their back is a
question for the editor work, which the build order already puts last.

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

### The fork, settled

**One continuous byte**, and both sides of the original fork are rejected.
A selector was one byte but a switch, so two patches with different waves
snap rather than travel. A detented sweep kept the continuum but cost
three bytes, and it spent them on combinations nobody dials — a wide
hard-edged bump and a wide soft one both read as a swell.

The byte is a single axis: the peak never leaves the bar line, and what
moves is how the bar fills around it. Every named shape lands on a value a
fader can actually reach.

| Value | Wave | Lit above half |
|----|----|----|
| 0 | builds across the bar, drops on the bar line | 50% |
| 32 | symmetric swell, peak on the bar line | 50% |
| 64 | snaps up on the bar line, decays across it | 50% |
| 96 | hard on for the first half of the bar | 49% |
| 127 | one-frame stab | 6% |

Below 64 the attack shrinks as the decay grows. Above 64 the attack is
gone, and the decay both shortens and flattens — which is what puts a hit
that holds and then falls at around 80, and a true square at 96. The 49%
at 96 is the raised-cosine edge, about a quarter of a percent of the bar.

**Saw down has to come before square.** A list would suggest the other
order, and that leaves a crossfade between two shapes blending into
something neither. In this order there is no crossfade at all: it is one
decay getting shorter and harder, and every value between is a wave worth
dialing.

**Triangle is not on the sweep, and that is free.** A triangle swell and a
sine swell look the same on a light fixture. The corner where it would
show is a destination that reads as motion rather than level, where
constant travel and eased travel differ — and rates, the obvious such
destination, are off-limits anyway.

**Hold and decay are tied**, and that is the price of the byte. One number
governs both how long the light holds and how fast it falls, so a short
hold with a long tail is not reachable.

**Everything peaks on the bar line.** No off-beat stab, no route
deliberately lagging another. The anchor does this rather than the wave,
and it is the question the fork was always going to hand back: whether a
route wants its own phase offset. That is one more byte, which would make
a route five and drop the ceiling to ten.

## Still open

1. **Which destinations are refused**, and whether the quantized ones —
   the fan's frequency in eighths, count as geometric whole numbers, the
   3-way ruler — are refused or allowed with the stepping treated as an
   effect. The same pass tags which controls are circular, and settles the
   rotation span each one takes.
2. **The PARs are one fixture, not four.** `brain/src/dmx_out.cpp:88`
   computes one colour and one level and writes the same eight bytes to
   all four addresses; the only per-fixture data is calibration trim. So
   the PARs cannot strobe one after the other, and no route design changes
   that — each would need a position the way a strip has one. Two pieces
   are missing: the per-fixture computation, and any record of which PAR
   stands leftmost, since `docs/wiring.md` settles the strips' order from
   PC 11 painting each one flat, while the fixture table is only addresses
   1, 9, 17 and 25 in chain order. Output-layer work that routes neither
   need nor fix, recorded here because a wanted look ran into it.

## Resuming this

**Read in this order.** This file; then `docs/generator.md` §§ "Where the
pulse reaches", "The fan is a wave" and "The scatter"; then the 101–119
block in `shared/aurora_protocol.h`; then the pulse machinery in
`brain/src/P_Generator.cpp` — `PulseSend`, `pulseWave`, `pulsePush`,
`anchoredPulsePhase`, `pushToward` — and its mirror in
`tools/preview.js`; then `docs/editor.md` for the panel model the new
one has to fit into.

**Answer the first question before building anything** — the second is
output-layer work that routes neither need nor fix. The wave
is settled and fixes a route at four bytes, which settled the count at
eight and took the pressure off the fifteen soldered amounts. How a push
lands is settled too, including what two routes on one destination do and
which clock each one reads. What morphing does to a route is settled too, and so is the order the
change lands in. All that is left is per-control work: which destinations
are refused, and which of them wrap.

**Then build in this order.** The editor comes last on purpose — its shape
depends on what a route turns out to be.

1. The destination table in the firmware: every modulatable parameter
   becomes a base plus a modulation sum, with a declared application rule
   and unit and the sum capped at the whole distance, while the existing
   six destinations stay hardwired. Nothing
   should change on the wall.
2. The same in `tools/preview.js`, cross-checked function by function.
3. The wave, both sides: the one-byte sweep in "The waves" above.
4. The CC map and the routes, in one change: retire 74, 76, 77 and
   101–115, add the route block, wire routes in place of the six hardwired
   sends on both sides, and check every consumer number by number the way
   the regroup did. One change rather than two, so the tree never holds a
   half-retired pulse for someone to mistake for the design.
5. The editor: the control list in `tools/patch.js`, the per-slider panel,
   the route marks, the route list, and `docs/editor.md`.
6. Fold what is settled here into `docs/generator.md` and leave this file
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
