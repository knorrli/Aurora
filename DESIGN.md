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
below are the result. The old model — nine finished looks and a color
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

Patches live in the brain and are selected by number. How they get there
and what one is made of is § "Patch storage" below.

### A morph target

A saved alternate set of values, belonging to one patch, that one fader
pulls toward. **Absolute** — the far end is a specific look you dialed
in and judged.

The test for whether something is a morph target: can you build the far
end and look at it? "This patch flat out" — yes. "This patch's other
color" — no, because every color is equally valid and there is no
particular one to save.

### A position

How far a fader, pedal or morph has been pushed. Not saved, and the only
thing that is actually *played*. Everything above is written at a desk.

### What stopped being a noun

- **A transition.** Two patches and a position between them.
- **A scene.** It was a patch.
- **A palette.** Values inside a patch. The nine named ones become saved
  points if they survive at all.
- **A preset.** A patch.
- **Shape, as distinct from color.** A pixel is hue, saturation and
  brightness; anything varying across the wall is one field over those
  three. Shape is that field routed to brightness. There is no second
  machine — see `docs/generator.md`.
- **Energy.** A morph target, reached by a fader or the pedal.
- **A shift**, retired 2026-09-22. A relative, patch-independent rule
  laid over whatever the patch says — hue rotated so far, brightness
  trimmed — which a morph target cannot express, because a morph target
  is absolute and belongs to one patch. The mechanism was sound and the
  job was not: its case was adapting every patch at once to a room, and
  that is a correction this rig will not need. It never reached a
  control either; the box has no surface left that was going to send
  one.

## Two structural decisions that come with the nouns

**The brain owns meaning; the controller sends gestures.** The controller
emits "fader 1 is at 60" and never learns what patch 4 contains. Forced
twice over: the DIN link carries about 320 messages a second and an
interpreted fader sweep would need more; and a patch library in both
boxes is the same two-boxes-must-agree failure `docs/architecture.md`
already rejected for the indicator pixels. A third argument, that a shift
has to be added to a base hue only the brain knows, went with the shift.

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

What it costs is **any** change to a patch in the room: with shifts
retired, nothing on the box adapts a look without the laptop. Accepted
deliberately — the laptop is there when patches are designed, and a look
that needs fixing at a venue can be fixed at soundcheck.


## Patch storage — settled 2026-09-22

Nothing built. The format and the medium are decided; the one thing left
open is named at the end.

### Three ways the rig gets driven

Every decision below was tested against all three, and the third is what
settled most of them.

| | What is connected | Who is in charge |
|---|---|---|
| **A** | Controller to brain, DIN | The controller, entirely |
| **B** | A DAW into the controller, soft-thru to the brain, DIN | The DAW sends patch changes; the controller does everything else and updates its own state as it forwards |
| **C** | A computer to the brain, no controller | A performance encoded as a MIDI track |

**B is how the rest of the band's rig already works.** Mainstage changes
patches on synths, pedals and drum pads by sending a *reference* — one
Program Change — and the receiving device holds what that number means.
Aurora joins on the same terms.

**The brain is the only device present in all three**, which is why it
holds the patches. Put them in the controller and C has none; put them in
the DAW project and B asks Mainstage to hold Aurora's state, which is
exactly what the rest of the rig does not do.

**The brain never talks back over DIN.** The stage link is one-way. A
controller forwarding a patch change updates its own state from what it
forwards, rather than being told. The brain's USB send path is for the
editor and nothing else.

### A patch is raw CC values

A patch is one byte per CC, indexed by CC number, plus the two things
that arrive as Program Change — preset and palette — and the ramp time in
beats. Not cooked parameters. Three arguments, and the first is the one
that would have been expensive to discover later.

- **Morph already interpolates in CC space, and at least one control
  depends on it.** Count's fader is deliberately geometric so that
  interpolating its CC linearly doubles by construction — see
  `docs/generator.md` § Open, item 4. Interpolate cooked values instead
  and that property is gone, along with any other nobody has noticed.
- **The CC map is becoming a public interface regardless.** A DAW
  automation lane is addressed by CC number, so an Ableton project breaks
  if the numbers move. That is the real cost of renumbering, and it
  arrives whether or not patches are CC-shaped. What raw storage buys in
  exchange is that a saved patch and an automation lane are the same
  numbers: a patch can be read straight into lane starting points, and a
  morph is those numbers moving.
- **Cooked values cannot be recovered anyway.** Every CC handler cooks on
  arrival and discards the byte — `setGeneratorCount` stores
  `round(20^(value/127))`, which takes 128 inputs to 20 outputs and
  cannot be inverted. Today nothing in the brain knows what it is set to;
  the only thing that does is the browser tab.

What it costs: seven bits per control, and every parameter must be
CC-addressable. Both were already true — everything the brain does is
reachable by MIDI by design, and mode flags that once shared a bitfield
have been moving to their own CCs since `docs/bench-facts.md` § "The
bounce rework holds on the wall" recorded the packed CC gone.

**A patch is several parameter sets, not one.** The base, plus the far
end of each fader's morph target, and each far end covers the whole
parameter set — § "What a patch holds for them". Four sets minimum, plus
whatever the pad's destination scheme needs. About 700 bytes a patch,
generously counted.

### Where it lives

**Up to 128 patches on the brain**, which is what a Program Change can
name. Not nine. The numpad's nine keys are a limit on the hand surface,
not on the machine — and in B a DAW addresses the whole range, so
stopping at nine would cap an automated show at the keypad's reach for no
gain.

A **set** is then an ordered mapping of nine of the library's patches
onto the keypad keys. Switching sets needs no laptop, because every patch
is already on the brain.

**In LittleFS on the program flash.** The Teensy 4.0's emulated EEPROM is
1080 bytes — `E2END 0x437` in the installed core — which holds one patch,
so it is categorically the wrong place. The flash is 1984 KB with the
firmware using well under a tenth of it, so 128 patches at 90 KB is not a
constraint and no external chip is needed.

**Patches survive a power cycle and not a firmware upload.** The second is
accepted rather than worked around: the editor holds the master library
and re-pushes after a flash. That is what makes an external flash or FRAM
chip unnecessary, and the storage interface is the same either way if
that judgment ever reverses.

**A small default set is compiled into the firmware**, so a brain that
boots with empty storage still lights the wall instead of standing dark
while a laptop is found.

### The editor owns the library

Arbitrarily large, on the computer, and authoritative. Patches are
designed there over USB, which is two-way and already compiled in: the
parameters, each fader's far end, the pad's targets, and which nine go on
the keypad.

**Tags, grouping and ordering are deferred**, not rejected. With fifty
patches a flat sorted list is enough, and what is worth searching by will
be obvious once fifty exist.

### Open: what a physical control does when it disagrees

The moment the brain can be set from anywhere but the box, every fader
can disagree with the state — a patch loaded from the numpad, or a DAW
moving a CC the fader also owns. Grab it and the wall jumps to wherever
that fader happens to be sitting. Jump on touch, pickup, and scaled
takeover are the three answers, each with a stage cost, and the faders
that live at the extremes behave differently here from the ones parked
mid-travel. Not a storage question, which is why it did not block this,
but it arrives with the first patch recall.

One measurement to take alongside it: nothing in the brain smooths an
incoming value. A 30-second morph driven from a DAW is 128 steps, one
every 235 ms, which on brightness may stair-step visibly. If it does, the
answer is smoothing in the brain, which raw-byte storage makes
straightforward.

## The surfaces — 2026-09-19

Worked out in discussion across two sessions. **None of it has been
played, and none of it has been seen on the wall.** Where something is a
judgment about how a thing reads rather than a consequence of the
constraints, it says so.

### The division everything else hangs on

**The faders move energy. The numpad moves character.**

Not every control affects intensity — rotating hue from red to blue does
not, and neither does reversing direction. But everything you reach for
*mid-song* does, because mid-song you are following the music. The
sideways moves, where the wall changes character at the same energy, are
what you make between sections.

So each surface gets one direction, and the momentary controls are the
short version of the sustained one above them:

|  | Sustained | Momentary |
|---|---|---|
| **Up / down** | the three faders | holding a key |
| **Sideways** | the numpad | the touchpad |

This is a discipline rather than something the box enforces — a fader and
a pad axis run on identical machinery. It earns its keep by making each
surface predictable without having to remember which patch you are on.

### The three faders are three routes to "more"

Not three concepts. Three answers to *what carries the increase*, mixed
against each other so one chorus reads hot and sparse and the next dense
and cool:

- **Color** — more means hotter, toward white.
- **Extent** — more means more of the wall lit.
- **Motion** — more means faster, harder, more agitated.

Each is a per-patch morph target: the far end is dialed in and judged for
that patch. A route may be weak or absent on a patch with nothing to do
with it, and that is fine. A fader that does little is safe; a fader that
does something unexpected is not.

**Motion needs a real toolkit or it collapses into the other two.** Speed,
pulse rate and pulse shape are the obvious material, and fan spread is
already in the generator. A travel *easing* — linear through to slow at
the ends and fast through the middle, so a shape reads as a bouncing ball
— does not exist and would have to be built.

**Where jitter belongs is unsettled, and it is a settle-by-looking
question.** Scattering a clean strobe into a chaotic one could be a lift
or a character change. *The test:* with music, notice which one you reach
for it to do. If it raises a chorus it is motion and it is a fader. If it
changes the feel of one, it belongs on the pad.

The question survives 2026-09-22's rework of what jitter should be —
`docs/generator.md` § "What jitter is for" — but what it is asked about
changes. A texture source has an amount per destination rather than one
knob, so the question becomes which of those amounts a route reaches.

### Which strips — a window, not a selection

The old controller selected strips with the pad's X axis, in three modes.
It was expressive and it wasted the axis: a continuous control with
hundreds of positions was acting as a five-way switch, so sliding felt
like stepping.

**Replace selection with a window.** The effect has a *center* and a
*width*, with soft edges, and the center may travel past both ends of the
wall. Each strip's share is how much of the window falls on it, so X
slides a soft region across the stage instead of snapping between strips.
"Affect all strips" stops being a mode and becomes the width control at
maximum.

The share is **how far that strip has traveled toward the destination,
not how bright it is.** A strip inside a narrow window leaning toward
Glitch is partly glitchy; the strips outside it are untouched and at full
brightness. Nothing dims.

The same number is already needed elsewhere: `docs/generator.md`
§ Open, "Fan is a linear staircase", wants fan generalized to
`amount × f(strip − center)` with the center allowed outside the five
strips, so that chevron and diagonal become one family. One parameter
serves both.

**Width lands on the 3-way rocker** — one strip, three strips, all five.
With five strips there is no fourth useful setting. Width is also what
trades snap against slide: narrow hands over between strips in a short
crossfade, wide moves several together.

**Mirror survives untouched.** Two windows rather than one — yours, and
its reflection about the middle of the wall — with each strip taking
whichever is larger. It works at every width. It has no middle state, so
it costs a 2-way rocker, *unless* it turns out to be something that is
simply always on, in which case it costs nothing.

**Overlay versus exclusive is a 2-way rocker.** Whether the pad's effect
lays on top of what the patch is already doing, or the affected strips
take it alone, has no middle state — so it costs a switch, not an axis.

**All of this is testable before anything is wired.** The brain renders
and `tools/index.html` already drives it over USB MIDI, so an XY pad in
the page, a width selector, a mirror toggle and a destination picker are
a real test on the real wall. Three things the model above is guessing
at would come back answered: whether arbitrary patch pairs morph through
anything worth seeing, whether a continuous window reads as a sweep or
as a smear, and whether a strip caught halfway between two patches looks
deliberate or broken.

What it cannot answer is feel. Thumb travel and spring-back need the
hardware, and no amount of mouse-dragging stands in for them.

### Changing patch

**Ground rule: a patch change arrives on the next beat.** Section changes
land in time with the music. Measured latency in the reading chain is
10–20 ms against 125 ms for a sixteenth at 120 BPM, so nothing else in
the path matters.

Underneath the three gestures there is **one mechanism**:

> From wherever you are, toward whatever key you last pressed, at
> whatever rate the driver says.

| Driver | What it gives |
|---|---|
| Nothing | A cut, on the beat |
| Time, while the key is held | A morph you stretch by holding |
| The touchpad | A transition you scrub by hand |

A cut is that mechanism with zero time. These are not three features
competing for the numpad — they are one thing with three drivers, which
is also why **nothing needs arming**: if the numpad always names a
destination, the pad can always be driving toward it.

**The start is always a snapshot of the live values, never a patch
number.** Press 3 when you are 40 % of the way to 7 and the "from" end
becomes what is on the wall at that instant. Otherwise re-targeting
lurches. One copy of the parameter set per destination change, which
costs nothing and has to be deliberate.

That makes abandoning a transition a non-feature: you change your mind by
naming where you came from. There is no "go back" gesture and none is
needed on the hand controls.

**One rule covers every release:**

> Release means settle at the patch you pressed.

- Tap — you are there on the beat. A cut.
- Hold, then release partway — the morph finishes at its ramp rate. You
  arrive; holding only stretched the journey.
- Hold past arrival — you are pushing into that patch's morph target, and
  release falls back to the patch. The accent.

So holding is a journey first and an accent second, and you can never be
stranded in an unnamed blend between two patches with no key that leads
anywhere.

**Ramp time belongs to the patch**, in beats: the rate the journey runs at
when nothing is driving it, and the rate a release finishes at.

**Telling a tap from a hold must not use a fixed threshold.** The obvious
scheme — still holding when the beat arrives? — fails worst when you play
well, because playing in time means pressing slightly *ahead* of the
beat, so an intended cut reads as a hold. **Snap the press to the nearest
beat, not the next one**, and judge the hold a fixed short time after
that beat. Fingers are much faster than the 250–350 ms a foot needs, so
the window is cheap. The exact figure is unmeasured.

### Blackout has two forms

**Key 0 is the musical blackout** — a patch like any other, so press cuts
to black on the beat and hold fades to black over the ramp. It needs no
special case, which is the argument for leaving key 0 alone rather than
giving it a second job.

**The telephone hook switch is the master kill.** A hook is a maintained
state rather than an event: hang up and the wall is out until the handset
is lifted. It works regardless of patch, it is unmistakable by feel, and
it cannot be left wrong without noticing — which is what the old
document wanted from "a blackout reachable blind" and never solved.

### What the keypad actually is

**Ten keys: 1–9 for patches, 0 for blackout.** The two unlabeled black
inlays flanking 0 do not press.

**Two keys at once do not read as nothing.** Each key shorts the common
line to a subset of four data lines, so a pair reads as the OR of their
patterns — and most pairs collide with a real key. `2+3` reads as key 0,
`3+4` as key 6, `3+7` as key 9. A fumbled press therefore selects a wrong
patch silently, and in one case blacks the wall out. The firmware should
reject a code that appears within a few tens of milliseconds of another.

Only `4+7` produces a pattern no single key makes. That is one extra
gesture against a real hazard, and it is not worth building on.

**The per-key codes are not in doubt** — they were measured key by key
and the decode has run stably for over a year. What has never been tried
is a pair, so the wired-OR reading above is inference from the shape of
the codes rather than observation. Pressing two keys on the box as it
stands settles it; see `TODO.md`.

## The PAR cans — 2026-09-19

Worked out in discussion the same day as the surfaces above.
`docs/visual-design.md` is the settled word on what a wash *looks* like;
this is what plays them. **Nothing here has been seen with four fixtures
lit — only one has ever been connected at once.**

### Where they stand, which decides everything else

Four BeamZ BCC145 on the stage floor, pointing at the back wall to light
the stage frame. **They will never be rigged or ceiling-mounted**: there
is no time and no crew for it at the gigs this band plays, so this is a
constraint rather than a starting point.

- They already have a **horizontal position** across the stage, so they
  can be addressed by position alongside the strips without arranging
  anything.
- They can only ever make a **low, soft pool on a vertical surface**. No
  beams, nothing in the air, nothing above head height.

**Aim each one at the wall between two strips, not at a strip.** That is
what "aim them off the strips" becomes for a floor fixture — the strips
stay against dark wall, so nothing collapses the contrast of the
graphic, and the pool lands in a gap that is meters wide.

### A PAR is a position, not a second machine

The generator is already a function of where you are on the wall. A strip
is a line of positions; a PAR is one position with no length. So the same
machine renders both, and every noun above covers them for free — a patch
holds them, a morph carries them, the numpad selects them. There is no wash page and no second saved thing.

That splits along the two layers the generator already has:

- **Most of the color layer is what a one-pixel fixture can render.**
  Sampled at each PAR's position, so the four of them differ from each
  other and from the strips instead of being four copies of one hue. The
  wander and a gradient measured across the strips both reach a PAR; a ruler
  measured within a shape does not, since a PAR has no shape to be inside.
  `dmx_out::tick()` reads one flat `presetColor` for all four today.

  Two looks already asked for need more than the hue offset they have: a
  white flash between strip strobes, and the PARs following a red-to-green
  gradient with the strips. **The white flash is reachable as of
  2026-09-22** — the pulse pushes their saturation toward white, on its own
  wave, so it can flash between the strips' strobes rather than with them.
  The gradient still needs the color layer sampled at each PAR's position,
  which is not built.
- **The shape layer cannot reach them, except the pulse**, which is
  brightness over time and needs no length. So a PAR follows a swell, a
  strobe and a breathe, and ignores a sweep. Slow and broad on the PARs,
  fast and fine on the strips, by construction rather than by discipline.
  **Built 2026-09-22**: level, hue offset and saturation each take the
  pulse with an amount and a wave of their own, so the washes can breathe
  under a strip strobe. All four move together — they take the unfanned
  phase, since a PAR is one position with no strip to be offset from.

**The pulse drives the dimmer channel, never the fixture's strobe
channel.** That channel is a free-running internal rate with nothing to
lock it to the beat, so it cannot play in time; `docs/wiring.md` calls it
what a tempo-synced fixture effect would need, and that is wrong. It
stays available as an unsynced shimmer.

### What a patch holds for them

Level, hue offset from the strips' hue, saturation as a scale down from
the strips' own, and how much of the pulse reaches them — the last of
these three times over, once for each of level, hue and saturation. All of
it relative to what the strips are doing — a relationship, not a second
look.

**Saturation is a scale rather than a setting**, on CC 62 since
2026-09-22, and that is what makes it a relationship: full is whatever the
strips are, zero is white, and nothing in between names a color of its
own. It is also where the pulse's push toward white measures from. Without
it that push had no origin but the strips' own S fader, so pulling the
washes pale and flashing them paler were the same control.

**Whether they match the strips or contrast against them is per-patch,
and it lives on the faders.** Each fader's far end already covers the
whole parameter set, so pushing Color hot can take the PARs from
matching the strips to sitting a half turn off them, as one gesture, with
no switch and no new control. What it costs is that the relationship
cannot be changed without also moving energy — consistent with the
division above, where a sideways move at constant energy is a patch
change.

It also retires one of the five anti-overpowering rules in
`docs/visual-design.md` as a *rule*: washes owning the bottom of the
energy range and strips the top becomes one shape among others that a far
end can be dialed to. That file already marks its control statements
provisional.

### On the pad: nine positions, not two zones

The four PARs fill exactly the four gaps between five strips, so the
window's X axis runs over nine alternating positions rather than five.

- **Width re-reads on the 3-way rocker** as one position, three, or all
  nine. The middle setting becomes one strip plus the two PARs flanking
  it, which is better than what it means with strips alone: a lit strip
  with shoulders in the room.
- **Mirror is unaffected.** Nine positions are still symmetric about
  strip 3.
- **The PARs can bridge the gaps the strips cannot**, so a window sliding
  across hands over through a pool instead of jumping from strip to
  strip. This is the strongest reason to interleave and it is *unproven*
  — a soft pool on a wall may read as continuous with a bar of pixels, or
  as a separate thing blinking in turn. Settle by looking; see `TODO.md`.

**Above some travel speed the PARs stop tracking position and hold the
patch's color.** They carry the movement while it reads and fall back to
being the color layer when it does not, which degrades into something
good rather than into a stutter.

The speed is now measured: a BCC145 stops returning to black below about
25 ms, and does not lag at any rate. So the comparison is on **how long
the window dwells on one position** — under 25 ms, hold the color. That
is tempo-independent, which is what it has to be. It works out at a
window crossing all nine positions in under about half a beat at 120 BPM,
which is faster than a sweep any song has wanted so far. See
`docs/bench-facts.md`.

### Rejected

- **Match against contrast on the fourth rocker.** It is one continuous
  number — how far round the PARs sit from the strips — and a switch is
  for things with no middle state. Matching the strips' hue is also the
  half already rejected on the wall, on 2026-09-06, for making the rig
  read as one light source. And it is a character move sitting on the
  energy surface, which would make it a global mode changing what every
  patch means.
- **Splitting the pad vertically** — strips above, PARs below, both at
  the center. Three zones on a continuous axis is the same mistake as the
  old five-way strip selection, one axis over; it takes Y away from
  scrubbing the transition; and it collides with X addressing the PARs by
  position, since a window centered on a PAR with the thumb in the strip
  half is two contradictory instructions.

### Switches belong to the patch — 2026-09-22

> **A switch is part of what a patch is. Only arriving at a patch moves
> one.**

Four things have no middle: alternate, bounce, gradient-or-region, and which
ruler the placed field is measured against. An amount can fade in and a
switch cannot, so a morph has never been able to interpolate one. What was
never settled is *when* it should change instead, and "at the halfway
point of the slider" was an arbitrary answer nobody had argued for.

**The touchpad settles it.** Its window gives each strip its own share —
how far *that strip* has traveled toward the destination — so there is no
single position to compare against a halfway mark. A switch keyed on
morph position would flip strip by strip as the window slid across, which
is the stepping the window exists to remove, and it would need per-strip
bounce and alternate in the renderer, which do not exist. So a switch
cannot be a function of morph position at all.

What each surface does follows:

| Surface | Arrives? | Switches |
|----|----|----|
| Keypad | Always — release settles at the patch you pressed | Move at the settle |
| Touchpad | Only as a driver of a keypad journey | Never during the gesture |
| Fader | Never — it holds anywhere | Never |

The keypad's settle is a better moment than halfway in a way worth
naming: it is a moment the performer caused and is already watching for,
so the one discontinuity left in the system sits where somebody put it.

**Refusing to save a mismatched far end was considered and dropped,
2026-09-22.** A morph between two looks whose switches differ is worth
saving; what it cannot do is flip a switch halfway across. The suggestion
on the table instead is that **a morph which completes arrives** — runs to
its far end and takes the switches there, which would make a fader an
arrival after all and change the table above. That is unsettled, and it
belongs with real patch storage rather than with the bench panel's A-to-B
slider, which exists to test that morphing works at all.

**What a fader cannot do, and why it does not matter.** A fader's far end
is a morph target belonging to *the same patch*, not a different patch, so
the constraint is an authoring rule rather than a runtime one: **a patch
and its own morph target share switches**. For the touchpad, whose
destination is
another patch, the loss is real but small — across the whole roster only
Bars uses bounce and only CrossSweep and Stutter use alternate, so leaning
toward most destinations loses nothing.

**It does not weaken morphing as expression.** The three faders are three
routes to *more*; expression is about degree, and a switch has no degree.
Half of "turns around at the end" is not a weaker turn, it is nothing.

**Rejected: dissolving the switches instead.** Alternate could be made
continuous — odd strips running at anything from full speed with the
others through to full speed against them — and that is worth having as a
*look*, recorded in `docs/generator.md` § Open, item 1. It does not
answer this question. Bounce cannot be dissolved at all, because wrap
against bounce is a topology rather than a rate, so the rule would still
be needed for it alone.

**Bounce turns on without moving the shape, 2026-09-22.** Position is
read out of the travel phase differently in each mode — a fraction of a
cell under wrap, a triangle between the cell's two walls under bounce —
and the phase tracker keeps the *phase* continuous rather than the
position, so the flip teleported the shape. Solving instead for the phase
that stands the core where it already stands, in the half of the triangle
matching the direction it was going, leaves it standing and lets it turn
at the end it was heading for.

Measured in the preview at count 1 with no fan, sampled across a whole
cell of travel, on a 45-pixel strip. Turning bounce on moved the core
about 11 px on average and 22 at worst whatever the width was; it now
moves 1.2 px on average at width 40, 0.2 px at width 12 and 5.1 px at
width 90. One frame's own travel is 0.03 px.

**What is left is geometry, not a loose end.** Bounce cannot stand a core
within half its own width of a cell wall, because that is where it turns,
so a shape standing there is put out onto the wall. The worst moves
measured — 7.6, 2.8 and 16.1 px for those three widths — are half a core
width in each case, which is the bound. And fan enters the two modes as an
offset of different quantities, so only the unfanned strip can be solved
for: at fan 90 the flip still averages 7.0 px across the five.

**Alternate keeps its jump, deliberately.** The two things wanted of it
cannot both be had. If turning it on leaves the odd strips where they
stand, then where they sit relative to the even ones is whatever the
moment of the flip made it, and the same patch entered twice is two
different walls; keeping that relationship fixed is exactly what makes the
flip a jump. Under bounce it is worse — the swing is symmetric inside its
cell, so a shape running it backwards stands in the same place at every
instant as one running it forwards, and reversing the odd strips where
they stand would leave them sitting exactly on top of the even ones, with
alternate visible on nothing but a tail.

So it is not worth repairing, because it is not going to stay a switch. A
per-strip **rate** offset in the fan family reaches alternate as one
setting of a continuous control, and a continuous control has nothing to
flip — see `docs/generator.md` § Open, item 1.

## Open

- **What the fourth rocker does.** Three of the four switches are spoken
  for — overlay/exclusive, mirror, and width on the 3-way. Two jobs are
  left chasing one switch: latching the pad, which was a deliberate
  exception to spring-back, and anything the pad's destination scheme
  turns out to need. If mirror is permanently on, the pressure
  disappears. PAR match against contrast was considered for it and
  rejected, above.
- **Where jitter belongs**, above. Settle by looking.
- **Everything else on the box.** The tempo button, the mic trigger, the
  two indicator pixels, the ten pixels under the pad. The 12-position
  rotary stays the tempo control; whether tempo division deserves a
  dedicated knob is a separate question.
- **Whether a pool bridges a gap**, above. Settle by looking.
- **What a control does when it disagrees with the state.** Raised by
  § "Patch storage", and it arrives with the first patch recall. Settle
  by looking, with a fader in hand.
- **Patch storage is scoped but not built.** The format, the medium and
  the library are settled in § "Patch storage"; what remains is the
  editor-to-brain protocol over USB and the LittleFS layout.

### Dissolved rather than answered

- **Whether the numpad may also arm pad gestures.** It does not need to.
  The numpad always names a destination and the pad always drives toward
  it, so there is nothing to arm and no hidden state to misread.
- **Cut or morph on a patch change.** Both, from one mechanism, chosen by
  the gesture rather than by a setting.
- **"Return to previous" as a keypad gesture.** The claim that the decode
  carried a spare center-key combo for it was an inherited comment, not a
  fact; there is no center key. Re-pressing the patch you came from does
  the same job, and if the gesture is ever wanted it belongs on the foot
  controller, which is where it lived historically.

## The rule that keeps this from circling

Every question gets sorted, the moment it appears, into one of two piles:

- **Settle by argument** — it follows from the purpose or the
  constraints, so reasoning can close it.
- **Settle by looking** — it is a judgment about how something reads on
  the wall, in a room, with music playing.

Questions in the second pile get **written down with the test that would
answer them, and then dropped.** They do not get argued, including when
the answer feels obvious. Feeling obvious is how untested guesses became
recommendations in the document this one replaces.
