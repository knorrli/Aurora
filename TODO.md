# Aurora TODO

**What this file is for.** The design docs hold the thinking — settled or
open, with the reasoning. This file holds only the doing: one line per
item, an action and a pointer, no argument. An open question with no next
action does not appear here at all; it lives in its doc until someone
decides to act on it.

The phase plan that used to live here was built on design decisions
cleared on 2026-09-18; it is recoverable at commit `33f2d1f`.

Everything lives on **`main`**. The last v1 commit is tagged
`aurora-nano-final`.

---

## What is built and proven

- **Brain on Teensy 4.0.** Builds clean, 76 KB of 2 MB.
- **All five strips lit**, each on its own supply, through a 74AHCT125.
- **USB MIDI path validated** — program change, CC, clock, notes,
  transport, tempo division, free-run on clock loss.
- **DMX out working** end to end. Two PARs addressed at `A001` and
  `A009`, 2026-09-21.
- **The parametric generator** on PC 10, with the redesigned color layer
  in the firmware as of 2026-09-21. See `docs/generator.md`.
- **The LFO's destinations**, 2026-09-22. One oscillator with one rate
  with eight routes off it, each naming the control it pushes, how far, at
  what multiple of the LFO and with what wave. Its phase is anchored to the
  bar and its rate is stepped to the periods a bar can hold. **Not yet seen
  on the wall.** See `docs/generator.md` § "Routes".
- **The washes' own saturation**, 2026-09-22, on CC 29. A scale down from
  the strips' saturation, and the origin the LFO's push toward white
  measures from. Not yet seen on a fixture. See `DESIGN.md` § "The PAR
  cans".
- **Position**, 2026-09-22. Where a still pattern stands in its cell, on
  CC 59. What travel left over eases away while the pattern stands still,
  so a patch saved still comes back to the same place instead of standing
  wherever the last traveling one ran out. See `docs/generator.md` § "A
  still pattern stands where it is told".
- **The bench panel** — `tools/editor.html`, drawn as the signal flow, with
  the patch library and the morph control. The routes
  are a third branch on it, since they reach both the others and the washes.
- **The wall on screen** — `tools/preview.js`, five strips and four PARs
  drawn by the brain's own renderer compiled to WebAssembly, so a look can
  be dialed with nothing plugged in and what lights is what the strips are
  sent.
- **The scatter**, 2026-09-23. Nine CCs at 71–79: a grid of cells, each on
  its own clock, each lighting a spot that pushes the strips' brightness, hue
  and whiteness. Ported from `tools/preview.js`, where it was designed, and
  checked sample for sample against it. Flashed but **not yet seen on the
  wall**. See `docs/generator.md` § "The scatter".
- **A strip-order rigging aid** on PC 11 — each strip a flat color in
  data-chain order.
- **One renderer**, 2026-09-24. `shared/render/` is the generator, the
  routes and the washes' color, and both the brain and the editor run it.
  Before the JavaScript copy was deleted the two were compared frame by
  frame: 72 000 frames of random patches and every saved look agreed to
  within 6 of 255. They part only where the old code was wrong: a route on
  the count, the width or the fan's shape now reaches the pattern's geometry
  rather than being read after it was laid out, and a route that switches a
  color source on is heard on the strips whose phase has it on, not on
  none of them.
  See `docs/architecture.md` § "One renderer, compiled twice".
- **Rates as route destinations, and a route's phase**, 2026-09-25. A route
  aimed at any of the five rates swings it both ways around its dialed value
  and averages to nothing, so the wall comes back into line every route
  cycle — checked in the renderer to the pixel over ten cycles. A route is
  five bytes now, the fifth delaying its wave into its cycle, and the CC map
  was regrouped to make room. **Not yet seen on the wall.** See
  `docs/modulation.md` §§ "A rate swings both ways" and "A route has a
  phase".

Measurements in `docs/bench-facts.md`.

## What is not built

- **The brain's perfboard.** Everything is still on a breadboard.
- **DIN MIDI in on the brain** (6N138 circuit on `Serial1`).
- **The brain enclosure.** Blocked on the perfboard — the box should be
  cut around the finished board, not before it.
- **The controller.** Still the old Nano box. The second Teensy is
  untouched and its pin map has never been drawn. The three faders have CCs
  of their own at 12–14, but `controller/src/controls.cpp` still sends the
  sticks to the color.
- **The foot pedal.**

## Parts

Checked items are confirmed in the drawer.

- [x] **Teensy 4.0 × 2** — brain on hand and running, controller
      ordered 2026-09-05.
    - [ ] *Deliberately skipped:* a third as a gig-bag spare. At CHF 51
          in Swiss retail it was judged too expensive; revisit via
          pjrc.com, Play-Zone or Pi-Shop. Note this leaves the
          "one spare board covers either failure" argument for the
          two-Teensy choice unfunded.
- [ ] **Header pins.** Teensy ships bare. Male 1×40 2.54 mm ordered
      2026-09-05; female sockets on hand. **Socket both Teensys rather
      than soldering them down** — a spare board only helps if a dead
      one can be swapped without a soldering iron at the venue.
- [x] **74AHCT125** × 2 — one fitted on the brain, one for the
      controller's indicator pixels. Must be HCT or AHCT; plain HC or
      AHC has the same threshold as the pixels and fixes nothing while
      looking identical on the shelf.
- [x] **6N138 opto-coupler** × 2, for DIN MIDI in on both nodes.
- [x] **5-pin DIN MIDI jacks**, panel-mount, more than three on hand.
- [x] **5-pin DIN MIDI cable** — confirm it spans the real stage
      distance plus slack.
- [x] **Resistors** — assorted pack covers every value needed.
- [x] **1N4148 / 1N914A signal diode** × 2 for MIDI in protection.
      Interchangeable here: same 200 mA fast small-signal part, 75 V
      reverse instead of 100 V, against a few volts of MIDI.
- [x] **Perfboard** for both assemblies.
- [x] **Tap tempo momentary switch** — reuse the existing one, it has a
      built-in LED.
- [x] **M5Stack DMX Unit (U183)** — isolated transceiver, isolated
      DC-DC, surge protection, switchable termination and the XLR-3
      socket in one part.
- [x] **Grove→Dupont cable** — on hand and proven. Strip the Dupont ends
      and solder to the perfboard; the module still unplugs at its own
      Grove socket, which is the end that matters at a venue. Leave
      slack and strain relief.
- [x] **DMX cable, XLR 3-pin** — on hand and working. Buy real 110 Ω
      cable for stage use.
- [x] **120 Ω terminating resistor** — belongs at the last fixture, not
      on the brain board.
- **Foot pedal parts.** Switches and guitar cables on hand. Ladder
  resistors (1 k / 2.2 k / 5.1 k / 10 k / 20 k, 1 %) on hand.
    - [ ] **The connector is open again.** Settle whether it is mono or
          stereo before drilling anything — see `docs/wiring.md` §
          "Foot pedal".

## The test that closes the performance controls

Nothing about how Aurora is *played* can be settled at a desk. The
performance controls — what the touchpad is for, where the scatter belongs,
whether the fourth rocker has a job, what a fader does when it disagrees
with the state — all wait on the same thing.

- [ ] **Play a full DJ set to Justice, "Women Worldwide."** Smooth
      transitions, breakdowns highlighted, builds, drops. If that set can
      be performed on the rebuilt controller, the performance model is
      good. If it cannot, the thing that got in the way is the answer.
      This is the acceptance test, not a demo — it needs the controller
      rewired first.
- [ ] **Find out whether the touchpad reads pressure usefully.** The
      4-wire pad theoretically gives a Z reading and `CC_TOUCH_PRESSURE`
      is already in the protocol, but it has never been tested on this
      hardware. Until it has, nothing in the design may depend on it. See
      `DESIGN.md` § Open, "What the touchpad is for".

## Hardware work that does not wait on the design

- [ ] **Set the remaining two BCC145 to `A017` and `A025`.** Watch the
      personality as well as the number — see `docs/wiring.md` §
      "Fixture profile".
- [ ] **Calibrate the fixtures** at rehearsal — RGB trims, and whether
      FastLED's brightness curve suits the fixture. Step **evenly
      spaced** values. See `docs/bench-facts.md`.
- [ ] **Set the per-fixture master scale by eye** at soundcheck. See
      `docs/visual-design.md` § "The washes".
- [ ] **Aim each PAR at the wall between two strips**, not at a strip.
      See `DESIGN.md` § "The PAR cans".
- [ ] **Look at whether a pool bridges a gap.** One slow sweep and one
      fast one, narrow window. See `DESIGN.md` § "The PAR cans".
- [ ] **Press two keys at once and watch the wall.** Needs no rewiring.
      See `DESIGN.md` § "What the keypad actually is".
- [ ] *Deferred to the controller rebuild:* confirming what idle reads
      and what produces `0b00111111` and `0b00111101`. Getting at the
      lines means desoldering brittle keypad wiring, so they wait until
      the pad is off the box anyway.

## On the wall, next session

Built 2026-09-22 and seen by nothing but the preview. One look each, driven
live from the editor.

- [ ] **Try to defeat the blackout gate.** Built 2026-09-23, never seen on
      hardware, and it is the one control that has to work when nothing
      else does. Three presses, each with a path that used to stay lit:
      with the transport stopped from the DAW, so no tempo pulse is
      arriving; with the mic trigger firing, which fills the whole array
      after the pattern has drawn; and with the washes at full, watching
      that the PARs go dark rather than dim. Then press any other key and
      confirm the wall comes back with nothing latched. See `DESIGN.md`
      § "Blackout has two forms".

- [ ] **Turn bounce on and off** at count 1, no fan, a narrow shape. The
      shape should stand still across the flip and turn at the end it was
      heading for, and so should every strip with the fan up, since each
      now carries its own travel phase. A pattern entered with bounce
      already on still starts its swing somewhere new, which is expected. See `DESIGN.md` § "Switches belong to the patch".
- [ ] **Dial the ten fan looks**, removed from the editor 2026-09-25 and
      recoverable from `git show 21a435c:tools/patch.js`, § starting points.
      Built 2026-09-23, seen by nothing but the preview, and the whole
      rework rests on them. Four of them answer something on their own:
      **Chevron ∧** is whether a quarter turn of phase gets Rain back,
      which is what put the chevron on the list in the first place;
      **Bars, unison strobe** is the patch the rework exists for;
      **Hypno together** is whether strips at different rates read as
      alive or as a pattern coming apart; **Comets** is whether the fixed
      hash draw looks unplanned or merely arbitrary, and whether it wants
      a seed. See `docs/generator.md` § "The fan is a wave".
- [ ] **Wind the frequency fader to the top and sweep the phase.** Two
      warts live there and neither has been seen: the phase stops sliding
      the pattern and only scales how deep the alternation is, and a
      quarter turn either side of the top every strip reads zero and the
      fan goes silent. Judge whether that reads as a control going quiet
      or as a fault.
- [ ] **Wind the placed field's region count up**, on the strip ruler with
      a hard edge. It should wash out smoothly rather than strobe. See
      `docs/bench-facts.md` § "Point-sampling a pattern aliases".
- [ ] **Pull CC 29 down, then open the LFO's PAR saturation.** Full
      should look like the strips as before; pulled down, the flash toward
      white should start from pale. See `DESIGN.md` § "The PAR cans".
- [ ] **Sweep the V fader under a scattered look.** The white pixels
      should dim with the colored ones. Then judge the 30 % white share,
      which has never been judgeable. Reachable now the scatter is flashed:
      count high, width and edge low, the light amount up. See
      `docs/bench-facts.md`.
- [ ] **Take Lit White from center down to zero**, on a flat fill with the
      S fader at full. At zero the wall should be exactly the color on the
      faders; at center it should wash every lit pixel to about 74 %
      saturation at nearly double the light. It is the one color control
      whose neutral is the bottom of the travel rather than the middle, so
      check what it sits at in the patches actually in use. See
      `docs/generator.md` § Open, "Five of the six white and dark controls".

- [ ] **Swing a rate.** Two looks, each a patch. The fan's rate spread at
      zero with a sine route on it: the strips should drift apart and come
      back into line on the bar. And the hypno look: speed dialed still, a
      four-beat LFO, a sine at a quarter-turn phase on speed and a second
      on the fan's rate spread — the wall should rise, stop on beat 2, fall,
      and stop again, all five together. A tail follows the swing, so a
      shape swung backwards leaves its glow behind it rather than running
      tail-first. See `docs/modulation.md` § "A rate swings both ways".
- [ ] **Morph between two patches of different tempo divisions.** Every
      shape jumps when the division lands at the end of the morph. Judge
      whether it reads as a glitch; if it does, carry the position across
      in `renderGenerator` the way a tracker carries a rate change. See
      `docs/architecture.md` § "Tempo division".
- [ ] **Judge the afterglow.** The *Swing* looks, from
      `git show 21a435c:tools/patch.js`, § starting points: a tail should
      stay behind its shape through the swing, shrink as it slows and be
      gone while it stands. Then whether 8 beats is the right top, whether
      half a beat sits at a useful place on the fader, and what the brain's
      frame time does with the path walk. Send two patches in turn from the
      editor: the second should come up without a streak from the first.
      See `docs/generator.md` § "The tail is an afterglow".

The LFO's destinations, the anchor against a click and the stepped rate
are unjudged too, and are listed below.

## Generator, next

None of this is committed to — it is an experiment that earned a second
session. Ordered by what blocks what. Reasoning in `docs/generator.md`.

- [ ] **Dial in five or six endpoints by eye and save them.** Nothing
      else about the morph is worth judging until these exist. See
      `docs/generator.md` § "The panel's roster settings are guesses".

- [ ] **Reach the PARs with more than a hue offset.** The PARs following
      a gradient with the strips, which needs the color layer sampled at
      each PAR's position — today all four are one color. The white flash
      between strip strobes, the other half of this, became reachable on
      2026-09-22 when the LFO gained their saturation. See `DESIGN.md`
      § "The PAR cans".
- [ ] **Dial the placed field's approximation of the sprinkle.** Region,
      strip ruler, count around 12, width and edge low, dark pushed up,
      over a full-width wall with V around half. Thirty seconds in
      `tools/preview.js`, and it decides how much of the texture source
      below has to be built. Nothing else on that topic moves until this
      has been looked at.
- [ ] **Decide whether the scatter needs spots with a lifetime.** Four of
      the five looks it was designed against land on the grid it already
      has; the fifth — raindrops, a spot born somewhere that then travels
      and outlives its cell — is the open fork, priced at eight to ten
      controls. The scatter's block has nine spare for exactly this. Judge
      it once the scatter is on the wall, not before. See
      `docs/generator.md` § "The fork this answered".
- [ ] **Judge the LFO's destinations on the wall.** Everything else about
      them is guesswork until this happens. Three looks are what it was
      built for, and each is one patch: the washes swelling under still
      strips, a white flash on the washes between strip strobes, and the
      shapes breathing on width while the light holds. See
      `docs/generator.md` § "Routes".
- [ ] **Judge the anchor against a click.** The *peak* is what lands on the
      beat, because "a deep slow swell peaks wherever it happens to" was
      the complaint. A square is that swell clipped around its own
      midpoint, so its flash is centered on the beat rather than starting
      there — at a two-beat period the light comes on half a beat early.
      Skew moves the flash inside the cycle and is the control to reach
      for. Whether the leading edge is the better thing to anchor is a
      question a click track answers and a bench cannot. See
      `docs/bench-facts.md` § "A phase derived from absolute time
      teleports".
- [ ] **Judge the stepped rate.** Thirteen positions where there were 128.
      The dotted values — 12, 6, 3, 1½, ¾, ⅜ beats — come back to the
      downbeat every three bars rather than every one, which is a musical
      relationship and may still read as adrift. Cutting them would leave
      seven positions, all powers of two.
- [ ] **Judge Bend on the wall.** The *Falling, bent* and *Bouncing, bent*
      anchors, from `git show 21a435c:tools/patch.js`, § starting points. Liked in the preview on sight; what the preview cannot
      show is whether shapes squashed at the slow end shimmer at full bend.
- [ ] **Decide whether a wrapping strip is a loop or a line.** Settled
      for bounce, open for wrap. See `docs/generator.md` § Open, "Is a
      strip a loop or a line?".
- [ ] **Decide the color layer's usable ranges.** The test is a set, not
      a bench. See `docs/generator.md` § Open, "Where the color layer's controls should stop".
- [ ] **Test the touchpad window from the laptop, before any rewire.**
      See `DESIGN.md` § "Which strips — a window, not a selection".

## Deferred until the model is stable

Decided 2026-09-25. The patch model is still being designed, and every patch
so far is a test that can be thrown away. The moment the brain holds a
library, a change to the patch format needs a compatibility story, so nothing
on the brain's side of the wire moves until the model has stopped changing.
Until then the editor drives the wall live, over CCs. The code already
written for it — `brain/src/patch_store.cpp`, `brain/src/patch_sync.cpp`,
`tools/protocol.html` — stays as it is, unrun.

- [ ] **Run the patch sync against the brain.** It has never met
      hardware. In order, all from `tools/protocol.html` but the last:
      ask a freshly flashed brain what it holds and confirm it says empty;
      push eight patches and read them back byte for byte; push a half
      library and confirm the commit is refused and the previous one
      survives; save the library to a file and push that same file back;
      then pull the mains mid-sync and confirm the old library is still
      whole. Time a full 128 while you are there — the flash write is the
      likely cost, not the transfer.

- [ ] **Teach the brain to recall a patch.** Storage and the wire exist and
      nothing puts a patch on the wall — the only reader of the library is
      the export path. A patch arriving has to write the [patch] and [switch] CCs
      through the same handlers a live CC goes through, which is what makes
      the raw bytes worth storing. Needs the keypad-to-patch lookup at the
      same time — the controller still sends `PC = key number` in
      `scan_numpad()`, which only works while key N means preset N. Recall
      has to call `resetGenerator`, the way the generator's program change
      does, or a patch lands with the last one's tails streaking.

- [ ] **A default set compiled into the firmware**, so an empty brain
      still lights the wall. See `DESIGN.md` § "Patch storage". It must
      never be written to storage: the brain reporting an empty library is
      how the editor tells a fresh flash from a small library, and writing
      the defaults in would destroy that distinction. It must also leave
      the brain on a patch other than key 0, or the blackout gate added
      2026-09-23 holds the wall dark — `selectedPreset` starts at 0.

## Open discussions

Raised in conversation and not yet settled. Each needs a decision before
it becomes a build item.

- [ ] **Patch variants — one patch, a few overrides.** Raised 2026-09-22.
      Tempo division and palette are both per-patch, so the same look at
      half time, or in a cooler palette, is a second patch today. Three
      things make this safe to leave alone. It resolves in the editor at
      sync time, so the brain still receives flat, independent patches and
      nothing about the wire or the storage depends on the answer.
      Duplication is cheap — 128 slots and 85 KB against 1.9 MB, where what
      runs out is keypad keys rather than room. And with no patches written
      yet, which duplication actually grates is a guess.
      It is not the retired shift returning. A shift was a live relative
      rule applied while playing, which is what made it fight with morph
      and need a control of its own; a variant is resolved at the desk
      before anything is sent. See `DESIGN.md` § "What stopped being a
      noun".
      One consequence if it is ever built: a library pulled back off the
      brain comes home as flat patches, since the parent structure never
      crosses the wire. Same as tags.

- [ ] **Decide what three faders at once come to.** Found 2026-09-23 while
      building the editor's fader panel, and it is a gap rather than a
      question anyone had parked: `DESIGN.md` § "The three faders are three
      routes to more" says each fader is a per-patch morph target and never
      says what two or three of them held together produce. The brain does not
      combine them at all yet, so nothing is wrong on the wall — but the first
      time two faders are up it will be.

      `tools/editor.html` proposes **the departures add**: each surface
      contributes its position times the distance from the patch to its own far
      end, and the sum is clamped per byte. It reduces to today's behavior for
      one fader, it leaves disjoint far ends alone, and it is the reading the
      color lane already uses for its three sources. The two alternatives are a
      weighted average, which makes one fader go weaker as another comes up,
      and a per-parameter winner, which needs a precedence rule nothing else in
      the rig has.

      Settle by looking, with the faders in hand. Whatever wins, the editor and
      the firmware have to say the same thing.

- [ ] **The editor's modulation, next.** Built 2026-09-24: a route lives on
      the control it moves — a **~** on the row opens its routes in a panel
      under it, the slider carries the band the routes reach and one mark per
      strip, and every track notches its center, the wave's named shapes and
      the steps of a stepped control. See `docs/editor.md` § "A route shows
      on the control it moves". What is left:

      - **Say the range in the control's own units** beside the fader, "8 →
        14 px/beat", from `render::convert` the way the labels already are.
        Deferred 2026-09-24 until the band alone is found wanting while
        dialing a route: little patch design has been done yet, and the
        readout numbers are not being looked at.
      - **Another visual pass** on the panel, its width above all, deferred
        until it has been used for a while.

- [ ] **Use the new editor at the bench.** `tools/editor.html` runs clean in a
      browser and has **never driven the rig**. The old page went anyway, since
      keeping it meant maintaining a second copy of the CC map through the
      route work and there is no assembled rig to test either against. Two
      things to watch for when there is: whether driving a morph's CCs at frame
      rate is too much traffic over USB, and whether the carrier/modulator
      split reads as well with the wall in front of you as it does on screen.
      Pushing a library is deferred with the rest of the brain's side — see
      § "Deferred until the model is stable".

- [ ] **Decide what a fader does when it disagrees with the state.**
      Jump on touch, pickup, or scaled takeover. Arrives with the first
      patch recall and with any DAW driving a CC a fader also owns. Needs
      a fader in hand, not a desk. See `DESIGN.md` § Open.

## Open on the controller

- [ ] **Decide where the peak-follower switch goes.** `docs/controls.md`
      is the record of every control on the box. The one thing it leaves
      open is that v1 read five switches on four pins, with A7 carrying
      fader-alt and preset-alt at once, and the rebuild drops the
      secondary board that the peak-follower switch reported to. Either it
      lands on the controller Teensy or it stays purely in-circuit and
      needs no pin. The rest of that shortfall disappears with the second
      board.

- [ ] **The foot pedal has no assignment.** Four momentary switches, and
      nothing in the map needs to change for them: the controller reads
      them and emits messages that already exist, the way the tap tempo
      button does. What is open is which four jobs they get. The keypad's
      hold is no longer part of this — it became CC 26 in the regroup.

## Key files

| When you want to… | Open |
|---|---|
| Know what we measured | `docs/bench-facts.md` |
| Know how the system is built | `docs/architecture.md` |
| Know what the lights do | `docs/visual-design.md` |
| Know how the generator works | `docs/generator.md` |
| Look up a pin or a circuit | `docs/wiring.md` |
| Look up a CC / PC / note number | `shared/aurora_protocol.h` |
| Work on how it is played | `DESIGN.md` |
| Build or dial a patch | `tools/editor.html`, and `docs/editor.md` for why it is shaped that way |

## Key commands

```bash
cd brain      && pio run -e teensy40 -t upload   # flash the brain
node tools/build-render.mjs                      # after changing shared/render/
cd controller && pio run -t upload               # future: controller node
pio device monitor                               # serial console
```
