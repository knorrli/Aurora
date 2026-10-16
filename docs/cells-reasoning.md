# Cells: the reasons

Temporary. This file holds the reasons behind `docs/design.md` § Cells and § Brightness, so a fresh session can build without re-deriving them. Delete it once the cell model is built and tested on the wall.

## Why the Shape engine is being rethought

- The current Shape engine is not bad. It is very good at steady looks: identical bars, evenly spaced, gliding or bouncing, soft or hard edges, a trail. Static, color-led patches dial well on it.
- It does not match how the user thinks. When the user designs a moving shape, they focus on one lit area and ask where it is born, how it moves, grows or changes over some beats, and how it ends. Shape has no such thing: it draws one bar and repeats it along the strip, so every bar is literally the same bar at the same moment of its life.
- In synth terms: today's Shape is a drone with LFOs. The user thinks in notes.
- Two looks that could not be built made this visible, but they are test cases, not the goal:
  - bars born at the center that grow, divide like a cell, and move away up and down (together, or taking turns);
  - 9 slots of 5 pixels, a lit slot stepping down one slot per beat, leaving a stepped fading trail.

## Why cells are notes

- One cell is one note. Birthplace is like the note value, size or brightness at birth like velocity.
- Per-cell envelopes and expression are like MPE: each note carries its own modulation, where today's routes bend every bar at once, like one pitch bend for a whole channel.
- Today's bars fit inside the model as the plainest case: a cell born at one end that travels to the other unchanged. That is why the plainest cell must stay dialable with size, count and speed, and why the static color-led patches must stay as easy as today.

## Why a cell is the thing, not the lit region

- If a lit region were a cell, a cell dividing at the center would be one note turning into two, which notes cannot do. Instead, two cells are born together at the center and move apart; while they overlap they look like one growing cell.
- If going dark ended a cell, a strobe would be a new note per flash, and a modulation could end the note it modulates. On a synth, tremolo at full depth never ends a note. So a cell ends only when its life is over.

## When a new cell is worth having

- Only when something begins that can be told apart from what is already there: a new place, or a different moment of life at the same time.
- A fully lit strip, a strip that breathes, static bars that strobe: nothing new begins, so each is one held cell with routes on it. That is how today's engine already does them.

## Why the clock is the player

- On a synth, something outside the note decides when it starts and ends. For cells that is the player, and it plays on the clock, like a sequencer or arpeggiator.
- Turning the band's MIDI or the mic into cells would make Aurora a visualizer of MIDI events, not a band backdrop. Live triggers stay oneshots.
- The user's dream was keyframes: draw a few moments, let the in-between fill itself, from smooth to stepped. Filling in between pictures fades a bar out at one place and in at another; it never moves it. Filling in between a cell's settings moves it. So the dream became: set one cell's life with envelopes, and let the player repeat and offset it.
- An arpeggiator over a step sequencer: everything else in Aurora is dialed, not programmed, and drawing every step is tedious.

## Why the player is only Rate, Chord and Spread

- Rate uses the LFO's steps because every rate in Aurora does; triplets come from the patch's Tempo Division. Born once is the held cell of a static or color-led patch.
- Chord exists because five still bars are five cells born at the same moment, which one birth at a time cannot make.
- Birthplace and accent were knobs first. What set them apart was not position or brightness but being read once at birth and kept. Making that a setting on every route removed both knobs and gives every control a per-cell value: random-width raindrops, each drop its own hue.
- The three cases, for brightness: a live LFO makes every cell pulse together; an envelope makes every cell fade the same way; a held route makes cells differ from each other but stay as born.
- A synth's equivalent is a sample-and-hold that latches at note-on. Live Position is pitch bend for every note; held Position is each note's pitch.
- All cells of one chord are born at the same moment, so a held route gives them all the same value. Varying across a chord, like cells bending opposite ways, is the same open question as scaling a route across the strips.

## Why envelopes are route sources

- On 2026-09-30 the user had Scatter's built-in timing controls (Gate, time-Edge, Slide) replaced by routes, because they worked differently from every other control. A graph wired to one property would be the same mistake.
- On a synth, envelopes are sources in the modulation matrix, aimed at destinations with an amount. One envelope can drive several targets, like a cell that grows and brightens together.
- Scatter's routes already run on each spot's own clock, so routes running per cell extend something that exists.
- The cost: the user sets an envelope, then aims it, instead of setting a property's change directly.

## Why envelopes are ADSR knobs, not drawn

- Everything else in Aurora is dialed, not programmed. Knobs fit that; a drawing tool would be the one place that is programmed.
- Freely drawn envelopes cost a time, a value and a curve per point, 30 to 45 values for three envelopes. Knobs cost 4 or 5 each.
- They cover most of the looks: bars moving up are one long attack, the cross is attack and decay with the life starting over, the stepped trail is hold then decay. Hold is why there are five stages, not four.
- What is lost: curves and steps inside a stage. Drawing comes back only if a look on the wall turns out to need it.
- Three envelopes is the minimum the user wants.
- The user never pictures moving an envelope live, but every value staying reachable by routes and layers is a core idea, which ruled out storing envelopes outside the controls.

## Why Scatter stays

- Scatter paints over the strip with its own color and mix. One cell engine has one player and one kind of cell at a time, so merging the two would lose looks like a faint bar pattern with white spots flashing over it.
- Merging would only have freed CCs by giving that up. Two groups of cells, one over the other, would cost more CCs, not fewer.

## Why the preview comes first

- Only 2, 8, 9, 19, 34 and 64 are free. 0, 32 and 120 to 127 are reserved by MIDI; 1, 7, 10 and 11 are written unasked by a DAW or keyboard on Aurora's channel.
- The ways past 128 are NRPN or a second MIDI channel. The user dislikes the second channel because it makes DAW control harder, and has not looked at NRPN.
- The preview runs the same render code as the brain on a plain list of values, so cells can be tested there with a longer list. No patches exist yet, so nothing has to be kept compatible. The CC decision waits until cells have proven themselves.

## Why brightness moves into the shape

- Brightness was underused in nearly every patch. The cause: the user designs the shape first and color later, and the only way to dim part of the wall was Dark, inside the color engines. Brightness was only reachable in the stage reached last, after the shape was finished and on/off.
- Brightness is two things that give the same light on an LED:
  - presence, how strongly a cell is there, pronounced or faint, decided while designing the shape;
  - shade, a color mixed toward black, decided while designing color. Dark in Field, Flow and Core is shade, as White is tint.
- A synth has the same pair: velocity and a closing filter both make a note quieter, but come from different places for different reasons. So both stay and multiply.
- A gradient across space is presence: the user decides it in the shape stage.
- The brightness curve copies Bend instead of a plus/minus amount: amount and where it peaks cover brighter at the top, at the bottom, at the center, or dimmer at the center, with a pair of controls the user already knows. Routing the LFO to where it peaks sweeps a bright zone over a still pattern.
- The follow switch mirrors Field's Direction (Vertical, Horizontal, Shape) and adds each cell's position, which is key tracking: one brightness per cell from where it is.
