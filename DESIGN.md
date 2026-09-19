# Aurora — the interaction model

How a performer drives Aurora. Everything that was here before was
cleared on 2026-09-18 and is recoverable at commit `33f2d1f`.

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

## What the bench changed, 2026-09-18

The same day this document was cleared, an experiment ran that changes
what everything below is about. It is written up in
`docs/generator.md`; the part that matters here is the result.

**A pattern stopped being a fixed thing you choose and became a point in
a continuous space you can move through.** One parametric generator
reaches most of the roster and everything between, and sliding from one
setting to another was judged on the wall to genuinely work.

Two things fall out of that:

- **A transition is not a kind of object.** It is two points and a number
  between them, and that number can be driven by a hand, by the clock over
  N bars, or jumped for a cut. The question of whether a transition is a
  scene, an effect, or a third noun does not need answering — it stops
  being a question.
- **There may be only one saved noun.** If a look is a set of parameter
  values and a transition is a path between two of them, then there is one
  saved object and one pointer rather than three kinds of thing.

Neither was settled that day; both were, on 2026-09-19, and the nouns
below are the result. The old model — nine finished looks and a colour
knob — is what made "what is a scene" unanswerable in the first place.

## The nouns, settled 2026-09-19

Arrived at from two days at the bench rather than deduced from first
principles. A five-layer scaffolding — purpose, constraints, nouns,
verbs, surfaces — was set up on 2026-09-18 to rebuild this document and
abandoned on 2026-09-19 with three of its five layers still blank. It had
done its one useful job, which was forcing the nouns to be settled before
the controls, and had become something to work around.

There are three saved things and one played thing.

### A patch

A set of parameter values, **plus the far end of each fader's morph
target**. "Intense Strobe" is not a separate patch; it is part of what
the Strobe patch is.

Patches live in the brain and are selected by number. Today they live in
the browser's local storage, which survives nothing.

### A morph target

A saved alternate set of values, belonging to one patch, that one fader
pulls toward. **Absolute** — the far end is a specific look you dialled
in and judged.

The test for whether something is a morph target: can you build the far
end and look at it? "This patch flat out" — yes. "This patch's other
colour" — no, because every colour is equally valid and there is no
particular one to save.

### A shift

A rule applied on top of whatever the patch says — the colour rotated so
far, the brightness trimmed. **Relative**, patch-independent, and nothing
is stored for it. Transpose, rather than a second saved arrangement.

The difference from a morph target only ever shows when the patch
changes. A morph target carries its **meaning** across: 60 % still means
"fairly intense" on the next patch, over completely different parameters.
A shift carries **the move itself** across: thirty degrees round is still
thirty degrees round, applied to whatever the new patch's colour is.

Shifts are also what makes a room adjustable without editing anything,
which is what lets the box have no editing on it at all.

### A position

How far a fader, pedal or morph has been pushed. Not saved, and the only
thing that is actually *played*. Everything above is written at a desk.

### What stopped being a noun

- **A transition.** Two patches and a position between them.
- **A scene.** It was a patch.
- **A palette.** Values inside a patch. The nine named ones become saved
  points if they survive at all.
- **A preset.** A patch.
- **Shape, as distinct from colour.** A pixel is hue, saturation and
  brightness; anything varying across the wall is one field over those
  three. Shape is that field routed to brightness. There is no second
  machine — see `docs/generator.md`.
- **Energy.** A morph target, reached by a fader or the pedal.

## Two structural decisions that come with the nouns

**The brain owns meaning; the controller sends gestures.** The controller
emits "fader 1 is at 60" and never learns what patch 4 contains. Forced
three times over: the DIN link carries about 320 messages a second and an
interpreted fader sweep would need more; a patch library in both boxes is
the same two-boxes-must-agree failure `docs/architecture.md` already
rejected for the indicator pixels; and a shift has to be added to a base
hue only the brain knows.

It also keeps the brain's stated indifference to who is talking to it
true, so a DAW can drive Aurora exactly as the controller does.

**There is no design mode on the controller.** Patches are built on the
laptop over USB, which is two-way and already compiled in. DIN is the
stage link: one-way, gestures only.

Three questions parked here on 2026-09-18 dissolved rather than being
answered, which is the sign this is the right shape:

- The touchpad springing back stops being a problem, because nothing on
  the box ever has to *set* a value.
- "Nothing shows which page is live" stops being a problem, because there
  are no pages.
- The numpad's job is settled: it selects patches, and only that.

What it costs is rewriting a patch in the room. Shifts cover adapting one;
they do not cover rebuilding one. That is judged the right thing to lose.


## The surfaces, so far — 2026-09-19

Decided in discussion, none of it built or played yet.

**The numpad selects patches, and that is its only job.** A press switches
instantly — no waiting to find out whether it was a tap or a hold, because
a patch change must never be late.

**Keeping the key held** pushes that patch toward its hold target and
releases back to it. Both halves of the same rule: *press* means go to
this patch, *hold* means push this patch toward its extreme. No key is
ever dead — holding the key you are already on is how you accent the
patch you are playing.

What that gives up is peeking at another patch and falling back, since
after a press you are *on* the patch you pressed. The keypad decode
already carries an unused **centre-key combo for "return to previous"**,
which covers it with one gesture that works from anywhere.

**The touchpad plays two morph targets, one per axis, defined per patch.**
It never sets anything, so springing back on release is now correct
rather than a problem to solve.

**The three faders hold their position.** Each carries one fixed *concept*
that does not change between patches — how it is realised is the patch's
business. "Intensity" means something different for Strobe than for
Starfield; it means the same *thing* on the fader either way. What the
three concepts should be is open.

Which gives, per patch and with nothing to arm: three faders, two pad
axes, one key-hold.

**Ramp time belongs to the target**, in beats. Zero is a stab — there on
press, gone on release. Sixteen is a build that arrives if you hold long
enough and collapses when you let go. One number, so an accent and a
build stop being two features. One time serves both directions until
something needs otherwise.

**One key at a time is a hardware fact, not a rule.** The keypad is a
static parallel code whose table decodes single keys and two combos;
two arbitrary keys together read as nothing.

## Open

- **What the three fader concepts are.** The one thing blocking the
  faders being usable.
- **Whether the numpad may also arm pad gestures.** A key would choose
  which of ten gestures the pad performs, rather than the patch deciding.
  Parked, not rejected: it reaches further than one pad assignment per
  patch, and it costs the thing that killed page-select — nothing shows
  what is armed, and a wrong guess misfires on stage where the wall
  cannot be seen. The ten pixels under the pad and the ten numpad keys
  are the same number, which is the obvious way to pay that cost off.
- **Everything else on the box.** The 12-step tempo rotary, the rocker
  switches, the tempo button, the telephone hook switch, the two
  indicator pixels, and which of the touchpad switches survive at all.
  Several now have no job, because the mode they were carrying is gone.
- **The PAR cans.** Every word above assumes the strips are the whole
  wall. There are four fixtures that should be played, and nothing has
  been said about how.
- **Cut or morph on a patch change**, and who decides — the patch, the
  key, or a control. Sits underneath all of the above.
- **Patches have nowhere to live.** They are in the browser's local
  storage today, which survives nothing. Getting them into the brain is
  unscoped work, and reading them back needs the brain's USB MIDI send
  path, which exists and has never been used. Not urgent: what is at risk
  is the storage mechanism, not the patches themselves, which are cheap
  to rebuild once there is a tool for making them.

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
