#include "tempo.h"

#include <Arduino.h>
#include "midi_io.h"
#include "pins.h"

namespace tempo {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

// External clock is considered stale after this many ms with no incoming
// clock byte. After that we free-run at the last known tempo, or fall back
// to tap if tap is the newer source.
static const uint32_t EXTERNAL_TIMEOUT_MS = 500;

// Below this BPM we assume tap is misclicks / abandoned.
static const uint16_t MIN_BPM = 20;
static const uint16_t MAX_BPM = 300;

// Default tempo until anything teaches us otherwise.
static const uint16_t DEFAULT_BPM = 120;

// Tempo LED flash length in ms (on-time per beat).
static const uint32_t TEMPO_LED_PULSE_MS = 40;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static Source   s_source            = Source::None;
static uint16_t s_bpm               = DEFAULT_BPM;
static uint32_t s_clock_period_us   = 0;  // microseconds between 24-PPQN ticks
static uint32_t s_next_clock_us     = 0;  // when to emit next clock
static uint32_t s_last_external_ms  = 0;  // ms of last external clock byte
static uint8_t  s_tick_counter      = 0;  // 0..23 for beat detection
static uint32_t s_last_tap_ms       = 0;
static bool     s_running           = true;
static uint32_t s_tempo_led_off_ms  = 0;

// For deriving external BPM: measure interval between every 24th clock byte.
static uint32_t s_last_beat_ms      = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void recompute_period_from_bpm() {
    // 60 s / BPM / 24 = seconds per clock tick.
    // In microseconds: 60_000_000 / BPM / 24 = 2_500_000 / BPM.
    if (s_bpm < MIN_BPM) s_bpm = MIN_BPM;
    if (s_bpm > MAX_BPM) s_bpm = MAX_BPM;
    s_clock_period_us = 2500000UL / s_bpm;
}

static void set_bpm(uint16_t new_bpm) {
    s_bpm = new_bpm;
    recompute_period_from_bpm();
}

static void pulse_tempo_led() {
    digitalWrite(PIN_TEMPO_LED, HIGH);
    s_tempo_led_off_ms = millis() + TEMPO_LED_PULSE_MS;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void begin() {
    pinMode(PIN_TEMPO_LED, OUTPUT);
    digitalWrite(PIN_TEMPO_LED, LOW);
    set_bpm(DEFAULT_BPM);
    s_next_clock_us = micros();
}

void tick() {
    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();

    // External-source timeout: if no external clock for a while AND we
    // were locked to external, drop to free-running.
    if (s_source == Source::External
        && (now_ms - s_last_external_ms) > EXTERNAL_TIMEOUT_MS) {
        // Keep last known tempo, but mark as "nobody's pushing us" so a
        // tap can pick up cleanly.
        s_source = Source::None;
    }

    // Emit clock if due. Using explicit period increment to avoid drift.
    if (s_running && s_clock_period_us != 0
        && (int32_t)(now_us - s_next_clock_us) >= 0) {
        midi_io::send_clock();
        s_next_clock_us += s_clock_period_us;

        s_tick_counter = (s_tick_counter + 1) % 24;
        if (s_tick_counter == 0) {
            pulse_tempo_led();
        }

        // Guard against runaway catch-up if we fell very far behind
        // (e.g. a long-blocking call happened). Snap forward if we're
        // more than one period behind.
        if ((int32_t)(now_us - s_next_clock_us) > (int32_t)s_clock_period_us) {
            s_next_clock_us = now_us + s_clock_period_us;
        }
    }

    // Tempo LED off at the end of its pulse window.
    if (s_tempo_led_off_ms && (int32_t)(now_ms - s_tempo_led_off_ms) >= 0) {
        digitalWrite(PIN_TEMPO_LED, LOW);
        s_tempo_led_off_ms = 0;
    }
}

void on_external_clock() {
    const uint32_t now_ms = millis();

    // Every 24 ticks is a beat — use inter-beat interval to derive BPM.
    // Counting from our internal s_tick_counter is wrong (that one is
    // locked to our *emitted* clock, not the incoming one). Use a
    // parallel counter:
    static uint8_t  s_ext_tick = 0;
    s_ext_tick = (s_ext_tick + 1) % 24;
    if (s_ext_tick == 0) {
        if (s_last_beat_ms != 0) {
            const uint32_t beat_ms = now_ms - s_last_beat_ms;
            if (beat_ms > 0) {
                const uint16_t new_bpm = (uint16_t)(60000UL / beat_ms);
                if (new_bpm >= MIN_BPM && new_bpm <= MAX_BPM) {
                    set_bpm(new_bpm);
                }
            }
        }
        s_last_beat_ms = now_ms;
    }

    s_source           = Source::External;
    s_last_external_ms = now_ms;
}

void on_external_start() {
    s_tick_counter = 0;
    s_next_clock_us = micros();
    s_running = true;
}

void on_external_continue() {
    s_running = true;
}

void on_external_stop() {
    s_running = false;
}

void on_tap() {
    const uint32_t now_ms = millis();

    // If the last tap was recent enough to be meaningful, derive tempo
    // from the interval.
    if (s_last_tap_ms != 0) {
        const uint32_t interval_ms = now_ms - s_last_tap_ms;
        if (interval_ms < 3000) {  // 3-second max, otherwise assume new session
            const uint16_t new_bpm = (uint16_t)(60000UL / interval_ms);
            if (new_bpm >= MIN_BPM && new_bpm <= MAX_BPM) {
                set_bpm(new_bpm);
                s_source = Source::Tap;
                s_running = true;
            }
        }
    }

    s_last_tap_ms = now_ms;
}

Source   current_source() { return s_source; }
uint16_t bpm()            { return s_bpm; }
bool     running()        { return s_running; }

} // namespace tempo
