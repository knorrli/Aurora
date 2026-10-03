#include "tempo.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "midi_link.h"
#include "pins.h"
#include "steady.h"

namespace {

const uint32_t MICROS_PER_MINUTE = 60000000UL;
const uint16_t STARTUP_BPM = 120;
const uint16_t SLOWEST_BPM = 20;
const uint16_t FASTEST_BPM = 300;
const uint32_t INCOMING_SILENCE_MICROS = 500000;
const uint8_t INCOMING_TICKS_BEFORE_FOLLOWING = AURORA_TICKS_PER_BEAT;
const uint8_t TAPS_AVERAGED = 4;
const uint32_t TAP_FORGOTTEN_MICROS = 2000000;
const uint8_t ALIGNING_BEATS = 4;
const uint32_t LED_LIT_MICROS = 50000;
const uint8_t TAP_BUTTON_AGREEING = 5;

uint32_t microsPerTick = MICROS_PER_MINUTE / STARTUP_BPM / AURORA_TICKS_PER_BEAT;
uint32_t nextTickMicros = 0;
uint32_t lastTickMicros = 0;
uint32_t lastBeatMicros = 0;
uint8_t ticksIntoBeat = 0;
bool running = true;

uint16_t aligningTicksLeft = 0;
uint32_t aligningMicrosPerTick = 0;

bool following = false;
uint32_t incomingMicros[INCOMING_TICKS_BEFORE_FOLLOWING + 1];
uint8_t incomingCount = 0;
uint32_t lastIncomingMicros = 0;

uint32_t taps[TAPS_AVERAGED];
uint8_t tapCount = 0;

Every tapButtonEvery(1000);
Steady tapButton(TAP_BUTTON_AGREEING, HIGH);

uint32_t clampedMicrosPerTick(uint32_t micros) {
    const uint32_t fastest = MICROS_PER_MINUTE / FASTEST_BPM / AURORA_TICKS_PER_BEAT;
    const uint32_t slowest = MICROS_PER_MINUTE / SLOWEST_BPM / AURORA_TICKS_PER_BEAT;
    return micros < fastest ? fastest : micros > slowest ? slowest : micros;
}

void emitTick(uint32_t micros) {
    midi_link::sendClock();
    lastTickMicros = micros;
    if (!running) return;
    ticksIntoBeat = (uint8_t)((ticksIntoBeat + 1) % AURORA_TICKS_PER_BEAT);
    if (ticksIntoBeat == 0) lastBeatMicros = micros;
}

uint32_t nextSpacing() {
    if (aligningTicksLeft == 0) return microsPerTick;
    aligningTicksLeft--;
    return aligningMicrosPerTick;
}

void freewheel(uint32_t micros) {
    if ((int32_t)(micros - nextTickMicros) < 0) return;
    emitTick(micros);
    const uint32_t spacing = nextSpacing();
    nextTickMicros += spacing;
    if ((int32_t)(micros - nextTickMicros) >= 0) nextTickMicros = micros + spacing;
}

void measureIncoming(uint32_t micros) {
    if (incomingCount > 0 && micros - lastIncomingMicros > INCOMING_SILENCE_MICROS) incomingCount = 0;
    lastIncomingMicros = micros;
    const uint8_t window = INCOMING_TICKS_BEFORE_FOLLOWING + 1;
    if (incomingCount < window) {
        incomingMicros[incomingCount++] = micros;
    } else {
        for (uint8_t i = 1; i < window; i++) incomingMicros[i - 1] = incomingMicros[i];
        incomingMicros[window - 1] = micros;
    }
    if (incomingCount == window) {
        microsPerTick = clampedMicrosPerTick((micros - incomingMicros[0]) / INCOMING_TICKS_BEFORE_FOLLOWING);
    }
}

bool incomingArriving(uint32_t micros) {
    return incomingCount > 0 && micros - lastIncomingMicros < INCOMING_SILENCE_MICROS;
}

void alignBeatTo(uint32_t micros) {
    if (!running) return;
    const uint32_t sinceTick = micros - lastTickMicros;
    const float intoBeat = ticksIntoBeat + (sinceTick < microsPerTick ? (float)sinceTick / microsPerTick : 1.0f);
    const uint16_t aligningTicks = ALIGNING_BEATS * AURORA_TICKS_PER_BEAT;
    const uint16_t count = intoBeat < AURORA_TICKS_PER_BEAT / 2
                         ? aligningTicks - ticksIntoBeat
                         : aligningTicks + AURORA_TICKS_PER_BEAT - ticksIntoBeat;
    aligningMicrosPerTick = (uint32_t)((uint64_t)aligningTicks * microsPerTick / count);
    aligningTicksLeft = count - 1;
    nextTickMicros = micros + aligningMicrosPerTick;
}

void readTapButton(uint32_t micros) {
    if (!tapButtonEvery.due(micros)) return;
    if (tapButton.settle(digitalRead(pins::TAP_BUTTON)) && tapButton.value() == LOW) tempo::tap(micros);
}

}

namespace tempo {

void begin(uint32_t micros) {
    pinMode(pins::TAP_BUTTON, INPUT_PULLUP);
    pinMode(pins::TAP_LED, OUTPUT);
    midi_link::sendStart();
    lastTickMicros = micros;
    lastBeatMicros = micros;
    nextTickMicros = micros + microsPerTick;
}

void update(uint32_t micros) {
    readTapButton(micros);
    if (following && micros - lastIncomingMicros > 2 * microsPerTick) {
        following = false;
        nextTickMicros = lastTickMicros + microsPerTick;
    }
    if (!following) freewheel(micros);
    digitalWrite(pins::TAP_LED, running && micros - lastBeatMicros < LED_LIT_MICROS);
}

void incomingTick(uint32_t micros) {
    measureIncoming(micros);
    if (!following) {
        if (incomingCount <= INCOMING_TICKS_BEFORE_FOLLOWING) return;
        following = true;
        aligningTicksLeft = 0;
        tapCount = 0;
        if (micros - lastTickMicros < microsPerTick / 2) return;
    }
    emitTick(micros);
}

void incomingStart(uint32_t micros) {
    midi_link::sendStart();
    running = true;
    ticksIntoBeat = 0;
    aligningTicksLeft = 0;
    lastBeatMicros = micros;
}

void incomingContinue() {
    midi_link::sendContinue();
    running = true;
}

void incomingStop() {
    midi_link::sendStop();
    running = false;
}

void tap(uint32_t micros) {
    if (incomingArriving(micros)) return;
    if (tapCount > 0 && micros - taps[tapCount - 1] > TAP_FORGOTTEN_MICROS) tapCount = 0;
    if (tapCount == TAPS_AVERAGED) {
        for (uint8_t i = 1; i < TAPS_AVERAGED; i++) taps[i - 1] = taps[i];
        tapCount--;
    }
    taps[tapCount++] = micros;
    if (tapCount >= 2) {
        const uint32_t microsPerTap = (micros - taps[0]) / (tapCount - 1);
        microsPerTick = clampedMicrosPerTick(microsPerTap / AURORA_TICKS_PER_BEAT);
    }
    alignBeatTo(micros);
}

uint32_t microsPerBeat() { return microsPerTick * AURORA_TICKS_PER_BEAT; }

}
