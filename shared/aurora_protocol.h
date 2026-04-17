// aurora_protocol.h — MIDI protocol between controller and brain
//
// Included by BOTH firmwares. Any change here affects both sides. That is
// the whole point: one source of truth for the wire format.
//
// Conventions:
//   - All MIDI is on channel 1 unless otherwise noted.
//   - PC (Program Change) selects discrete state (preset, palette).
//   - CC (Control Change) carries continuous parameters (faders, touchpad
//     Y, palette spread).
//   - Note on/off is used for discrete events (trigger flash).
//   - MIDI clock (0xF8) drives tempo. Brain never sees clock from outside
//     sources directly — the controller is always the clock source to the
//     brain (Option A in DESIGN.md).
//
// This file is still a stub. Fill in as the controller/brain firmwares
// land.

#ifndef AURORA_PROTOCOL_H
#define AURORA_PROTOCOL_H

#include <stdint.h>

// --- MIDI channel ----------------------------------------------------------

static const uint8_t AURORA_MIDI_CHANNEL = 1;

// --- Program Change: preset selection --------------------------------------
//
// Maps the controller's numpad 1-9 (and 0) to preset slots on the brain.
// Row organization mirrors the physical numpad (see README / DESIGN.md).

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
};

// Palette selection goes on the same PC bus but in a separate numeric
// range so the brain can tell them apart without a mode flag.

static const uint8_t AURORA_PC_PALETTE_BASE = 64;   // palettes 0-8 -> PC 64-72
static const uint8_t AURORA_PC_PALETTE_NONE = AURORA_PC_PALETTE_BASE + 63; // "no palette, monochrome"

// --- Control Change: continuous parameters --------------------------------

enum AuroraCC : uint8_t {
    CC_HUE              = 20, // palette hue center / legacy hue fader
    CC_SATURATION       = 21, // palette spread / legacy saturation fader
    CC_VALUE            = 22, // brightness
    CC_SCULPT_Y         = 30, // touchpad sculpt-mode Y axis
    CC_SCULPT_X         = 31, // touchpad sculpt-mode X axis
    CC_TOUCH_INTENSITY  = 32, // touchpad pressure
    CC_MODE_FLAGS       = 40, // bitfield: alt-mode, touchpad-strip-mode, hold, etc.
};

// --- Notes: discrete events -----------------------------------------------

enum AuroraNote : uint8_t {
    NOTE_TRIGGER_FLASH   = 60, // mic / external trigger
    NOTE_TAP_TEMPO_EDGE  = 61, // optional, if we want brain-side handling
};

// --- Preset-variant axis (inside sculpt-mode Y) ---------------------------
//
// Interpretation of CC_SCULPT_Y per preset lives in the brain firmware.
// The controller just sends the raw 0-127 value; the brain decides what
// 30% of Y means for Rain vs Plasma vs Comet.

#endif // AURORA_PROTOCOL_H
