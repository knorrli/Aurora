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
finished look, and the continuous controls only changed its colour. So
the discrete choice carried nearly all the identity and the faders
decorated it.

Three consequences, all of which were live complaints:

- Changing look meant a discrete jump, visually abrupt.
- You could only ever reach the looks someone had written.
- There was no way to stumble onto something good, which on a synth is
  half of what playing one is.

## The model

One machine: **a shape, repeated along each strip, optionally travelling,
with the five strips optionally run out of step with each other.**

Each strip is divided into `count` equal cells. Every cell contains the
same shape at the same position within it. The shape has a solid core and
fades that reach outward into the gap on either side.

That is the whole thing. Everything below is a parameter of it.

## Parameters

| CC | Control | Meaning | Range |
|----|---------|---------|-------|
| 70 | Width | The solid core, as a proportion of one cell | 0–100 % |
| 71 | Count | How many shapes along the strip | 1–20 |
| 72 | Edge | How far the glow reaches into the gap, both sides | 0–100 % of the gap |
| 73 | Tail | How far the trail reaches behind, into the gap | 0–100 % of the gap |
| 74 | Speed | Travel along the strip. Bipolar — centre is still, either side travels | ±60 px/beat |
| 75 | Fan | How far the five strips run out of step | 0–100 % of a cell |
| 76 | Jitter | Randomness in position and brightness, re-rolled once per swell | 0–100 % |
| 77 | Pulse depth | How hard the brightness swells | 0–100 % |
| 78 | Pulse rate | How long one swell takes | 16 → 0.25 beats |
| 79 | Flags | Bit 0: odd strips run against the even ones. Bit 1: reverse at the strip end instead of wrapping | — |
| 80 | Pulse shape | Hard on/off square through to smooth sine | — |

Colour is hue, whiteness and darkness, and sits downstream of all of this —
this branch decides whether a pixel is lit, never what colour it is.

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
limit: at 100 % the two neighbouring fades meet at zero, so they never
overlap and nothing has to be summed. **Edge at full always closes the
gaps, whatever the width is** — which is the behaviour anyone expects from
a softness control.

Consequence worth knowing: at width 100 % there is no gap left, so edge
and tail do nothing. That is consistent rather than broken.

### The pulse drives brightness only

Width was a second destination once and was taken out. A shape was
anchored by its head then, and the head sits at one end of the strip when
nothing is travelling, so a swell read as a fill creeping in from that
end and a fast swell read as a travelling wipe rather than a flash. That
put a strobe out of reach.

A shape is anchored by its centre now, which removes the cause: growing
from the middle is a breath outward rather than a wipe from one end. So
**width is wanted back as a pulse destination** — one multiply, and the
wall says within a minute whether the old failure is gone. Decided
2026-09-21, not built.

Spatial growth is meanwhile available by hand on the width control.

A sine can never produce an on/off edge no matter how deep it goes, which
is why pulse shape exists as a separate control.

## What it reaches

Thirteen of the roster, approximately, from nine knobs and two switches:

| Look | Roughly |
|---|---|
| Fill | width full, not moving |
| Sweep | a third width, hard edge, travelling, fan zero |
| Rain | the same with fan up and some tail |
| CrossSweep | Sweep with alternate direction |
| Bars | Sweep on bounce instead of wrap |
| Breathe | width full, not moving, pulse deep and slow, sine |
| Wave | the same with fan up |
| Chase | width full, pulse at maximum, fan at maximum |
| Comet | a narrow shape with a long tail, travelling, strips fanned |
| Starfield | tiny shapes, high count, jitter up |
| Strobe | width full, pulse at maximum, fast, square |
| Stutter | Strobe with alternate direction and some fan |
| Glitch | tiny, high count, jitter at maximum |

**Fan is doing three jobs at once** — it is the Sweep→Rain axis, the
Breathe→Wave axis, *and* the thing that turns a pulse into a chase across
the strips. The first two were already known to be one number; that the
third falls out of the same parameter is the strongest evidence the
decomposition is real rather than fitted after the fact.

**Plasma and Aurora are not in here and should not be.** They are a colour
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
- **Colour belongs inside the morph.** Hue, saturation and brightness were
  outside the model as designed; morphing them along with the shape turned
  out to be part of what makes it read as one gesture rather than a
  parameter sweep.
- **Count is genuinely new territory.** Nothing in the roster sits between
  one shape and a dozen, so three to six shapes is unexplored, and
  high count with a wide edge produces a continuous travelling ripple that
  no existing pattern can make.

### The panel's roster settings are guesses

The roster entries in `tools/index.html` were written by reasoning from
each look's description, not by dialling it in and comparing. Several
have since been found wrong on the wall. They are fine as starting
points to wander from, and they are **not** fit to morph between: a
morph between two wrong destinations says nothing about the morph.

Five or six looks dialled in by eye and saved as patches is what the
morph work needs underneath it, and nothing else about the morph is
worth judging until they exist.

### What this does to "transition"

A transition stops being a kind of object. It is two patches and a number
between them, and the number can be driven by a hand, by the clock over N
bars, or jumped instantly for a cut.

That dissolves a question that had been open and blocking for weeks —
whether a transition is a scene, an effect, or a third kind of thing —
rather than answering it.

## The colour layer, redesigned 2026-09-21

A colour is **hue, whiteness and darkness**. Everything else is a push on
those three, measured from the three faders, and the pushes add. With
every control centred the wall is exactly the colour on the faders — which
is what makes the dialling order work: set the colour flat, then open a
push and watch it depart from something you chose.

Three things push, and what separates them is what each is anchored to.

| Source | Anchored to |
|----|----|
| The placed field | a position you pick something to measure against |
| The wander | nothing — it is never in the same place twice |
| The light level | how lit the shape branch left that pixel |

The layer reads the **shape branch's** light level and never its own.
Feeding its own darkness back in would make colour depend on colour: pull
the wall down for a quiet verse and the hue would slide with it.

### What you place

Two primitives, either of them measured against one of three rulers.

- A **slide** runs one way across its ruler with the base colour at the
  centre. The amount is how far *one* end departs, so the two ends land
  twice that far apart.
- A **region** is a bump — base, departure, back to base — built from the
  shape branch's own core and fades. Count, width and edge therefore mean
  the same thing in both branches.

The ruler is **across the five strips**, **along a strip**, or **within a
shape** — a shape's leading tip through to the end of its tail, travelling
with it.

Six combinations, and every look asked for on the wall lands on exactly
one of them:

| Look | Primitive | Ruler |
|----|----|----|
| A rainbow across the five strips | slide | wall |
| One colour in the middle, mirrored outward | region | wall |
| Cyan through violet to pink up a strip | slide | strip |
| A hard green cell at the centre of a strip | region | strip |
| A bar with a red head and a green tail | slide | shape |
| A band across the middle of a shape | region | shape |

**Mirroring is a region, not a mode of the slide.** A fold flag on the
slide was nearly built and would have made the same look reachable two
ways with different controls. Dropping it, each primitive does one thing
and the table has no duplicate rows.

### The two sides of a shape are not the same length

A tail reaches far further than an edge fade, so the shape ruler
normalises its two sides separately: 0 at the leading tip, **0.5 at the
core's centre**, 1 at the end of the tail.

Normalising the whole span at once put the ruler's middle halfway between
the two tips, which with a long tail is well behind the core. A region
asked to sit at the middle of a shape then landed nowhere near the bright
part. Found on screen before any of it was flashed.

### What lives

Colour never quite the same in two places, with the difference always
moving. Three controls, each sayable in words before you turn it: **how
much** for each of the three qualities, **how fast**, and **how big** —
the whole wall moving as one, down through patches a strip-length across,
down to individual pixels shimmering.

**Never repeating is built in rather than dialled.** Two terms whose rates
sit at the golden ratio can never come back into step. An earlier attempt
put that on a control — how far apart the two speeds sit — which is making
the performer operate the mechanism rather than the look.

This is the one job the previous field did well and the redesign nearly
lost. Three superimposed waves looked right on the wall for a year and
were impossible to reason about; one wave could be reasoned about and
marched.

### Colour from the light level

Colour read off how lit a pixel already is, so a comet's tail cools
instead of only dimming. Anchored at the dim end: the faders are what a
fade runs out to, and the core is the departure. It reads the shape's own
profile, before jitter and the pulse.

**It is not made redundant by the shape ruler**, and the reason is worth
keeping. The pulse swells brightness with no spatial component at all, and
jitter scatters it at random; neither has a position for a ruler to
measure. Only this source reaches them. Point it at a flashing wall and
the wall goes hot as it flashes.

The two are also not interchangeable where they overlap. On a comet, hue
from the light level bunches the whole colour change into the few pixels
behind the core, because the tail is dim and nearly flat over most of its
length — there is almost nothing left for colour to follow. A slide within
a shape measures position instead and spreads evenly the whole way. So a
white head wants the light level, and colour along the tail wants the
slide:

| Want | Set |
|----|----|
| White head | light level → to white, full |
| Colour along the tail | slide, within a shape, hue |

### Why the previous field was replaced

Two things it could not do, both asked for on the wall, and both geometry:
hold a colour still somewhere, and put one colour on each strip.

It was a cloud with a count knob attached. Making it reach a flat floor so
it could place a colour is the same change that made Plasma and Aurora
read more regular — geometry was bought with aliveness, because one field
had to be both. **That is the fault the redesign fixes**, and it is why
there are two separate things here rather than one with more parameters.

Three smaller findings survive from it, and all three are built in above:
a colour must have a place of its own or the fader stops reading as a
thing that sets; darkening wants a geometric taper because it is a ratio
of light; and steepening an edge is what turns a general unevenness into
regions you can see.

### Two branches, one vocabulary

The shape branch decides whether a pixel is lit at all, which is what
makes gaps and darkness. The colour layer only ever decides what colour a
lit pixel is. That is a fixed order in a chain, not two destinations off
one source — collapsing them would permit routing colour to lit-or-not,
which is the shape branch again with fewer controls.

What is shared is the vocabulary. Count, width, edge and speed mean the
same thing in both, and count is the same *unit*: shapes along one strip
against regions along one ruler.

Two things follow from drawing it that way, and both are in
`tools/index.html`:

- **The two branches do not reach the same lights.** A PAR is one pixel,
  so the shape branch cannot reach it — count, width, edge, tail, speed
  and fan all describe positions along a strip. The colour layer is
  largely what a one-pixel fixture can render, which is why the PARs'
  level and hue offset are a relationship to the strips rather than a
  second look. See `DESIGN.md` § "The PAR cans". The shape ruler is the
  exception and does not reach them at all.
- **The pulse is the exception that crosses.** It sits in the shape
  branch, but it is the one shape-side thing a PAR can show, so it is
  drawn as a send rather than as part of the branch.

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
is, and the wander's are what colour it is, and neither is a slider
anywhere. They are the outputs of their branches. Putting amounts at the
target would mean inventing rows for things that are not controls, purely
to have somewhere to hang the amount. Width is the one exception, and
splitting one destination from the other two is worse than either
consistent choice.

The escape from the one thing this costs — you cannot see what is pushing
a given control — is a read-only marker beside the target showing that
the pulse reaches it, and how hard. It is also the destination-side UI
already half-built, should modulators ever multiply.

**Deferred: a fixed-amount matrix.** More destinations, all of them
always present, each with a bipolar amount that may be zero. Worth having
eventually; the pulse reaching hue, or count, is not reachable today.

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

### The colour layer has no jitter

Jitter is a per-pixel random displacement re-rolled once per swell: it
breaks the regular grid the shape layer otherwise guarantees, and it is
among the most visible controls there is.

Nothing on the colour side does that. Every push it can make is smooth, so
a grainy, boiling colour is unreachable — a starfield in hue rather than
in brightness. Wanted as its own effect rather than folded into the
wander, which was deliberately given interference instead of randomness.
Whether it serves a band backdrop or is only a thing the machine could now
do is a judgement for a set, not a bench.

## Open

1. **Fan is a linear staircase, and the roster's Rain is a chevron.** Its
   per-strip offsets are 20, 28, 34, 28, 20 — symmetric, peaking on the
   centre strip. The generator computes `fan × strip_index / 5`, which is
   purely linear and can only make a diagonal.

   The likely answer, worked out 2026-09-18 but not built: generalise to
   `amount × f(strip − centre)` and make the **centre** a parameter that
   may range beyond the five strips. A centre at strip 2 gives the
   chevron; a centre outside the span leaves you on one side of it only,
   which is a diagonal. So centre alone interpolates between the two, and
   inverted chevrons come free. Two parameters cover the whole family.

   **A third shape is wanted: random.** Asked for on the wall 2026-09-21
   as "the strobing should not be on all strips at the same time, it
   should feel random". Fan is what decides per-strip timing, and a
   staircase can only ever be orderly. A per-strip offset drawn from the
   hash instead would cover it, which makes this one parameter with three
   settings rather than a separate control.
2. **Is a strip a loop or a line?** Answered for bounce, still open for
   wrap. Bounce now turns when the core's own edge meets the strip end, so
   under bounce a strip is a line and nothing crosses a boundary at all.
   Wrapping travel is still a loop, and a tail falling off one end still
   reappears at the other. Whether that wants clipping, a boundary fade —
   which costs the ends of every pattern that ought to reach them — or
   nothing at all is a judgement for the wall.
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
   crudest possible path. Count in particular probably wants to double
   rather than add — 1, 2, 4, 8, 16 — since half the travel is currently
   spent between 9 and 17 where it barely reads. Per-parameter timing,
   the synth equivalent of giving each one its own envelope, is the bigger
   version and is not yet known to be needed.
5. **The pulse shape taper** was spread geometrically across the fader on
   a guess. Where the midpoint should sit is a feel judgement nobody has
   made with music playing.
6. **Where the colour layer's controls should stop.** Combinations that
   look bad are easy to reach — a hard edge with deep darkening and a wide
   hue swing is three strong things at once. Whether that wants narrower
   ranges or just practice is a judgement nobody has made with music
   playing. The test is a set, not a bench.
7. **Whether the roster survives at all**, or becomes a set of named
   points in this space. Nothing forces the choice yet.
8. **A region can only push inward, so one asked-for look is inverted.**
   "Base colour on the centre strip, the outer ones departing" needs a
   bump turned inside out — base within the core, the departure outside
   it. What comes out instead is the complement: the centre strip
   departing and the outer ones on the base colour.

   One boolean on the region covers it and is not a crutch, since "a
   place that differs" and "everywhere except a place" are both real
   descriptions. Not added, because the design conversation was careful
   about controls arriving without a look behind them, and this one has
   not been looked at on the wall yet.
9. **Nothing can make a chevron across the five strips.** Two properties
   of a per-strip offset are easy to confuse: where its zero sits, and
   what shape it makes. The colour layer's wander has its zero on the
   centre strip; the shape branch's fan has its zero on strip 1. Both are
   straight lines, and moving the zero of a straight line only slides it.
   A chevron needs a fold, which is what item 1 above is asking for, and
   neither branch has one.
10. **A second placed field was designed for and not built.** Both rulers
   at once — a strip painted with a slide *and* shapes crossing it
   carrying their own — was agreed as the thing to leave until wanted,
   on the argument that the first one would be written so the second cost
   almost nothing.

   It was not. `placedAt()` reads file statics rather than taking its
   parameters as an argument, so a second field is a small refactor
   before it is a feature. Doing that refactor is cheaper than the
   argument for deferring it implied it would be.
## Tools

- `tools/preview.js` — the wall on screen: five strips and four PARs,
  rendered from a port of `P_Generator.cpp` and `dmx_out.cpp`. The colour
  layer was designed here before it was flashed, and the constants in the
  two are meant to stay identical. It diverges deliberately in two places,
  both written at the top of the file.

  Phase is the one thing it cannot match. `trackedPhase` carries an offset
  across every rate change so a fader never makes a shape jump, and
  nothing resets it — the brain's offset comes from its history since
  boot, the page's from load. Speed, spacing and the relationship between
  strips compare; where a travelling shape *is* does not.
- `tools/index.html` — sliders over Web MIDI, patch save and recall, the
  morph control, the roster as one-click starting points, and a row of
  colour looks that set the colour layer only.

  **Serve it rather than opening the file.** `python3 -m http.server` from
  `tools/`, then `http://localhost:8000/`. A `file://` page gets a
  throwaway origin on every load, so Chrome has nothing to attach the Web
  MIDI permission to and asks again every time; `localhost` is a real
  origin and a secure context, and the grant sticks. Chrome or Edge only —
  Safari and Firefox have no Web MIDI. Reload after every flash, as that
  resets the Teensy's USB and the page keeps a dead port.
- **PC 11 paints each strip a flat colour** — red, orange, green, cyan,
  blue in data-chain order — for working out strip order while rigging.
