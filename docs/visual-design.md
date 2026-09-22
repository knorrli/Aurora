# Aurora — visual design

What the lights actually do: the pattern roster, the palette system, the
energy axis, and how the washes relate to the strips.

This survives the interaction-model reset intact. It describes the
material; `DESIGN.md` is working out how a performer handles it. Where
this file names a control, treat that as provisional — the *look* is the
settled part.

---

> **The roster may not stay the unit of choice.** An experiment on
> 2026-09-18 reached most of these looks, and everything between them,
> from one parametric generator — see `docs/generator.md`. Nothing is
> decided, and every pattern below still exists and still runs. But the
> nine slots are no longer the only way to get at this material, and the
> pattern pairs below turned out to be evidence for the generator rather
> than a fact about how patterns have to be organized.

## The pattern roster

Nine slots organized by intensity, each with a default and a variant:

| Row | Slot 1 | Slot 2 | Slot 3 |
|---|---|---|---|
| **1 — ambient** | Fill / Starfield | Breathe / Wave | Plasma / Aurora |
| **2 — groove** | Pulse / Bars | Sweep / Rain | CrossSweep / ? |
| **3 — intensity** | Chase / Comet | Strobe / Stutter | Chaos / Glitch |

Retired patterns — XFill, RisingBlocks/Stars, FallingBlocks/Stars,
Invert, RainBounce, StripByStripRandom, FillStars — are parked in
`P_Retired.cpp` under `#if 0`, at zero flash and SRAM cost.

**Storm is retired**, decided 2026-09-05 at the bench. Its rain half is
identical to Rain — both call the shared helper in wrapping mode — so
its only distinguishing feature was a full-wall white flash on a
12.5 %-per-beat dice roll. That duplicates a white-flash accent, fired
at random instead of played. It was also wrong for its slot: Rain is for
downtempo and calm sections, and random flashes across a calm wall read
as jarring rather than atmospheric.

Auto-firing lightning is still a reasonable thing to want, since a
performer with both hands busy cannot play it. If it returns it belongs
as a rate parameter on an existing pattern, not as a slot of its own.

## Each pattern has one continuous variation axis

The default and the variant are two ends of a single number for seven of
the nine pairs, not a crossfade between two renderers:

| Pair | What the number physically changes |
|---|---|
| Fill → Starfield | Gaps open between lit pixels; twinkle depth rises from zero |
| Breathe → Wave | How far the phase spreads along the strip. At zero the wall breathes in unison; wound up, the breath becomes a traveling wave |
| Plasma → Aurora | Mixes the two hue sources — stacked sines into Perlin noise |
| Pulse → Bars | A crossfade. The one pair with nothing structural in common |
| Sweep → Rain | How far the strips run out of step, and the tail fade with it. At zero, hard-edged blocks scrolling in lockstep; wound up, staggered comets |
| Chase → Comet | Tail length shrinks from a whole strip to a comet's tail |
| Strobe → Stutter | How much of the wall each flash covers, from all of it down to half. The alternating half only becomes visible as you wind it in |
| Chaos → Glitch | Blocks shrink, update faster, and white creeps in |

**In the generator, Rain and Comet arrive at the same look.** Found on
screen 2026-09-21 with `tools/preview.js`. Their anchor settings differ by
four nudges — width 40 to 30, edge 18 to 30, tail 74 to 99, fan 90 to 127
— and nothing changes in kind. The cause is the fan: the offsets below are
a chevron, and the generator's fan is linear, so it can only make a
diagonal, and a diagonal with a tail is what Comet already is. Rain did
not survive the move into the generator; it arrived as Comet. The fix is
`docs/generator.md` § Open, "Fan is a linear staircase".

**Rain is mechanically Sweep fanned out.** Both scroll a block along
every strip, same direction, same speed, same four steps per beat. Sweep
holds every strip at the same position; Rain holds them at fixed
per-strip offsets of 20, 28, 34, 28, 20 and gives each block a fading
tail. Nothing else separates them — which is what makes the axis a
single number, and the same shape as Breathe → Wave.

**CrossSweep has no partner, and that is correct.** What makes it
interesting is that direction alternates strip by strip, and direction
has no midpoint — half of "runs opposite" is not a state. Phase offset
has a midpoint; direction does not. So it holds a slot in its own right
and its variant is free to vary anything continuous: tail length, block
length, a speed difference between the two groups. **Which one is still
open.**

**Chase → Comet is the one pair describing a relationship *between*
strips** rather than something each strip does alone, which makes it the
risky one if the axis is ever applied per strip. Watching it made this
harder rather than easier: the two ends differ in tail length (45 px vs
15), speed (45 vs 6 px per beat) *and* strip order (3,4,5,1,2 vs
1,3,5,2,4). Tail and speed blend fine on one knob; the two orders have
no midpoint, so one of them would have to be used at both ends. Comet
earns its slot on looks alone — whether the *pair* survives is open.

## Palettes are shapes, not colors

> **None of this is built as a palette**, and most of it no longer needs
> to be. The color layer in `docs/generator.md` decides color from where
> a pixel is *and* from how lit it is, which is the split these nine were
> divided along — Ember, Deep and Two-pole are brightness-driven, Haze and
> Banded positional. Both are now reachable, and reachable together. What
> is still missing is the naming: these are settings of that layer, not a
> roster it has to grow. Read the nine as looks to dial rather than as
> code to write.


A library of "a blue one, a green one, an orange one" would be nine ways
of duplicating the hue control we already have. What a palette carries
is everything hue cannot: how wide the spread is, what shape it travels,
whether brightness and saturation move along with the hue, and whether
it is a smooth gradient or hard steps.

They are stored as **offsets from a center** — hue offset, saturation and
value per entry — rather than as absolute colors, so rotating one costs
nothing at sample time. A plain `CRGBPalette16` is therefore the wrong
container.

Names describe behavior, not scenery. Calling one "Lava" would be a lie
the moment it is rotated to blue.

| # | Name | What it does |
|---|------|--------------|
| 1 | **Flat** | No variation at all. Monochrome, as a palette something can commit to |
| 2 | **Narrow** | About ±15° around the center, full saturation, even brightness. One color with depth. The everyday one |
| 3 | **Wide** | About ±60°. A real gradient, still one family — blue through purple into magenta, wherever it is placed |
| 4 | **Two-pole** | The center hue and its opposite, transitioning fast rather than blending through the muddy middle |
| 5 | **Ember** | Hue barely moves; brightness and saturation do. Dark and deep at one end, bright and near-white at the other. Gives comet tails and rain trails real color instead of just dimming |
| 6 | **Haze** | Saturation falls away toward white while brightness stays up. Airy and pale — made for the ambient row |
| 7 | **Deep** | Full saturation throughout, brightness falling to near-dark at one end. The opposite move to Ember |
| 8 | **Banded** | Four hard steps instead of a smooth ramp. Reads as stripes and blocks — for Bars, chase and moving blocks, where a gradient turns to mush at speed |
| 9 | **Spark** | Mostly the base color with a small hot accent of the opposite hue. Pops and glints without becoming a rainbow |

Two rules, both chosen for simplicity and both easy to revisit:

- **Spread scales everything** — hue, saturation and value deviation
  together. One rule: it is "how far from flat". The consequence to
  watch is that Ember and Deep lose their dark ends at low spread, which
  is either correct or annoying depending on how they read on the wall.
- **All nine rotate with hue.** None are anchored. That is what makes
  them shapes rather than colors; the cost is that Ember placed on blue
  is a cold thing that no longer reads as fire. Add an anchor flag only
  if one turns out to need it.

At zero spread everything collapses to single-color behavior, so the
clean monochrome look is preserved as one end of a knob.

*Considered and left out:* a **Triad** — center plus ±120° — as a
deliberately loud option for a peak. It sits closest to the
rainbow-across-the-stage look we are trying to avoid, and covers similar
ground to Wide more aggressively. It is the obvious tenth if one is
wanted.

## The energy axis

**The problem.** Aurora expresses intensity by *switching pattern* — the
three rows are a discrete ladder climbed by picking a slot. Music does
not build in three steps. There is no number anywhere in the system
meaning "the wall is at 30 % right now", and no way to move it to 80 %
over eight bars.

**Energy is one value, 0–255, that every pattern reads and interprets in
its own terms** — density, height, speed, tail length, flash
probability, brightness. Nine patterns each covering a range rather than
sitting at a point.

This is not the same problem as "nothing evolves". Slow autonomous
variation solves *looping*; energy solves *dynamics*. The wall varying
on its own does not let the performer play it. Both are wanted.

Three sources feed it, and they compose:

- **Hand** — sets the base value.
- **Foot** — ramps it over N beats and releases back. A build played
  without hands.
- **Ear** — the mic envelope, as an offset.

**The discipline is that audio drives energy only** — never color,
never pattern. That is the difference between lights running during a
song and lights playing it, without the flickering-visualizer look.

Brightness stops being a live performer control and becomes a soundcheck
trim, because absolute brightness is set once per room and energy scales
it anyway. Brightness is the crudest version of the thing actually
wanted.

**Untested.** Nothing about energy has been built or seen. How it splits
between hand, foot and ear is unknown.

## Idle richness

Three cheap tricks, none individually noticeable, together making "just
let it run" feel breathing rather than looping:

1. **Palette animation** — very slow hue drift on the whole palette.
   Replaces the current hue-oscillation alt-mode, which is too strong to
   leave on.
2. **Per-strip micro-offsets** — each strip carries a small hue offset,
   about ±5°. Reads as depth, not as different colors.
3. **Breath on brightness** — an 8–16-beat LFO adds a barely-perceptible
   inhale and exhale.

## The washes

Four BeamZ BCC145 of our own, driven over DMX in 8-channel mode.

**They stopped being a color echo** on 2026-09-06. Driving them at the
strips' own hue makes the rig read as one light source; the single change
that most stops that is holding them a half turn off the strips. Worth
more than it looks on paper.

**Keeping them from overpowering the strips**, in order of effect:

1. **Aim them off the strips.** A wash falling on the surface the strips
   are mounted on lights the background of the graphic and collapses its
   contrast. Point them at the band, across the stage, or at the
   audience. Free, and most of the problem.
2. **Set a ceiling by eye, once, in the room.** A per-fixture master
   scale at soundcheck so washes-at-full read as equal *weight* to
   strips-at-full, never exceeded afterwards. A separate number from the
   RGBW trims: those correct hue, this one caps authority.
3. **Different color, not the same color.** Two fixtures on one hue
   means the brighter wins and the dimmer disappears. Strips saturated,
   washes low and desaturated or complementary, and they read as two
   layers rather than one thing plus glare.
4. **Split the energy range.** Washes own the bottom, strips own the
   top: low energy is a warm room with the strips ticking over, and as
   energy climbs the strips take over while the washes retreat to
   accents. They never both peak, so overpowering cannot happen by
   construction — and the rig changes character as it builds rather than
   just getting brighter.
5. **Dark is a state.** The most effective thing a wash does is usually
   be off until it matters.

**Three effects observed** the first time strips and one BCC145 ran
together, all of them *relationships between two light sources* rather
than one adding to the other:

- A soft, dim, complementary wash against saturated strips.
- Alternating quickly between broad wash and strip accents.
- The strips holding a fixed color or white while the wash changes
  underneath them.

The third is the interesting one: the color performance moves off the
strips entirely and they become pure shape. It is also the cheapest
source of variety over a long set, because a wash changing color under
a running pattern re-reads the whole wall without touching the pattern.

**Open — how far the washes may carry a look.** An earlier rule said
every wash contribution must be additive and the strips must carry every
look alone, so that songs stay portable to rooms where the washes cannot
be rigged. That rule forbids all three effects above. The counter-case:
the fixtures are ours and the rooms are the same rooms, so the realistic
failure is "one died" or "no time to rig them", which argues for
degrading by count rather than to zero. Unresolved, and it needs the
fixtures and the strips lit together.

## Open visual questions

Each of these needs looking at, not arguing about.

1. **What CrossSweep's variant should be.**
2. **Whether Stutter's halves should alternate per strip.** Tried
   2026-09-05 against the all-strips-in-sync version and preferred: odd
   strips take the opposite half from even ones, so the wall reads as a
   checkerboard inverting on the beat rather than one horizontal line
   sliding up and down. The in-sync version was judged too static.
   Sweeping coverage from full down to half confirmed the axis behaves —
   at full coverage the alternation is invisible, and as it winds down
   dark wedges enter from opposite ends on neighboring strips until the
   permanently-lit middle band vanishes. Needs a second look at proper
   viewing distance before it replaces current behavior.
3. **Strobe's duty cycle.** Currently a flash of a quarter of the beat,
   clamped to 20–200 ms, so 125 ms at 120 BPM. A shorter flash reads as
   more percussive, a longer one as more of a pulse. One constant, and
   only the wall can settle it.
4. **Glitch's white share.** Reviewed 2026-09-09 on a clean data link
   and it holds up — 12 pixels per frame reads as a dense fast shimmer
   rather than countable dots, and it works across the whole color
   wheel, best between cyan and magenta. But the 30 % white share cannot
   be judged until the V-fader defect in `docs/bench-facts.md` is fixed.
5. **Whether a per-strip variation axis reads as an effect or a fault**
   on Chase → Comet.
6. **What the washes do per pattern** — follow, antiphase, step across
   on the beat, hold dark, flash only. The vocabulary itself is open,
   and it needs the fixtures and strips lit together.
7. **Whether the sub-pixel glide is smooth enough at real viewing
   distance.**
