#include "tempo.h"

#include <Arduino.h>
#include <math.h>
#include "aurora_protocol.h"

namespace tempo {

// A measured tempo outside this range is a burst of ticks or a glitch,
// not a song — a transport start or a USB reconnect can deliver a whole
// beat's worth of ticks at once. Same range the controller enforces in
// controller/src/tempo.cpp.
static const uint16_t kMinBpm = 20;
static const uint16_t kMaxBpm = 300;

static const uint32_t kSilenceUs = 500000;
static const uint16_t kDefaultBpm = 120;

// Catch-up guard: after a stall long enough to owe this many ticks, skip
// forward instead of manufacturing every one of them.
static const uint32_t kMaxCatchUpTicks = 32;

static uint16_t ticksPerPulse = AURORA_TICKS_PER_BEAT;

static uint32_t ticksReceived = 0;
static uint32_t lastTickUs = 0;
static uint32_t lastRealClockUs = 0;
static uint32_t usPerTick = 0;

static uint16_t ticksSinceBeat = 0;
static uint32_t lastBeatUs = 0;

static float positionTicks = 0.0f;
static uint32_t lastPulseIndex = 0;
static bool pulsedThisFrame = false;
static bool isRunning = true;

static uint32_t usPerTickForBpm(uint16_t beatsPerMinute) {
    return 60000000UL / beatsPerMinute / AURORA_TICKS_PER_BEAT;
}

void begin() {
    usPerTick = usPerTickForBpm(kDefaultBpm);
    lastTickUs = micros();
    lastRealClockUs = lastTickUs;
}

static void advanceWithoutClock(uint32_t now) {
    if ((uint32_t)(now - lastRealClockUs) < kSilenceUs) return;

    if ((uint32_t)(now - lastTickUs) > usPerTick * kMaxCatchUpTicks) {
        lastTickUs = now - usPerTick;
    }
    while ((uint32_t)(now - lastTickUs) >= usPerTick) {
        ticksReceived++;
        lastTickUs += usPerTick;
    }
}

void tick() {
    const uint32_t now = micros();

    if (isRunning) {
        advanceWithoutClock(now);

        float withinTick = (float)(uint32_t)(now - lastTickUs) / (float)usPerTick;
        if (withinTick > 1.0f) withinTick = 1.0f;
        positionTicks = (float)ticksReceived + withinTick;
    }

    const uint32_t pulseIndex = ticksReceived / ticksPerPulse;
    pulsedThisFrame = (pulseIndex != lastPulseIndex);
    lastPulseIndex = pulseIndex;
}

void onClock() {
    const uint32_t now = micros();

    ticksReceived++;
    lastTickUs = now;
    lastRealClockUs = now;

    if (++ticksSinceBeat < AURORA_TICKS_PER_BEAT) return;
    ticksSinceBeat = 0;

    if (lastBeatUs != 0) {
        const uint32_t beatUs = now - lastBeatUs;
        if (beatUs >= 60000000UL / kMaxBpm && beatUs <= 60000000UL / kMinBpm) {
            usPerTick = beatUs / AURORA_TICKS_PER_BEAT;
        }
    }
    lastBeatUs = now;
}

void onStart() {
    ticksReceived = 0;
    ticksSinceBeat = 0;
    lastBeatUs = 0;
    lastPulseIndex = 0;
    positionTicks = 0.0f;
    lastTickUs = micros();
    lastRealClockUs = lastTickUs;
    isRunning = true;
}

void onContinue() {
    lastTickUs = micros();
    lastRealClockUs = lastTickUs;
    isRunning = true;
}

void onStop() {
    isRunning = false;
}

void setDivision(uint8_t division) {
    ticksPerPulse = aurora_ticks_per_gate(division);
}

float beats() {
    return positionTicks / (float)ticksPerPulse;
}

float cyclePosition(float lengthInBeats) {
    if (lengthInBeats <= 0.0f) return 0.0f;
    return fmodf(beats(), lengthInBeats) / lengthInBeats;
}

bool pulsed() { return pulsedThisFrame; }
float bpm() { return 60000000.0f / (float)(usPerTick * AURORA_TICKS_PER_BEAT); }
uint32_t beatLengthMs() { return (uint32_t)usPerTick * ticksPerPulse / 1000; }
uint16_t ticksPerAnimationBeat() { return ticksPerPulse; }
bool running() { return isRunning; }

} // namespace tempo
