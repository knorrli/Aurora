# Aurora — the interaction model

Empty on purpose. Everything that was here was cleared on 2026-09-18 and
is recoverable at commit `33f2d1f`.

What was cleared: accumulated decisions about how a performer drives
Aurora — scenes, accents, transitions, pedal verbs, song structure. They
had grown to contradict each other and were constraining the design more
than informing it.

What was kept, and is not up for redesign here:

- `docs/bench-facts.md` — what we measured and observed. Facts.
- `docs/architecture.md` — how the system is built and why. Settled.
- `docs/visual-design.md` — patterns, palettes, energy, washes. Settled.
- `docs/wiring.md` — pins and circuits.

---

## How this document gets rebuilt

Top down, one layer at a time. Nothing below opens until the layer above
is written here as a sentence worth defending.

1. **Purpose.** What Aurora is for, and what it is explicitly not for.
2. **Constraints.** The facts no design may violate.
3. **Nouns.** What objects exist at all.
4. **Verbs.** What can happen to those objects, and what drives it.
5. **Surfaces.** Which control touches which verb.

## What the bench changed, 2026-09-18

The same day this document was cleared, an experiment ran that changes
what the layers below are about. It is written up in
`docs/generator.md`; the part that matters here is the result.

**A pattern stopped being a fixed thing you choose and became a point in
a continuous space you can move through.** One parametric generator
reaches most of the roster and everything between, and sliding from one
setting to another was judged on the wall to genuinely work.

Two things fall out of that, and they land directly on the layers:

- **A transition is not a kind of object.** It is two points and a number
  between them, and that number can be driven by a hand, by the clock over
  N bars, or jumped for a cut. The question of whether a transition is a
  scene, an effect, or a third noun does not need answering — it stops
  being a question.
- **There may be only one saved noun.** If a look is a set of parameter
  values and a transition is a path between two of them, then layer 3 has
  one object and one pointer rather than three kinds of thing.

Neither is settled here. Layer 3 still has to be worked through properly,
and the generator is an experiment rather than a commitment. But the
layers should be built knowing this is available, because the old model —
nine finished looks and a colour knob — is what made "what is a scene"
unanswerable in the first place.

## Parked: what the numpad is for — 2026-09-18

Layer-5 material that arrived early, recorded so it is not lost and
deliberately **not** promoted to a decision. The layers above it still
have to be worked through first.

The generator has more parameters than the box has controls, and some of
them are families rather than single values — fan wants a centre and an
amount, pulse wants depth, rate and shape. Two ideas came up for what the
numpad could do about that, and they turn out not to compete:

- **Page select.** The numpad picks which parameter group is live and the
  touchpad's two axes edit it. This is how a synth with more parameters
  than knobs has always worked, and it gives the numpad a real job — today
  it is the visual centre of the instrument and its least-used control.
- **Patch select.** The numpad picks one of nine saved patches and the pad
  morphs toward it, as far as you push. This builds directly on the morph
  result and needs no mode indicator at all.

**They belong to different activities**, which is why both can exist: page
select is for dialling a patch in, patch select is for playing. That split
is not offline versus onstage — at a party you would do both continuously
— and the A/B switch already exists to carry it.

Three things that need answering before any of it is real:

1. **The pad springs back, deliberately.** That is settled and
   well-reasoned: an absolute control that holds its value has to answer
   what happens when your thumb next lands somewhere else, and every
   answer is bad. But a pad that springs back cannot *set* anything. As an
   editor it has to hold, so spring-back probably becomes a property of
   which activity you are in rather than of the pad.
2. **Nothing shows which page is live.** No display, no wall visibility,
   and the numpad keys do not light. A mis-selected page means a gesture
   silently does something else, and unlike a synth you cannot hear the
   mistake.
3. **Two axes is not always two parameters.** Pulse is three. Either pages
   cap at two, or the faders become the rest of the page — which costs
   having hue and brightness always to hand.

## The rule that keeps this from circling

Every question gets sorted, the moment it appears, into one of two piles:

- **Settle by argument** — it follows from the purpose or the
  constraints, so reasoning can close it.
- **Settle by looking** — it is a judgement about how something reads on
  the wall, in a room, with music playing.

Questions in the second pile get **written down with the test that would
answer them, and then dropped.** They do not get argued, including when
the answer feels obvious. Feeling obvious is how untested guesses became
recommendations in the document this one replaces.
