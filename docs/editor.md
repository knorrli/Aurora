# The patch editor

`tools/editor.html`, with `tools/patch.js` (what a control is), `tools/library.js`
(what a patch is and how it crosses the wire), `tools/editor.js` (the page) and
`tools/preview.js` (the wall, drawn by the brain's own renderer in
`tools/render.js`). Plain scripts rather than modules, so it opens from a
`file://` URL with no server. Chrome only: it is the browser with Web MIDI.

Built 2026-09-23 in one pass, **untested against hardware**. It runs in a
browser and the preview is a real check, so what is verified is that the page
builds a patch, blends it, and produces the right bytes — not that the brain
likes them.

It replaced `tools/index.html`, deleted 2026-09-24. That page had no patch in
the DESIGN sense — one flat parameter set in `localStorage`, no far ends, no
accent, no library, no wire — and it carried its own copy of the CC map, the
control descriptions and the look presets, so every protocol change had to be
made twice by hand.

## The shape: carrier, modulators, outputs

The color layer's sources are not unrelated boxes of sliders. Every one of them
lands on the same three qualities and the pushes add, which is a modulation
matrix, so the page is drawn as one.

From the top, below the patch head and the audition:

- **The outputs**: the five strips on the left, carrying the three color
  faders, and the four PARs on the right, with the controls that relate them to
  the strips.
- **The LFO**, the one modulator every route runs off.
- **The shape lane**: Form, Travel and Fan — with the strips' three faders,
  what the wall shows with nothing pushing on it.
- **The other modulators**, four cards with one anatomy — a source on the
  left, its amounts on the right — whatever each one reaches:

| Source | In time | On the wall | Reaches |
|----|----|----|----|
| The scatter | random, per cell | random, per cell | brightness, hue, whiteness |
| The placed field | still, or drifting | aimed — a ruler you pick | hue, whiteness, darkness |
| The wander | smooth, never repeating | smooth, everywhere | hue, whiteness, darkness |
| The light level | — | read off the shape lane | hue, whiteness, darkness |

The placed field's primitive and ruler sit inside its card, so they do not look
as though they govern the other sources.

## A far end is an override, not a copy

A patch is five parameter sets on the wire. In the editor it is one set plus
four sparse maps of what each far end overrides, and `materialize()` is where
the two meet.

This is not a storage optimization. A far end moves roughly four controls and
leaves the other sixty-eight alone, so a copy says a patch holds seventy-two
decisions where it holds four. Worse, a copy goes stale: dial the base after
building a far end and the copy keeps the old value for everything you have
since touched, so the far end stops being *this patch, but more* and becomes a
different patch that happens to share a name.

Inheriting means the far ends follow the base everywhere you did not
deliberately push. What you get on screen:

- a badge on each tab saying how many controls that far end overrides
- an amber bar on every overridden row, and a tick on its fader showing where
  the patch itself sits
- clicking a label drops the override and follows the patch again, which is
  what "reset" means on a far end — on the base it still means no push
- a far end dialed back onto the base value stops being an override, so the
  count never lies

## Edits go into a draft

The library is 128 fixed slots, and a slot is the Program Change that plays its
patch, so nothing ever renumbers: deleting a patch empties its slot, and the
keypad keys on it stay on the empty slot. The list shows the filled ones.

The first change to a patch opens a draft of it — base, far ends and the
patch-wide row together — and the library does not change until the draft is
saved. **save** writes it back into its own slot. The **slot** field under it
saves into any slot, says what is there, and asks before replacing another
patch; with no draft open that is how a patch is duplicated. **discard** drops
the draft, and choosing another patch with one open asks first. **new** starts
a draft with no slot, which it takes when saved. There is no undo history.

The draft is kept in `localStorage` apart from the library, so a closed tab
still loses nothing: it comes back on reload, marked unsaved. The two buttons
under **The brain** that look like saving are both exports of the saved
library, never the draft — **save a file** is the copy that survives the
browser and belongs in the repo, **push the library** is the copy the brain
holds.

## Auditioning

A far end is never seen on its own. It is a destination, judged by how the trip
toward it looks.

So the scrubber **springs back**: hold it to watch the trip, let go and you are
editing the far end again. Touching any control mid-trip snaps home first, so
the sliders and the wall are never showing two different things. Run plays the
trip over a set number of seconds, and loop runs it back and forth.

Three walls are on screen while a far end is up: the live one, which is
whatever is being sent, and small stills of the base and the far end.

**The small ones are drawn at the big one's instant.** A phase in the renderer
carries an offset so that moving a rate does not teleport the wall, which is
what makes Speed usable with a fader. The cost is that the offset is a history:
two walls that have seen different rate changes sit a constant distance apart
for ever. Left alone, the stills read as exactly off-phase from the live wall
from the first time Speed is touched, and a comparison you cannot trust is
worse than no comparison. So each still renders from a throwaway copy of the
live wall's phases. What that gives up is that a far end differing only in a
rate looks identical in a still; the audition is what shows that.

### Three faders at once

One tab shows one far end, which is not how the rig is played: three faders sit
on the box and a key may be held on top of them. **The surfaces, all at once**
on the Base tab is four sliders that place all of them and shows what comes
out.

**The departures add.** Each surface contributes its position times the
distance from the patch to its own far end, and the sum is clamped per byte.
One surface alone is exactly what its own tab shows, so nothing changed for the
case that already worked. Two far ends that move different controls — the usual
case, since each moves about four — do not interact at all. Where two move the
same control they pull against each other and the sum is what you get.

Adding is the reading the rest of Aurora already uses: the color layer's sources
push on the same three qualities and their pushes add. The alternatives are a
weighted average, which makes one fader weaker as another comes up, and a
per-parameter winner, which needs a rule about who wins that nothing else in
the rig has.

**This is the editor proposing a rule, not showing one.** `DESIGN.md` says each
fader is a morph target and never says what three of them at once come to, and
the brain does not combine them yet. If the rule changes, this panel and the
firmware have to change together. See `TODO.md`.

### Moving a far end to another surface

A far end dialed under Extent that turns out to be a Motion idea does not have
to be rebuilt. All four far-end tabs carry **copy from**, **move onto** and
**swap with**, the accent included: it belongs to no fader, but it is a far end
like the other three and a look is as likely to end up there as anywhere. Only
the override map travels — the base is the patch and stays where it is — which
is what makes this three lines rather than a merge.

On the base tab the same machinery auditions a **journey to another patch**,
which is the patch-change morph rather than a fader. The switches stay at the
source patch's the whole way, because they land on the release and not on
arrival.

The accent tab carries **as heard from**. An accent plays with the switches of
the patch you came from, so an accent dialed against its own is judged on a
picture it will rarely show. Pick the patch it will actually follow.

## A route shows on the control it moves

Every control a route may reach has a **~** at the end of its row, and it opens
that control's routes in one panel floating under the row, so the row stays in
view while a route is dialed and the page below does not move. One panel is
open at a time. It lists the routes aimed here with their amount, LFO
multiple and wave, adds one from the eight shared slots, and frees one. A
destination has no middle, so like every switch it belongs to the patch:
routes are added and freed on the base, and a far end overrides only how far
and how fast. **bypass** silences a route while you listen and is
never saved: the wall and the brain are sent a free slot, and the patch keeps
the route. Choosing another patch clears it. The rates have no **~**, since the renderer refuses them.

A slider a route is aimed at carries a band from the dialed value to the
furthest the routes can push it, and five marks, one per strip, where each
strip has it this frame. The marks move together unless the fan spreads the
strips' LFO phases, and then they move the way the strips do. Both are read off
the renderer, not worked out in the page. They are painted into the slider's
own track so the handle stays on top: the handle is the value that is saved,
and nothing modulation does may cover it.

The same tracks carry notches: the center of every control that departs both
ways from 64, found by asking the renderer where its value changes sign; the
swell, snap and hard half-bar on a route's wave; and the steps of the LFO's
rate, the fan's frequency and a route's ratio, each in the middle of its run
of bytes.

## Rulings made without the performer

Built overnight with no one to ask, so these were decided rather than agreed.
Each is cheap to reverse.

**The four inert controls under a gradient are dimmed, not repurposed.** Count,
Width, Edge and Speed reach nothing while the primitive is a gradient —
`placedAt` returns on its first line — and gradient is where the switch starts.
They dim and say why. Giving Edge a meaning there (bending the ramp) is a
firmware change and this was not a firmware night.

**Strip order and blackout are bench tools, not patches.** They sit in the top
bar, send a Program Change straight past the patch, and say so. Program Change
means patch now, so there is no preset row left for them to live in.

**Every patch runs the generator.** A patch has no pattern of its own: the
editor sends `PRESET_GENERATOR` as the Program Change when it puts a patch on
the wall, and writes the same number into the pattern byte of the SysEx head,
which the brain's side of the wire still carries.

**A parameter set is 128 bytes and the editor writes zero to the ones it does
not own.** Arriving at a patch writes the `[patch]` and `[switch]` CCs through
the brain's own handlers, so a byte no handler claims is never read. The editor
owns all 72 of them, which is the 63 that existed plus the scatter's nine.

**Ramp times are stepped through `AURORA_LFO_PERIODS`.** One table for every
musical duration in the rig rather than a second convention.

## What went away

**Jitter is gone**, out of the protocol and out of both renderers as of
2026-09-23. The scatter replaces it, and until the scatter is in the firmware
the rig has no texture a patch can ask for. That is deliberate — jitter cannot
be aimed anywhere and its grain can only ever be one pixel wide, so keeping a
control for it would mean building patches around something already replaced.

**The view from the target's end is gone**, as of 2026-09-24: a note under
each routable label naming what pushed it, and a table, "What reaches what",
of every modulator against brightness, hue, whiteness and darkness. The band
and the lit **~** carry the routes, and the table was never looked at. If
"what is making the wall whiter" turns out to need an answer the cards do not
give, the table is in the history and comes back.

## Not built

The brain cannot recall a patch yet, so pushing a library stores it and nothing
plays it. The editor drives the wall live over CC, which is what makes it
useful before that exists.

Patch variants, tags and grouping are deferred, not rejected.
