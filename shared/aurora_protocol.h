#ifndef AURORA_PROTOCOL_H
#define AURORA_PROTOCOL_H

#include <stdint.h>

static const uint8_t AURORA_MIDI_CHANNEL = 1;

enum AuroraProgram : uint8_t {
    PROGRAM_BLACKOUT = 0,
    PROGRAM_SHOW     = 10,
};

enum AuroraCC : uint8_t {
    CC_TEMPO_DIVISION        = 2,  // [switch]

    CC_PAR_HUE_SHUFFLE       = 3,  // [patch][plain]
    CC_PAR_HUE_SHUFFLE_EVERY = 4,  // [patch]
    CC_PAR_LFO_SHUFFLE       = 5,  // [patch][plain]

    CC_FADER_COLOR           = 12, // [ambient]
    CC_FADER_EXTENT          = 13, // [ambient]
    CC_FADER_MOTION          = 14, // [ambient]

    CC_TOUCHPAD_X            = 15, // [gesture]
    CC_TOUCHPAD_Y            = 16, // [gesture]
    CC_TOUCHPAD_PRESSURE     = 17, // [gesture]
    CC_TOUCHPAD_ENGAGE       = 18, // [gesture]

    CC_ROCKER_TOUCHPAD_A     = 19, // [ambient]
    CC_ROCKER_TOUCHPAD_B     = 20, // [ambient]
    CC_ROCKER_TOUCHPAD_C     = 21, // [ambient]
    CC_ROCKER_TOUCHPAD_D     = 22, // [ambient]
    CC_ROCKER_FADERS         = 23, // [ambient]

    CC_AUDIO_FOLLOWER        = 24, // [ambient]
    CC_AUDIO_THRESHOLD       = 25, // [ambient]

    CC_KEY_HELD              = 26, // [gesture]
    CC_PAR_VALUE             = 27, // [patch][plain]
    CC_PAR_HUE_OFFSET        = 28, // [patch][circular][plain]
    CC_PAR_SATURATION        = 29, // [patch][plain]
    CC_PAR_HUE_SPREAD        = 30, // [patch][plain]
    CC_PAR_LFO_SPREAD        = 31, // [patch][plain]

    CC_HUE                   = 33, // [patch][circular]
    CC_SATURATION            = 34, // [patch]
    CC_VALUE                 = 35, // [patch]

    CC_FIELD_FORM            = 36, // [switch]
    CC_FIELD_DIRECTION       = 37, // [switch]
    CC_FIELD_HUE             = 38, // [patch]
    CC_FIELD_WHITE           = 39, // [patch]
    CC_FIELD_DARK            = 40, // [patch]
    CC_FIELD_COUNT           = 41, // [patch]
    CC_FIELD_WIDTH           = 42, // [patch]
    CC_FIELD_EDGE            = 43, // [patch]
    CC_FIELD_SPEED           = 44, // [patch][rate]
    CC_FLOW_HUE              = 45, // [patch]
    CC_FLOW_WHITE            = 46, // [patch]
    CC_FLOW_DARK             = 47, // [patch]
    CC_FLOW_RATE             = 48, // [patch][rate]
    CC_FLOW_DENSITY          = 49, // [patch]
    CC_LIGHT_HUE             = 50, // [patch]
    CC_LIGHT_WHITE           = 51, // [patch]
    CC_LIGHT_DARK            = 52, // [patch]
    CC_PALETTE               = 53, // [switch]

    CC_SHAPE_BOUNCE          = 54, // [switch]
    CC_SHAPE_WIDTH           = 55, // [patch]
    CC_SHAPE_COUNT           = 56, // [patch][plain]
    CC_SHAPE_EDGE            = 57, // [patch]
    CC_SHAPE_TAIL            = 58, // [patch]
    CC_SHAPE_POSITION        = 59, // [patch][circular]
    CC_SHAPE_SPEED           = 60, // [patch][rate]

    CC_FAN_FREQUENCY         = 61, // [patch][plain]
    CC_FAN_PHASE             = 62, // [patch][circular][plain]
    CC_FAN_RANDOMIZE         = 63, // [patch][plain]
    CC_FAN_SPREAD            = 64, // [patch][plain]
    CC_FAN_SPEED             = 65, // [patch][rate][plain]
    CC_FAN_LFO               = 66, // [patch][plain]

    CC_LFO_RATE              = 67, // [patch]

    CC_SHAPE_BEND            = 68, // [patch][plain]
    CC_SHAPE_BEND_AT         = 69, // [patch][plain]

    CC_SCATTER_SPREAD        = 70, // [patch]
    CC_SCATTER_RATE          = 71, // [patch][rate]
    CC_SCATTER_COUNT         = 72, // [patch]
    CC_SCATTER_WIDTH         = 73, // [patch]
    CC_SCATTER_EDGE          = 74, // [patch]
    CC_SCATTER_RANDOMIZE     = 75, // [patch]
    CC_SCATTER_SLIDE         = 76, // [patch]
    CC_SCATTER_VALUE         = 77, // [patch]
    CC_SCATTER_HUE           = 78, // [patch]
    CC_SCATTER_WHITE         = 79, // [patch]
};

struct AuroraControlDefault {
    uint8_t cc;
    uint8_t value;
};

static const AuroraControlDefault AURORA_CONTROL_DEFAULTS[] = {
    { CC_PAR_HUE_SHUFFLE_EVERY,   42 },
    { CC_PAR_VALUE,              127 },
    { CC_PAR_SATURATION,         127 },
    { CC_PAR_HUE_SPREAD,          64 },
    { CC_PAR_LFO_SPREAD,          64 },
    { CC_HUE,                     20 },
    { CC_SATURATION,             127 },
    { CC_VALUE,                  127 },
    { CC_FIELD_DIRECTION,         64 },
    { CC_FIELD_HUE,               64 },
    { CC_FIELD_WIDTH,             64 },
    { CC_FIELD_EDGE,              64 },
    { CC_FIELD_SPEED,             64 },
    { CC_FLOW_HUE,                64 },
    { CC_FLOW_RATE,               50 },
    { CC_FLOW_DENSITY,            20 },
    { CC_LIGHT_HUE,               64 },
    { CC_SHAPE_WIDTH,             40 },
    { CC_SHAPE_EDGE,              18 },
    { CC_SHAPE_POSITION,          64 },
    { CC_SHAPE_SPEED,             80 },
    { CC_FAN_FREQUENCY,           32 },
    { CC_FAN_SPREAD,              64 },
    { CC_FAN_SPEED,               64 },
    { CC_FAN_LFO,                 64 },
    { CC_LFO_RATE,                64 },
    { CC_SHAPE_BEND,              64 },
    { CC_SHAPE_BEND_AT,           64 },
    { CC_SCATTER_RATE,            60 },
    { CC_SCATTER_COUNT,           80 },
    { CC_SCATTER_WIDTH,           34 },
    { CC_SCATTER_EDGE,            40 },
    { CC_SCATTER_RANDOMIZE,      110 },
    { CC_SCATTER_SLIDE,           64 },
    { CC_SCATTER_VALUE,           64 },
    { CC_SCATTER_HUE,             64 },
};

static const uint8_t WAVE_SWELL  = 32;
static const uint8_t WAVE_SNAP   = 64;
static const uint8_t WAVE_SQUARE = 96;

static const uint8_t AURORA_ROUTES = 8;

static const uint8_t AURORA_ROUTE_BASE[AURORA_ROUTES] = {
    80, 85, 90, 95, 100, 105, 110, 115,
};

enum AuroraRouteField : uint8_t {
    ROUTE_DESTINATION = 0,
    ROUTE_AMOUNT      = 1,
    ROUTE_RATIO       = 2,
    ROUTE_WAVE        = 3,
    ROUTE_PHASE       = 4,
    ROUTE_FIELDS      = 5,
};

static const uint8_t AURORA_ROUTE_DEFAULTS[ROUTE_FIELDS] = {
    0, 64, 0, WAVE_SWELL, 0,
};

static inline uint8_t aurora_route_cc(uint8_t route, uint8_t field) {
    return (uint8_t)(AURORA_ROUTE_BASE[route] + field);
}

static const uint8_t AURORA_ROUTE_MAX_RATIO = 8;

static inline uint8_t aurora_route_ratio(uint8_t value) {
    const uint8_t last = AURORA_ROUTE_MAX_RATIO - 1;
    const uint8_t step = (uint8_t)(((uint16_t)value * last + 63) / 127);
    return (uint8_t)(1 + (step > last ? last : step));
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

enum AuroraFieldForm : uint8_t {
    FIELD_FORM_GRADIENT       = 0,
    FIELD_FORM_REGION         = 1,
    FIELD_FORM_ALL_BUT_REGION = 2,
};

enum AuroraFieldDirection : uint8_t {
    FIELD_DIRECTION_HORIZONTAL = 0,
    FIELD_DIRECTION_VERTICAL   = 1,
    FIELD_DIRECTION_SHAPE      = 2,
};

enum AuroraNote : uint8_t {
    NOTE_TRIGGER_FLASH = 60,
};

static const uint8_t AURORA_SYSEX_ID       = 0x7D;
static const uint8_t AURORA_SYSEX_SIGNATURE_A    = 0x41;
static const uint8_t AURORA_SYSEX_SIGNATURE_B    = 0x55;

static const uint8_t AURORA_SYSEX_HEADER_LENGTH = 5;
static const uint8_t AURORA_SYSEX_FRAME_LENGTH  = AURORA_SYSEX_HEADER_LENGTH + 1;

enum AuroraSysEx : uint8_t {
    SYSEX_SYNC_BEGIN     = 0x01,
    SYSEX_PATCH_HEAD     = 0x02,
    SYSEX_PATCH_PART     = 0x03,
    SYSEX_SYNC_COMMIT    = 0x04,
    SYSEX_SYNC_ABORT     = 0x05,
    SYSEX_QUERY_LIBRARY  = 0x06,
    SYSEX_QUERY_PATCH    = 0x07,

    SYSEX_ACK            = 0x40,
    SYSEX_LIBRARY_INFO   = 0x41,
    SYSEX_PATCH_HEAD_OUT = 0x42,
    SYSEX_PATCH_PART_OUT = 0x43,
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

static const uint8_t AURORA_PATCH_FORMAT    = 1;
static const uint8_t AURORA_PATCH_MAX       = 128;
static const uint8_t AURORA_PATCH_CC_COUNT  = 128;
static const uint8_t AURORA_PATCH_NAME_LENGTH  = 16;
static const uint8_t AURORA_KEYPAD_KEYS     = 9;

static const uint8_t AURORA_SLOT_MAP_LENGTH    = (AURORA_PATCH_MAX + 6) / 7;

static inline bool aurora_slot_filled(const uint8_t *map, uint8_t slot) {
    return (map[slot / 7] >> (slot % 7)) & 1;
}

enum AuroraPatchPart : uint8_t {
    PATCH_BASE          = 0,
    PATCH_TARGET_COLOR  = 1,
    PATCH_TARGET_EXTENT = 2,
    PATCH_TARGET_MOTION = 3,
    PATCH_TARGET_ACCENT = 4,
    AURORA_PATCH_PARTS  = 5,
};

static const uint8_t AURORA_PATCH_HEAD_LENGTH = 4 + AURORA_PATCH_NAME_LENGTH;

static const uint16_t AURORA_PATCH_LENGTH =
    AURORA_PATCH_HEAD_LENGTH + (uint16_t)AURORA_PATCH_PARTS * AURORA_PATCH_CC_COUNT;

#define AURORA_PROTOCOL_VERSION_MAJOR 0
#define AURORA_PROTOCOL_VERSION_MINOR 11

#endif
