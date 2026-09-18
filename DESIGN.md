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
