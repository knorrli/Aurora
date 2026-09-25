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
//           10 : the parametric generator (experiment; see shared/render/generator.cpp)
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
    // One pattern whose shape comes entirely from CC 53–70 rather than
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
// Layout. Regrouped 2026-09-25; the reasoning is in docs/cc-regroup.md.
//
//      0,  1 : AVOID — bank select MSB and modulation
//      2 –   9 : transport / meta                        (7 = AVOID, volume)
//     10,  11 : AVOID — pan and expression
//     12 –  26 : the controller — what each control stands at
//     27 –  31 : washes / DMX fixtures
//           32 : AVOID — bank select LSB
//     33 –  52 : color — the three faders, the placed field, the wander,
//                 the lit reach, and the two color switches
//     53 –  70 : the generator — shape, the fan, and the one clock
//     71 –  79 : the scatter — a texture source and its three amounts
//     80 – 119 : modulation routes, eight of five bytes
//    120 – 127 : AVOID — channel mode messages
//
// Nothing between 2 and 119 is outside a block, and the routes take the one
// run of 40 the map has. Spare: 3–6, 8, 9, 30, 31 and 70.
//
// The four AVOIDed numbers in the middle are the ones a DAW writes without
// being asked: volume, pan, expression and bank select, which travels with
// the Program Changes Mainstage sends. Aurora answers on every channel today
// — see TODO.md § Known defects — so dodging them is the only protection
// there is until both receivers filter.
//
// 12–26 is the modwheel model and the reason this block exists: a control
// reports the value it stands at and says nothing about what that does. The
// patch decides. Every one of them is [ambient] or [gesture], because where
// a hand has left a fader is not something a patch holds.
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
// reach. A patch sends it after its controls, never before: it is how the
// brain knows a patch has landed, and the generator clears its tails there,
// since a tail recorded while the controls were still arriving would streak
// from the old shapes to the new. The palette numbers at PC 64–126 are reserved for something that
// is not built and the brain ignores them — see handleProgramChange in
// brain/src/midi_in.cpp.
//
// ---------------------------------------------------------------------------

enum AuroraCC : uint8_t {
    // Two tags beyond [patch] and [switch] say what a modulation route may do
    // with a control. [rate] feeds a running total, so a route aimed at one
    // swings it both ways around the dialed value and averages to nothing over
    // a cycle, or the wall would move permanently instead of returning.
    // [circular] has no top or bottom, hue being the plain case, so a
    // push is a rotation rather than a fraction of the distance to a limit.
    // See docs/modulation.md.

    // Excluded, never to be assigned: 0, 1, 7, 10, 11, 32, 120–127. Not for
    // being named in the MIDI spec — CC 64, 74, 91 and 38 are named there
    // and are in use below — but because something else on the chain sends
    // them unasked. Bank Select (0, 32) rides ahead of the Program Change
    // that selects a patch; a DAW track's volume, pan and expression
    // automation send 7, 10 and 11; a mod wheel sends 1. And 120–127 are
    // Channel Mode messages, sent on transport stop, on panic and on track
    // disarm, usually to every channel, so AURORA_MIDI_CHANNEL is no
    // protection. See docs/modulation.md for which are least dangerous to
    // break first if the map ever runs out.

    // 2–9 — transport / meta
    //
    // A half-time look is a real musical idea, and a song that wants one
    // wants it for every patch in that song — so a patch that needs the
    // other feel is a duplicate with a different division, not a reason to
    // keep this out of a patch.
    //
    // The 12-position rotary on the box is this control; it has no CC of its
    // own. Moved off 10, where it shared a number with pan.
    CC_TEMPO_DIVISION      = 2,  // [patch] note value one tempo pulse stands
                                 // for; value is an AuroraTempoDivision index
    // 3–9 reserved (transport / meta), skipping 7. 10 and 11 excluded

    // 12–26 — the controller
    //
    // Every control on the box, reporting the value it stands at and nothing
    // about what that value does. The patch decides the meaning; a sender
    // with two knobs and no touchpad can drive the axes without knowing a pad
    // exists. docs/controls.md is the inventory these come from.
    //
    // None of them is [patch]. A fader position is where a hand left it, not
    // something a look holds.

    // The three sticks on the left of the box. Each is a route to "more" for
    // the patch that is up — see DESIGN.md § "The three faders are three
    // routes to 'more'" — so the brain needs to know where each one stands in
    // order to morph toward that patch's Color, Extent and Motion sets. These
    // are not the color at 33–35, which is what a patch holds.
    CC_FADER_COLOR         = 12, // [ambient] more means hotter, toward white
    CC_FADER_EXTENT        = 13, // [ambient] more means more of the wall lit
    CC_FADER_MOTION        = 14, // [ambient] more means faster, harder

    // Positions that persist. v1 held the last one when the finger lifted and
    // it was useful, so engage is a control of its own rather than "a finger
    // is down": the axes keep their value and engage says whether the pad's
    // effect is live at all.
    CC_PAD_X               = 15, // [gesture]
    CC_PAD_Y               = 16, // [gesture]
    CC_PAD_PRESSURE        = 17, // [gesture] raw, 0–127, never yet measured
    CC_PAD_ENGAGE          = 18, // [gesture] 0 = the pad reaches nothing

    // The four rockers around the touchpad and the one under the faders.
    // Named for where they sit, because what each one does is undecided —
    // v1's meanings retired with the model that needed them. Which physical
    // rocker is which, and which are 2-way against 3-way, is not recorded
    // anywhere; see docs/controls.md.
    CC_ROCKER_PAD_A        = 19, // [ambient] below-left of the pad
    CC_ROCKER_PAD_B        = 20, // [ambient] above the pad, left
    CC_ROCKER_PAD_C        = 21, // [ambient] above the pad, center
    CC_ROCKER_PAD_D        = 22, // [ambient] above the pad, right
    CC_ROCKER_FADERS       = 23, // [ambient] below the fader panel. Shared a
                                 // pin with preset-alt in v1 and now has its
                                 // own number; the pin comes with the rebuild

    // Only if the peak follower moves onto the controller when the secondary
    // board goes. If it stays a circuit that never reaches a pin, these two
    // go back to spare.
    CC_AUDIO_FOLLOWER      = 24, // [ambient] peak follower on/off
    CC_AUDIO_THRESHOLD     = 25, // [ambient] the level the gate fires at

    // Which key the keypad names arrives as a Program Change. This is whether
    // that key is still down — a state, not an event, because the morph
    // stretches for as long as it reads 127 and lands when it reads 0. A note
    // pair would say the key's identity a second time, and the model names
    // one destination at a time. See DESIGN.md § "Changing patch".
    CC_KEY_HELD            = 26, // [gesture] 127 while the key is held
    // 27–31 — washes / DMX fixtures. All three are [patch]: DESIGN.md
    // § "What a patch holds for them" names level, hue offset and saturation
    // as the whole of what a patch keeps for the PARs.
    CC_WASH_LEVEL          = 27, // [patch][plain] wash master, independent of
                                 // the
                                 // strips so the washes can be pulled down
                                 // under a running pattern. PRESET_OFF
                                 // overrides it and darkens them: numpad 0 is
                                 // an emergency stop, and one key has to kill
                                 // the rig on its own.
    CC_WASH_HUE_OFFSET     = 28, // [patch][circular][plain] rotates the washes
                                 // off
                                 // the strips' hue, so they can sit
                                 // complementary or
                                 // merely adjacent instead of matching. 0
                                 // matches; 64 of 127 is the opposite side of
                                 // the wheel.
    // Scales the washes down from the strips' saturation: 127 matches them,
    // 0 is white. A relationship rather than a color of their own, like
    // every other wash control — see DESIGN.md § "The PAR cans". It is also
    // where a route's push on it measures from.
    CC_WASH_SATURATION     = 29, // [patch][plain] 
    // 30–31 reserved (washes); 32 excluded

    // 33–52 — color. One block, where it used to be split across 20–29 and
    // 90–99 because twenty controls do not fit in ten.
    //
    // 33–35 are what a patch holds; the three sticks that used to set them
    // are at 12–14 now and mean something else. The placed field, the wander
    // and the lit reach are all departures measured from these three, and the
    // pushes add — so with every one of them centered the wall is exactly the
    // color here.
    CC_HUE                 = 33, // [patch][circular] hue center
    CC_SATURATION          = 34, // [patch]
    CC_VALUE               = 35, // [patch] brightness

    // One switch per CC, because a CC cannot be read back: nothing here can
    // ask the brain what the other switches are currently set to, so a sender
    // that packed several into one byte would have to know all of them to
    // change any one of them. Splitting them also makes each one an ordinary
    // switch lane in a DAW rather than a number to be looked up.
    //
    // Off below 64 and on from 64 up, except the ruler, which is banded into
    // thirds. These two and the generator's at 53–54 are what DESIGN.md
    // § "Switches belong to the patch" argues about: saved and recalled,
    // never interpolated.
    CC_COLOR_REGION        = 36, // [switch] 0 = one gradient across the
                                 // ruler, 127 = regions
    CC_COLOR_RULER         = 37, // [switch] 0 = across the five strips,
                                 // 64 = along a strip, 127 = within a shape
    CC_PLACED_HUE          = 38, // [patch] bipolar: how far one end of the
                                 // ruler departs from the base hue
    CC_PLACED_WHITE        = 39, // [patch] bipolar: toward white, or toward
                                 // a pure hue
    CC_PLACED_DARK         = 40, // [patch] bipolar: toward dark, or toward
                                 // full
    CC_PLACED_COUNT        = 41, // [patch] regions along the ruler, 1–20
    CC_PLACED_WIDTH        = 42, // [patch]
    CC_PLACED_EDGE         = 43, // [patch] hard through to a fade
    CC_PLACED_SPEED        = 44, // [patch][rate] bipolar: the field drifting
                                 // along its own ruler
    CC_WANDER_HUE          = 45, // [patch] bipolar: how far the hue wanders
    CC_WANDER_WHITE        = 46, // [patch] bipolar
    CC_WANDER_DARK         = 47, // [patch] bipolar
    CC_WANDER_RATE         = 48, // [patch][rate] 0 = frozen, up to two beats
                                 // per cycle
    CC_WANDER_SCALE        = 49, // [patch] 0 = the whole wall moving as one,
                                 // up to a fine grain
    // Anchored at the dim end: the faders are what a fade runs out to, and
    // the core is the departure.
    CC_LIT_HUE             = 50, // [patch] bipolar: 64 = none, ±64 hue at
                                 // the core
    CC_LIT_WHITE           = 51, // [patch] 0 = none, up = white at the core
    CC_LIT_DARK            = 52, // [patch] bipolar: 64 = none, down takes the
                                 // core toward dark and up toward full
    // 53–70 — the generator. Only read while PRESET_GENERATOR is active.
    // 53 reserved (the generator)
    CC_GEN_BOUNCE          = 54, // [switch] turn at the cell's edge instead
                                 // of wrapping
    CC_GEN_WIDTH           = 55, // [patch] how much of one cell the shape's
                                 // solid core takes
    CC_GEN_COUNT           = 56, // [patch][plain] how many shapes along the
                                 // strip,
                                 // 1–20, geometric so a morph doubles
    CC_GEN_EDGE            = 57, // [patch] symmetric softness at both ends
    CC_GEN_TAIL            = 58, // [patch] beats a passed pixel glows, 0 =
                                 // none, squared up to 8
    // Bipolar: 64 is the middle of the cell, and half a cell each way covers
    // every place a shape can stand, because the pattern repeats once per
    // cell. Read only while the pattern is still; travel sets its own place.
    // One thing to watch in a morph: the two ends of this control are the
    // same place on the wall, so interpolating from one toward the other
    // slides the shape the long way across the cell rather than across the
    // seam.
    CC_GEN_POSITION        = 59, // [patch][circular] where a still pattern
                                 // stands in its cell
    CC_GEN_SPEED           = 60, // [patch][rate] bipolar: 64 is still, either
                                 // side travels

    // 61–66 — the fan, whole and in one place. One wave runs across the five
    // strips; each amount decides how far it pushes one quantity, so a wall
    // of staggered bars can strobe in unison. A single offset reaching
    // everything cyclic could not. See docs/generator.md § "The fan is a
    // wave".
    //
    // Frequency is stepped to eighths of a turn and the phase runs on 128ths
    // of one, because the two together have to read *exactly* zero on a
    // strip: read near zero and a strip is not still, it crawls. Still has to
    // mean still here for the same reason it does on CC 60.
    //
    // Two things to know at the top of the frequency range, where every strip
    // sits opposite its neighbors: the phase only scales how deep the
    // alternation is rather than moving it, and a quarter turn either side of
    // it every strip reads zero and the fan goes quiet.
    CC_GEN_FAN_FREQ        = 61, // [patch][plain] 0 = all five alike, up to
                                 // two
                                 // turns across the wall
    CC_GEN_FAN_PHASE       = 62, // [patch][circular][plain] where the wave
                                 // sits on
                                 // the strips: a staircase through a chevron
    CC_GEN_FAN_RANDOM      = 63, // [patch][plain] 0 = the wave, 127 = a fixed
                                 // draw
                                 // per strip
    // The three amounts. Bipolar, and 100 % spreads the five strips over
    // exactly one cell or one swell — both ends of a range are the same wall
    // with the wave turned over. The rate is an absolute speed added to
    // CC 60's, on CC 60's own squared curve, so mirroring one fader about its
    // center against the other cancels exactly: that is what stands one strip
    // still while the rest run.
    CC_GEN_FAN             = 64, // [patch][plain] how far apart the five
                                 // strips
                                 // stand in their cells
    CC_GEN_FAN_RATE        = 65, // [patch][rate][plain] how far apart their
                                 // speeds stand, either side of Speed
    CC_GEN_FAN_PULSE       = 66, // [patch][plain] how far apart they stand in
                                 // the
                                 // swell. The washes take the unfanned phase
                                 // whatever this says: a PAR is one position
                                 // with no strip to be offset from.

    // One clock for the whole rig, and nothing beside it: where a route aims
    // and how hard belongs to the route. Every route reads it, so no route
    // may aim at it.
    CC_GEN_PULSE_RATE      = 67, // [patch] beats per swell; stepped, see
                                 // AURORA_PULSE_PERIODS

    // Travel slowed and sped by which pixel a shape is on, across the strip,
    // or across each cell while bouncing, so a shape stretches where it is
    // fast and squashes where it is slow. Bend is how much and Bend at is
    // where the fastest point sits. See docs/generator.md § "Bend: speed set
    // by where a shape is on the strip".
    CC_GEN_BEND            = 68, // [patch][plain] bipolar: 64 is even, plus
                                 // is fast at the peak, minus slow there
    CC_GEN_BEND_AT         = 69, // [patch][plain] 0 = the bottom, 64 = the
                                 // middle, 127 = the top

    // 70 reserved (the generator)

    // 71–79 — the scatter. The third source in the family: the pulse is
    // regular in time and has no place on the wall, the wander is smooth over
    // both, the scatter is random over both. Six controls shape it and three
    // amounts aim it. See docs/generator.md § "The scatter".
    CC_SCATTER_RATE        = 71, // [patch][rate] how often a cell relights
    CC_SCATTER_COUNT       = 72, // [patch] cells along a strip, 1–20; the
                                 // scatter's own grid, not the shape's
    // Width is the spot's core on both axes at once — how much of its cell it
    // covers and how much of its cycle it is lit — and edge softens both the
    // same way. A separate size and duration were two controls saying one
    // thing.
    CC_SCATTER_WIDTH       = 73, // [patch]
    CC_SCATTER_EDGE        = 74, // [patch] hard through to a fade, in space
                                 // and in time together
    // 0 puts every cell on one clock, so the whole wall flashes as one; full
    // spreads their phases and rates out of the hash and they stop blinking
    // together. Moving it re-keys every cell, so everything in flight jumps —
    // the price of holding no state.
    CC_SCATTER_STAGGER     = 75, // [patch]
    CC_SCATTER_DRIFT       = 76, // [patch] bipolar: how far, and which way, a
                                 // spot slides across its own cell over its
                                 // life. A displacement, not a rate.
    CC_SCATTER_LIGHT       = 77, // [patch] amount toward full light / toward
                                 // dark
    CC_SCATTER_HUE         = 78, // [patch] amount, bipolar, up to half the
                                 // wheel
    CC_SCATTER_WHITE       = 79, // [patch] amount toward white / toward a
                                 // pure hue
    // 80–119 routes, see AURORA_ROUTE_BASE.
};


// ---------------------------------------------------------------------------
// Modulation routes
// ---------------------------------------------------------------------------
//
// A route is five bytes: which control it pushes, how far, how fast against
// the one clock, what wave does the pushing, and how far into its cycle the
// wave is delayed. Eight of them, aimed at any control the map does not
// refuse. See docs/modulation.md.
//
// A patch is a flat 128 bytes whatever this says, and a route nobody has set
// arrives as zeroes — an amount of zero, which does nothing.

// Where the named shapes sit on the wave byte. Whole numbers a fader lands on
// exactly, which is why 32 and 96 rather than thirds of the range.
#define GEN_WAVE_SWELL    32
#define GEN_WAVE_SAW_DOWN 64
#define GEN_WAVE_SQUARE   96

// The shortest stab the rig can draw, as a fraction of a cycle: about one
// 7-8 ms frame at 120 bpm, and below it a stab lands between frames and
// flickers instead of shortening. See docs/bench-facts.md § "Frame timing".
#define GEN_PULSE_MIN_WIDTH 0.06f

static const uint8_t AURORA_ROUTES = 8;

static const uint8_t AURORA_ROUTE_BASE[AURORA_ROUTES] = {
    80, 85, 90, 95, 100, 105, 110, 115,
};

enum AuroraRouteField : uint8_t {
    // [switch] — which control this route pushes, named by that control's own
    // CC number. Never interpolated: a morph that slid it would spend the
    // journey pushing one control with a number dialed for another.
    ROUTE_DESTINATION = 0,
    ROUTE_AMOUNT      = 1,  // [patch] bipolar; a fraction of the distance left
    ROUTE_RATIO       = 2,  // [patch] whole multiples of the clock, never
                            // divisions — a halved route peaks on whichever of
                            // two cycles the offset's integer part lands on,
                            // and nothing controls that
    ROUTE_WAVE        = 3,  // [patch] one axis, a build through a swell to a
                            // stab
    ROUTE_PHASE       = 4,  // [patch] 128ths of the route's own cycle the
                            // wave starts after the bar line
    ROUTE_FIELDS      = 5,
};

static inline uint8_t aurora_route_cc(uint8_t route, uint8_t field) {
    return (uint8_t)(AURORA_ROUTE_BASE[route] + field);
}

// Whole multiples only, 1 through 8. Dividing would break the anchor: the
// anchor fixes only the fraction of the tracker's offset, because a whole
// cycle of offset is invisible — true for the clock and for any whole
// multiple of it. A route at half rate takes two base cycles, and which of
// the two it peaks on depends on the offset's integer part, which nothing
// controls. Nudging the rate can flip it to the opposite phase.
static const uint8_t AURORA_ROUTE_MAX_RATIO = 8;

static inline uint8_t aurora_route_ratio(uint8_t value) {
    const uint8_t last = AURORA_ROUTE_MAX_RATIO - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return (uint8_t)(1 + (step > last ? last : step));
}


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

// Three-way, banded the way CC_ROCKER_PAD_A is: a third of the range
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
// slot runs 0–127, and names are clamped to printable ASCII. The slot map is
// the one thing packed, seven slots to a byte.
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
// patches are stale and no patch needs an identity beyond its slot.
//
// A library is AURORA_PATCH_MAX fixed slots, any of them empty. A slot is
// the Program Change that names its patch and never changes, so deleting or
// adding a patch renumbers nothing a DAW's automation or the keypad points
// at. SYSEX_SYNC_BEGIN carries the slot map, and a patch is sent under its
// slot.
//
// Everything lands in a staging file and becomes live only when
// SYSEX_SYNC_COMMIT arrives. A sync cut off anywhere — unplugged cable,
// crashed tab, closed laptop — leaves the previous library whole and
// current. There is no state in which the brain holds half of one library
// and half of another.
//
// **A sync is strictly ordered**: the lowest filled slot's head, then its
// five sets, then the next filled slot up, and so on through the map
// declared in SYSEX_SYNC_BEGIN. The brain appends as it receives and never
// seeks, which keeps one flash write per message and a constant amount of
// RAM in use. Anything out of order is refused rather than stored, because a
// missing patch cannot be told from a reordering once both have been
// written.
//
// The brain answers SYSEX_SYNC_BEGIN and SYSEX_SYNC_COMMIT and stays quiet
// through the data in between. Those two are what the editor needs: the
// first says the brain is listening and speaks this format, the second says
// the library is stored and how much of it arrived. Acknowledging every
// data message would say nothing extra — USB does not deliver a corrupted
// packet, and a lost one shows up as a wrong index at once and a short
// patch short at commit.
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
    SYSEX_SYNC_BEGIN      = 0x01, // format, keymap[9], slot map
    SYSEX_PATCH_HEAD      = 0x02, // slot, then AURORA_PATCH_HEAD_LEN bytes
    SYSEX_PATCH_SET       = 0x03, // slot, set, then 128 CC bytes
    SYSEX_SYNC_COMMIT     = 0x04, // no payload
    SYSEX_SYNC_ABORT      = 0x05, // no payload
    SYSEX_QUERY_LIBRARY   = 0x06, // no payload
    SYSEX_QUERY_PATCH     = 0x07, // slot

    // 0x40–0x5F — brain to editor
    SYSEX_ACK             = 0x40, // type being answered, status, detail
    SYSEX_LIBRARY_INFO    = 0x41, // version[2], format, AuroraLibraryState,
                                  // keymap[9], slot map
    SYSEX_PATCH_HEAD_OUT  = 0x42, // same payload as SYSEX_PATCH_HEAD
    SYSEX_PATCH_SET_OUT   = 0x43, // same payload as SYSEX_PATCH_SET
};

enum AuroraSysExStatus : uint8_t {
    SYSEX_OK              = 0,
    SYSEX_ERR_FORMAT      = 1, // a patch format this firmware does not speak
    SYSEX_ERR_SEQUENCE    = 2, // data outside a sync, or not the expected piece
    SYSEX_ERR_INCOMPLETE  = 3, // commit arrived with patches still missing
    SYSEX_ERR_STORAGE     = 4, // the flash refused a write or the rename failed
    SYSEX_ERR_RANGE       = 5, // a slot, length or map outside what a library can hold
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

// Which slots hold a patch: slot n is bit (n % 7) of byte (n / 7).
static const uint8_t AURORA_SLOT_MAP_LEN    = (AURORA_PATCH_MAX + 6) / 7;

static inline bool aurora_slot_filled(const uint8_t *map, uint8_t slot) {
    return (map[slot / 7] >> (slot % 7)) & 1;
}

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
#define AURORA_PROTOCOL_VERSION_MINOR 11

#endif // AURORA_PROTOCOL_H
