#ifndef AURORA_PROTOCOL_H
#define AURORA_PROTOCOL_H

#include <stdint.h>

static const uint8_t AURORA_MIDI_CHANNEL = 1;

enum AuroraProgram : uint8_t {
    PROGRAM_BLACKOUT   = 0,
    PROGRAM_SONG_FIRST = 109,
};

static const uint8_t AURORA_SONGS = 19;

enum AuroraCC : uint8_t {
    CC_PAD_HOLD                 = 3,
    CC_PAD_MODE                 = 4,
    CC_PAD_WIDTH                = 5,
    CC_PAD_GAPS                 = 6,

    CC_FADER_COLOR              = 12,
    CC_FADER_MOTION             = 13,
    CC_FADER_EXTENT             = 14,

    CC_PAD_X                    = 15,
    CC_PAD_Y                    = 16,
    CC_PAD_TOUCH                = 18,
};

enum AuroraControl : uint8_t {
    CONTROL_PALETTE               = 0, // [switch]
    CONTROL_HUE                   = 1, // [patch][circular]
    CONTROL_SATURATION            = 2, // [patch]

    CONTROL_PAR_HUE_OFFSET        = 3, // [patch][circular]
    CONTROL_PAR_SATURATION        = 4, // [patch]
    CONTROL_PAR_VALUE             = 5, // [patch]
    CONTROL_PAR_HUE_LAYOUT        = 6, // [switch]
    CONTROL_PAR_HUE_RANGE         = 7, // [patch]

    CONTROL_TEMPO_DIVISION        = 8, // [switch]
    CONTROL_LFO_RATE              = 9, // [patch]
    CONTROL_ARP_MODE              = 10, // [switch]
    CONTROL_ARP_DIRECTION         = 11, // [switch]
    CONTROL_ARP_SPREAD            = 12, // [patch]

    CONTROL_CELL_ENDS             = 13, // [switch]
    CONTROL_CELL_CHORD            = 14, // [patch]
    CONTROL_CELL_SPREAD           = 15, // [patch]
    CONTROL_CELL_POSITION         = 16, // [patch]
    CONTROL_CELL_BEND             = 17, // [patch]
    CONTROL_CELL_BEND_AT          = 18, // [patch]

    CONTROL_CELL_WIDTH            = 19, // [patch]
    CONTROL_CELL_EDGE             = 20, // [patch]
    CONTROL_CELL_TAIL             = 21, // [patch]

    CONTROL_CELL_END              = 22, // [switch]
    CONTROL_CELL_RATE             = 23, // [switch]
    CONTROL_CELL_DELAY            = 24, // [patch]
    CONTROL_CELL_GATE             = 25, // [patch]

    CONTROL_CELL_DIRECTION        = 26, // [switch]
    CONTROL_CELL_BRIGHTNESS       = 27, // [patch]
    CONTROL_CELL_CURVE            = 28, // [patch]
    CONTROL_CELL_CURVE_AT         = 29, // [patch]

    CONTROL_SCATTER_COUNT         = 30, // [patch]
    CONTROL_SCATTER_WIDTH         = 31, // [patch]
    CONTROL_SCATTER_EDGE          = 32, // [patch]
    CONTROL_SCATTER_RATE          = 33, // [patch][rate]
    CONTROL_SCATTER_SPEED         = 34, // [patch]
    CONTROL_SCATTER_RANDOMIZE     = 35, // [patch]
    CONTROL_SCATTER_POSITION      = 36, // [patch]
    CONTROL_SCATTER_MIX           = 37, // [patch]
    CONTROL_SCATTER_HUE           = 38, // [patch]
    CONTROL_SCATTER_SATURATION    = 39, // [patch]
    CONTROL_SCATTER_VALUE         = 40, // [patch]

    CONTROL_FIELD_FORM            = 41, // [switch]
    CONTROL_FIELD_DIRECTION       = 42, // [switch]
    CONTROL_FIELD_COUNT           = 43, // [patch]
    CONTROL_FIELD_WIDTH           = 44, // [patch]
    CONTROL_FIELD_EDGE            = 45, // [patch]
    CONTROL_FIELD_SPEED           = 46, // [patch][rate]
    CONTROL_FIELD_POSITION        = 47, // [patch]
    CONTROL_FIELD_HUE             = 48, // [patch]
    CONTROL_FIELD_WHITE           = 49, // [patch]
    CONTROL_FIELD_DARK            = 50, // [patch]

    CONTROL_FLOW_DENSITY          = 51, // [patch]
    CONTROL_FLOW_RATE             = 52, // [patch][rate]
    CONTROL_FLOW_HUE              = 53, // [patch]
    CONTROL_FLOW_WHITE            = 54, // [patch]
    CONTROL_FLOW_DARK             = 55, // [patch]

    CONTROL_CORE_HUE              = 56, // [patch]
    CONTROL_CORE_WHITE            = 57, // [patch]
    CONTROL_CORE_DARK             = 58, // [patch]

    CONTROL_ENVELOPE_1_MOVES      = 59, // [switch]
    CONTROL_ENVELOPE_1_DELAY      = 60, // [patch]
    CONTROL_ENVELOPE_1_ATTACK     = 61, // [patch]
    CONTROL_ENVELOPE_1_HOLD       = 62, // [patch]
    CONTROL_ENVELOPE_1_DECAY      = 63, // [patch]
    CONTROL_ENVELOPE_1_SUSTAIN    = 64, // [patch]
    CONTROL_ENVELOPE_1_RELEASE    = 65, // [patch]

    CONTROL_ENVELOPE_2_MOVES      = 66, // [switch]
    CONTROL_ENVELOPE_2_DELAY      = 67, // [patch]
    CONTROL_ENVELOPE_2_ATTACK     = 68, // [patch]
    CONTROL_ENVELOPE_2_HOLD       = 69, // [patch]
    CONTROL_ENVELOPE_2_DECAY      = 70, // [patch]
    CONTROL_ENVELOPE_2_SUSTAIN    = 71, // [patch]
    CONTROL_ENVELOPE_2_RELEASE    = 72, // [patch]

    CONTROL_ENVELOPE_3_MOVES      = 73, // [switch]
    CONTROL_ENVELOPE_3_DELAY      = 74, // [patch]
    CONTROL_ENVELOPE_3_ATTACK     = 75, // [patch]
    CONTROL_ENVELOPE_3_HOLD       = 76, // [patch]
    CONTROL_ENVELOPE_3_DECAY      = 77, // [patch]
    CONTROL_ENVELOPE_3_SUSTAIN    = 78, // [patch]
    CONTROL_ENVELOPE_3_RELEASE    = 79, // [patch]
};

struct AuroraControlDefault {
    uint8_t control;
    uint8_t value;
};

static const AuroraControlDefault AURORA_CONTROL_DEFAULTS[] = {
    { CONTROL_HUE,                  20 },
    { CONTROL_SATURATION,          127 },
    { CONTROL_CELL_BRIGHTNESS,     127 },
    { CONTROL_PAR_SATURATION,      127 },
    { CONTROL_PAR_VALUE,           127 },
    { CONTROL_PAR_HUE_RANGE,        64 },
    { CONTROL_ARP_SPREAD,          127 },
    { CONTROL_LFO_RATE,             64 },
    { CONTROL_CELL_END,            127 },
    { CONTROL_CELL_WIDTH,           20 },
    { CONTROL_CELL_POSITION,        64 },
    { CONTROL_CELL_BEND,            64 },
    { CONTROL_CELL_BEND_AT,         64 },
    { CONTROL_CELL_SPREAD,         127 },
    { CONTROL_CELL_GATE,            64 },
    { CONTROL_CELL_CURVE,           64 },
    { CONTROL_CELL_CURVE_AT,        64 },
    { CONTROL_ENVELOPE_1_SUSTAIN,  127 },
    { CONTROL_ENVELOPE_2_SUSTAIN,  127 },
    { CONTROL_ENVELOPE_3_SUSTAIN,  127 },
    { CONTROL_SCATTER_COUNT,        80 },
    { CONTROL_SCATTER_WIDTH,        34 },
    { CONTROL_SCATTER_EDGE,         40 },
    { CONTROL_SCATTER_RATE,         80 },
    { CONTROL_SCATTER_RANDOMIZE,   110 },
    { CONTROL_SCATTER_SPEED,        64 },
    { CONTROL_SCATTER_POSITION,     64 },
    { CONTROL_SCATTER_HUE,          64 },
    { CONTROL_SCATTER_SATURATION,  127 },
    { CONTROL_SCATTER_VALUE,       127 },
    { CONTROL_FIELD_COUNT,          24 },
    { CONTROL_FIELD_WIDTH,          64 },
    { CONTROL_FIELD_EDGE,           64 },
    { CONTROL_FIELD_SPEED,          64 },
    { CONTROL_FIELD_POSITION,       64 },
    { CONTROL_FIELD_HUE,            64 },
    { CONTROL_FLOW_DENSITY,         20 },
    { CONTROL_FLOW_RATE,            50 },
    { CONTROL_FLOW_HUE,             64 },
    { CONTROL_CORE_HUE,             64 },
};

static const uint8_t WAVE_SWELL  = 32;
static const uint8_t WAVE_FALL   = 64;
static const uint8_t WAVE_SQUARE = 96;

static const uint8_t AURORA_ROUTE_PHASE_STEPS = 16;

static inline uint8_t aurora_route_phase_step(uint8_t value) {
    const uint8_t last = AURORA_ROUTE_PHASE_STEPS - 1;
    return (uint8_t)(((uint16_t)value * last + 63) / 127);
}

static inline float aurora_route_phase(uint8_t value) {
    return (float)aurora_route_phase_step(value) / (float)AURORA_ROUTE_PHASE_STEPS;
}

static const uint8_t AURORA_ROUTES = 8;

enum AuroraRouteField : uint8_t {
    ROUTE_AMOUNT      = 0,
    ROUTE_PHASE       = 1,
    ROUTE_DESTINATION = 2,
    ROUTE_SOURCE      = 3,
    ROUTE_CYCLE       = 4,
    ROUTE_RATIO       = 5,
    ROUTE_WAVE        = 6,
    ROUTE_PATTERN     = 7,
    ROUTE_SMOOTH      = 8,
    ROUTE_HOLD        = 9,
    ROUTE_POLARITY    = 10,
    ROUTE_ARP         = 11,
    ROUTE_FIELDS      = 12,
};

static const uint8_t AURORA_ROUTE_DEFAULTS[ROUTE_FIELDS] = {
    64, 0, 0, 0, 0, 0, WAVE_SWELL, 0, 0, 0, 0, 0,
};

static const uint8_t AURORA_FIRST_ROUTE_CONTROL = CONTROL_ENVELOPE_3_RELEASE + 1;
static const uint8_t AURORA_PATCH_CONTROLS = AURORA_FIRST_ROUTE_CONTROL + ROUTE_FIELDS * AURORA_ROUTES;

static_assert(AURORA_FIRST_ROUTE_CONTROL + (ROUTE_PHASE + 1) * AURORA_ROUTES <= 128, "");

static inline uint8_t aurora_route_control(uint8_t route, uint8_t field) {
    return (uint8_t)(AURORA_FIRST_ROUTE_CONTROL + field * AURORA_ROUTES + route);
}

static inline bool aurora_route_field_of(uint8_t control, uint8_t &route, uint8_t &field) {
    if (control < AURORA_FIRST_ROUTE_CONTROL || control >= AURORA_PATCH_CONTROLS) return false;
    route = (uint8_t)((control - AURORA_FIRST_ROUTE_CONTROL) % AURORA_ROUTES);
    field = (uint8_t)((control - AURORA_FIRST_ROUTE_CONTROL) / AURORA_ROUTES);
    return true;
}

static const uint8_t AURORA_ROUTE_MAX_RATIO = 8;

enum AuroraRouteSource : uint8_t {
    ROUTE_SOURCE_LFO        = 0,
    ROUTE_SOURCE_PAD_X      = 1,
    ROUTE_SOURCE_PAD_Y      = 2,
    ROUTE_SOURCE_ENVELOPE_1 = 3,
    ROUTE_SOURCE_ENVELOPE_2 = 4,
    ROUTE_SOURCE_ENVELOPE_3 = 5,
    ROUTE_SOURCE_STRIP      = 6,
    ROUTE_SOURCE_NOTE       = 7,
    ROUTE_SOURCE_BIRTH      = 8,
    ROUTE_SOURCES           = 9,
};

static inline uint8_t aurora_stepped(uint8_t value, uint8_t count) {
    const uint8_t last = (uint8_t)(count - 1);
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return step > last ? last : step;
}

static inline uint8_t aurora_route_source(uint8_t value) { return aurora_stepped(value, ROUTE_SOURCES); }

static inline uint8_t aurora_route_ratio(uint8_t value) {
    return (uint8_t)(1 + aurora_stepped(value, AURORA_ROUTE_MAX_RATIO));
}

static inline bool aurora_route_once(uint8_t value) { return value >= 64; }

static inline bool aurora_route_held(uint8_t value) { return value >= 64; }

static inline bool aurora_route_bipolar(uint8_t value) { return value >= 64; }

static inline bool aurora_source_is_group(uint8_t source) {
    return source >= ROUTE_SOURCE_STRIP && source < ROUTE_SOURCES;
}

enum AuroraGroupPattern : uint8_t {
    GROUP_SEQUENCE   = 0,
    GROUP_MIRROR     = 1,
    GROUP_EVENS_ODDS = 2,
    GROUP_PAIRS      = 3,
    GROUP_RANDOM     = 4,
    GROUP_PATTERNS   = 5,
};

static inline uint8_t aurora_group_pattern(uint8_t value) {
    const uint8_t last = GROUP_PATTERNS - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return step > last ? last : step;
}

static const uint8_t AURORA_ENVELOPES = 3;

enum AuroraEnvelopeStage : uint8_t {
    ENVELOPE_DELAY   = 0,
    ENVELOPE_ATTACK  = 1,
    ENVELOPE_HOLD    = 2,
    ENVELOPE_DECAY   = 3,
    ENVELOPE_SUSTAIN = 4,
    ENVELOPE_RELEASE = 5,
    ENVELOPE_STAGES  = 6,
};

static inline uint8_t aurora_envelope_control(uint8_t envelope, uint8_t stage) {
    return (uint8_t)(CONTROL_ENVELOPE_1_DELAY + envelope * (ENVELOPE_STAGES + 1) + stage);
}

static inline uint8_t aurora_envelope_moves_control(uint8_t envelope) {
    return (uint8_t)(CONTROL_ENVELOPE_1_MOVES + envelope * (ENVELOPE_STAGES + 1));
}

static inline bool aurora_source_is_envelope(uint8_t source) {
    return source >= ROUTE_SOURCE_ENVELOPE_1 && source <= ROUTE_SOURCE_ENVELOPE_3;
}

enum AuroraArp : uint8_t {
    ARP_UNISON = 0,
    ARP_STEPS  = 1,
    ARP_RIPPLE = 2,
};

static const uint8_t AURORA_PAR_ARP_CONTROLS[] = {
    CONTROL_PAR_HUE_OFFSET, CONTROL_PAR_SATURATION, CONTROL_PAR_VALUE,
};
static const uint8_t AURORA_PAR_ARP_CONTROL_COUNT =
    sizeof(AURORA_PAR_ARP_CONTROLS) / sizeof(AURORA_PAR_ARP_CONTROLS[0]);

static inline bool aurora_par_arp_control(uint8_t control) {
    for (uint8_t index = 0; index < AURORA_PAR_ARP_CONTROL_COUNT; index++) {
        if (AURORA_PAR_ARP_CONTROLS[index] == control) return true;
    }
    return false;
}

static inline uint8_t aurora_route_arp(uint8_t value) {
    if (value < 43) return ARP_UNISON;
    return value < 86 ? ARP_STEPS : ARP_RIPPLE;
}

static const uint8_t AURORA_BIPOLAR_CONTROLS[] = {
    CONTROL_HUE, CONTROL_PAR_HUE_OFFSET, CONTROL_PAR_HUE_RANGE, CONTROL_ARP_SPREAD,
    CONTROL_CELL_POSITION, CONTROL_CELL_BEND, CONTROL_CELL_BEND_AT, CONTROL_CELL_CURVE, CONTROL_CELL_CURVE_AT,
    CONTROL_FIELD_POSITION, CONTROL_FIELD_HUE, CONTROL_FLOW_HUE, CONTROL_CORE_HUE,
};
static const uint8_t AURORA_BIPOLAR_CONTROL_COUNT =
    sizeof(AURORA_BIPOLAR_CONTROLS) / sizeof(AURORA_BIPOLAR_CONTROLS[0]);

static inline bool aurora_bipolar_capable(uint8_t control) {
    for (uint8_t index = 0; index < AURORA_BIPOLAR_CONTROL_COUNT; index++) {
        if (AURORA_BIPOLAR_CONTROLS[index] == control) return true;
    }
    return false;
}

enum AuroraArpMode : uint8_t {
    ARP_MODE_SEQUENCE   = 0,
    ARP_MODE_BOUNCE     = 1,
    ARP_MODE_EVENS_ODDS = 2,
    ARP_MODE_PAIRS      = 3,
    ARP_MODE_MIRROR     = 4,
    ARP_MODE_RANDOM     = 5,
    ARP_MODES           = 6,
};

static inline uint8_t aurora_arp_mode(uint8_t value) {
    const uint8_t last = ARP_MODES - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return step > last ? last : step;
}

enum AuroraHueLayout : uint8_t {
    HUE_LAYOUT_GRADIENT   = 0,
    HUE_LAYOUT_EVENS_ODDS = 1,
    HUE_LAYOUT_PAIRS      = 2,
    HUE_LAYOUT_MIRROR     = 3,
    HUE_LAYOUT_RANDOM     = 4,
    HUE_LAYOUTS           = 5,
};

static inline uint8_t aurora_hue_layout(uint8_t value) {
    const uint8_t last = HUE_LAYOUTS - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return step > last ? last : step;
}

enum AuroraTempoDivision : uint8_t {
    TEMPO_DIVISION_QUARTER        = 0,
    TEMPO_DIVISION_BAR            = 1,
    TEMPO_DIVISION_HALF           = 2,
    TEMPO_DIVISION_EIGHTH         = 3,
    TEMPO_DIVISION_EIGHTH_TRIPLET = 4,
    TEMPO_DIVISION_SIXTEENTH      = 5,
};

static const uint16_t AURORA_TICKS_PER_BEAT = 24;

static const float AURORA_LFO_PERIODS[] = {
    16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 0.375f, 0.25f,
};
static const uint8_t AURORA_LFO_PERIOD_COUNT =
    sizeof(AURORA_LFO_PERIODS) / sizeof(AURORA_LFO_PERIODS[0]);

static inline float aurora_lfo_period(uint8_t value) {
    const uint8_t last = AURORA_LFO_PERIOD_COUNT - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return AURORA_LFO_PERIODS[step > last ? last : step];
}

static const uint8_t AURORA_TRANSITION_STEPS = AURORA_LFO_PERIOD_COUNT + 1;

static inline float aurora_transition_beats(uint8_t value) {
    const uint8_t last = AURORA_TRANSITION_STEPS - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return step >= last ? 0.0f : AURORA_LFO_PERIODS[step];
}

static inline uint16_t aurora_ticks_per_division(uint8_t division) {
    switch (division) {
        case TEMPO_DIVISION_BAR:            return 96;
        case TEMPO_DIVISION_HALF:           return 48;
        case TEMPO_DIVISION_EIGHTH:         return 12;
        case TEMPO_DIVISION_EIGHTH_TRIPLET: return 8;
        case TEMPO_DIVISION_SIXTEENTH:      return 6;
        default:                       return AURORA_TICKS_PER_BEAT;
    }
}

static inline bool aurora_switch_is_on(uint8_t value) { return value >= 64; }

static inline uint8_t aurora_three_way_position(uint8_t value) {
    if (value < 43) return 0;
    if (value < 86) return 1;
    return 2;
}

static inline uint8_t aurora_four_way_position(uint8_t value) {
    return (uint8_t)(value >> 5);
}

enum AuroraCellEnd : uint8_t {
    CELL_END_GONE       = 0,
    CELL_END_START_OVER = 1,
    CELL_END_STAY       = 2,
};

enum AuroraDirection : uint8_t {
    DIRECTION_VERTICAL      = 0,
    DIRECTION_HORIZONTAL    = 1,
    DIRECTION_CELL          = 2,
    DIRECTION_CELL_POSITION = 3,
};

enum AuroraFieldForm : uint8_t {
    FIELD_FORM_GRADIENT       = 0,
    FIELD_FORM_REGION         = 1,
    FIELD_FORM_ALL_BUT_REGION = 2,
};

enum AuroraPadMode : uint8_t {
    PAD_MODE_PER_PATCH = 0,
    PAD_MODE_MORPH     = 1,
    PAD_MODE_EFFECTS   = 2,
};

enum AuroraPadWidth : uint8_t {
    PAD_WIDTH_CENTER = 0,
    PAD_WIDTH_MIDDLE = 1,
    PAD_WIDTH_ALL    = 2,
};

static inline bool aurora_pad_marks(uint8_t width, bool gaps, uint8_t strip, uint8_t strips) {
    const uint8_t middle = strips / 2;
    const uint8_t reach = width == PAD_WIDTH_CENTER ? 0 : width == PAD_WIDTH_MIDDLE ? 1 : middle;
    const bool within = strip + reach >= middle && strip <= middle + reach;
    return within && (!gaps || (strip + reach - middle) % 2 == 0);
}

enum AuroraCorner : uint8_t {
    CORNER_BOTTOM_LEFT  = 0,
    CORNER_BOTTOM_RIGHT = 1,
    CORNER_TOP_LEFT     = 2,
    CORNER_TOP_RIGHT    = 3,
};

enum AuroraPadEffect : uint8_t {
    PAD_EFFECT_FOCUS       = 0,
    PAD_EFFECT_FREEZE      = 1,
    PAD_EFFECT_DOUBLE_TIME = 2,
    PAD_EFFECT_STUTTER     = 3,
    PAD_EFFECT_STROBE      = 4,
    PAD_EFFECTS            = 5,
};

enum AuroraNote : uint8_t {
    NOTE_KEY_HELD             = 57,
    NOTE_PATCH_ONESHOT_FIRST  = 58,
    NOTE_PATCH_ONESHOT_SECOND = 59,
    NOTE_ONESHOT_FIRST        = 60,
};

static const uint8_t AURORA_ONESHOTS = 20;
static const uint8_t AURORA_CORNERS = 4;
static const uint8_t AURORA_KIT_PLACES = AURORA_ONESHOTS + AURORA_CORNERS;
static const uint8_t AURORA_NO_ONESHOT = 127;

static const uint8_t AURORA_SYSEX_ID       = 0x7D;
static const uint8_t AURORA_SYSEX_SIGNATURE_A    = 0x41;
static const uint8_t AURORA_SYSEX_SIGNATURE_B    = 0x55;

static const uint8_t AURORA_SYSEX_HEADER_LENGTH = 5;
static const uint8_t AURORA_SYSEX_FRAME_LENGTH  = AURORA_SYSEX_HEADER_LENGTH + 1;
static const uint16_t AURORA_PIN_POSITION_STEPS = 16383;

enum AuroraSysEx : uint8_t {
    SYSEX_SYNC_BEGIN     = 0x01,
    SYSEX_PATCH_HEAD     = 0x02,
    SYSEX_PATCH_LAYER     = 0x03,
    SYSEX_SYNC_COMMIT    = 0x04,
    SYSEX_SYNC_ABORT     = 0x05,
    SYSEX_QUERY_LIBRARY  = 0x06,
    SYSEX_QUERY_PATCH    = 0x07,
    SYSEX_ONESHOT        = 0x08,
    SYSEX_QUERY_ONESHOT  = 0x09,
    SYSEX_PIN            = 0x0A,
    SYSEX_SONGS_BEGIN    = 0x0B,
    SYSEX_SONG           = 0x0C,
    SYSEX_SONGS_COMMIT   = 0x0D,
    SYSEX_DIAL_LAYER     = 0x0E,
    SYSEX_DIAL           = 0x0F,
    SYSEX_PIN_LAYER      = 0x10,
    SYSEX_UNPIN          = 0x11,

    SYSEX_ACK            = 0x40,
    SYSEX_LIBRARY_INFO   = 0x41,
    SYSEX_PATCH_HEAD_OUT = 0x42,
    SYSEX_PATCH_LAYER_OUT = 0x43,
    SYSEX_ONESHOT_OUT    = 0x44,
};

enum AuroraSysExStatus : uint8_t {
    SYSEX_OK               = 0,
    SYSEX_ERROR_FORMAT     = 1,
    SYSEX_ERROR_SEQUENCE   = 2,
    SYSEX_ERROR_INCOMPLETE = 3,
    SYSEX_ERROR_STORAGE    = 4,
    SYSEX_ERROR_RANGE      = 5,
};

enum AuroraLibraryState : uint8_t {
    LIBRARY_STORED     = 0,
    LIBRARY_EMPTY      = 1,
    LIBRARY_UNREADABLE = 2,
};

static const uint8_t AURORA_PATCH_FORMAT    = 8;
static const uint8_t AURORA_LAST_PATCH_SLOT = PROGRAM_SONG_FIRST - 1;
static const uint8_t AURORA_PATCH_NAME_LENGTH  = 16;
static const uint8_t AURORA_KEYPAD_KEYS     = 9;
static const uint8_t AURORA_BANKS           = 12;

static const uint8_t AURORA_SLOT_MAP_LENGTH    = (AURORA_LAST_PATCH_SLOT + 7) / 7;
static const uint8_t AURORA_KIT_MAP_LENGTH     = (AURORA_KIT_PLACES + 6) / 7;
static const uint8_t AURORA_MARK_MAP_LENGTH    = (AURORA_PATCH_CONTROLS + 6) / 7;
static const uint8_t AURORA_LIBRARY_HEAD_LENGTH = AURORA_SLOT_MAP_LENGTH + AURORA_KIT_MAP_LENGTH + 2;

static inline bool aurora_map_has(const uint8_t *map, uint8_t index) {
    return (map[index / 7] >> (index % 7)) & 1;
}

static inline bool aurora_is_patch_slot(uint8_t slot) {
    return slot != PROGRAM_BLACKOUT && slot <= AURORA_LAST_PATCH_SLOT;
}

enum AuroraPatchLayer : uint8_t {
    PATCH_LAYER_BASE          = 0,
    PATCH_LAYER_COLOR  = 1,
    PATCH_LAYER_MOTION = 2,
    PATCH_LAYER_EXTENT = 3,
    PATCH_LAYER_ACCENT = 4,
    AURORA_PATCH_LAYERS  = 5,
};

static const uint8_t AURORA_PATCH_HEAD_LENGTH = 4 + AURORA_PATCH_NAME_LENGTH;

static const uint16_t AURORA_PATCH_LENGTH =
    AURORA_PATCH_HEAD_LENGTH + (uint16_t)AURORA_PATCH_LAYERS * AURORA_PATCH_CONTROLS;

static const uint8_t AURORA_SONG_SECTIONS = 32;
static const uint8_t AURORA_SONG_HEAD_LENGTH = 4;

static const uint16_t AURORA_ONESHOT_LENGTH =
    1 + AURORA_PATCH_NAME_LENGTH + AURORA_MARK_MAP_LENGTH + AURORA_PATCH_CONTROLS;

#endif
