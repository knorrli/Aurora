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
| 76 | Jitter | Randomness in position and brightness | 0–100 % |
| 77 | Pulse depth | How hard the brightness swells | 0–100 % |
| 78 | Pulse rate | How long one swell takes | 16 → 0.25 beats |
| 79 | Flags | Bit 0: odd strips run against the even ones. Bit 1: reverse at the strip end instead of wrapping | — |
| 80 | Pulse shape | Hard on/off square through to smooth sine | — |

Colour stays where it already was, on hue, saturation and brightness, and sits
downstream of all of this.

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

An earlier version had it drive width as well. Because a shape shrinks
toward its head, and the head sits at one end of the strip when nothing is
travelling, a swell read as a fill creeping in from that end. A fast swell
read as a travelling wipe rather than a flash, which made a strobe
unreachable.

Brightness only. Spatial growth is still available by hand on the width
control, and a **modulation destination** — letting the pulse be routed to
width, or hue, or speed — is the obvious extension when one is wanted.

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

### What this does to "transition"

A transition stops being a kind of object. It is two patches and a number
between them, and the number can be driven by a hand, by the clock over N
bars, or jumped instantly for a cut.

That dissolves a question that had been open and blocking for weeks —
whether a transition is a scene, an effect, or a third kind of thing —
rather than answering it.

## The colour field, added 2026-09-19

A second field, sampled per pixel and applied to whatever the shape above
has lit. At zero depth it does nothing and the wall is one flat colour.

It exists because Plasma and Aurora turned out to be **the same three
lines** — take a number from where you are and when it is, add it to the
hue — separated only by where the number comes from and by four constants
each had hardcoded. Those constants are now the parameters.

| CC | Control | Meaning |
|----|---------|---------|
| 23 | Speed | how fast the field moves. Bipolar; centre is frozen |
| 24 | Hue depth | how far hue swings from the fader's centre |
| 25 | Count | how many blobs along a strip, 0.35–17 |
| 26 | Fan | how far the five strips differ |
| 27 | Source | stacked sines through to Perlin noise |
| 28 | To white | saturation falls where the field is high |
| 29 | To dark | brightness falls where the field is low |
| 90 | Edge | hard-edged regions through to a smooth ramp |

What the wall settled, with the detail in `docs/bench-facts.md`:

- **To white works and is the safe one.** Desaturation happens at full
  brightness, where the LED has all its resolution. Red at about half
  depth was judged a usable backdrop.
- **To dark needed both a floor and an edge to be worth having.** It
  reaches 2 % rather than 0, on a geometric taper so the whole knob does
  something, and it only became legible once the field could be
  steepened.
- **Edge is what made the field readable at all.** Same problem and same
  fix as the pulse's shape control.
- **Source does not earn its place.** No perceptible difference of
  character at this resolution.

Three of the nine palettes in `docs/visual-design.md` — Ember, Deep, and
what Two-pole implies — are defined by brightness falling at one end, and
therefore have the hardware problem above. Haze, which falls toward white
instead, is the one with evidence behind it.

### Settled 2026-09-21: two branches, one vocabulary

The field's controls were each arrived at separately, because the wall
asked for them, and every one landed on a parameter the shape generator
already has. They were called Grain, Drift and Spread; they are Count,
Speed and Fan, and they now carry those names in the protocol, the
firmware and the bench panel. Count is the same *unit* in both branches —
blobs along one strip against shapes along one strip — so a number
carries across.

What this is **not** is one machine with a routing control. The shape
branch decides whether a pixel is lit at all, which is what makes gaps
and darkness; the field only ever modifies a pixel the shape has already
lit, and at zero depth the wall is one flat colour. That is a fixed order
in a chain, not two destinations off one source. Collapsing them would
permit routing the field to lit-or-not, which is the shape branch again
with fewer controls.

So: two branches from the same beat, meeting exactly once — the last
statement of `Generator()` multiplies the shape's brightness by the
field's colour. What is shared is the vocabulary, not the machine.

Two things follow from drawing it that way, and both are in
`tools/index.html`:

- **The two branches do not reach the same lights.** A PAR is one pixel,
  so the shape branch cannot reach it — count, width, edge, tail, speed
  and fan all describe positions along a strip. The colour branch is
  exactly what a one-pixel fixture can render, which is why the PARs'
  level and hue offset are a relationship to the strips rather than a
  second look. See `DESIGN.md` § "The PAR cans".
- **The pulse is the exception that crosses.** It sits in the shape
  branch, but it is the one shape-side thing a PAR can show, so it is
  drawn as a send rather than as part of the branch.

### Modulation, settled 2026-09-21

The pulse is a modulator, and the field's Depth group is a second one: a
source, a set of destinations, and an **amount** for each. That is the
synth pattern, and naming it makes a fourth word the two branches share,
alongside Form, Travel and Shape.

**The amounts live at the source.** Both arrangements exist on real
instruments — an amount beside the modulator saying where it goes, or an
amount beside each target saying what reaches it — and the choice is a
real one. It goes to the source here because most of Aurora's
destinations are not controls: the pulse's main target is how lit a pixel
is, and the field's are what colour it is, and neither is a slider
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
shape bends a swell from square to sine; field edge bends the field from
hard regions to a smooth ramp; easing bends a traversal from linear to
slow-at-the-ends. Same idea every time, which is what makes **Shape** a
shared word rather than a coincidence.

### Jitter and Source are not the same control

Both look like randomness and only one is. Jitter is a per-pixel random
displacement re-rolled four times a beat — it boils, and it breaks the
regular grid the shape layer otherwise guarantees. Source blends the
field between stacked sines and Perlin noise, and noise is smooth,
continuous and fully deterministic; what changes across that control is
whether the variation is periodic, not whether it is random. The evidence
agrees: Source was judged to make no perceptible difference, while Jitter
is among the most visible controls there is. One idea at two strengths
would not do that.

What the comparison does turn up is a hole. **The colour branch has no
jitter** — the field can only ever be smooth, so a grainy, boiling colour
is unreachable. Whether that serves a band backdrop or is only a thing
the machine could now do is a judgement for a set, not a bench.

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
2. **Is a strip a loop or a line?** Answered for bounce, still open for
   wrap. Bounce now turns when the core's own edge meets the strip end, so
   under bounce a strip is a line and nothing crosses a boundary at all.
   Wrapping travel is still a loop, and a tail falling off one end still
   reappears at the other. Whether that wants clipping, a boundary fade —
   which costs the ends of every pattern that ought to reach them — or
   nothing at all is a judgement for the wall.
3. **Morph moves every parameter in lockstep and linearly.** That is the
   crudest possible path. Count in particular probably wants to double
   rather than add — 1, 2, 4, 8, 16 — since half the travel is currently
   spent between 9 and 17 where it barely reads. Per-parameter timing,
   the synth equivalent of giving each one its own envelope, is the bigger
   version and is not yet known to be needed.
4. **The pulse shape taper** was spread geometrically across the fader on
   a guess. Where the midpoint should sit is a feel judgement nobody has
   made with music playing.
5. **Where the field's controls should stop.** Combinations that look bad
   are easy to reach — a hard edge with deep darkening and a wide hue
   swing is three strong things at once. Whether that wants narrower
   ranges or just practice is a judgement nobody has made with music
   playing. The test is a set, not a bench.
6. **Whether the roster survives at all**, or becomes a set of named
   points in this space. Nothing forces the choice yet.

## Tools

- `tools/index.html` — sliders over Web MIDI, patch save and recall, the
  morph control, and the roster as one-click starting points. Serve it
  with `python3 -m http.server` from `tools/` and open it in Chrome or
  Edge; Safari and Firefox have no Web MIDI. Reload after every flash, as
  that resets the Teensy's USB and the page keeps a dead port.
- **PC 11 paints each strip a flat colour** — red, orange, green, cyan,
  blue in data-chain order — for working out strip order while rigging.
