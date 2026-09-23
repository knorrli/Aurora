# The patch editor

`tools/editor.html`, with `tools/patch.js` (what a control is), `tools/library.js`
(what a patch is and how it crosses the wire), `tools/editor.js` (the page) and
`tools/preview.js` (the wall). Plain scripts rather than modules, so it opens
from a `file://` URL with no server.

Built 2026-09-23 in one pass, **untested against hardware**. It runs in a
browser and the preview is a real check, so what is verified is that the page
builds a patch, blends it, and produces the right bytes — not that the brain
likes them.

It replaces `tools/index.html`, which is kept until this one has been used at
the bench. The old page has no patch in the DESIGN sense: one flat parameter
set in `localStorage`, no far ends, no accent, no library, no wire.

## The shape: carrier, modulators, outputs

The old page drew three branches — shape, color, pulse — and put the color
layer's three sources inside the color branch as three unrelated boxes of
sliders. They are not unrelated. Every one of them lands on the same three
qualities and the pushes add, which is a modulation matrix drawn as scenery.

So the page is in three parts instead.

**The carrier** is what the wall shows with nothing pushing on it: the shape
lane's Form and Travel, and the color lane's three faders. Nothing else.

**The modulators** are five cards with one anatomy — a source on the left, its
amounts on the right — whatever each one reaches:

| Source | In time | On the wall | Reaches |
|----|----|----|----|
| The pulse | regular, anchored to the bar | nowhere — it has no position | brightness, width, hue, and all three PAR controls |
| The scatter | random, per cell | random, per cell | brightness, hue, whiteness |
| The placed field | still, or drifting | aimed — a ruler you pick | hue, whiteness, darkness |
| The wander | smooth, never repeating | smooth, everywhere | hue, whiteness, darkness |
| The light level | — | read off the shape lane | hue, whiteness, darkness |

**The outputs** are the five strips and the four PARs, and the PARs are the only
one with controls of its own because they are a relationship to the strips.

Two things fall out of drawing it this way, and both were open questions before.

The placed field's primitive and ruler stop looking like they govern the whole
color lane, because they are inside its card and nothing else is. That was
`TODO.md` § "The color panel does not say what its switches govern", and the
grouping was the whole of it — no control moved, and none was added.

And the amounts being at the source stops costing the view from the target's
end, because the page carries that view separately: **What reaches what** is the
same matrix with destinations down the side and sources across the top, showing
only what is actually pushing. Click a number and it goes to the control.

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

## Auditioning

A far end is never seen on its own. It is a destination, judged by how the trip
toward it looks.

So the scrubber **springs back**: hold it to watch the trip, let go and you are
editing the far end again. Touching any control mid-trip snaps home first, so
the sliders and the wall are never showing two different things. Run plays the
trip over a set number of seconds, and loop runs it back and forth.

Three walls are on screen while a far end is up: the live one, which is
whatever is being sent, and small stills of the base and the far end.

On the base tab the same machinery auditions a **journey to another patch**,
which is the patch-change morph rather than a fader. The switches stay at the
source patch's the whole way, because they land on the release and not on
arrival.

The accent tab carries **as heard from**. An accent plays with the switches of
the patch you came from, so an accent dialed against its own is judged on a
picture it will rarely show. Pick the patch it will actually follow.

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

**What tells the brain to run the generator is the patch's own pattern byte.**
The editor sends `PC = pattern` when it puts a patch on the wall. That is the
same Program Change the old page hard-coded to 10, sourced from the patch
instead of from the page.

**A parameter set is 128 bytes and the editor writes zero to the ones it does
not own.** Arriving at a patch writes the `[patch]` and `[switch]` CCs through
the brain's own handlers, so a byte no handler claims is never read. The editor
owns all 72 of them, which is the 63 that existed plus the scatter's nine.

**Ramp times are stepped through `AURORA_PULSE_PERIODS`.** One table for every
musical duration in the rig rather than a second convention.

**Moving a patch in the library moves its Program Change.** The keypad follows;
a DAW's automation lane does not, and the page says so when you do it.

## Not built

The brain cannot recall a patch yet, so pushing a library stores it and nothing
plays it. The editor drives the wall live over CC, which is what makes it
useful before that exists.

The scatter has no firmware. It renders in the preview only, and the card says
so.

Palette is a number with nothing behind it. Patch variants, tags and grouping
are deferred, not rejected.
