#include "faders.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "midi_link.h"
#include "pins.h"
#include "steady.h"
#include "tempo.h"

namespace {

enum Push : uint8_t { RESTING, CLIMBING, LATCHED, DROPPED };

const uint8_t CONTROLS[faders::COUNT] = { CC_FADER_COLOR, CC_FADER_MOTION, CC_FADER_EXTENT };
const uint8_t TOP = 127;
const float LOWEST_READING = 40.0f;
const float HIGHEST_READING = 4080.0f;
const float SMOOTHING = 0.125f;
const float STEP_HYSTERESIS = 0.7f;
const uint32_t TAP_LONGEST_MICROS = 200000;
const uint8_t BEATS_PER_BAR = 4;
const uint8_t CUE_AGREEING = 10;

Every readEvery(1000);
Every cueEvery(2000);
Steady cueRocker(CUE_AGREEING, HIGH);

float smoothed[faders::COUNT];
uint8_t stand[faders::COUNT];
uint8_t held[faders::COUNT];
uint8_t sent[faders::COUNT];
Push push[faders::COUNT];
uint32_t pushedAtMicros[faders::COUNT];

float positionOf(float reading) {
    const float span = (reading - LOWEST_READING) / (HIGHEST_READING - LOWEST_READING);
    return constrain(span, 0.0f, 1.0f) * TOP;
}

float readInverted(uint8_t fader) {
    return (float)(pins::ANALOG_FULL - analogRead(pins::FADERS[fader]));
}

bool cueOn() { return cueRocker.value() == LOW; }

uint8_t pushLevel(uint8_t fader, uint32_t micros) {
    switch (push[fader]) {
        case LATCHED: return TOP;
        case CLIMBING: {
            const uint32_t bar = tempo::microsPerBeat() * BEATS_PER_BAR;
            const uint32_t climbed = micros - pushedAtMicros[fader];
            if (climbed >= bar) return TOP;
            return (uint8_t)(held[fader] + (uint32_t)(TOP - held[fader]) * climbed / bar);
        }
        default: return 0;
    }
}

uint8_t levelOf(uint8_t fader, uint32_t micros) {
    const uint8_t pushed = pushLevel(fader, micros);
    return held[fader] > pushed ? held[fader] : pushed;
}

void send(uint8_t fader, uint8_t level) {
    sent[fader] = level;
    midi_link::sendControl(CONTROLS[fader], level);
}

void sendChanged(uint32_t micros) {
    for (uint8_t fader = 0; fader < faders::COUNT; fader++) {
        const uint8_t level = levelOf(fader, micros);
        if (level != sent[fader]) send(fader, level);
    }
}

void readFaders() {
    for (uint8_t fader = 0; fader < faders::COUNT; fader++) {
        smoothed[fader] += (readInverted(fader) - smoothed[fader]) * SMOOTHING;
        const float position = positionOf(smoothed[fader]);
        if (fabsf(position - stand[fader]) > STEP_HYSTERESIS) stand[fader] = (uint8_t)lroundf(position);
        if (!cueOn()) held[fader] = stand[fader];
    }
}

void readCue() {
    const bool wasOn = cueOn();
    if (!cueRocker.settle(digitalRead(pins::CUE_ROCKER))) return;
    if (!wasOn || cueOn()) return;
    for (uint8_t fader = 0; fader < faders::COUNT; fader++) held[fader] = stand[fader];
    faders::sendAll();
}

}

namespace faders {

void begin() {
    pinMode(pins::CUE_ROCKER, INPUT_PULLUP);
    for (uint8_t fader = 0; fader < COUNT; fader++) {
        smoothed[fader] = readInverted(fader);
        stand[fader] = (uint8_t)lroundf(positionOf(smoothed[fader]));
        held[fader] = stand[fader];
    }
}

void sendAll() {
    const uint32_t micros = ::micros();
    for (uint8_t fader = 0; fader < COUNT; fader++) send(fader, levelOf(fader, micros));
}

void update(uint32_t micros) {
    if (cueEvery.due(micros)) readCue();
    if (readEvery.due(micros)) readFaders();
    sendChanged(micros);
}

void pushDown(Fader fader, uint32_t micros) {
    if (push[fader] == LATCHED) {
        push[fader] = DROPPED;
        return;
    }
    push[fader] = CLIMBING;
    pushedAtMicros[fader] = micros;
}

void pushUp(Fader fader, uint32_t micros) {
    if (push[fader] == CLIMBING && micros - pushedAtMicros[fader] < TAP_LONGEST_MICROS) push[fader] = LATCHED;
    else push[fader] = RESTING;
}

bool cueing() { return cueOn(); }
uint8_t playing(Fader fader) { return sent[fader]; }
uint8_t standing(Fader fader) { return stand[fader]; }

}
