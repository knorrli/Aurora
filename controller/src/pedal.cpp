#include "pedal.h"

#include <Arduino.h>

#include "faders.h"
#include "pins.h"
#include "recall.h"
#include "steady.h"
#include "tempo.h"

namespace {

enum Role : uint8_t { PREV, NEXT, COLOR, MOTION, EXTENT, TAP, ONESHOT_1, ONESHOT_2 };

const uint8_t SWITCHES_PER_LINE = 4;
const uint8_t COMBINATIONS = 1 << SWITCHES_PER_LINE;
const float PULL_UP_OHMS = 1000.0f;
const float SWITCH_OHMS[SWITCHES_PER_LINE] = { 2200.0f, 5100.0f, 10000.0f, 20000.0f };
const int16_t MATCH_TOLERANCE = 20;
const uint8_t AGREEING = 3;
const int16_t UNMATCHED = -1;

struct Line {
    uint8_t pin;
    Role roles[SWITCHES_PER_LINE];
    Steady pressed;
};

Line lines[] = {
    { pins::PEDAL_TIP, { NEXT, PREV, TAP, ONESHOT_1 }, Steady(AGREEING, 0) },
    { pins::PEDAL_RING, { COLOR, MOTION, EXTENT, ONESHOT_2 }, Steady(AGREEING, 0) },
};
const uint8_t LINE_COUNT = sizeof(lines) / sizeof(lines[0]);

Every every(2000);
int16_t levels[COMBINATIONS];

int16_t levelOf(uint8_t combination) {
    float conductance = 0.0f;
    for (uint8_t i = 0; i < SWITCHES_PER_LINE; i++) {
        if (combination & (1 << i)) conductance += 1.0f / SWITCH_OHMS[i];
    }
    if (conductance == 0.0f) return pins::ANALOG_FULL;
    const float toGround = 1.0f / conductance;
    return (int16_t)lroundf(pins::ANALOG_FULL * toGround / (PULL_UP_OHMS + toGround));
}

int16_t match(int16_t reading) {
    for (uint8_t combination = 0; combination < COMBINATIONS; combination++) {
        if (abs(reading - levels[combination]) <= MATCH_TOLERANCE) return combination;
    }
    return UNMATCHED;
}

void act(Role role, bool down, uint32_t micros) {
    switch (role) {
        case PREV: if (down) recall::stepDown(-1); else recall::stepUp(); return;
        case NEXT: if (down) recall::stepDown(1); else recall::stepUp(); return;
        case COLOR: if (down) faders::pushDown(faders::COLOR, micros); else faders::pushUp(faders::COLOR, micros); return;
        case MOTION: if (down) faders::pushDown(faders::MOTION, micros); else faders::pushUp(faders::MOTION, micros); return;
        case EXTENT: if (down) faders::pushDown(faders::EXTENT, micros); else faders::pushUp(faders::EXTENT, micros); return;
        case TAP: if (down) tempo::tap(micros); return;
        case ONESHOT_1: if (down) recall::oneshotDown(0, recall::FULL_STRENGTH); else recall::oneshotUp(0); return;
        case ONESHOT_2: if (down) recall::oneshotDown(1, recall::FULL_STRENGTH); else recall::oneshotUp(1); return;
    }
}

void readLine(Line &line, uint32_t micros) {
    const int16_t combination = match(analogRead(line.pin));
    if (combination == UNMATCHED) return;
    const uint8_t before = (uint8_t)line.pressed.value();
    if (!line.pressed.settle(combination)) return;
    const uint8_t changed = before ^ (uint8_t)line.pressed.value();
    for (uint8_t i = 0; i < SWITCHES_PER_LINE; i++) {
        if (changed & (1 << i)) act(line.roles[i], line.pressed.value() & (1 << i), micros);
    }
}

}

namespace pedal {

void begin() {
    for (uint8_t combination = 0; combination < COMBINATIONS; combination++) levels[combination] = levelOf(combination);
}

void update(uint32_t micros) {
    if (!every.due(micros)) return;
    for (uint8_t i = 0; i < LINE_COUNT; i++) readLine(lines[i], micros);
}

}
