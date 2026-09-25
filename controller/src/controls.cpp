#include "controls.h"

#include <Arduino.h>
#include "aurora_protocol.h"
#include "midi_io.h"
#include "pins.h"
#include "tempo.h"

namespace controls {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

static const uint16_t FADER_CHANGE_THRESHOLD    = 2;   // in MIDI units (0–127)
static const uint32_t KEYPAD_DEBOUNCE_MS        = 100;
static const uint32_t TAP_DEBOUNCE_MS           = 30;
static const uint32_t SWITCH_DEBOUNCE_MS        = 40;
static const uint32_t TRIGGER_DEBOUNCE_MS       = 50;

// ---------------------------------------------------------------------------
// State cache — only emit MIDI on change
// ---------------------------------------------------------------------------

static uint8_t  s_last_hue              = 0xFF;
static uint8_t  s_last_saturation       = 0xFF;
static uint8_t  s_last_value            = 0xFF;

static int8_t   s_last_preset = -1;
static uint32_t s_last_keypad_read_ms    = 0;

static uint32_t s_last_tap_edge_ms       = 0;
static bool     s_tap_prev_state         = false;

static uint32_t s_last_trigger_ms        = 0;
static bool     s_trigger_prev_state     = false;

static uint8_t  s_last_strip_mode        = 0xFF;
static uint8_t  s_last_effect_mode       = 0xFF;
static uint8_t  s_last_hold_mode         = 0xFF;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline uint8_t scale_to_midi(uint16_t adc_value) {
    // 0–1023 → 0–127. Use >> 3 rather than map() for speed.
    return (uint8_t)(adc_value >> 3);
}

// ---------------------------------------------------------------------------
// Keypad — 3×4 matrix read via PORTB (pins 8–12)
// Copied from the brain firmware's readKeypad() — the bit patterns are
// determined by how the physical keypad is wired, so they stay identical.
// ---------------------------------------------------------------------------

static int8_t scan_keypad() {
    byte pinb = PINB | 0b00100000;  // upper bits masked in

    // All keys held → mute / off. Reserved for future, currently we
    // don't emit anything special for mute.
    if (pinb == 0b00111111) return -2;

    // Center key combo — "return to previous". The old firmware tracked
    // previous preset locally; in the MIDI world we'd need the brain to
    // track it. For now, emit nothing.
    if (pinb == 0b00111101) return -3;

    switch (pinb) {
        case 0b00100001:
        case 0b00110111: return 1;
        case 0b00100011: return 2;
        case 0b00110001: return 3;
        case 0b00100101: return 4;
        case 0b00100111: return 5;
        case 0b00110101: return 6;
        case 0b00101001: return 7;
        case 0b00101011: return 8;
        case 0b00111001: return 9;
        case 0b00110011: return 0;
        default:         return -1;
    }
}

// ---------------------------------------------------------------------------
// Faders
// ---------------------------------------------------------------------------

static void scan_faders() {
    const uint8_t hue = scale_to_midi(analogRead(PIN_FADER_HUE));
    const uint8_t sat = scale_to_midi(analogRead(PIN_FADER_SATURATION));
    const uint8_t val = scale_to_midi(analogRead(PIN_FADER_VALUE));

    if (abs((int)hue - (int)s_last_hue) >= FADER_CHANGE_THRESHOLD) {
        midi_io::send_control_change(CC_HUE, hue);
        s_last_hue = hue;
    }
    if (abs((int)sat - (int)s_last_saturation) >= FADER_CHANGE_THRESHOLD) {
        midi_io::send_control_change(CC_SATURATION, sat);
        s_last_saturation = sat;
    }
    if (abs((int)val - (int)s_last_value) >= FADER_CHANGE_THRESHOLD) {
        midi_io::send_control_change(CC_VALUE, val);
        s_last_value = val;
    }
}

// ---------------------------------------------------------------------------
// Preset numpad
// ---------------------------------------------------------------------------

static void scan_numpad() {
    const uint32_t now = millis();
    if ((now - s_last_keypad_read_ms) < KEYPAD_DEBOUNCE_MS) return;
    s_last_keypad_read_ms = now;

    const int8_t key = scan_keypad();
    if (key < 0) return;  // nothing pressed or special combo we don't handle yet

    if (key != s_last_preset) {
        midi_io::send_program_change((uint8_t)key);
        s_last_preset = key;
    }
}

// ---------------------------------------------------------------------------
// Tap tempo button
// ---------------------------------------------------------------------------

static void scan_tap() {
    const bool pressed = (digitalRead(PIN_TAP_TEMPO) == LOW);
    const uint32_t now = millis();

    if (pressed && !s_tap_prev_state
        && (now - s_last_tap_edge_ms) >= TAP_DEBOUNCE_MS) {
        tempo::on_tap();
        s_last_tap_edge_ms = now;
    }
    s_tap_prev_state = pressed;
}

// ---------------------------------------------------------------------------
// Mic trigger (envelope follower output, digital)
// ---------------------------------------------------------------------------

static void scan_trigger() {
    const bool active = (digitalRead(PIN_MIC_TRIGGER) == HIGH);
    const uint32_t now = millis();

    if (active && !s_trigger_prev_state
        && (now - s_last_trigger_ms) >= TRIGGER_DEBOUNCE_MS) {
        midi_io::send_note_on(NOTE_TRIGGER_FLASH, 127);
        s_last_trigger_ms = now;
    }
    // Note: we send only the rising edge as a Note On; the brain treats
    // it as a transient. Never send Note Off — not musically meaningful.
    s_trigger_prev_state = active;
}

// ---------------------------------------------------------------------------
// Mode switches (digital + analog rotary)
// ---------------------------------------------------------------------------

static uint8_t read_strip_mode() {
    const uint16_t v = analogRead(PIN_TOUCHPAD_STRIP_MODE);
    if      (v > 1000) return 127; // mirrored-exclusive
    else if (v >  400) return  64; // all
    else               return   0; // mirrored
}

// A rocker reports where it stands and says nothing about what that means —
// the patch decides. So these send positions under names that describe where
// the switch *is* on the panel, not what v1 made it do.
//
// The pin names still carry v1's meanings and the mapping from pin to panel
// position is not recorded anywhere; docs/controls.md says so. Correct these
// three when the box is metered.
//
// The fifth switch has no pin. v1 read five rockers on four inputs by packing
// fader-alt and preset-alt onto A7, which is why CC_ROCKER_FADERS goes unsent
// until the rebuild gives it one.
static void scan_switches() {
    const uint8_t effect = digitalRead(PIN_TOUCHPAD_EFFECT) ? 127 : 0;
    const uint8_t hold   = digitalRead(PIN_HOLD_MODE)        ? 127 : 0;
    const uint8_t strip  = read_strip_mode();

    if (effect != s_last_effect_mode) {
        midi_io::send_control_change(CC_ROCKER_PAD_B, effect);
        s_last_effect_mode = effect;
    }
    if (hold != s_last_hold_mode) {
        midi_io::send_control_change(CC_ROCKER_PAD_C, hold);
        s_last_hold_mode = hold;
    }
    if (strip != s_last_strip_mode) {
        midi_io::send_control_change(CC_ROCKER_PAD_A, strip);
        s_last_strip_mode = strip;
    }
}

// ---------------------------------------------------------------------------
// Touchpad
// ---------------------------------------------------------------------------
// Stubbed for now — the Adafruit TouchScreen library + 4-wire read is
// identical to what the brain does today; porting is straightforward but
// bulky. When we're ready to flesh this out, emit:
//   CC_PAD_X, CC_PAD_Y  — positions that persist when the finger lifts
//   CC_PAD_PRESSURE
//   CC_PAD_ENGAGE       — whether the pad reaches anything at all, which is
//                         a control of its own and not "a finger is down"

static void scan_touchpad() {
    // TODO: move R_Touchpad logic here, convert to CC emission.
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void begin() {
    pinMode(PIN_TAP_TEMPO,       INPUT_PULLUP);
    pinMode(PIN_MIC_TRIGGER,     INPUT);
    pinMode(PIN_TOUCHPAD_EFFECT, INPUT);
    pinMode(PIN_HOLD_MODE,       INPUT);

    pinMode(PIN_FADER_HUE,        INPUT);
    pinMode(PIN_FADER_SATURATION, INPUT);
    pinMode(PIN_FADER_VALUE,      INPUT);

    // Keypad pins 8-12: default pullups via matrix wiring. Arduino handles.
}

void tick() {
    scan_faders();
    scan_numpad();
    scan_tap();
    scan_trigger();
    scan_switches();
    scan_touchpad();
}

} // namespace controls
