// Prints the MIDI arriving over USB, and turns MIDI clock into the tempo
// pulse every Aurora animation runs on.
//
// Plug the Teensy into the laptop, open the serial monitor, and point a
// DAW or MIDI utility at the "Teensy MIDI" port. Typing 0-5 in the serial
// monitor changes the tempo division, same as CC 10 would.
//
// Each pulse also reports how many clock ticks were counted since the
// previous pulse and the tightest / widest spacing between them, which is
// what distinguishes a badly-timed sender from ticks being lost or
// delivered in bursts by USB.
//
// Reading the result:
//   tick count matching the division   — nothing is being dropped.
//   min and max spacing close together — the clock is arriving evenly.
//   min near zero, max large           — ticks are arriving in bursts.
//   every pulse marked "free-run"      — no clock is arriving at all;
//                                        laptop is likely sending to a
//                                        different port.
//   nothing at all                     — the sketch is not running.

#include <Arduino.h>
#include "aurora_protocol.h"

static const uint32_t kClockSilenceUs = 500000;
static const uint32_t kDefaultBeatUs = 500000;
static const uint32_t kPulseLedMs = 30;

static uint8_t division = TEMPO_DIV_QUARTER;
static uint16_t ticksPerPulse = AURORA_TICKS_PER_BEAT;

static uint16_t ticksSincePulse = 0;
static uint16_t ticksSinceBeat = 0;

static uint32_t beatIntervalUs = kDefaultBeatUs;
static uint32_t lastBeatUs = 0;
static uint32_t lastClockUs = 0;
static uint32_t lastTickUs = 0;
static uint32_t lastPulseUs = 0;

static uint16_t ticksCounted = 0;
static uint32_t minTickGapUs = 0xFFFFFFFF;
static uint32_t maxTickGapUs = 0;

static uint32_t ledOffMs = 0;
static bool transportRunning = true;

static const char *divisionName(uint8_t d) {
    switch (d) {
        case TEMPO_DIV_BAR:            return "bar";
        case TEMPO_DIV_HALF:           return "half";
        case TEMPO_DIV_EIGHTH:         return "eighth";
        case TEMPO_DIV_EIGHTH_TRIPLET: return "eighth-triplet";
        case TEMPO_DIV_SIXTEENTH:      return "sixteenth";
        default:                       return "quarter";
    }
}

static uint32_t pulseIntervalUs() {
    return beatIntervalUs / AURORA_TICKS_PER_BEAT * ticksPerPulse;
}

static void setDivision(uint8_t d) {
    if (d >= TEMPO_DIV_COUNT) return;
    division = d;
    ticksPerPulse = aurora_ticks_per_gate(d);
    ticksSincePulse = 0;

    Serial.print("division  ");
    Serial.print(divisionName(division));
    Serial.print("  (");
    Serial.print(ticksPerPulse);
    Serial.println(" ticks per pulse)");
}

static void firePulse(bool freeRunning) {
    const uint32_t now = micros();
    const uint32_t sinceLast = lastPulseUs ? now - lastPulseUs : 0;
    lastPulseUs = now;

    digitalWrite(LED_BUILTIN, HIGH);
    ledOffMs = millis() + kPulseLedMs;

    Serial.print("pulse  ");
    Serial.print(60000000.0f / beatIntervalUs, 1);
    Serial.print(" BPM  ");
    Serial.print(divisionName(division));
    Serial.print("  +");
    Serial.print(sinceLast / 1000.0f, 1);
    Serial.print(" ms");

    if (freeRunning) {
        Serial.println("  [free-run]");
        return;
    }

    Serial.print("  ");
    Serial.print(ticksCounted);
    Serial.print(" ticks  gap ");
    Serial.print(minTickGapUs / 1000.0f, 1);
    Serial.print("/");
    Serial.print(maxTickGapUs / 1000.0f, 1);
    Serial.println(" ms");

    ticksCounted = 0;
    minTickGapUs = 0xFFFFFFFF;
    maxTickGapUs = 0;
}

static void onClock() {
    const uint32_t now = micros();
    lastClockUs = now;

    if (lastTickUs != 0) {
        const uint32_t gap = now - lastTickUs;
        if (gap < minTickGapUs) minTickGapUs = gap;
        if (gap > maxTickGapUs) maxTickGapUs = gap;
    }
    lastTickUs = now;
    ticksCounted++;

    // Beat length is measured on its own 24-tick counter rather than the
    // pulse counter, so BPM stays correct at every division.
    if (++ticksSinceBeat >= AURORA_TICKS_PER_BEAT) {
        ticksSinceBeat = 0;
        if (lastBeatUs != 0) beatIntervalUs = now - lastBeatUs;
        lastBeatUs = now;
    }

    if (++ticksSincePulse >= ticksPerPulse) {
        ticksSincePulse = 0;
        firePulse(false);
    }
}

static void onStart() {
    ticksSinceBeat = 0;
    ticksSincePulse = 0;
    lastBeatUs = 0;
    lastTickUs = 0;
    ticksCounted = 0;
    minTickGapUs = 0xFFFFFFFF;
    maxTickGapUs = 0;
    transportRunning = true;
    Serial.println("start");
}

static void onContinue() {
    transportRunning = true;
    Serial.println("continue");
}

static void onStop() {
    transportRunning = false;
    Serial.println("stop");
}

static void onProgramChange(uint8_t channel, uint8_t program) {
    Serial.print("PC   ch");
    Serial.print(channel);
    Serial.print("  ");
    Serial.print(program);
    if (aurora_pc_is_preset(program)) Serial.print("  (preset)");
    if (aurora_pc_is_palette(program)) Serial.print("  (palette)");
    Serial.println();
}

static void onControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    Serial.print("CC   ch");
    Serial.print(channel);
    Serial.print("  ");
    Serial.print(control);
    Serial.print(" = ");
    Serial.println(value);

    if (control == CC_TEMPO_DIVISION) setDivision(value);
}

static void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    Serial.print("note ch");
    Serial.print(channel);
    Serial.print("  ");
    Serial.print(note);
    Serial.print("  vel ");
    Serial.println(velocity);
}

static void serviceFreeRun() {
    if (!transportRunning) return;
    const uint32_t now = micros();
    if (now - lastClockUs < kClockSilenceUs) return;
    if (now - lastPulseUs < pulseIntervalUs()) return;
    firePulse(true);
}

static void serviceSerialKeys() {
    while (Serial.available()) {
        const int key = Serial.read();
        if (key >= '0' && key <= '9') setDivision(key - '0');
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);

    usbMIDI.setHandleClock(onClock);
    usbMIDI.setHandleStart(onStart);
    usbMIDI.setHandleContinue(onContinue);
    usbMIDI.setHandleStop(onStop);
    usbMIDI.setHandleProgramChange(onProgramChange);
    usbMIDI.setHandleControlChange(onControlChange);
    usbMIDI.setHandleNoteOn(onNoteOn);
}

void loop() {
    while (usbMIDI.read()) { }

    serviceFreeRun();
    serviceSerialKeys();

    if (ledOffMs && (int32_t)(millis() - ledOffMs) >= 0) {
        digitalWrite(LED_BUILTIN, LOW);
        ledOffMs = 0;
    }
}
