#include "tempo.h"

#include <Arduino.h>

#include "aurora_protocol.h"

namespace tempo {

static const uint16_t SLOWEST_BPM = 20;
static const uint16_t FASTEST_BPM = 300;
static const uint16_t STARTUP_BPM = 120;
static const uint32_t MICROS_PER_MINUTE = 60000000UL;
static const uint32_t CLOCK_SILENCE_MICROS = 500000;
static const uint32_t MOST_TICKS_TO_CATCH_UP = 32;

static uint16_t ticksPerDivisionBeat = AURORA_TICKS_PER_BEAT;

static uint32_t ticks = 0;
static uint32_t lastTickMicros = 0;
static uint32_t lastClockMicros = 0;
static uint32_t microsPerTick = MICROS_PER_MINUTE / STARTUP_BPM / AURORA_TICKS_PER_BEAT;

static uint16_t ticksSinceQuarterNote = 0;
static uint32_t lastQuarterNoteMicros = 0;

static float positionTicks = 0.0f;
static uint32_t lastDivisionBeat = 0;
static bool divisionBeatStarted = false;
static bool transportRunning = true;

void begin() {
    lastTickMicros = micros();
    lastClockMicros = lastTickMicros;
}

static void freeRunWithoutClock(uint32_t now) {
    if (now - lastClockMicros < CLOCK_SILENCE_MICROS) return;
    if (now - lastTickMicros > microsPerTick * MOST_TICKS_TO_CATCH_UP) lastTickMicros = now - microsPerTick;
    while (now - lastTickMicros >= microsPerTick) {
        ticks++;
        lastTickMicros += microsPerTick;
    }
}

void advance() {
    divisionBeatStarted = false;
    if (!transportRunning) return;

    const uint32_t now = micros();
    freeRunWithoutClock(now);

    float withinTick = (float)(now - lastTickMicros) / (float)microsPerTick;
    if (withinTick > 1.0f) withinTick = 1.0f;
    positionTicks = (float)ticks + withinTick;

    const uint32_t divisionBeat = ticks / ticksPerDivisionBeat;
    divisionBeatStarted = divisionBeat != lastDivisionBeat;
    lastDivisionBeat = divisionBeat;
}

static void measureTempo(uint32_t now) {
    if (++ticksSinceQuarterNote < AURORA_TICKS_PER_BEAT) return;
    ticksSinceQuarterNote = 0;
    if (lastQuarterNoteMicros != 0) {
        const uint32_t quarterNoteMicros = now - lastQuarterNoteMicros;
        if (quarterNoteMicros >= MICROS_PER_MINUTE / FASTEST_BPM
            && quarterNoteMicros <= MICROS_PER_MINUTE / SLOWEST_BPM) {
            microsPerTick = quarterNoteMicros / AURORA_TICKS_PER_BEAT;
        }
    }
    lastQuarterNoteMicros = now;
}

void onClock() {
    const uint32_t now = micros();
    lastClockMicros = now;
    measureTempo(now);
    if (!transportRunning) return;
    ticks++;
    lastTickMicros = now;
}

void onStart() {
    ticks = 0;
    ticksSinceQuarterNote = 0;
    lastQuarterNoteMicros = 0;
    lastDivisionBeat = 0;
    positionTicks = 0.0f;
    lastTickMicros = micros();
    lastClockMicros = lastTickMicros;
    transportRunning = true;
}

void onContinue() {
    lastTickMicros = micros();
    lastClockMicros = lastTickMicros;
    transportRunning = true;
}

void onStop() {
    transportRunning = false;
}

void setDivision(uint8_t division) {
    ticksPerDivisionBeat = aurora_ticks_per_division(division);
    lastDivisionBeat = ticks / ticksPerDivisionBeat;
}

float quarterNotes() { return positionTicks / (float)AURORA_TICKS_PER_BEAT; }
bool beatStarted() { return divisionBeatStarted; }
bool running() { return transportRunning; }

}
