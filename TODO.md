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
- **The nine hand-written patterns and all nine variants** render, on
  PC 1–9. See `docs/visual-design.md`.
- **The parametric generator** on PC 10, with the redesigned color layer
  in the firmware as of 2026-09-21. See `docs/generator.md`.
- **The pulse's destinations**, 2026-09-22. One oscillator with one rate
  reaching the strips' brightness, width and hue and the washes' level, hue
  offset and saturation, each with its own amount and its own wave. Its
  phase is anchored to the bar and its rate is stepped to the periods a bar
  can hold. Flashed but **not yet seen on the wall**. See
  `docs/generator.md` § "Where the pulse reaches".
- **The washes' own saturation**, 2026-09-22, on CC 35. A scale down from
  the strips' saturation, and the origin the pulse's push toward white
  measures from. Not yet seen on a fixture. See `DESIGN.md` § "The PAR
  cans".
- **Position**, 2026-09-22. Where a still pattern stands in its cell, on
  CC 66. What travel left over eases away while the pattern stands still,
  so a patch saved still comes back to the same place instead of standing
  wherever the last traveling one ran out. See `docs/generator.md` § "A
  still pattern stands where it is told".
- **The bench panel** — `tools/editor.html`, drawn as the signal flow, with
  the patch library, the morph control and a row of color looks. The routes
  are a third branch on it, since they reach both the others and the washes.
- **The wall on screen** — `tools/preview.js`, five strips and four PARs
  rendered from a port of the firmware, so a look can be dialed with
  nothing plugged in. The color layer was designed here before it was
  flashed.
- **The scatter**, 2026-09-23. Nine CCs at 83–91: a grid of cells, each on
  its own clock, each lighting a spot that pushes the strips' brightness, hue
  and whiteness. Ported from `tools/preview.js`, where it was designed, and
  checked sample for sample against it. Flashed but **not yet seen on the
  wall**. See `docs/generator.md` § "The scatter".
- **A strip-order rigging aid** on PC 11 — each strip a flat color in
  data-chain order.
- **The cross-check harness**, 2026-09-24. `node tools/crosscheck.mjs`
  lifts a function and its dependencies out of `brain/src/P_Generator.cpp`
  and out of `tools/preview.js`, drives both over the same grid and reports
  where they disagree. Six checks seeded, and it was shown to catch a
  deliberately broken edge fade in three of them at once. The two renderers
  are the same maths written twice and nothing else was keeping them
  honest.

Measurements in `docs/bench-facts.md`.

## What is not built

- **The brain's perfboard.** Everything is still on a breadboard.
- **DIN MIDI in on the brain** (6N138 circuit on `Serial1`).
- **The brain enclosure.** Blocked on the perfboard — the box should be
  cut around the finished board, not before it.
- **The controller.** Still the old Nano box. The second Teensy is
  untouched and its pin map has never been drawn.
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

Built 2026-09-22 and seen by nothing but the preview. One look each — and
one that is not a look at all, listed first because it gates the rest.

- [ ] **Run the patch sync against the brain.** Nothing below it has ever
      met hardware. In order, all from `tools/protocol.html` but the last:
      ask a freshly flashed brain what it holds and confirm it says empty;
      push eight patches and read them back byte for byte; push a half
      library and confirm the commit is refused and the previous one
      survives; save the library to a file and push that same file back;
      then pull the mains mid-sync and confirm the old library is still
      whole. Time a full 128 while you are there — the flash write is the
      likely cost, not the transfer.

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
- [ ] **Dial the ten fan looks**, in the *Fan looks* row of either page.
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
- [ ] **Pull CC 35 down, then open the pulse's PAR saturation.** Full
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
      `docs/generator.md` § "Open" item 11.

The pulse's destinations, the anchor against a click and the stepped rate
are unjudged too, and are listed below.

## Generator, next

None of this is committed to — it is an experiment that earned a second
session. Ordered by what blocks what. Reasoning in `docs/generator.md`.

- [ ] **Dial in five or six endpoints by eye and save them.** Nothing
      else about the morph is worth judging until these exist. See
      `docs/generator.md` § "The panel's roster settings are guesses".

**The color layer has been seen on a wall and holds up**, and the
preview it was designed in matches the strips closely enough to keep
designing there. See `docs/bench-facts.md`.

- [x] **Judge the color layer on the wall.** Done 2026-09-21. Flat is
      flat, and the preview matches the strips almost exactly — a touch
      paler on the wall, which is the diffuser. See `docs/bench-facts.md`
      § "The color layer's flat state is flat" and § "The screen and the
      wall agree".
- [x] **Average the placed field across each pixel.** Done 2026-09-22.
      Both branches are read at the same four samples now. Measured in the
      preview; not yet seen on the wall. See `docs/bench-facts.md` §
      "Point-sampling a pattern aliases".
- [ ] **Turn a region inside out.** One boolean, and it is what "base
      color on the center strip, outer ones departing" needs. See
      `docs/generator.md` § Open, item 8.
- [x] **Thread the placed field's parameters as an argument.** Done
      2026-09-22. A second field is now a second `PlacedField` rather than
      a second set of file statics. What the two would sum to is still
      open. See `docs/generator.md` § Open, item 10.
- [ ] **Randomness in color** — a starfield in hue rather than in
      brightness. The only randomness the color layer has no way to make.
      Absorbed 2026-09-22 into the scatter, where it is one destination
      rather than a project of its own, and the scatter's hue amount is
      already designed. See `docs/generator.md` § "The scatter".
- [x] **Give the PARs their own saturation.** Done 2026-09-22, on CC 35.
      A scale down from the strips' saturation: full matches them, zero is
      white. The pulse's push toward white measures from it, and the
      "the pulse reaches this" marker now has somewhere to sit for every
      destination. Not yet seen on a fixture. See `DESIGN.md` § "The PAR
      cans".
- [ ] **Reach the PARs with more than a hue offset.** The PARs following
      a gradient with the strips, which needs the color layer sampled at
      each PAR's position — today all four are one color. The white flash
      between strip strobes, the other half of this, became reachable on
      2026-09-22 when the pulse gained their saturation. See `DESIGN.md`
      § "The PAR cans".
- [x] **Fan gains a shape and a center.** Done 2026-09-23, as a wave with
      a frequency and a phase rather than a curve with a center — a V
      changes sign at most once across the wall, so it could not reach
      strips running opposite their neighbors. See `docs/generator.md`
      § "The fan is a wave".

- [ ] **Put the nine fan looks on the wall.** Dialed in the bench page's
      *Fan looks* row and rendered in `tools/preview.js` and the firmware,
      judged on neither. Four things it would settle are listed in
      `docs/generator.md` § Open, item 1 — the one that matters most is
      whether a quarter turn of phase gets Rain back, since Rain arriving
      as Comet is what started this.
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
- [ ] **Judge the pulse's destinations on the wall.** Everything else about
      them is guesswork until this happens. Three looks are what it was
      built for, and each is one patch: the washes swelling under still
      strips, a white flash on the washes between strip strobes, and the
      shapes breathing on width while the light holds. See
      `docs/generator.md` § "Where the pulse reaches".
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
- [ ] **Travel easing** — a Shape curve in Travel, beside Speed and Fan.
      See `docs/generator.md` § "Travel easing is a curve".
- [x] **Preserve position when bounce flips.** Done 2026-09-22; the phase
      is solved for rather than carried across. Alternate keeps its jump
      deliberately — it cannot be fixed without making it invisible under
      bounce, and a per-strip rate offset is going to replace it. See
      `DESIGN.md` § "Switches belong to the patch".
- [ ] **Decide whether a wrapping strip is a loop or a line.** Settled
      for bounce, open for wrap. See `docs/generator.md` § Open, "Is a
      strip a loop or a line?".
- [ ] **Decide the color layer's usable ranges.** The test is a set, not
      a bench. See `docs/generator.md` § Open, item 6.
- [ ] **Test the touchpad window from the laptop, before any rewire.**
      See `DESIGN.md` § "Which strips — a window, not a selection".

## Open discussions

Raised in conversation and not yet settled. Each needs a decision before
it becomes a build item.

- [ ] **Modulation routing — one clock, and routes to anything.** Raised
      2026-09-23 and half settled; `docs/modulation.md` is the whole of it.
      Settled there: one clock with a per-route ratio rather than several
      oscillators, ratios that multiply and never divide, no free-running
      modulator, a destination named by its own CC number, the pulse's
      eighteen destination CCs retired and the fifteen single-byte amounts
      kept. Six questions remain, and the one that decides the others is
      the wave — a selector at four CCs a route and twelve routes, or a
      detented sweep at six and eight. Resume there.

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

- [ ] **What a palette is, and where it lives.** Raised 2026-09-22 and
      deliberately not settled: a fixed set of palettes on the brain, with
      each patch referencing one, so a patch's controls move inside a
      constrained color space rather than the whole wheel. The patch record
      already carries a `palette` byte with no meaning attached, so the
      per-patch half costs nothing. What is genuinely open is where the
      palettes themselves live — a second section in the synced library is
      the obvious answer, and it is safe to defer because a format change
      costs a re-sync rather than a migration.
      **A palette is a switch**, settled 2026-09-22, which is why the byte
      sits in the patch head rather than in each parameter set: a far end
      cannot sit in a different palette from its patch. It lands the way
      every switch lands — on a patch change, or on release at the end of a
      journey or an accent. See `DESIGN.md` § "Switches belong to the patch".

      One wording job comes with it. `docs/visual-design.md` § "Palettes are
      shapes, not colors" uses the word for what is now a patch or a shape,
      and that section needs rewriting before anything here is built.

- [x] **Editor layout.** Answered 2026-09-23 by `tools/editor.html`, which is
      carrier, modulators and outputs rather than three branches — see
      `docs/editor.md`. The two boxes that earned no space are gone, and the
      join that said "the two multiply" went with them: it was not true of the
      pulse and is not true of the scatter, and what replaced it is a
      destination table that says where every source actually lands.

      What was asked, against the old page: `tools/index.html` grew the preview
      and the color controls without a regroup, and it is cramped. Two boxes earn no
      space: the one holding only the "send 120 BPM clock" button, never
      used, and "Strips", which is empty. The question is what the
      groupings should be, not where today's boxes go.
      The pulse left the shape branch on 2026-09-22 and became a third
      branch of its own, because it reaches the color layer and the washes
      as well and could no longer sit inside one of the two things it
      pushes on. Whether the join beneath still reads — it says "the two
      multiply", which is true of shape and color and says nothing about
      the third box now above it — is part of this question.
- [x] **The color panel does not say what its switches govern, and four
      controls are dead in the default state.** Answered 2026-09-23, and the
      first half turned out to be grouping alone: in `tools/editor.html` the
      placed field is a modulator card and its primitive, ruler and four shape
      controls are inside it with nothing else, so there is no longer anything
      for them to look as though they govern. No control moved and none was
      added.

      The second half was **ruled without the performer**, which is the half to
      reopen if it is wrong: the four dim under a gradient and say why. Giving
      Edge a meaning there — bending the ramp from straight to eased — is a
      firmware change and stays available. The subsection names went with the
      regroup, and the ruler buttons keep the strips' words rather than the
      wall's, because "within a shape" is not a direction and the swap would
      have stopped the three being one vocabulary.

      What was found, and still stands as the reason:  Gradient / region and the
      three rulers drive the placed field and nothing else —
      `placedIsRegion` and `placedRuler` are read nowhere outside that
      path in `brain/src/P_Generator.cpp` — but they sit above three
      subsections and read as though they govern all three.
      Worse, `placedAt` returns on its first line under gradient, so Count,
      Width, Edge and Speed — four of the seven controls under "What you
      place" — do nothing at all whenever that switch is on gradient, which
      is where it starts. The preview agrees, so they really are inert
      rather than merely subtle. Found by playing the panel and wondering
      why the faders did nothing.
      Two ways out, and the panel rework has to pick one. Indicate it —
      dim the four while gradient is selected, the way a bypassed branch
      already dims, so a control that cannot do anything stops looking
      like it should. Or give them a meaning there, which only one of the
      four can take: Count has nothing to repeat and Width nothing to
      size, since a gradient spans the ruler once by definition, and Speed
      cannot move it without wrapping a monotone ramp and putting a hard
      jump where the two ends meet. Edge is the one that could — bending
      the ramp from straight to eased is the same idea as pulse shape and
      travel easing, which would make it a fourth use of one word rather
      than a new control.
      Two more open parts. The subsection names "What you place", "What
      lives" and "From the light level" are the design's own words and are
      longer than the controls under them. And the ruler buttons name the
      strips where they could name the wall: "across the strips" is
      horizontal and "along a strip" is vertical, since the five stand
      spaced across the stage against the back wall. Whether that swap is
      an improvement is not obvious — "within a shape" is not a direction
      at all, so the three would stop being one vocabulary.
- [ ] **Travel easing — is the look wanted?** Built shape is settled in
      `docs/generator.md` § "Travel easing is a curve, not a modulation
      route", and it is a build item above. What has never been discussed
      is whether a band backdrop asks for a thrown-ball traversal.
- [ ] **Gate length on the pulse, as the far end of Shape.** A short stab
      rather than an even square. Wanted from both sides independently, so
      the want is real. Not a control of its own: fold it into the bottom
      of the Shape fader, so the one axis runs stab -> square -> swell ->
      sine. That is what makes it work — a gate only has a length where the
      wave is square, and putting it anywhere else on the fader would mean
      shifting the threshold at the sine end, which flat-bottoms the swell
      and stops it reaching full. Costs: the sine half loses a quarter of
      the fader, so the midpoint drifts from 93 back to about 102, and
      Strobe, Stutter and Glitch all move off 0.
      **Bounded by the frame rate, not by taste.** At 7-8 ms per frame
      (`docs/bench-facts.md`) and the fastest rate of 0.25 beats, a 6 %
      stab at 120 BPM is 8 ms, which is one frame — below that it lands
      between frames and flickers instead of shortening. A gate measured
      in percent of the cycle is therefore reliable at slow rates and not
      at fast ones; a floor derived from frame time would fix that.
      Build it only if a shorter stab is still wanted after playing with
      the linear taper, since the dead bottom of the fader that made this
      attractive is largely what the taper reclaimed.

- [x] **Fan's full shape, and a rate offset beside it.** Done 2026-09-23,
      the two together as one wave with three amounts. The claim that a
      rate of the opposite sign reproduces alternate holds under wrap and
      not under bounce, where a reversed strip stands in the same place at
      every instant — so the switch survives. `DESIGN.md` § "Alternate
      keeps its jump" said it would not; that is corrected there.

- [ ] **A modulator aimed at the fan's rate amount.** "The bars drift
      apart, come back into alignment, drift the other way" needs that
      amount swinging through zero, which is a sixth pulse destination.
      The objection that rates are not destinations does not bind here —
      a bipolar push integrates back to nothing once a cycle, so the
      strips realign rather than drifting permanently. What blocks it is
      three numbers in the pulse's own range, which is full at 100-114.
      See the regroup below.

- [x] **Apply the regrouped CC map.** Done 2026-09-23, and
      `docs/cc-regroup.md` is the map. Every number assigned, every
      consumer checked against the header — both firmwares by symbol,
      `tools/patch.js` and `tools/index.html` number by number. It added
      seven CCs that never existed: the three fader routes at 12-14, the
      fader-mode rocker at 23, the keypad's held gate at 26, and two for
      the audio section at 24-25 if the peak follower ever lands on a pin.
      CC 40's packed bitmap, the ten per-preset slots and jitter all
      retired. The editor's stored library was cleared rather than
      migrated, and no library had ever reached the brain.

      **The map is not out of numbers and never was** — what it had run
      out of was room in the right categories, which is the rule at the
      top of `shared/aurora_protocol.h`. It now stands at 81 assigned and
      33 spare, with room for one more pulse destination and nine more in
      the scatter.

- [x] **Decide what happens to the parked CCs.** Done 2026-09-23 with the
      regroup. The four v1 mode switches kept their slots and lost their
      meanings — they are `CC_ROCKER_PAD_A` through `_D` now, reporting
      where a rocker stands and saying nothing about what that does. The
      four touchpad gestures became `CC_PAD_X`, `_Y`, `_PRESSURE` and
      `_ENGAGE`, which is the right shape under every answer to what the
      pad is for, so numbering did not have to wait on that question.
      Nothing was reclaimed to settle a design question.

- [x] **Classify every CC as patch state, gesture or ambient.** Done
      2026-09-22. All 72 assigned CCs carry a tag in
      `shared/aurora_protocol.h`, which also states what each tag means:
      59 `[patch]`, 4 `[switch]`, 4 `[ambient]`, 4 `[gesture]`, and CC 40
      `[legacy]`. Two
      things the pass turned up. `[gesture]` and `[ambient]` answer all
      three storage questions identically — not saved, not recalled, not
      morphed — so they stay apart only on the fourth question, which is
      a property of the box rather than of the CC and is not recorded
      there. And which pattern runs arrives as a Program Change rather
      than a CC, so a patch holds one thing this classification does not
      reach.

- [x] **Patch storage — the editor-to-brain protocol.** Built 2026-09-22
      and **never run against hardware**. SysEx over USB, specified in
      `shared/aurora_protocol.h` § "System Exclusive" and reasoned in
      `DESIGN.md` § "How a library gets there". A sync replaces the whole
      library, streams in strictly ordered, stages to a file and goes live
      on one rename, so an interrupted sync leaves the previous library
      whole. `brain/src/patch_store.cpp` owns the flash,
      `brain/src/patch_sync.cpp` owns the wire, and `tools/protocol.html`
      pushes a library of generated bytes and reads it back to compare.

      A patch is 660 bytes and the Teensy core will not receive a SysEx
      message over 290 — `USB_MIDI_SYSEX_MAX` in `cores/teensy4/usb_midi.h`,
      a bare `#define` no build flag reaches. Staying under it is worth
      more than raising it: below that size the core hands over each
      message whole in one callback and nothing is reassembled.

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

- [x] **Render the scatter in the firmware.** Done 2026-09-23, the same day it
      was settled. `scatterAt`, the two reaches in `colorAt` and the
      `pushToward` on brightness that runs before an unlit pixel is culled.
      Checked against `tools/preview.js` rather than argued: both `scatterAt`
      implementations were driven over eight settings and 36 000 samples and
      agree everywhere but one, a sample sitting 1.2e-15 inside the core
      boundary at full width, where a float rounds onto the other side of the
      comparison. The wall has texture again.

- [ ] **Use the new editor at the bench.** `tools/editor.html` runs clean in a
      browser and has **never driven the rig**. The old page went anyway, since
      keeping it meant maintaining a second copy of the CC map through the
      route work and there is no assembled rig to test either against. Three
      things to watch for when there is: whether pushing a library actually
      lands (the SysEx path has never run against hardware either), whether
      driving a morph's CCs at frame rate is too much traffic over USB, and
      whether the carrier/modulator split reads as well with the wall in front
      of you as it does on screen.

- [ ] **Teach the brain to recall a patch.** Storage and the wire exist and
      nothing puts a patch on the wall — the only reader of the library is
      the export path. A patch arriving has to write the [patch] and [switch] CCs
      through the same handlers a live CC goes through, which is what makes
      the raw bytes worth storing. Needs the keypad-to-patch lookup at the
      same time — the controller still sends `PC = key number` in
      `scan_numpad()`, which only works while key N means preset N.

- [x] **The editor has no patch in the DESIGN sense.** Answered 2026-09-23 by
      a new editor rather than a rework: `tools/editor.html` with
      `tools/patch.js`, `tools/library.js` and `tools/editor.js`. It builds a
      whole patch — name, pattern, palette, both ramp times, the base and four
      far ends — pushes a library over SysEx, pulls one back, and reads and
      writes the JSON file. **Never run against hardware.** The model and the
      rulings made without the performer are `docs/editor.md`.

      Both gaps the reading found are closed. The CC map is 72 now, not 52:
      `CC_TEMPO_DIVISION` and the per-pattern slots at 50–59 are controls, and
      the scatter added nine. And the unowned bytes of a 128-byte set are
      written zero, because arriving at a patch writes the tagged CCs through
      the brain's own handlers and a byte no handler claims is never read.

      In the editor a far end is the base plus what it overrides, not a second
      copy of all 72 — see `docs/editor.md` § "A far end is an override".
      `materialize()` is where five whole sets appear for the wire.

      What the old page did, for the record:
      It
      saves a flat map of cooked parameter names to `localStorage`: one
      parameter set, no fader far ends, no accent target, no ramp times,
      and the pattern is not in it. Until that is reworked, nothing can
      build a patch to push — `tools/protocol.html` generates its bytes
      rather than dialing them. Coupled to the editor-layout question in
      § "Open discussions" and to the accent-preview item below.
      Two gaps found 2026-09-23 while reading it against the wire: the
      editor's `CC` map is 52 parameters where a patch saves 63, missing
      `CC_TEMPO_DIVISION` and the per-pattern slots at 50-59; and no
      control owns the remaining bytes of a 128-byte set, so what goes in
      them has to be ruled before a patch can be built.

- [x] **Take the preset buttons out of the editor's MIDI surface.** Done
      2026-09-23, and the two things reserved for the performer were ruled
      rather than asked, both cheap to reverse. Strip order and blackout became
      bench tools in the top bar that send a Program Change straight past the
      patch and say so. And what tells the brain to run the generator is the
      patch's own pattern byte: the editor sends `PC = pattern` when it puts a
      patch on the wall, which is the same message the old page hard-coded to
      10, sourced from the patch instead of from the page.

      What the old page said:
      **Take the preset buttons out of the editor's MIDI surface.**
      Program Change means patch now, so "generator (PC 10)", "Plasma
      (PC 3)", "blackout (PC 0)" and "strip order (PC 11)" in
      `tools/index.html` send patch selects. The anchor chips stay — they
      are local starting points — but `applyPatch` opens with
      `setPreset(10)` and that line goes with the box. Two things fall
      out and neither is mechanical: nothing then tells the brain to run
      the generator, so the editor cannot drive the wall until patch
      recall exists; and strip order is a rigging aid with nowhere left
      to live. See `DESIGN.md` § "Patch storage".

- [x] **Export the library to a file the repo can hold.** Done 2026-09-22,
      untested. `tools/protocol.html` saves what the brain holds as JSON —
      names, keypad assignment, every parameter set, one set per line so a
      changed patch is a few changed lines in a diff — and pushes such a
      file back. That is what stops the master library living on one
      laptop, and it is the recovery path if that laptop is lost.

- [x] **Keep the raw CC bytes in the brain.** Done 2026-09-22.
      `midi_in::ccBytes()` in `brain/src/midi_in.h` hands back the last
      byte received on each CC; `handleControlChange` records it beside
      the cooked value. Nothing reads it yet. One thing it does not
      solve: a CC nothing has sent reads 0, which is not what a bipolar
      control is rendering, so a snapshot taken before the brain has
      been driven records 0 rather than what is lit. Compiling a default
      set into the firmware is what closes that.

- [x] **The editor must preview an accent with the switches held back.** Done
      2026-09-23. The accent tab carries an **as heard from** picker naming any
      patch in the library, and the switches render from that patch for the
      whole audition. A and B come from two named sets of one patch rather than
      from "capture whatever is on screen", and within a patch a far end has no
      switches of its own to take.

      What the old page said:
      **The editor must preview an accent with the switches held back.**
      Falls out of switches landing on release, 2026-09-22: in
      performance an accent plays with the *source* patch's switches,
      not the destination's, so an accent dialed in `tools/index.html`
      against the patch's own switches is judged on a picture it will
      rarely show. See `DESIGN.md` § "Switches belong to the patch".
      Most of the machinery is already there: `applyMorph` interpolates
      everything except the four switches, which is exactly the rule. What
      it lacks is A and B coming from two named sets of one patch rather
      than from "capture whatever is on screen".

- [ ] **A default set compiled into the firmware**, so an empty brain
      still lights the wall. See `DESIGN.md` § "Patch storage". It must
      never be written to storage: the brain reporting an empty library is
      how the editor tells a fresh flash from a small library, and writing
      the defaults in would destroy that distinction. It must also leave
      the brain on a patch other than key 0, or the blackout gate added
      2026-09-23 holds the wall dark — `selectedPreset` starts at 0.
- [x] **What a completed morph does.** Settled 2026-09-22. A completed
      morph arrives only if it was going to a patch, so a fader never
      arrives and the surfaces table in `DESIGN.md` § "Switches belong to
      the patch" stands. Switches move when the key is released, which is
      always on a beat because every keypad effect is — held back through
      the accent deliberately, so that letting go drops the accent and
      changes the topology on one beat. Four consequences were written
      into `DESIGN.md`: a patch holds five parameter sets rather than
      four, the accent target being its own; there are two ramp times per
      patch, journey and accent, both stepped to values that come back to
      the grid; a journey release re-times the remaining distance to land
      on the next beat; and an accent release holds course to the next
      beat and then drops in one step, because there the drop is the
      gesture.

- [ ] **Decide what a fader does when it disagrees with the state.**
      Jump on touch, pickup, or scaled takeover. Arrives with the first
      patch recall and with any DAW driving a CC a fader also owns. Needs
      a fader in hand, not a desk. See `DESIGN.md` § Open.

- [ ] **Jump the pattern to Position on the beat.** A reset that fires on
      the grid rather than a place to sit: bars swiping up from the center,
      snapping back to the center on the beat and swiping again. It is the
      same family as the pulse's anchoring — something landing on the
      musical grid rather than wherever it drifted to — and it needed
      Position first, because a jump needs somewhere to jump to. Open
      questions: what fires it (every beat, a division, a switch), and
      whether the snap is instant or the settle it already has.

## Known defects

- [x] **Glitch's white pixels ignored the V fader.** Fixed 2026-09-22;
      white is made from the fader's own brightness now. The 30 % white
      share can be tuned, and has not been. See `docs/bench-facts.md`.

- [ ] **Both receivers ignore the MIDI channel.** The convention at the top
      of `shared/aurora_protocol.h` says all Aurora traffic is on
      `AURORA_MIDI_CHANNEL`, "so a shared cable / merger can carry other
      devices' traffic without confusion", and both senders honour it.
      Neither receiver does: `brain/src/midi_in.cpp` and
      `controller/src/midi_io.cpp` both take the channel byte and drop it
      with `(void)channel`, the latter commenting that it is permissive
      "for now". So the protection that comment describes has never
      existed, and on a shared cable every CC, Program Change and Note On
      in the rig reaches Aurora.

      Two lines, one per receiver, and the constant already exists. It
      does *not* protect against traffic on Aurora's own channel, which is
      why the CC map dodges pan, volume, expression and bank select
      anyway. It is also what makes a second channel available as 128 more
      numbers when the map runs out — see `docs/cc-regroup.md`.

- [x] **The three faders have no CC.** Fixed 2026-09-23 in the regroup:
      CC 12, 13 and 14 carry their positions, `[ambient]`, so the brain can
      morph toward the patch's Color, Extent and Motion sets. What a patch
      holds is separate, at 38-40. The controller still sends the sticks to
      the color, which is the next thing to change in
      `controller/src/controls.cpp`.

- [ ] **Decide where the peak-follower switch goes.** `docs/controls.md`
      is the record of every control on the box. The one thing it leaves
      open is that v1 read five switches on four pins, with A7 carrying
      fader-alt and preset-alt at once, and the rebuild drops the
      secondary board that the peak-follower switch reported to. Either it
      lands on the controller Teensy or it stays purely in-circuit and
      needs no pin. The rest of that shortfall disappears with the second
      board.

- [x] **Decide what the phone's cradle does.** Settled 2026-09-23: it
      stays the blackout, now driven through the end-of-frame gate so it is
      finally instant. There is a handset, but it was modded into a crude
      microphone on a guitar jack and almost never sits on the cradle, so
      the hook is held by a finger — `DESIGN.md` is corrected where it
      argued the hook is a state you cannot leave wrong.

- [ ] **The foot pedal has no assignment.** Four momentary switches, and
      nothing in the map needs to change for them: the controller reads
      them and emits messages that already exist, the way the tap tempo
      button does. What is open is which four jobs they get. The keypad's
      hold is no longer part of this — it became CC 26 in the regroup.

## Housekeeping

- [x] **Regroup the CC table.** Done 2026-09-23. `docs/cc-regroup.md` is
      the map, the blocks are contiguous and sized from what each grew
      into, and the two-block pulse and the stranded Position are both
      gone.

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
cd controller && pio run -t upload               # future: controller node
pio device monitor                               # serial console
```
