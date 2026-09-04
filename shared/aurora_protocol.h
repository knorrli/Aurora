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
//     directly — the controller always re-emits (Option A in DESIGN.md).
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
//   3. If it changes behaviour: update DESIGN.md too.
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
//     10 –  63 : RESERVED for preset expansion (more slots, banks)
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
    // 10–63 reserved
};

static const uint8_t AURORA_PC_PALETTE_BASE      = 64;
static const uint8_t AURORA_PC_PALETTE_COUNT     = 9;   // palettes 0–8
static const uint8_t AURORA_PC_PALETTE_MONOCHROME = 127; // "no palette"

// Helpers

static inline bool aurora_pc_is_preset(uint8_t pc)  { return pc <= 9;  }
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
//     20 –  29 : colour / palette
//     30 –  39 : touchpad / sculpt mode
//     40 –  49 : mode flags & switches
//     50 –  59 : per-preset parameter slots (interpretation is per preset)
//     60 –  79 : RESERVED for future continuous parameters
//     80 –  89 : RESERVED for band / song-specific automation
//     90 – 119 : RESERVED
//    120 – 127 : AVOID (standard MIDI: channel mode messages)
//
// ---------------------------------------------------------------------------

enum AuroraCC : uint8_t {
    // 10–19 — transport / meta
    CC_TEMPO_DIVISION      = 10, // note value one tempo pulse stands for;
                                 // value is an AuroraTempoDivision index
    // 11–19 reserved (transport / meta)

    // 20–29 — colour / palette
    CC_HUE                 = 20, // palette hue center / H fader
    CC_SATURATION          = 21, // palette spread / S fader
    CC_VALUE               = 22, // brightness / V fader
    CC_PALETTE_ANIM_RATE   = 23, // how fast palette rotates (0 = static)
    CC_PALETTE_ANIM_DEPTH  = 24, // how far palette drifts from center
    // 25–29 reserved (colour / palette)

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
    // 45–49 reserved (mode flags)

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
    // 60–79 reserved (future continuous parameters)
    // 80–89 reserved (band / song-specific automation)
    // 90–119 reserved
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
// behaviour rather than something exotic. The order here is therefore not
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
#define AURORA_PROTOCOL_VERSION_MINOR 2

#endif // AURORA_PROTOCOL_H
