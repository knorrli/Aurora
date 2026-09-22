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
//      does.
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
//     81 –  89 : RESERVED for band / song-specific automation
//     90 –  99 : color, second half (twenty controls will not fit in ten)
//    100 – 114 : the pulse's destinations, three apiece
//           115 : where a still pattern stands — a shape control that did
//                 not fit in 70–79
//    116 – 119 : RESERVED
//    120 – 127 : AVOID (standard MIDI: channel mode messages)
//
// ---------------------------------------------------------------------------

enum AuroraCC : uint8_t {
    // 10–19 — transport / meta
    CC_TEMPO_DIVISION      = 10, // note value one tempo pulse stands for;
                                 // value is an AuroraTempoDivision index
    // 11–19 reserved (transport / meta)

    // 20–29 — color, first half. 20–22 are the three faders; 23–29 carry
    // the placed field. The second half is at 90–99.
    //
    // A color is hue, whiteness and darkness. Everything below is a push on
    // those three, measured from the faders, and the pushes add — so with
    // every one of them centered the wall is exactly the color on the faders.
    // See P_Generator.cpp § "The color layer".
    CC_HUE                 = 20, // hue center / H fader
    CC_SATURATION          = 21, // saturation / S fader
    CC_VALUE               = 22, // brightness / V fader

    // The placed field is something aimed: a gradient running one way across a
    // ruler with the faders' color at its center, or regions sitting on that
    // ruler. Count, width and edge mean here exactly what they mean in the
    // shape block below.
    // 23 free — the primitive and the ruler moved to 47 and 48, where a
    // sender can set one without having to know the other.
    CC_PLACED_HUE          = 24, // bipolar: 64 is flat, either side is how far
                                 // ONE end departs — the two ends land twice
                                 // that far apart
    CC_PLACED_WHITE        = 25, // bipolar: up is toward white at one end,
                                 // down is toward a pure hue
    CC_PLACED_DARK         = 26, // bipolar: down is toward dark, up toward
                                 // full — which needs the V fader left below
                                 // the top to have anywhere to go
    CC_PLACED_COUNT        = 27, // regions along the ruler, 1–20. Ignored by
                                 // a gradient, which spans the ruler once
    CC_PLACED_WIDTH        = 28, // a region's solid core, as a proportion of
                                 // one cell
    CC_PLACED_EDGE         = 29, // hard-edged region through to a smooth fade

    // 30–39 — touchpad / sculpt
    CC_SCULPT_Y            = 30, // touchpad Y in sculpt mode (per-preset axis)
    CC_SCULPT_X            = 31, // touchpad X in sculpt mode (usually strip select)
    CC_TOUCH_PRESSURE      = 32, // raw pressure, 0–127
    CC_TOUCH_ACTIVE        = 33, // 0 = not touched, 127 = touched
    // 34–39 reserved (touchpad)

    // 40–49 — mode flags & switches
    CC_MODE_FLAGS          = 40, // bit-packed, see AuroraModeBits below
    CC_TOUCHPAD_STRIP_MODE = 41, // 0 = mirrored, 64 = all, 127 = exclusive
    CC_TOUCHPAD_EFFECT     = 42, // 0 = paint/fill, 64 = paint/invert, 127 = sculpt
    CC_HOLD_MODE           = 43, // 0 = off, 127 = on
    CC_VERTICAL_MODE       = 44, // 0 = Y-modulates-saturation, 127 = Y-modulates-hue

    // One switch per CC, because a CC cannot be read back: nothing here can
    // ask the brain what the other switches are currently set to, so a sender
    // that packed several into one byte would have to know all of them to
    // change any one of them. A sequencer setting "regions" would silently
    // put the ruler back across the wall. Splitting them also makes each one
    // an ordinary switch lane in a DAW rather than a number to be looked up.
    //
    // Everything here reads as off below 64 and on from 64 up, except the
    // ruler, which is banded like CC_TOUCHPAD_STRIP_MODE above.
    CC_GEN_ALTERNATE       = 45, // odd strips run the journey backwards
    CC_GEN_BOUNCE          = 46, // turn at the cell's edge instead of wrapping
    CC_COLOR_REGION       = 47, // 0 = one gradient across the ruler, 127 = regions
    CC_COLOR_RULER        = 48, // 0 = across the five strips, 64 = along a
                                 // strip, 127 = within a shape
    // 49 reserved (mode flags)

    // 50–59 — per-preset parameter slots
    //
    // These are deliberately generic. Each preset decides what its own
    // slots A–J mean. The controller shouldn't need to know — it just
    // passes the value through. Brain-side documentation lives next to
    // the preset implementation.
    CC_PRESET_PARAM_A      = 50,
    CC_PRESET_PARAM_B      = 51,
    CC_PRESET_PARAM_C      = 52,
    CC_PRESET_PARAM_D      = 53,
    CC_PRESET_PARAM_E      = 54,
    CC_PRESET_PARAM_F      = 55,
    CC_PRESET_PARAM_G      = 56,
    CC_PRESET_PARAM_H      = 57,
    CC_PRESET_PARAM_I      = 58,
    CC_PRESET_PARAM_J      = 59,

    // 60–69 — washes / DMX fixtures
    CC_WASH_LEVEL          = 60, // wash master, independent of the strips
                                 // so the washes can be pulled down under a
                                 // running pattern. PRESET_OFF overrides it
                                 // and darkens them: numpad 0 is an
                                 // emergency stop, and one key has to kill
                                 // the rig on its own.
    CC_WASH_HUE_OFFSET     = 61, // rotates the washes off the strips' hue,
                                 // so they can sit complementary or merely
                                 // adjacent instead of matching. 0 matches;
                                 // 64 of 127 is the opposite side of the
                                 // wheel.
    // 62–69 reserved (washes)

    // 70–79 — generator shape. Only read while PRESET_GENERATOR is active.
    CC_GEN_WIDTH           = 70, // how much of one cell the shape covers
    CC_GEN_COUNT           = 71, // how many shapes along the strip, 1–20
    CC_GEN_EDGE            = 72, // symmetric softness at both ends
    CC_GEN_TAIL            = 73, // asymmetric fade behind the shape only
    CC_GEN_SPEED           = 74, // bipolar: 64 is still, either side travels
    CC_GEN_FAN             = 75, // how far the five strips run out of step
    CC_GEN_JITTER          = 76, // randomness in position and brightness
    // The pulse is one oscillator with one rate, and 77/79/80 are its
    // amount and its wave where it reaches the strips' brightness. Every
    // other destination carries its own three at 100–114.
    CC_GEN_PULSE_DEPTH     = 77, // how far the trough digs below full light
    CC_GEN_PULSE_RATE      = 78, // beats per swell; stepped, see
                                 // AURORA_PULSE_PERIODS below
    CC_GEN_PULSE_SKEW      = 79, // bipolar: 64 is an even rise and fall,
                                 // either side slides the peak toward a ramp
    CC_GEN_PULSE_SHAPE     = 80, // 0 = hard on/off square, 127 = smooth sine

    // 81–89 reserved (band / song-specific automation)

    // 90–99 — color, second half. Twenty controls will not fit in ten slots,
    // so color stays in two blocks; what makes this a half rather than an
    // overflow is that the split falls between whole ideas. 23–29 is the
    // placed field, this is everything that is not aimed anywhere.
    CC_PLACED_SPEED        = 90, // bipolar: 64 is still, either side drifts
                                 // the regions along the ruler

    // The wander: color never quite the same in two places, with the
    // difference always moving. Two terms at the golden ratio, so it cannot
    // come back into step and never repeats — built in rather than dialed,
    // because dialing how far apart two speeds sit is operating the
    // mechanism rather than the look.
    CC_WANDER_HUE          = 91, // bipolar: how far the hue wanders either side
    CC_WANDER_WHITE        = 92, // bipolar: how far whiteness wanders
    CC_WANDER_DARK         = 93, // bipolar: how far darkness wanders
    CC_WANDER_RATE         = 94, // 0 = frozen, up to two beats per cycle
    CC_WANDER_SCALE        = 95, // 0 = the whole wall moving as one, 127 =
                                 // individual pixels shimmering

    // Color read off how lit the shape branch left a pixel. The one source
    // that reaches the pulse and jitter, since neither has a position for a
    // ruler to measure. Anchored at the dim end: the faders are what a fade
    // runs out to, and the core is the departure.
    CC_LIT_HUE             = 96, // bipolar: 64 = none, +-64 hue at the core
    CC_LIT_WHITE           = 97, // 0 = none, up = white at the core
    CC_LIT_DARK            = 98, // bipolar: 64 = none, down takes the core
                                 // toward dark and up toward full
    // 99 reserved (color)

    // 100–114 — where else the pulse reaches. One oscillator, one rate: a
    // destination sets how far it is pushed and what wave pushes it, never
    // how fast. Three per destination, always in the order amount, shape,
    // skew, so the block reads as a table.
    //
    // Every destination is always connected and its amount may be zero,
    // because a morph can interpolate an amount and cannot snap a
    // connection on — see DESIGN.md § "Switches belong to the patch".
    //
    // Amounts are bipolar and 64 is no push. The sign picks which of the
    // destination's two limits the push runs toward, so it can never clip
    // and a control already sitting at a limit has nowhere to go that way.
    // The strips' brightness at 77 is the exception and is unipolar: there
    // is nothing above full light, so its only direction is down.
    CC_PULSE_WIDTH         = 100, // toward full width / toward nothing
    CC_PULSE_WIDTH_SHAPE   = 101,
    CC_PULSE_WIDTH_SKEW    = 102,

    // Added after the placed field, the wander and the light level have
    // summed — one push on the color layer's output rather than one per
    // source, which leaves that layer's design alone.
    CC_PULSE_HUE           = 103, // bipolar, up to half the wheel each way
    CC_PULSE_HUE_SHAPE     = 104,
    CC_PULSE_HUE_SKEW      = 105,

    // The washes. A PAR is one position with no length, so the shape branch
    // cannot reach it and the pulse can — see DESIGN.md § "The PAR cans".
    CC_PULSE_PAR_LEVEL     = 106, // toward full / toward dark
    CC_PULSE_PAR_LEVEL_SHAPE = 107,
    CC_PULSE_PAR_LEVEL_SKEW  = 108,

    CC_PULSE_PAR_HUE       = 109, // bipolar, up to half the wheel each way
    CC_PULSE_PAR_HUE_SHAPE = 110,
    CC_PULSE_PAR_HUE_SKEW  = 111,

    // Toward white is the flash between strip strobes that DESIGN.md
    // records as asked for and unreachable.
    CC_PULSE_PAR_SAT       = 112, // toward a pure hue / toward white
    CC_PULSE_PAR_SAT_SHAPE = 113,
    CC_PULSE_PAR_SAT_SKEW  = 114,

    // A shape control, and it belongs in the 70–79 block. That block was
    // full before this was wanted, and moving one control on its own would
    // mean renumbering a controller twice — once now and once at the
    // regroup. It waits here for the regroup.
    //
    // Bipolar: 64 is the middle of the cell, and half a cell each way covers
    // every place a shape can stand, because the pattern repeats once per
    // cell. Read only while the pattern is still; travel sets its own place.
    CC_GEN_POSITION        = 115, // where a still pattern stands in its cell

    // 116–119 reserved
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
// Mode flag bitfield (carried on CC_MODE_FLAGS)
// ---------------------------------------------------------------------------
//
// CC values are 7 bit (0–127). We use the low 7 bits as a bitfield of
// boolean mode flags. Prefer dedicated CCs for anything that might ever
// need more than on/off — this bitfield is for things that genuinely
// are two-state.
//
// ---------------------------------------------------------------------------

enum AuroraModeBits : uint8_t {
    MODE_BIT_FADER_ALT         = 1 << 0, // legacy: hue oscillation around base
    MODE_BIT_PRESET_ALT        = 1 << 1, // legacy: default vs. alt variant
    MODE_BIT_PALETTE_ANIMATION = 1 << 2, // palette rotates through hue wheel
    MODE_BIT_RESERVED_3        = 1 << 3, // reserved
    MODE_BIT_RESERVED_4        = 1 << 4, // reserved
    MODE_BIT_RESERVED_5        = 1 << 5, // reserved
    MODE_BIT_RESERVED_6        = 1 << 6, // reserved
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
// Version string
// ---------------------------------------------------------------------------
//
// Bump AURORA_PROTOCOL_VERSION whenever you add / remove / renumber
// anything above. Lets either side refuse to talk to an incompatible peer.
// Format: major.minor, major = breaking, minor = additive.
//
// ---------------------------------------------------------------------------

#define AURORA_PROTOCOL_VERSION_MAJOR 0
#define AURORA_PROTOCOL_VERSION_MINOR 7

#endif // AURORA_PROTOCOL_H
