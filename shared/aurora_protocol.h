// aurora_protocol.h — MIDI protocol between the Aurora controller and brain.
//
// INCLUDED BY BOTH FIRMWARES. Any change here affects both sides. That is
// the whole point: one source of truth for the wire format. Changing a
// number here should always cause both nodes to rebuild — that's a
// feature, not a bug.
//
// ---------------------------------------------------------------------------
// Conventions
// ---------------------------------------------------------------------------
//   * All Aurora traffic is on MIDI channel AURORA_MIDI_CHANNEL (default 1).
//     Keep this narrow so a shared cable / merger can carry other devices'
//     traffic without confusion.
//   * Program Change selects DISCRETE state (preset, palette).
//   * Control Change carries CONTINUOUS parameters (faders, sculpt axes,
//     mode-flag bitfields).
//   * Note On is used for TRANSIENT events (trigger flash, tap tempo
//     informational edges). Note Off is currently unused.
//   * MIDI clock (0xF8) drives tempo. The brain never sees external clock
//     directly — the controller always re-emits. See
//     docs/architecture.md § "Clock routing: the controller is the source".
//
// ---------------------------------------------------------------------------
// Growing the protocol over time
// ---------------------------------------------------------------------------
//
// Each category below is laid out on aligned numeric ranges with large
// reserved gaps. Never assign a new CC / PC / note outside its stated
// range unless you are explicitly extending that category. The reserved
// slots are wiggle room — use the next free one, don't cram a new value
// into someone else's category.
//
// Checklist when adding a new MIDI message:
//   1. Pick the next free slot in the right range.
//   2. Add an enum entry AND a short comment describing what the message
//      does. A CC also needs a [patch] / [switch] / [gesture] / [ambient]
//      tag — see "What a patch change does to each CC".
//   3. If it changes behavior: update the matching doc under docs/.
//   4. Implement on both controller (emit) and brain (consume).
//
// ===========================================================================

#ifndef AURORA_PROTOCOL_H
#define AURORA_PROTOCOL_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Channel
// ---------------------------------------------------------------------------

static const uint8_t AURORA_MIDI_CHANNEL = 1;

// ---------------------------------------------------------------------------
// Program Change — discrete state selection
// ---------------------------------------------------------------------------
//
// Layout:
//      0 –   9 : preset select (0 = off, 1–9 = preset slots)
//           10 : the parametric generator (experiment; see P_Generator.cpp)
//           11 : strip-order rigging aid
//     11 –  63 : RESERVED for preset expansion (more slots, banks)
//     64 –  72 : palette select (palettes 0–8)
//     73 – 126 : RESERVED for palette expansion / future discrete states
//          127 : RESERVED — interpreted by the brain as "no palette,
//                monochrome" (pairs with numpad 0 in palette mode).
//
// ---------------------------------------------------------------------------

enum AuroraPreset : uint8_t {
    PRESET_OFF               = 0,
    // Row 1 — ambient
    PRESET_FILL_OR_STARFIELD = 1,
    PRESET_BREATHE_OR_WAVE   = 2,
    PRESET_PLASMA_OR_AURORA  = 3,
    // Row 2 — groove
    PRESET_PULSE_OR_BARS     = 4,
    PRESET_SWEEP_OR_CROSS    = 5,
    PRESET_RAIN_OR_STORM     = 6,
    // Row 3 — intensity
    PRESET_STRIP_OR_COMET    = 7,
    PRESET_STROBE_OR_STUTTER = 8,
    PRESET_CHAOS_OR_GLITCH   = 9,
    // One pattern whose shape comes entirely from CC 70–79 rather than
    // from a hand-written renderer. Under evaluation; it does not replace
    // any slot above.
    PRESET_GENERATOR         = 10,
    // Rigging aid rather than a look: each strip a flat hue, so the order of
    // the data chain can be read off the wall while the strips are being hung.
    PRESET_STRIP_ORDER       = 11,
    // 12–63 reserved
};

static const uint8_t AURORA_PC_PALETTE_BASE      = 64;
static const uint8_t AURORA_PC_PALETTE_COUNT     = 9;   // palettes 0–8
static const uint8_t AURORA_PC_PALETTE_MONOCHROME = 127; // "no palette"

// Helpers

static inline bool aurora_pc_is_preset(uint8_t pc)  { return pc <= PRESET_STRIP_ORDER; }
static inline bool aurora_pc_is_palette(uint8_t pc) {
    return pc >= AURORA_PC_PALETTE_BASE
        && pc <  AURORA_PC_PALETTE_BASE + AURORA_PC_PALETTE_COUNT;
}
static inline uint8_t aurora_pc_palette_index(uint8_t pc) {
    return pc - AURORA_PC_PALETTE_BASE;
}

// ---------------------------------------------------------------------------
// Control Change — continuous parameters
// ---------------------------------------------------------------------------
//
// Layout:
//      0 –   9 : AVOID (standard MIDI: bank select, modulation, etc.)
//     10 –  19 : transport / meta control
//     20 –  29 : color — three faders plus the color field
//     30 –  39 : touchpad / sculpt mode
//     40 –  49 : mode flags & switches
//     50 –  59 : per-preset parameter slots (interpretation is per preset)
//     60 –  69 : washes / DMX fixtures
//     70 –  79 : generator shape parameters
//           80 : generator pulse shape
//     81 –  89 : the scatter — a texture source and its three amounts
//     90 –  99 : color, second half (twenty controls will not fit in ten)
//    100 – 114 : the pulse's destinations, three apiece
//           115 : where a still pattern stands — a shape control that did
//                 not fit in 70–79
//    116 – 119 : RESERVED
//    120 – 127 : AVOID (standard MIDI: channel mode messages)
//
// Eighteen of these have no case in the brain's handleControlChange: the
// sculpt axes at 30–33, the touchpad and hold modes at 41–44, and the
// per-preset slots at 50–59. They are controller controls, and the
// controller's feature set is not finalized — the wiring waits on that, not
// on a decision here. Not dead numbering, and not a question about the
// protocol.
//
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// What a patch change does to each CC
// ---------------------------------------------------------------------------
//
// A patch is one byte per CC — DESIGN.md § "Patch storage" — so every CC in
// the enum below carries a tag saying what a patch does with it. Three
// questions, which come apart only at the switches:
//
//   | tag       | a patch saves it | arriving writes it | a morph slides it |
//   |-----------|------------------|--------------------|-------------------|
//   | [patch]   |       yes        |        yes         |        yes        |
//   | [switch]  |       yes        |        yes         |        no         |
//   | [gesture] |       no         |        no          |        no         |
//   | [ambient] |       no         |        no          |        no         |
//   | [legacy]  |       —          |        —           |        —          |
//
//   * [patch] is the look. Arriving at a patch overwrites it and a morph
//     interpolates the raw byte on the way. A DAW automation lane on one of
//     these is fighting the keypad for the same number.
//   * [switch] is the look too, but it has no middle, so nothing can
//     interpolate it: it moves only when a keypad press settles. See
//     DESIGN.md § "Switches belong to the patch".
//   * [gesture] is where a hand is at this instant — thumb position,
//     pressure. It has no resting value, so there is nothing to save.
//   * [ambient] is the box rather than the look: the standing position of
//     each switch on the panel. A patch change leaves these alone, and has
//     to — the brain cannot move a maintained switch, so writing one would
//     leave the panel and the state disagreeing with nothing able to
//     reconcile them.
//   * [legacy] is a v1 leftover on its way out. The questions do not apply
//     to it and it is not a design decision anyone needs to revisit.
//
// [gesture] and [ambient] answer all three questions the same way. They stay
// apart because a fourth question — does a hand move this live — is not
// answered here at all. Which CCs a surface owns changes as the controller
// is built, so that answer belongs in the controller firmware; it is what
// decides where DESIGN.md § Open's takeover question lands.
//
// Which pattern runs arrives as a Program Change rather than a CC, and a
// patch holds it, so there is one thing in a patch this table does not
// reach. The palette numbers at PC 64–126 are reserved for something that
// is not built and the brain ignores them — see handleProgramChange in
// brain/src/midi_in.cpp.
//
// ---------------------------------------------------------------------------

enum AuroraCC : uint8_t {
    // 10–19 — transport / meta
    //
    // A half-time look is a real musical idea, and a song that wants one
    // wants it for every patch in that song — so a patch that needs the
    // other feel is a duplicate with a different division, not a reason to
    // keep this out of a patch.
    CC_TEMPO_DIVISION      = 10, // [patch] note value one tempo pulse stands
                                 // for; value is an AuroraTempoDivision
                                 // index
    // 11–19 reserved (transport / meta)

    // 20–29 — color, first half. 20–22 are the three faders; 23–29 carry
    // the placed field. The second half is at 90–99.
    //
    // A color is hue, whiteness and darkness. Everything below is a push on
    // those three, measured from the faders, and the pushes add — so with
    // every one of them centered the wall is exactly the color on the faders.
    // See P_Generator.cpp § "The color layer".
    CC_HUE                 = 20, // [patch] hue center / H fader
    CC_SATURATION          = 21, // [patch] saturation / S fader
    CC_VALUE               = 22, // [patch] brightness / V fader

    // The placed field is something aimed: a gradient running one way across a
    // ruler with the faders' color at its center, or regions sitting on that
    // ruler. Count, width and edge mean here exactly what they mean in the
    // shape block below.
    // 23 free — the primitive and the ruler moved to 47 and 48, where a
    // sender can set one without having to know the other.
    CC_PLACED_HUE          = 24, // [patch] bipolar: 64 is flat, either side
                                 // is how far ONE end departs — the two ends
                                 // land twice that far apart
    CC_PLACED_WHITE        = 25, // [patch] bipolar: up is toward white at one
                                 // end, down is toward a pure hue
    CC_PLACED_DARK         = 26, // [patch] bipolar: down is toward dark, up
                                 // toward full — which needs the V fader left
                                 // below the top to have anywhere to go
    CC_PLACED_COUNT        = 27, // [patch] regions along the ruler, 1–20.
                                 // Ignored by a gradient, which spans the
                                 // ruler once
    CC_PLACED_WIDTH        = 28, // [patch] a region's solid core, as a
                                 // proportion of one cell
    CC_PLACED_EDGE         = 29, // [patch] hard-edged region through to a
                                 // smooth fade

    // 30–39 — touchpad / sculpt
    CC_SCULPT_Y            = 30, // [gesture] touchpad Y in sculpt mode
                                 // (per-preset axis)
    CC_SCULPT_X            = 31, // [gesture] touchpad X in sculpt mode
                                 // (usually strip select)
    CC_TOUCH_PRESSURE      = 32, // [gesture] raw pressure, 0–127
    CC_TOUCH_ACTIVE        = 33, // [gesture] 0 = not touched, 127 = touched
    // 34–39 reserved (touchpad)

    // 40–49 — mode flags & switches. 41–44 are the standing positions of
    // switches on the box, which is what makes them [ambient].
    CC_MODE_FLAGS          = 40, // [legacy] v1 flag bitmap. Not a pattern to
                                 // follow and not an open question — see
                                 // "CC_MODE_FLAGS bits" below before raising
                                 // it.
    CC_TOUCHPAD_STRIP_MODE = 41, // [ambient] 0 = mirrored, 64 = all,
                                 // 127 = exclusive
    CC_TOUCHPAD_EFFECT     = 42, // [ambient] 0 = paint/fill, 64 =
                                 // paint/invert, 127 = sculpt
    CC_HOLD_MODE           = 43, // [ambient] 0 = off, 127 = on
    CC_VERTICAL_MODE       = 44, // [ambient] 0 = Y-modulates-saturation,
                                 // 127 = Y-modulates-hue

    // One switch per CC, because a CC cannot be read back: nothing here can
    // ask the brain what the other switches are currently set to, so a sender
    // that packed several into one byte would have to know all of them to
    // change any one of them. A sequencer setting "regions" would silently
    // put the ruler back across the wall. Splitting them also makes each one
    // an ordinary switch lane in a DAW rather than a number to be looked up.
    //
    // Everything here reads as off below 64 and on from 64 up, except the
    // ruler, which is banded like CC_TOUCHPAD_STRIP_MODE above.
    //
    // These four are the ones DESIGN.md § "Switches belong to the patch"
    // argues about: saved and recalled, never interpolated.
    CC_GEN_ALTERNATE       = 45, // [switch] odd strips run the journey
                                 // backwards
    CC_GEN_BOUNCE          = 46, // [switch] turn at the cell's edge instead
                                 // of wrapping
    CC_COLOR_REGION       = 47, // [switch] 0 = one gradient across the ruler,
                                 // 127 = regions
    CC_COLOR_RULER        = 48, // [switch] 0 = across the five strips, 64 =
                                 // along a strip, 127 = within a shape
    // 49 reserved (mode flags)

    // 50–59 — per-preset parameter slots
    //
    // These are deliberately generic. Each preset decides what its own
    // slots A–J mean. The controller shouldn't need to know — it just
    // passes the value through. Brain-side documentation lives next to
    // the preset implementation.
    //
    // All ten are [patch]. A slot means different things under two presets,
    // so morphing between patches that sit on different presets interpolates
    // a number whose meaning changes underneath it — which is true of the
    // whole parameter set when the preset differs, not of these slots alone.
    CC_PRESET_PARAM_A      = 50, // [patch]
    CC_PRESET_PARAM_B      = 51, // [patch]
    CC_PRESET_PARAM_C      = 52, // [patch]
    CC_PRESET_PARAM_D      = 53, // [patch]
    CC_PRESET_PARAM_E      = 54, // [patch]
    CC_PRESET_PARAM_F      = 55, // [patch]
    CC_PRESET_PARAM_G      = 56, // [patch]
    CC_PRESET_PARAM_H      = 57, // [patch]
    CC_PRESET_PARAM_I      = 58, // [patch]
    CC_PRESET_PARAM_J      = 59, // [patch]

    // 60–69 — washes / DMX fixtures. All three are [patch]: DESIGN.md
    // § "What a patch holds for them" names level, hue offset and saturation
    // as the whole of what a patch keeps for the PARs.
    CC_WASH_LEVEL          = 60, // [patch] wash master, independent of the
                                 // strips so the washes can be pulled down
                                 // under a running pattern. PRESET_OFF
                                 // overrides it and darkens them: numpad 0 is
                                 // an emergency stop, and one key has to kill
                                 // the rig on its own.
    CC_WASH_HUE_OFFSET     = 61, // [patch] rotates the washes off the strips'
                                 // hue, so they can sit complementary or
                                 // merely adjacent instead of matching. 0
                                 // matches; 64 of 127 is the opposite side of
                                 // the wheel.
    // Scales the washes down from the strips' saturation: 127 matches them,
    // 0 is white. A relationship rather than a color of their own, like
    // every other wash control — see DESIGN.md § "The PAR cans". It is also
    // where the pulse's push at 112–114 measures from, which it could not do
    // while the only saturation in the rig was the strips' own fader.
    CC_WASH_SATURATION     = 62, // [patch]
    // 63–69 reserved (washes)

    // 70–79 — generator shape. Only read while PRESET_GENERATOR is active.
    // All [patch]: this block is the look, and the three fader routes are
    // built out of it.
    CC_GEN_WIDTH           = 70, // [patch] how much of one cell the shape
                                 // covers
    CC_GEN_COUNT           = 71, // [patch] how many shapes along the strip,
                                 // 1–20
    CC_GEN_EDGE            = 72, // [patch] symmetric softness at both ends
    CC_GEN_TAIL            = 73, // [patch] asymmetric fade behind the shape
                                 // only
    CC_GEN_SPEED           = 74, // [patch] bipolar: 64 is still, either side
                                 // travels
    // 75, 99 and 119 are the fan's three amounts, and 116-118 shape the wave
    // they share. One wave runs across the five strips; each amount decides
    // how far it pushes one quantity, so a wall of staggered bars can strobe
    // in unison. A single offset reaching everything cyclic could not.
    //
    // Bipolar, and 100 % spreads the five strips over exactly one cell. Both
    // ends of the range are the same wall with the wave turned over, and at
    // the very top the two outer strips come back into step with each other,
    // for the same reason CC 115's two ends are one place.
    CC_GEN_FAN             = 75, // [patch] how far apart the five strips
                                 // stand in their cells
    // Superseded by the scatter at 81–89 and kept only while the firmware
    // still renders it. See the note there.
    CC_GEN_JITTER          = 76, // [patch] randomness in position and
                                 // brightness
    // The pulse is one oscillator with one rate, and 77/79/80 are its
    // amount and its wave where it reaches the strips' brightness. Every
    // other destination carries its own three at 100–114.
    CC_GEN_PULSE_DEPTH     = 77, // [patch] how far the trough digs below full
                                 // light
    CC_GEN_PULSE_RATE      = 78, // [patch] beats per swell; stepped, see
                                 // AURORA_PULSE_PERIODS below
    CC_GEN_PULSE_SKEW      = 79, // [patch] bipolar: 64 is an even rise and
                                 // fall, either side slides the peak toward a
                                 // ramp
    CC_GEN_PULSE_SHAPE     = 80, // [patch] 0 = hard on/off square, 127 =
                                 // smooth sine

    // 81–89 — the scatter.
    //
    // The third modulation source, and the first one with a position: the
    // pulse is a value over time with nowhere on the wall, the wander is
    // smooth over both, and this one is random over both. A grid of cells
    // along a strip, each with its own clock, each lighting a spot that
    // appears, holds, fades, and may slide across its own cell as it does.
    // Stateless — a cell's clock comes out of a hash, so there is no
    // particle list to keep.
    //
    // It is what CC_GEN_JITTER should have been. Jitter deforms the shape
    // branch from inside its own sampling and is therefore not a value that
    // can be aimed anywhere, which is why it can only ever take light away
    // and why its grain is always one pixel wide. See docs/generator.md
    // § "What jitter is for". CC 76 stays where it is until the firmware
    // renders this block; the two are not meant to coexist for long.
    //
    // All [patch]. The three amounts are bipolar with 64 as no push, and the
    // sign picks which limit the push runs toward, exactly as the pulse's do
    // at 100–114 — so a spot inside an already-full shape has nowhere to go
    // and is covered by it with no occlusion rule anywhere.
    CC_SCATTER_RATE        = 81, // [patch] how often a cell relights
    CC_SCATTER_COUNT       = 82, // [patch] cells along a strip, 1–20; the
                                 // same unit as CC_GEN_COUNT
    // The spot's core on both axes at once: how much of its cell it covers,
    // and how much of its cycle it is lit. One quantity rather than a size
    // and a duration, which is what keeps this block to nine slots.
    CC_SCATTER_WIDTH       = 83, // [patch]
    CC_SCATTER_EDGE        = 84, // [patch] hard through to a fade, in space
                                 // and in time alike
    // 0 puts every cell on one clock, so the whole wall flashes as one; full
    // scatters their phases and rates and they stop blinking together.
    // Moving it re-keys every cell, so everything in flight jumps — the
    // price of holding no state, and confined to this one control.
    CC_SCATTER_STAGGER     = 85, // [patch]
    CC_SCATTER_DRIFT       = 86, // [patch] bipolar: how far, and which way, a
                                 // spot slides across its own cell over its
                                 // life. A displacement, not a rate, which is
                                 // why it is not named Speed
    CC_SCATTER_LIGHT       = 87, // [patch] amount toward full light / toward
                                 // dark. It pushes what the shape branch
                                 // left, so it needs a gap to light and
                                 // light to darken
    CC_SCATTER_HUE         = 88, // [patch] amount, bipolar, up to half the
                                 // wheel each way
    CC_SCATTER_WHITE       = 89, // [patch] amount toward white / toward a
                                 // pure hue

    // 90–99 — color, second half. Twenty controls will not fit in ten slots,
    // so color stays in two blocks; what makes this a half rather than an
    // overflow is that the split falls between whole ideas. 23–29 is the
    // placed field, this is everything that is not aimed anywhere. All
    // [patch].
    CC_PLACED_SPEED        = 90, // [patch] bipolar: 64 is still, either side
                                 // drifts the regions along the ruler

    // The wander: color never quite the same in two places, with the
    // difference always moving. Two terms at the golden ratio, so it cannot
    // come back into step and never repeats — built in rather than dialed,
    // because dialing how far apart two speeds sit is operating the
    // mechanism rather than the look.
    CC_WANDER_HUE          = 91, // [patch] bipolar: how far the hue wanders
                                 // either side
    CC_WANDER_WHITE        = 92, // [patch] bipolar: how far whiteness wanders
    CC_WANDER_DARK         = 93, // [patch] bipolar: how far darkness wanders
    CC_WANDER_RATE         = 94, // [patch] 0 = frozen, up to two beats per
                                 // cycle
    CC_WANDER_SCALE        = 95, // [patch] 0 = the whole wall moving as one,
                                 // 127 = individual pixels shimmering

    // Color read off how lit the shape branch left a pixel. The one source
    // that reaches the pulse and jitter, since neither has a position for a
    // ruler to measure. Anchored at the dim end: the faders are what a fade
    // runs out to, and the core is the departure.
    CC_LIT_HUE             = 96, // [patch] bipolar: 64 = none, +-64 hue at
                                 // the core
    CC_LIT_WHITE           = 97, // [patch] 0 = none, up = white at the core
    CC_LIT_DARK            = 98, // [patch] bipolar: 64 = none, down takes the
                                 // core toward dark and up toward full
    // Bipolar. The one amount that does nothing to where a shape stands: it
    // offsets where each strip sits in the swell, which is what turns a
    // strobe into a chase across the wall. The washes take the unfanned
    // phase whatever it says — a PAR is one position with no strip to be
    // offset from.
    //
    // It sits here, away from the rest of the fan, because 116-119 were the
    // last four free numbers and the family needs five. The regroup CC 115
    // waits for is where it should join them.
    CC_GEN_FAN_PULSE       = 99, // [patch] how far the five strips run out of
                                 // step in the swell

    // 100–114 — where else the pulse reaches. One oscillator, one rate: a
    // destination sets how far it is pushed and what wave pushes it, never
    // how fast. Three per destination, always in the order amount, shape,
    // skew, so the block reads as a table.
    //
    // Every destination is always connected and its amount may be zero,
    // because a morph can interpolate an amount and cannot snap a
    // connection on — see DESIGN.md § "Switches belong to the patch". That
    // is also why all fifteen are [patch] and none of them is a [switch].
    //
    // Amounts are bipolar and 64 is no push. The sign picks which of the
    // destination's two limits the push runs toward, so it can never clip
    // and a control already sitting at a limit has nowhere to go that way.
    // The strips' brightness at 77 is the exception and is unipolar: there
    // is nothing above full light, so its only direction is down.
    CC_PULSE_WIDTH         = 100, // [patch] toward full width / toward nothing
    CC_PULSE_WIDTH_SHAPE   = 101, // [patch]
    CC_PULSE_WIDTH_SKEW    = 102, // [patch]

    // Added after the placed field, the wander and the light level have
    // summed — one push on the color layer's output rather than one per
    // source, which leaves that layer's design alone.
    CC_PULSE_HUE           = 103, // [patch] bipolar, up to half the wheel
                                  // each way
    CC_PULSE_HUE_SHAPE     = 104, // [patch]
    CC_PULSE_HUE_SKEW      = 105, // [patch]

    // The washes. A PAR is one position with no length, so the shape branch
    // cannot reach it and the pulse can — see DESIGN.md § "The PAR cans".
    CC_PULSE_PAR_LEVEL     = 106, // [patch] toward full / toward dark
    CC_PULSE_PAR_LEVEL_SHAPE = 107, // [patch]
    CC_PULSE_PAR_LEVEL_SKEW  = 108, // [patch]

    CC_PULSE_PAR_HUE       = 109, // [patch] bipolar, up to half the wheel
                                  // each way
    CC_PULSE_PAR_HUE_SHAPE = 110, // [patch]
    CC_PULSE_PAR_HUE_SKEW  = 111, // [patch]

    // Toward white is the flash between strip strobes that DESIGN.md
    // records as asked for. Measured from CC 62, so pulling the washes
    // pale leaves the flash less far to travel.
    CC_PULSE_PAR_SAT       = 112, // [patch] toward a pure hue / toward white
    CC_PULSE_PAR_SAT_SHAPE = 113, // [patch]
    CC_PULSE_PAR_SAT_SKEW  = 114, // [patch]

    // A shape control, and it belongs in the 70–79 block. That block was
    // full before this was wanted, and moving one control on its own would
    // mean renumbering a controller twice — once now and once at the
    // regroup. It waits here for the regroup.
    //
    // Bipolar: 64 is the middle of the cell, and half a cell each way covers
    // every place a shape can stand, because the pattern repeats once per
    // cell. Read only while the pattern is still; travel sets its own place.
    //
    // [patch], with one thing to watch when a morph is built: the two ends
    // of this control are the same place on the wall, so interpolating from
    // one end toward the other slides the shape the long way across the cell
    // rather than across the seam.
    CC_GEN_POSITION        = 115, // [patch] where a still pattern stands in
                                  // its cell

    // 116-119 — the fan's wave, shared by all three amounts at 75, 99 and
    // 119. Frequency and phase are one LFO running across the strips instead
    // of through time; randomize crossfades the five toward a fixed draw.
    //
    // Frequency stops at half a cycle per strip because five strips cannot
    // sample anything faster: there every strip sits opposite its neighbors,
    // which is alternate. Two things to know at that end of the fader — the
    // phase only scales how deep the alternation is rather than moving it,
    // and at a quarter and three quarters of a turn it reads zero on every
    // strip and the fan goes quiet.
    // Stepped to eighths of a turn across the wall, seventeen positions, and
    // the phase runs on 128ths of a turn rather than 127ths. Both because the
    // two together have to read *exactly* zero on a strip: a strip the wave
    // reads near zero at is not still, it crawls, and half a pixel a beat
    // crosses the strip in a minute. Still has to mean still here for the
    // same reason it does on CC 74, and an eighth of a turn is not a number
    // 127 steps can land on. A whole turn is the same wall as none, which is
    // what makes 128 the right divisor for the phase.
    CC_GEN_FAN_FREQ        = 116, // [patch] 0 = all five alike, up to two
                                  // turns across the wall
    CC_GEN_FAN_PHASE       = 117, // [patch] where the wave sits on the
                                  // strips: a staircase through a chevron
    CC_GEN_FAN_RANDOM      = 118, // [patch] 0 = the wave, 127 = a fixed draw
                                  // per strip
    // Bipolar, and an absolute speed added to CC 74's, not a proportion of
    // it. So Speed is what the strip the wave reads zero at travels at, and
    // this is how far the others differ from it — which is what puts a still
    // strip in the middle of a moving wall, or at its ends.
    //
    // On the same squared curve as CC 74, so that mirroring one about its
    // center against the other cancels exactly. Standing the wave's *peak*
    // still needs that cancellation, and two controls on different curves can
    // only ever nearly cancel — which leaves a crawl rather than a standstill.
    CC_GEN_FAN_RATE        = 119, // [patch] how far apart the five strips'
                                  // speeds stand
};

// ---------------------------------------------------------------------------
// Tempo division (carried on CC_TEMPO_DIVISION)
// ---------------------------------------------------------------------------
//
// The controller always emits true 24-PPQN clock — never a pre-divided
// rate — so the brain always knows the real tempo. This value says how
// often the brain should turn those ticks into a tempo pulse.
//
// Every division below is a whole number of ticks, so triplets are exact.
//
// QUARTER is 0 so that a controller which has not yet sent this CC, or
// which sends 0 on connect, lands on the ordinary one-pulse-per-beat
// behavior rather than something exotic. The order here is therefore not
// musical; the controller maps its rotary positions onto it.
//
// ---------------------------------------------------------------------------

enum AuroraTempoDivision : uint8_t {
    TEMPO_DIV_QUARTER        = 0, // 24 ticks — one pulse per beat
    TEMPO_DIV_BAR            = 1, // 96 ticks — one pulse per 4/4 bar
    TEMPO_DIV_HALF           = 2, // 48 ticks
    TEMPO_DIV_EIGHTH         = 3, // 12 ticks
    TEMPO_DIV_EIGHTH_TRIPLET = 4, //  8 ticks
    TEMPO_DIV_SIXTEENTH      = 5, //  6 ticks
    TEMPO_DIV_COUNT          = 6,
};

static const uint16_t AURORA_TICKS_PER_BEAT = 24;

// ---------------------------------------------------------------------------
// The pulse's periods (carried on CC_GEN_PULSE_RATE)
// ---------------------------------------------------------------------------
//
// Stepped rather than continuous, because the pulse's phase is anchored to
// the musical grid and only a period a bar holds a whole number of can land
// on a downbeat. A period of 2.64 beats is in time and never on time: it
// walks through the bar for ever and no anchoring can stop it.
//
// Halves and their dotted values. The dotted ones do not divide a 4/4 bar
// on their own — three beats comes back to the downbeat every three bars —
// which is a musical relationship rather than a drift.
//
// In animation beats, so the tempo division rotary scales the whole table.
//
// ---------------------------------------------------------------------------

static const float AURORA_PULSE_PERIODS[] = {
    16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 0.375f, 0.25f,
};
static const uint8_t AURORA_PULSE_PERIOD_COUNT =
    sizeof(AURORA_PULSE_PERIODS) / sizeof(AURORA_PULSE_PERIODS[0]);

static inline float aurora_pulse_period(uint8_t value) {
    const uint8_t last = AURORA_PULSE_PERIOD_COUNT - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return AURORA_PULSE_PERIODS[step > last ? last : step];
}

static inline uint16_t aurora_ticks_per_gate(uint8_t division) {
    switch (division) {
        case TEMPO_DIV_BAR:            return 96;
        case TEMPO_DIV_HALF:           return 48;
        case TEMPO_DIV_EIGHTH:         return 12;
        case TEMPO_DIV_EIGHTH_TRIPLET: return 8;
        case TEMPO_DIV_SIXTEENTH:      return 6;
        default:                       return AURORA_TICKS_PER_BEAT;
    }
}

// ---------------------------------------------------------------------------
// Switches and banded choices
//
// A switch is a switch rather than a knob because it has no middle: half of
// "runs opposite" is not a state, and neither is half of "turns around at the
// end". A morph therefore cannot interpolate one, and by the same argument
// nothing else should try — see DESIGN.md § "Switches belong to the patch".
// ---------------------------------------------------------------------------

static inline bool aurora_cc_is_on(uint8_t value) { return value >= 64; }
static inline uint8_t aurora_cc_switch(bool on)   { return on ? 127 : 0; }

// Three-way, banded the way CC_TOUCHPAD_STRIP_MODE is: a third of the range
// each, so 0, 64 and 127 land squarely in the middle of their own band.
static inline uint8_t aurora_cc_band3(uint8_t value) {
    if (value < 43) return 0;
    if (value < 86) return 1;
    return 2;
}

enum AuroraColorRuler : uint8_t {
    COLOR_RULER_WALL  = 0, // position is which of the five strips a pixel is on
    COLOR_RULER_STRIP = 1, // position is how far along its strip a pixel is
    COLOR_RULER_SHAPE = 2, // a shape's leading tip through to the end of its
                            // tail, traveling with it
};

// ---------------------------------------------------------------------------
// CC_MODE_FLAGS bits — LEGACY. Do not extend, do not copy, do not reopen.
// ---------------------------------------------------------------------------
//
// THE RULE, settled: one flag, switch or banded choice per CC. Nothing packs
// several into one byte. A CC cannot be read back, so a sender wanting to
// change one packed flag would have to already know all the others — the
// argument is written out in full at CC 45–48.
//
// CC 40 is the one thing in the protocol that breaks that rule. It predates
// it. It survives only because the bits below still reach v1 code paths that
// have not been ripped out yet, and the features behind them are retired:
// variant switching is v1 functionality, not something waiting to be
// finished. There are deliberately no spare bits here. A new flag gets its
// own CC in 40–49.
//
// ---------------------------------------------------------------------------

enum AuroraModeBits : uint8_t {
    MODE_BIT_FADER_ALT         = 1 << 0, // retired: hue oscillation around
                                         // base, superseded by the wander at
                                         // CC 91–95
    MODE_BIT_PRESET_ALT        = 1 << 1, // retired: variant switching
    MODE_BIT_PALETTE_ANIMATION = 1 << 2, // never built; nothing reads it
    // bit 7 is the high bit of a 7-bit CC value and must stay 0
};

// ---------------------------------------------------------------------------
// Note On — transient events
// ---------------------------------------------------------------------------
//
// Layout:
//      0 –  59 : AVOID (common musical pitch range — keep Aurora events
//                in the control range to avoid accidental triggering
//                from instruments sharing the cable)
//     60 –  69 : trigger events
//     70 –  79 : preset-specific event slots (e.g. "reset animation now")
//     80 –  95 : RESERVED
//     96 – 127 : RESERVED (high control range)
//
// Velocity is a free payload. Currently unused for everything except
// trigger strength.
//
// ---------------------------------------------------------------------------

enum AuroraNote : uint8_t {
    // 60–69 — trigger events
    NOTE_TRIGGER_FLASH   = 60, // mic / external trigger — velocity = strength
    NOTE_TAP_TEMPO_EDGE  = 61, // informational; brain usually ignores
    // 62–69 reserved (trigger events)

    // 70–79 — preset-specific event slots
    NOTE_PRESET_EVENT_A  = 70,
    NOTE_PRESET_EVENT_B  = 71,
    NOTE_PRESET_EVENT_C  = 72,
    NOTE_PRESET_EVENT_D  = 73,
    // 74–79 reserved (preset events)

    // 80–127 reserved
};

// ---------------------------------------------------------------------------
// MIDI Real Time messages we care about
// ---------------------------------------------------------------------------
//
// The Arduino MIDI Library exposes these as callbacks — listed here for
// completeness / reference.
//
//   0xF8  Clock   : 24 per quarter note. Drives every phase-based preset.
//   0xFA  Start   : DAW transport start. Brain may reset phase on this.
//   0xFB  Continue: DAW transport continue.
//   0xFC  Stop    : DAW transport stop. Brain freezes animation at phase.
//
// ---------------------------------------------------------------------------
// System Exclusive — moving patches between the editor and the brain
// ---------------------------------------------------------------------------
//
// SysEx carries nothing that happens during a song. It exists to move the
// patch library over USB, and it never appears on the DIN link. The
// reasoning is DESIGN.md § "Patch storage"; what follows is the wire.
//
// Framing:
//
//     F0 7D 41 55 <type> <payload …> F7
//
// 0x7D is the non-commercial manufacturer ID, free for private use and
// never assigned to a product. 0x41 0x55 is "AU", so a merger carrying
// another 0x7D device does not hand us its traffic.
//
// Every byte after F0 is 7-bit already: a parameter byte is a CC value, a
// patch index runs 0–127, and names are clamped to printable ASCII. Nothing
// needs a packing scheme to survive the transport.
//
// Messages stay far below 290 bytes, which is USB_MIDI_SYSEX_MAX in
// cores/teensy4/usb_midi.h — a bare #define, so no build flag raises it.
// Staying under it is worth more than raising it would be: below that size
// the core hands the handler each message whole in a single callback, and
// nothing has to be reassembled across calls.
//
// ### A sync replaces the whole library
//
// The editor holds the master library and pushes all of it. There is no
// "patch 47 changed" message, and deliberately so — the brain's storage is
// a mirror of what the editor last sent, so neither side tracks which
// patches are stale and no patch needs an identity beyond its index.
//
// Everything lands in a staging file and becomes live only when
// SYSEX_SYNC_COMMIT arrives. A sync cut off anywhere — unplugged cable,
// crashed tab, closed laptop — leaves the previous library whole and
// current. There is no state in which the brain holds half of one library
// and half of another.
//
// **A sync is strictly ordered**: patch 0's head, then its five sets, then
// patch 1, and so on to the count declared in SYSEX_SYNC_BEGIN. The brain
// appends as it receives and never seeks, which keeps one flash write per
// message and a constant amount of RAM in use. Anything out of order is
// refused rather than stored, because a gap cannot be told from a
// reordering once both have been written.
//
// The brain answers SYSEX_SYNC_BEGIN and SYSEX_SYNC_COMMIT and stays quiet
// through the data in between. Those two are what the editor needs: the
// first says the brain is listening and speaks this format, the second says
// the library is stored and how much of it arrived. Acknowledging every
// data message would say nothing extra — USB does not deliver a corrupted
// packet, and a lost one shows up as a wrong index at once and a short
// count at commit.
//
// ---------------------------------------------------------------------------

static const uint8_t AURORA_SYSEX_ID       = 0x7D; // non-commercial
static const uint8_t AURORA_SYSEX_SIG_A    = 0x41; // 'A'
static const uint8_t AURORA_SYSEX_SIG_B    = 0x55; // 'U'

// F0 + id + two signature bytes + type, and F7 at the end.
static const uint8_t AURORA_SYSEX_HEADER_LEN = 5;
static const uint8_t AURORA_SYSEX_FRAME_LEN  = AURORA_SYSEX_HEADER_LEN + 1;

enum AuroraSysEx : uint8_t {
    // 0x01–0x1F — editor to brain
    SYSEX_SYNC_BEGIN      = 0x01, // format, count, keymap[9]
    SYSEX_PATCH_HEAD      = 0x02, // index, then AURORA_PATCH_HEAD_LEN bytes
    SYSEX_PATCH_SET       = 0x03, // index, set, then 128 CC bytes
    SYSEX_SYNC_COMMIT     = 0x04, // no payload
    SYSEX_SYNC_ABORT      = 0x05, // no payload
    SYSEX_QUERY_LIBRARY   = 0x06, // no payload
    SYSEX_QUERY_PATCH     = 0x07, // index

    // 0x40–0x5F — brain to editor
    SYSEX_ACK             = 0x40, // type being answered, status, detail
    SYSEX_LIBRARY_INFO    = 0x41, // see AuroraLibraryState
    SYSEX_PATCH_HEAD_OUT  = 0x42, // same payload as SYSEX_PATCH_HEAD
    SYSEX_PATCH_SET_OUT   = 0x43, // same payload as SYSEX_PATCH_SET
};

enum AuroraSysExStatus : uint8_t {
    SYSEX_OK              = 0,
    SYSEX_ERR_FORMAT      = 1, // a patch format this firmware does not speak
    SYSEX_ERR_SEQUENCE    = 2, // data outside a sync, or not the expected piece
    SYSEX_ERR_INCOMPLETE  = 3, // commit arrived with patches still missing
    SYSEX_ERR_STORAGE     = 4, // the flash refused a write or the rename failed
    SYSEX_ERR_RANGE       = 5, // an index or count outside what a patch can hold
};

// What the brain is running, reported in SYSEX_LIBRARY_INFO.
enum AuroraLibraryState : uint8_t {
    LIBRARY_STORED        = 0, // a synced library is live
    LIBRARY_EMPTY         = 1, // nothing stored; the compiled default set is
                               // lit. The defaults are never written to
                               // flash, so this stays true until a sync —
                               // which is what lets the editor tell a fresh
                               // flash from a small library.
    LIBRARY_UNREADABLE    = 2, // something is stored that cannot be read
};

// ---------------------------------------------------------------------------
// What a patch is made of on the wire and in flash
// ---------------------------------------------------------------------------
//
// One byte per CC, five times over — DESIGN.md § "Patch storage". The head
// carries the four things that are not CCs, and the name, which exists for
// one reason: a library exported back off the brain has to be a library
// rather than a heap of anonymous looks.
//
// Set order is fixed and is part of the format. The three fader far ends
// follow DESIGN.md § "The three faders are three routes to more".

static const uint8_t AURORA_PATCH_FORMAT    = 1;
static const uint8_t AURORA_PATCH_MAX       = 128; // what a Program Change names
static const uint8_t AURORA_PATCH_CC_COUNT  = 128;
static const uint8_t AURORA_PATCH_NAME_LEN  = 16;
static const uint8_t AURORA_KEYPAD_KEYS     = 9;

enum AuroraPatchSet : uint8_t {
    PATCH_SET_BASE     = 0,
    PATCH_SET_COLOR    = 1, // the Color fader's far end
    PATCH_SET_EXTENT   = 2, // the Extent fader's far end
    PATCH_SET_MOTION   = 3, // the Motion fader's far end
    PATCH_SET_ACCENT   = 4, // where holding the key of the current patch goes
    AURORA_PATCH_SETS  = 5,
};

// pattern, palette, journey ramp, accent ramp, then the name.
//
// The palette byte is carried and stored and nothing reads it. What a
// palette is has not been settled — TODO.md § "What a palette is, and where
// it lives" — and the byte costs nothing to reserve, since which palette a
// patch uses is a property of the patch either way.
static const uint8_t AURORA_PATCH_HEAD_LEN = 4 + AURORA_PATCH_NAME_LEN;

static const uint16_t AURORA_PATCH_LEN =
    AURORA_PATCH_HEAD_LEN + (uint16_t)AURORA_PATCH_SETS * AURORA_PATCH_CC_COUNT;

// ---------------------------------------------------------------------------
// Version string
// ---------------------------------------------------------------------------
//
// Bump AURORA_PROTOCOL_VERSION whenever you add / remove / renumber
// anything above. Lets either side refuse to talk to an incompatible peer.
// Format: major.minor, major = breaking, minor = additive.
//
// ---------------------------------------------------------------------------

#define AURORA_PROTOCOL_VERSION_MAJOR 0
#define AURORA_PROTOCOL_VERSION_MINOR 10

#endif // AURORA_PROTOCOL_H
