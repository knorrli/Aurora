# The generator

An experiment that worked. Started 2026-09-18 as a bench test and came out
of it as a candidate foundation rather than a feature.

It is not committed to yet, and it replaces nothing: the nine hand-written
patterns still sit on PC 1–9, untouched. The generator is PC 10.

---

## Why it exists

Aurora kept being described as a light synthesizer and kept not behaving
like one. The diagnosis that made the difference is not the usual one —
"a synth has continuous controls and Aurora has discrete ones". Synths
have plenty of discrete controls: waveform, filter type, LFO shape.

The real difference is **what the discrete choice buys you**. On a synth
the discrete choice picks *raw material* — a saw wave is harsh, ugly and
nobody plays one. The character comes from the continuous shaping applied
to it, and the same saw becomes a bass, a pad, a lead or a pluck.

Aurora's ratio was inverted. "Plasma" is not raw material, it is a
finished look, and the continuous controls only changed its color. So
the discrete choice carried nearly all the identity and the faders
decorated it.

Three consequences, all of which were live complaints:

- Changing look meant a discrete jump, visually abrupt.
- You could only ever reach the looks someone had written.
- There was no way to stumble onto something good, which on a synth is
  half of what playing one is.

## The model

One machine: **a shape, repeated along each strip, optionally traveling,
with the five strips optionally run out of step with each other.**

Each strip is divided into `count` equal cells. Every cell contains the
same shape at the same position within it. The shape has a solid core and
fades that reach outward into the gap on either side.

That is the whole thing. Everything below is a parameter of it.

## Parameters

| CC | Control | Meaning | Range |
|----|---------|---------|-------|
| 45 | Alternate | Odd strips run against the even ones | switch |
| 46 | Bounce | Reverse at the strip end instead of wrapping | switch |
| 70 | Width | The solid core, as a proportion of one cell | 0–100 % |
| 71 | Count | How many shapes along the strip | 1–20 |
| 72 | Edge | How far the glow reaches into the gap, both sides | 0–100 % of the gap |
| 73 | Tail | How far the trail reaches behind, into the gap | 0–100 % of the gap |
| 74 | Speed | Travel along the strip. Bipolar — center is still, either side travels | ±60 px/beat |
| 75 | Fan | How far the five strips run out of step | 0–100 % of a cell |
| 76 | Jitter | Randomness in position and brightness, re-rolled once per swell | 0–100 % |
| 77 | Pulse depth | How far the trough digs below full light | 0–100 % |
| 78 | Pulse rate | How long one swell takes. Stepped | 16 → 0.25 beats |
| 79 | Pulse skew | Bipolar — center is an even rise and fall, either side slides the peak toward a ramp | — |
| 80 | Pulse shape | Hard on/off square through to smooth sine | — |
| 115 | Position | Where a still pattern stands in its cell. Bipolar — center is the middle | ±half a cell |
| 100–114 | Pulse destinations | Where else the swell reaches, three apiece | see below |

Color is hue, whiteness and darkness, and sits downstream of all of this —
this branch decides whether a pixel is lit, never what color it is.

**The two flags are switches rather than knobs because neither has a
middle.** Half of "runs opposite" is not a state, and neither is half of
"turns around at the end". Everything else is continuous.

### The units rule, which cost three attempts to find

> **Speed is an absolute distance. Width, edge and tail are proportions
> of the shape. Count changes the cell, and nothing else should notice.**

Getting this wrong produced three separate symptoms on the bench, all with
the same cause — measuring something in cells when cells shrink as the
count rises:

- Speed in cells meant travel slowed to a crawl as shapes were added. It
  is motion through real space, so it has to be pixels per beat.
- Edge in pixels meant the fade starved as cells narrowed, leaving visible
  stepping; edge as a proportion of the shape's own width meant most of
  the fader did nothing at high counts. It has to be a proportion of the
  **gap**, which is what the fade actually has room to occupy.
- Tail had the same problem for the same reason.

### Fades reach outward, never inward

An earlier version had the edge fade eat inward from the shape's boundary.
Softening therefore made a shape look *smaller*, and the dark gap between
shapes stayed hard no matter how soft the shapes got.

Fading outward into the gap fixes both, and gives the parameter a natural
limit: at 100 % the two neighboring fades meet at zero, so they never
overlap and nothing has to be summed. **Edge at full always closes the
gaps, whatever the width is** — which is the behavior anyone expects from
a softness control.

Consequence worth knowing: at width 100 % there is no gap left, so edge
and tail do nothing. That is consistent rather than broken.

### A still pattern stands where it is told, built 2026-09-22

Travel is a running total, and it is what says where the pattern stands.
Stopping does not clear it, so a stopped pattern stood wherever the last
traveling one ran out — arbitrary, invisible to every control, and different
on each recall. A patch saved still did not come back to the same place, on
the bench or on stage.

Position is where the pattern stands once Speed is centered, and half a cell
each way covers everywhere it can be: the pattern repeats once per cell, so a
full cell of offset lands back where it started. Center is the middle of the
cell, which at count 1 is the middle of the strip — a single lit pixel in the
center of each strip, which is the thing that could not be dialed before.

**What is left over eases away rather than snapping.** While the pattern
stands still, the running total walks to the nearest whole cell over about two
beats. A whole cell is invisible, so home is never more than half a cell away,
and bringing Speed to a stop settles rather than jumps. It is the same fix as
the pulse's anchoring, one section down, against the same cause.

**Under bounce Position does nothing.** The swing is anchored to the cell's
walls — that is what keeps the turn where Width puts it — so there is no
offset to give it without pushing the core out of its own cell.

Fan still spreads the five strips from wherever Position puts them. Centered
on all five means Position centered and Fan at zero.

**Still has to mean still, or Position cannot hold.** The settle runs at a
standstill and nothing else, so one step either side of center used to be a
crawl of about a pixel a minute that never settled: the readout said still,
and after a few minutes the pattern was visibly off-center. Anything under
0.05 px/beat is now zero, which costs the two fader steps that already read
as still.

**An off-center shape at nearly full width shows the next copy.** With bounce
off the strip is a loop — that is what carries a traveling shape off one end
and back on the other — so a shape standing off-center and grown until its far
edge crosses the end is met by the next copy coming in behind it: body, a dark
pixel, then a lit sliver at the far end. It is the loop working, not a fault,
and it is only reachable at count 1 with the shape both off-center and nearly
as wide as the strip.

### Where the pulse reaches, built 2026-09-22

One oscillator with one rate, pushing on six things at once. Each
destination carries its own **amount**, its own **shape** and its own
**skew** — the same phase, shaped differently — which is what buys the
washes breathing while the strips strobe.

| Destination | Amount | CC |
|---|---|---|
| The strips' brightness | unipolar: how far the trough digs | 77, 80, 79 |
| Width | toward full width / toward nothing | 100–102 |
| The strips' hue | ± half the wheel, after the color layer has summed | 103–105 |
| PAR level | toward full / toward dark | 106–108 |
| PAR hue offset | ± half the wheel | 109–111 |
| PAR saturation | toward a pure hue / toward white | 112–114 |

Every destination is measured from a control the performer set, which is
what makes the wall rest at exactly what was dialed. PAR saturation was
the one without such a control and measured from the strips' S fader
instead; it measures from CC 62 now.

**A push runs from the dialed value toward one of its two limits, and the
amount's sign picks which.** Nothing can clip, a control already sitting
at a limit simply has nowhere to go that way, and the wall rests at
exactly what was dialed whenever the swell is at its bottom. It is the
same rule the color layer's darkness push already used.

**The strips' brightness is the one destination with no sign**, because
there is nothing above full light for a push to run toward. Its amount is
how far the trough digs below whatever the shape branch already lit, which
is what Depth has always meant. Making it bipolar would spend half of the
most-used fader on an inverted strobe nobody has asked for.

**Width is back, and the reason it failed before is gone.** A shape was
anchored by its head then, and the head sits at one end of the strip when
nothing is traveling, so a swell read as a fill creeping in from that end
and a fast swell read as a traveling wipe rather than a flash. A shape is
anchored by its center now, so growing it is a breath outward. Under
bounce the turn stays where the *dialed* width puts it: letting the swell
move it would put the pulse into the travel rate, which is the one thing a
destination may never be.

**The washes take the unfanned phase.** Fan is where a strip stands in the
cycle, and a PAR is one position with no strip to be offset from. Reaching
the four of them separately is a different job — see `DESIGN.md` § "The
PAR cans".

**Fan is not a destination**, and waits on the fan rework. It is the one
that is not a plain push: the pulse's own per-strip phase is
`fract(pulse + stripPhase)`, so aiming it at fan feeds the pulse back into
itself.

**Rates are not destinations at all.** Speed, placed speed and wander rate
all feed a running total, so a pulse aimed at one shifts position
permanently: turn the amount up and back down and the shape sits somewhere
else with every control where it started. The looks that wanted them want
easing, which is locked to the traversal and cannot drift.

A sine can never produce an on/off edge no matter how deep it goes, which
is why pulse shape exists as a separate control.

### The pulse lands on the bar, built 2026-09-22

Two halves, and neither works without the other.

**The phase is anchored.** The offset that keeps a rate change from
teleporting — see `docs/bench-facts.md` § "A phase derived from absolute
time teleports" — is also what left the cycle's zero wherever the rate was
last touched. A whole cycle of offset is invisible, so only the fraction
has to go: it is eased out over about two cycles, which walks the pulse
back onto the grid without ever jumping. Measured in the preview: a rate
moved mid-flight pushes the phase to at most about 1.4× its settled speed
for a moment, and a morph sweeping the whole rate fader never steps more
than a tenth above nominal in a frame.

**The rate is stepped.** 16, 12, 8, 6, 4, 3, 2, 1½, 1, ¾, ½, ⅜, ¼ beats —
halves and their dotted values, thirteen positions. Anchoring alone only
puts the cycle's zero on the music's zero; a period of 2.64 beats walks
through the bar for ever and no anchoring can stop it. Only a period a bar
can hold a whole number of stays put, which is what the table is.

**What is anchored is the peak**, because the complaint was that a deep
slow swell peaks wherever it happens to. A sine therefore reaches full on
the downbeat. A square, which is that sine clipped around its own
midpoint, is symmetric about the peak, so its flash is *centered* on the
beat rather than starting there — at a two-beat period that is a flash
beginning half a beat early. Skew is the control that moves the flash
inside the cycle, and is what to reach for if the edge wants to land on
the beat instead. Whether the anchor should be the leading edge rather
than the peak is a judgment for a click track, not a bench.

## What it reaches

Thirteen of the roster, approximately, from nine knobs and two switches:

| Look | Roughly |
|---|---|
| Fill | width full, not moving |
| Sweep | a third width, hard edge, traveling, fan zero |
| Rain | the same with fan up and some tail |
| CrossSweep | Sweep with alternate direction |
| Bars | Sweep on bounce instead of wrap |
| Breathe | width full, not moving, pulse deep and slow, sine |
| Wave | the same with fan up |
| Chase | width full, pulse at maximum, fan at maximum |
| Comet | a narrow shape with a long tail, traveling, strips fanned |
| Starfield | tiny shapes, high count, jitter up |
| Strobe | width full, pulse at maximum, fast, square |
| Stutter | Strobe with alternate direction and some fan |
| Glitch | tiny, high count, jitter at maximum |

**Fan is doing three jobs at once** — it is the Sweep→Rain axis, the
Breathe→Wave axis, *and* the thing that turns a pulse into a chase across
the strips. The first two were already known to be one number; that the
third falls out of the same parameter is the strongest evidence the
decomposition is real rather than fitted after the fact.

**Plasma and Aurora are not in here and should not be.** They are a color
field rather than a moving shape, so they are either a second generator or
they stay hand-written. Either is fine.

## What the bench proved, 2026-09-18

- **The space between the known looks is playable, not mush.** This was
  the question the whole experiment existed to ask. Moving a single
  parameter from one setting to another passes through territory that
  holds up.
- **Morphing between two patches works.** Breathe → Strobe reads as a
  build. Rain → Stutter with speed and hue moving too was judged to
  "genuinely work". Two patches built from scratch, with no reference to
  the roster, morphed well between each other.
- **Color belongs inside the morph.** Hue, saturation and brightness were
  outside the model as designed; morphing them along with the shape turned
  out to be part of what makes it read as one gesture rather than a
  parameter sweep.
- **Count is genuinely new territory.** Nothing in the roster sits between
  one shape and a dozen, so three to six shapes is unexplored, and
  high count with a wide edge produces a continuous traveling ripple that
  no existing pattern can make.

### The panel's roster settings are guesses

The roster entries in `tools/index.html` were written by reasoning from
each look's description, not by dialing it in and comparing. Several
have since been found wrong on the wall. They are fine as starting
points to wander from, and they are **not** fit to morph between: a
morph between two wrong destinations says nothing about the morph.

Five or six looks dialed in by eye and saved as patches is what the
morph work needs underneath it, and nothing else about the morph is
worth judging until they exist.

### What this does to "transition"

A transition stops being a kind of object. It is two patches and a number
between them, and the number can be driven by a hand, by the clock over N
bars, or jumped instantly for a cut.

That dissolves a question that had been open and blocking for weeks —
whether a transition is a scene, an effect, or a third kind of thing —
rather than answering it.

## The color layer, redesigned 2026-09-21

A color is **hue, whiteness and darkness**. Everything else is a push on
those three, measured from the three faders, and the pushes add. With
every control centered the wall is exactly the color on the faders — which
is what makes the dialing order work: set the color flat, then open a
push and watch it depart from something you chose.

Three things push, and what separates them is what each is anchored to.

| Source | Anchored to |
|----|----|
| The placed field | a position you pick something to measure against |
| The wander | nothing — it is never in the same place twice |
| The light level | how lit the shape branch left that pixel |

The layer reads the **shape branch's** light level and never its own.
Feeding its own darkness back in would make color depend on color: pull
the wall down for a quiet verse and the hue would slide with it.

### What you place

Two primitives, either of them measured against one of three rulers.

- A **gradient** runs one way across its ruler with the base color at the
  center. The amount is how far *one* end departs, so the two ends land
  twice that far apart.
- A **region** is a bump — base, departure, back to base — built from the
  shape branch's own core and fades. Count, width and edge therefore mean
  the same thing in both branches.

The ruler is **across the five strips**, **along a strip**, or **within a
shape** — a shape's leading tip through to the end of its tail, traveling
with it.

Six combinations, and every look asked for on the wall lands on exactly
one of them:

| Look | Primitive | Ruler |
|----|----|----|
| A rainbow across the five strips | gradient | wall |
| One color in the middle, mirrored outward | region | wall |
| Cyan through violet to pink up a strip | gradient | strip |
| A hard green cell at the center of a strip | region | strip |
| A bar with a red head and a green tail | gradient | shape |
| A band across the middle of a shape | region | shape |

**Mirroring is a region, not a mode of the gradient.** A fold flag on the
gradient was nearly built and would have made the same look reachable two
ways with different controls. Dropping it, each primitive does one thing
and the table has no duplicate rows.

### The two sides of a shape are not the same length

A tail reaches far further than an edge fade, so the shape ruler
normalizes its two sides separately: 0 at the leading tip, **0.5 at the
core's center**, 1 at the end of the tail.

Normalizing the whole span at once put the ruler's middle halfway between
the two tips, which with a long tail is well behind the core. A region
asked to sit at the middle of a shape then landed nowhere near the bright
part. Found on screen before any of it was flashed.

### What lives

Color never quite the same in two places, with the difference always
moving. Three controls, each sayable in words before you turn it: **how
much** for each of the three qualities, **how fast**, and **how big** —
the whole wall moving as one, down through patches a strip-length across,
down to individual pixels shimmering.

**Never repeating is built in rather than dialed.** Two terms whose rates
sit at the golden ratio can never come back into step. An earlier attempt
put that on a control — how far apart the two speeds sit — which is making
the performer operate the mechanism rather than the look.

This is the one job the previous field did well and the redesign nearly
lost. Three superimposed waves looked right on the wall for a year and
were impossible to reason about; one wave could be reasoned about and
marched.

### Color from the light level

Color read off how lit a pixel already is, so a comet's tail cools
instead of only dimming. Anchored at the dim end: the faders are what a
fade runs out to, and the core is the departure. It reads the shape's own
profile, before jitter and the pulse.

**It is not made redundant by the shape ruler**, and the reason is worth
keeping. The pulse swells brightness with no spatial component at all, and
jitter scatters it at random; neither has a position for a ruler to
measure. Only this source reaches them. Point it at a flashing wall and
the wall goes hot as it flashes.

The two are also not interchangeable where they overlap. On a comet, hue
from the light level bunches the whole color change into the few pixels
behind the core, because the tail is dim and nearly flat over most of its
length — there is almost nothing left for color to follow. A gradient within
a shape measures position instead and spreads evenly the whole way. So a
white head wants the light level, and color along the tail wants the
gradient:

| Want | Set |
|----|----|
| White head | light level → to white, full |
| Color along the tail | gradient, within a shape, hue |

### Why the previous field was replaced

Two things it could not do, both asked for on the wall, and both geometry:
hold a color still somewhere, and put one color on each strip.

It was a cloud with a count knob attached. Making it reach a flat floor so
it could place a color is the same change that made Plasma and Aurora
read more regular — geometry was bought with aliveness, because one field
had to be both. **That is the fault the redesign fixes**, and it is why
there are two separate things here rather than one with more parameters.

Three smaller findings survive from it, and all three are built in above:
a color must have a place of its own or the fader stops reading as a
thing that sets; darkening wants a geometric taper because it is a ratio
of light; and steepening an edge is what turns a general unevenness into
regions you can see.

### Two branches, one vocabulary

The shape branch decides whether a pixel is lit at all, which is what
makes gaps and darkness. The color layer only ever decides what color a
lit pixel is. That is a fixed order in a chain, not two destinations off
one source — collapsing them would permit routing color to lit-or-not,
which is the shape branch again with fewer controls.

What is shared is the vocabulary. Count, width, edge and speed mean the
same thing in both, and count is the same *unit*: shapes along one strip
against regions along one ruler.

Two things follow from drawing it that way, and both are in
`tools/index.html`:

- **The two branches do not reach the same lights.** A PAR is one pixel,
  so the shape branch cannot reach it — count, width, edge, tail, speed
  and fan all describe positions along a strip. The color layer is
  largely what a one-pixel fixture can render, which is why the PARs'
  level and hue offset are a relationship to the strips rather than a
  second look. See `DESIGN.md` § "The PAR cans". The shape ruler is the
  exception and does not reach them at all.
- **The pulse is the exception that crosses.** It sits in the shape
  branch, but it is the one shape-side thing a PAR can show, so it is
  drawn as a send rather than as part of the branch.

What is shared runs deeper than the words. Both branches are now read at
the same four samples across each pixel, because a region a pixel or two
wide aliases exactly as a shape that size does — see `docs/bench-facts.md`
§ "Point-sampling a pattern aliases". The wander and the light level want
none of it: one is sines and the other reads a level the shape branch has
already averaged.

### What the wall found, 2026-09-21

The first session spent trying to reach a look already in mind, rather
than exploring for a good one. It turned up three faults, all of which
made the machine look less capable than it is — worth recording because
each had been invisible while the space was being wandered rather than
aimed at.

- **Edge wrapped around the strip's ends under bounce.** The core was
  clamped to the strip and the fades were not, so a glow leaving one end
  arrived at the other, and edge could not be used with bounce at all.
  Which shape lights a pixel is now the strip's business rather than the
  shape function's: the two images either side of a sample are
  considered, and under bounce an image standing off the end is not
  there to be seen.
- **Alternate never reversed anything.** Travel is one value every strip
  shares, and the flag only flipped the direction distance was measured
  in, which mirrors a shape where it stands. That is visible solely on a
  shape with a tail, which is why Comet was the one place it showed.
  Odd strips now run the journey backwards. CrossSweep had never worked.
- **Jitter was clocked against nothing.** It re-rolled four times a beat
  on a grid of its own, so it could never coincide with a flash and had
  no job but smearing whatever stood there. It re-rolls once per swell
  now, at the darkest point of the cycle, which turns it into "the shape
  appears somewhere new each time" — a look that could not be built out
  of fan and speed, and the first argument anyone has made for keeping
  the control.

**Fan's name is wrong.** It reads as "vertical offset between strips" and
it is really a per-strip phase offset applied to everything cyclic,
travel and pulse alike. That it turns a pulse into a chase is recorded
above as evidence the decomposition is real; it is also the part nobody
guesses from the word.

### What the wall found, 2026-09-22

Three faults with one cause, found while dialing looks chosen for not
being in the roster. Two parts of the code disagreed about what a
position is: travel under bounce was measured along the whole strip,
while fan was added as a shift inside a cell. At count 1 those are the
same thing, and count 1 is where bounce had been judged.

- **Bounce only worked at count 1.** Above it the journey collapsed into
  a single cell, so the shapes slid through their cells and re-entered at
  the far side, and whichever image straddled the strip's end was clipped
  — one block appearing to bounce while the others wrapped.
- **All five strips turned on the same frame.** The swing was computed
  once for the wall, so fan had nothing to stagger.
- **Fan shortened a strip at full width.** A displaced core stands past
  the strip's end, where the rule that stops a fade crossing a boundary
  removes the overhang. At width 100 % and fan 100 % the five strips lit
  45, 36, 27, 27 and 36 of their 45 pixels.

The core now swings inside its own cell, and fan offsets where a strip
stands in its own swing. Count 1 is unchanged; above it, bounce is a row
of blocks each turning in its own cell.

**Nothing visible was given up by confining it.** Every shape is an image
of one shape, so they move in lockstep and can never pass one another,
and sliding a field of evenly spaced identical shapes by exactly one cell
reproduces the picture pixel for pixel. Travel beyond one cell was never
visible, under bounce or wrap. Shapes that cross on one strip need a
second field running its own count and speed, which is a second instance
of the machine rather than another parameter.

**Bounce speed now matches the dial.** The rate comes from the cell and
the core's own width, so the core crosses the wall at the pixels per beat
it claims. It ran slow by a factor of `1 − width`, which is visible on a
wide shape at count 1.

### The tail is history, not geometry, 2026-09-22

A tail is where the core has **been**, not a shape hung off it. While travel
runs one way the two are the same number — how far behind the core a point
lies, and how long ago the core was there — which is why a single signed
offset served for both. They come apart only where the core turns.

Drawn as geometry, which side trails is decided by the direction of travel,
so at a turn the whole trail changes sides in one frame. In the preview that
is a jump of 157 of 255 on a single pixel between consecutive frames, against
28 for the same shape mid-travel. On the wall it reads as the shape flinching
away from the end.

Under bounce the core's position is a triangle, so "when was the core last
here" has a closed form: every point on the swing is crossed exactly twice a
cycle, going up and coming down, and the more recent crossing is the one whose
trail is still lying there. The distance is the path the core walked in that
time, which folds the trail back on itself rather than moving it. The core
then walks back out through what it laid down, and for a moment there is trail
on both sides of it — the old one fading where it lies, the fresh one growing
from the turn. The same jump measures 34 against 35 mid-travel, which is to
say there is no longer anything special about the turn.

This is how the hand-written rig did it, and it ran a year on stage.

Three things follow:

- **Under wrap nothing changes.** Travel is monotonic there, so the path and
  the straight offset are the same number. Six wrap cases — one shape, six
  shapes, alternate, fanned, still, and with the color pushes open — render
  pixel for pixel identically.
- **A trail cannot leave its cell under bounce.** The core never does, so its
  history cannot either. A geometric tail spilled into the neighboring cell;
  a folded one has nowhere to spill to.
- **The color layer's shape ruler follows it.** The ruler's trailing half is
  taken from the same measure as the tail's brightness, or color along a tail
  paints where the tail is not.

**A still shape keeps its tail**, which was expected to be the price and is
not. Bounce with no travel falls through to the same path wrap uses, where the
two measures agree, so a static lopsided shape is still reachable.

**What it approximates.** The trail's length is computed from the current
speed rather than the speed it was laid down at, so sweeping speed stretches
and squashes the trail already lying there instead of leaving history where it
fell. Invisible unless speed is swept hard.

### Turning bounce on leaves the shape where it stands, built 2026-09-22

The two modes read position out of the same travel phase differently — a
fraction of a cell under wrap, a triangle between the cell's two walls under
bounce — and the tracker that carries the phase across a rate change keeps the
**phase** continuous, not the position. So the switch teleported the shape by
about a quarter of a cell.

The phase is solved for at the flip instead: the one that stands the core where
it already stands, taken from the half of the triangle traveling the way the
shape already was, so it carries on and turns at the end it was heading for.
Measurements are in `DESIGN.md` § "Switches belong to the patch", which is also
where a switch moving only on arrival is argued.

Two things it cannot preserve, both geometry. **Bounce has nowhere to put a
core standing within half its own width of a cell wall**, since that is where
it turns, so a shape standing there is put out onto the wall — the bound on
what is left, and largest at the widths where the swing is shortest anyway.
And **fan offsets a different quantity in each mode**, so only the unfanned
strip can be solved for and the other four still move with fan up.

### Modulation, settled 2026-09-21

The pulse is a modulator, and the wander is a second one: a source, a set
of destinations, and an **amount** for each. That is the
synth pattern, and naming it makes a fourth word the two branches share,
alongside Form, Travel and Shape.

**The amounts live at the source.** Both arrangements exist on real
instruments — an amount beside the modulator saying where it goes, or an
amount beside each target saying what reaches it — and the choice is a
real one. It goes to the source here because most of Aurora's
destinations are not controls: the pulse's main target is how lit a pixel
is, and the wander's are what color it is, and neither is a slider
anywhere. They are the outputs of their branches. Putting amounts at the
target would mean inventing rows for things that are not controls, purely
to have somewhere to hang the amount. Width is the one exception, and
splitting one destination from the other two is worse than either
consistent choice.

The escape from the one thing this costs — you cannot see what is pushing
a given control — is a read-only marker beside the target showing that
the pulse reaches it, and how hard. Built in `tools/index.html`: Width,
Hue, PAR level, PAR hue offset and PAR saturation each carry one. The last
of those had no control to sit beside until the washes got a saturation of
their own, which was its own argument for building one.

**Built 2026-09-22: a fixed-amount matrix.** Six destinations, all of them
always present, each with an amount that may be zero and a wave of its own.
See § "Where the pulse reaches" above.

**Not deferred, rejected: patchable routing.** Every patch has to be a
valid morph destination from any live state, and a connection is either
made or not — so there is no halfway between "pulse to hue" and "pulse to
count", and a morph across two patches with different routing has to snap
the graph. That is the abrupt jump this whole experiment exists to
remove. Amounts interpolate; connections do not, and an amount rising
from zero is a connection fading in, which no patch cable can do.

A per-control version — every control with its own wave and timing rather
than one source fanned out — survives the morph objection, since all of
it is continuous numbers. What it does not survive is the parameter
count: roughly three parameters per modulatable control outgrows
one CC per parameter, so it drags in the patch-storage work first. It
wants to be wanted before it is built.

### Travel easing is a curve, not a modulation route

Easing — slow at the strip's two ends, fast through the middle, so a
shape reads as a ball thrown across the wall — looks like an LFO on
speed, and is not one.

A modulator doing that job would have to be phase-locked to the travel
cycle exactly: zero speed at each turn, maximum at mid-travel, every
time. The travel period is not something anyone dials. It falls out of
speed, count, and under bounce the width as well, since the turn comes
when the core's edge meets the end. Any rate a person can set will be
slightly wrong and will slide, which gives a wobble drifting through the
bounce rather than a bounce. Shaping the travel phase directly makes one
traversal one cycle by construction, with no rate to get wrong.

**It shapes the strip, not the cell.** At counts above one every shape
slows and speeds up together, so the whole field breathes across the
wall. Shaping the cell instead would have each shape easing inside its
own cell, which is a different look and not the one wanted.

So easing is one more knob in Travel, beside Speed and Fan — and it is
the third instance of a control the generator already has twice. Pulse
shape bends a swell from square to sine; a region's edge bends it from a
hard cell to a smooth fade; easing bends a traversal from linear to
slow-at-the-ends. Same idea every time, which is what makes **Shape** a
shared word rather than a coincidence.

### The color layer has no jitter

Jitter is a per-pixel random displacement re-rolled once per swell: it
breaks the regular grid the shape layer otherwise guarantees, and it is
among the most visible controls there is.

Nothing on the color side does that. Every push it can make is smooth, so
a grainy, boiling color is unreachable — a starfield in hue rather than
in brightness. Wanted as its own effect rather than folded into the
wander, which was deliberately given interference instead of randomness.
Whether it serves a band backdrop or is only a thing the machine could now
do is a judgment for a set, not a bench.

## Open

1. **Fan is a linear staircase, and the roster's Rain is a chevron.** Its
   per-strip offsets are 20, 28, 34, 28, 20 — symmetric, peaking on the
   center strip. The generator computes `fan × strip_index / 5`, which is
   purely linear and can only make a diagonal.

   The likely answer, worked out 2026-09-18 but not built: generalize to
   `amount × f(strip − center)` and make the **center** a parameter that
   may range beyond the five strips. A center at strip 2 gives the
   chevron; a center outside the span leaves you on one side of it only,
   which is a diagonal. So center alone interpolates between the two, and
   inverted chevrons come free. Two parameters cover the whole family.

   **A third shape is wanted: random.** Asked for on the wall 2026-09-21
   as "the strobing should not be on all strips at the same time, it
   should feel random". Fan is what decides per-strip timing, and a
   staircase can only ever be orderly. A per-strip offset drawn from the
   hash instead would cover it, which makes this one parameter with three
   settings rather than a separate control.

   **The same per-strip shape wants a second destination: rate.** Fan
   offsets *when* a strip runs — the same journey started at different
   times, locked together for ever. Offsetting *how fast* instead lets the
   strips drift apart and keep drifting, which is a family fan cannot
   reach at any setting. At a ratio of −1 on the odd strips it reproduces
   alternate exactly, so today's look survives as an endpoint and
   everything between that and "all five together" is new — the midpoint
   being the odd strips standing still while the even ones run. It is
   nearly free once fan takes a shape and a center, because it is the same
   function aimed at a different number, which is the argument for doing
   the two in one go rather than one of them now.

   What it costs is that a patch stops looking like one thing. Strips at
   different rates never come back into step, so the wall is whatever the
   drift has accumulated since you arrived, and the same patch reached
   twice does not look the same. That is not automatically an objection —
   the color layer's wander puts its two rates at the golden ratio to buy
   exactly this, and it was judged the best thing the field it replaced
   could do. So it is settle-by-looking. A per-patch **drift reset** — on
   entering the patch, and optionally again every N beats — would hand
   that choice to the patch, and is the first thing to try if the wall
   says "no longer a pattern" rather than "alive".
2. **Is a strip a loop or a line?** Answered for bounce, still open for
   wrap. Under bounce a shape turns where its own edge meets its cell's
   boundary, so nothing crosses a boundary at all and a strip is a line —
   at counts above one, a row of short lines.
   Wrapping travel is still a loop, and a tail falling off one end still
   reappears at the other. Whether that wants clipping, a boundary fade —
   which costs the ends of every pattern that ought to reach them — or
   nothing at all is a judgment for the wall.
3. **Jitter has one scale, and it is the wrong one for solid shapes.**
   The noise is keyed on the pixel — `hash8(strip, pixel, bucket)` — so
   every pixel gets its own displacement and its own level, which is dirt
   on the picture. Asked for on the wall 2026-09-21: solid blocks, whole
   and full on, appearing in random places rather than scattered pixels
   and loose clumps.

   Keying the same two effects on the **cell** instead would give that —
   a block displaced intact, or killed intact — and it is close to a
   change of which index goes into the hash rather than a new concept. So
   jitter probably wants a scale, pixel through to cell, rather than a
   second control.

   **Jitter also has only one rate, and it is the pulse's.** Re-rolling
   once per swell is what makes a flashing shape land somewhere new each
   time, but it leaves Starfield unable to twinkle: at its anchor the
   swell is `16 × 0.5^(20/127 × 6)` ≈ 8.3 beats, so the wall jumps between
   random arrangements about twice a bar. Winding the rate up cannot fix
   it, because that is the same knob driving the brightness flash. Found
   on the wall 2026-09-21.

   Together with the random fan shape above this is the "chaotic strobe"
   that could not be built out of fan, speed and pulse. Both halves are
   randomness at a scale the machine does not currently have, which is
   why they are worth doing in one go.
4. **Morph moves every parameter in lockstep and linearly.** That is the
   crudest possible path. The count half of it is answered: count's own
   fader is geometric now, so interpolating its CC linearly doubles by
   construction and the morph needs no special case. Per-parameter timing,
   the synth equivalent of giving each one its own envelope, is the bigger
   version and is not yet known to be needed.
5. **Where the pulse shape fader's midpoint should sit.** The sweep is
   linear, because the visible swelling tracks softness in proportion:
   measured as the fraction of a cycle the swell spends moving rather than
   pinned, half the visible travel lands at CC 93. Spread geometrically
   over the same range it landed at CC 117, with the whole middle of the
   fader reading as one flat square. Whether 93 is where it should sit is
   still a feel judgment nobody has made with music playing.
6. **Where the color layer's controls should stop.** Combinations that
   look bad are easy to reach — a hard edge with deep darkening and a wide
   hue swing is three strong things at once. Whether that wants narrower
   ranges or just practice is a judgment nobody has made with music
   playing. The test is a set, not a bench.
7. **Whether the roster survives at all**, or becomes a set of named
   points in this space. Nothing forces the choice yet.
8. **A region can only push inward, so one asked-for look is inverted.**
   "Base color on the center strip, the outer ones departing" needs a
   bump turned inside out — base within the core, the departure outside
   it. What comes out instead is the complement: the center strip
   departing and the outer ones on the base color.

   One boolean on the region covers it and is not a crutch, since "a
   place that differs" and "everywhere except a place" are both real
   descriptions. Not added, because the design conversation was careful
   about controls arriving without a look behind them, and this one has
   not been looked at on the wall yet.
9. **Nothing can make a chevron across the five strips.** Two properties
   of a per-strip offset are easy to confuse: where its zero sits, and
   what shape it makes. The color layer's wander has its zero on the
   center strip; the shape branch's fan has its zero on strip 1. Both are
   straight lines, and moving the zero of a straight line only slides it.
   A chevron needs a fold, which is what item 1 above is asking for, and
   neither branch has one.
10. **A second placed field was designed for and not built.** Both rulers
   at once — a strip painted with a gradient *and* shapes crossing it
   carrying their own — was agreed as the thing to leave until wanted,
   on the argument that the first one would be written so the second cost
   almost nothing.

   It was not, and as of 2026-09-22 it is. Everything one field is —
   which primitive, which ruler, its three reaches, its count, width,
   edge, drift rate and the phase tracker that drift runs on — travels
   together as one `PlacedField`, and every function that reads a field
   takes it as an argument. A second field is a second instance and a
   second block of CCs; what is still unanswered is what the two sum to,
   since two fields pushing the same three qualities can cancel.
## Tools

- `tools/preview.js` — the wall on screen: five strips and four PARs,
  rendered from a port of `P_Generator.cpp` and `dmx_out.cpp`. The color
  layer was designed here before it was flashed, and the constants in the
  two are meant to stay identical. It diverges deliberately in two places,
  both written at the top of the file.

  Travel's phase is the one thing it cannot match. `trackedPhase` carries
  an offset across every rate change so a fader never makes a shape jump,
  and nothing resets it — the brain's offset comes from its history since
  boot, the page's from load. Speed, spacing and the relationship between
  strips compare; where a traveling shape *is* does not.

  The **pulse's** phase does compare, which is the point of anchoring it.
  Both sides ease their offset back onto the grid, and the panel's clock
  button sends a transport start and restarts the page's beat zero
  together, so the bar a swell lands on is the same bar in both.
- `tools/index.html` — sliders over Web MIDI, patch save and recall, the
  morph control, the roster as one-click starting points, and a row of
  color looks that set the color layer only.

  **Serve it rather than opening the file.** `python3 -m http.server` from
  `tools/`, then `http://localhost:8000/`. A `file://` page gets a
  throwaway origin on every load, so Chrome has nothing to attach the Web
  MIDI permission to and asks again every time; `localhost` is a real
  origin and a secure context, and the grant sticks. Chrome or Edge only —
  Safari and Firefox have no Web MIDI. Reload after every flash, as that
  resets the Teensy's USB and the page keeps a dead port.
- **PC 11 paints each strip a flat color** — red, orange, green, cyan,
  blue in data-chain order — for working out strip order while rigging.
