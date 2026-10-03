#include "rotary.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "pins.h"
#include "recall.h"
#include "steady.h"

namespace {

const float STEP = (float)pins::ANALOG_FULL / AURORA_BANKS;
const float MATCH_SHARE = 0.3f;
const uint8_t AGREEING = 3;
const int16_t BETWEEN_DETENTS = 0;

Every every(5000);
Steady bank(AGREEING, 1);

int16_t positionOf(uint16_t reading) {
    const float steps = reading / STEP;
    const int16_t nearest = (int16_t)lroundf(steps);
    if (nearest < 1 || nearest > AURORA_BANKS) return BETWEEN_DETENTS;
    if (fabsf(steps - nearest) > MATCH_SHARE) return BETWEEN_DETENTS;
    return nearest;
}

bool read() {
    const int16_t position = positionOf(analogRead(pins::ROTARY));
    return position != BETWEEN_DETENTS && bank.settle(position);
}

}

namespace rotary {

void begin() {
    for (uint8_t i = 0; i < AGREEING; i++) read();
    recall::setBank((uint8_t)bank.value());
}

void update(uint32_t micros) {
    if (every.due(micros) && read()) recall::setBank((uint8_t)bank.value());
}

}
