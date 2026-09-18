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

Colour stays where it already was, on hue, spread and brightness, and sits
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
- **Colour belongs inside the morph.** Hue, spread and brightness were
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

## Open

1. **Fan is a linear staircase, and the roster's Rain is a chevron.** Its
   per-strip offsets are 20, 28, 34, 28, 20 — symmetric, peaking on the
   centre strip. The generator can only make a diagonal. Wants its own
   control: a fan *shape*, from diagonal through to symmetric.
2. **Is a strip a loop or a line?** The maths treats it as a loop — the
   pattern repeats with a period of one cell, so at count 1 a tail falling
   off one end reappears at the other. Consistent, but not what a strip
   with two physical ends looks like, and it reads as wonky when a tail
   wraps. Real options: clip what crosses the boundary, soften the last
   few pixels at each end so things enter and leave, or use bounce, which
   has no boundary to cross. Each costs something — a boundary fade dims
   the ends of every pattern that ought to reach them. **Decide the model
   rather than patching the cases.**
3. **A shape anchors to its head, not its centre.** So a static shape sits
   at one end of the strip rather than the middle, and there is no way to
   make something expand outward from the centre.
4. **Morph moves every parameter in lockstep and linearly.** That is the
   crudest possible path. Count in particular probably wants to double
   rather than add — 1, 2, 4, 8, 16 — since half the travel is currently
   spent between 9 and 17 where it barely reads. Per-parameter timing,
   the synth equivalent of giving each one its own envelope, is the bigger
   version and is not yet known to be needed.
5. **The pulse shape taper** was spread geometrically across the fader on
   a guess. Where the midpoint should sit is a feel judgement nobody has
   made with music playing.
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
