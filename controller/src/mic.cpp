#include "mic.h"

#include <Arduino.h>

#include "pins.h"
#include "recall.h"
#include "steady.h"

namespace {

enum Stage : uint8_t { LISTENING, CATCHING_PEAK, RESTING };

const uint32_t SAMPLE_MICROS = 100;
const float MIDDLE = pins::ANALOG_FULL / 2.0f;
const float LOUDEST = pins::ANALOG_FULL / 2.0f;
const float MIDDLE_FOLLOWING = 1.0f / 4096.0f;
const float LEVEL_FALLING = 1.0f / 500.0f;
const uint32_t PEAK_MICROS = 5000;
const uint32_t REST_MICROS = 80000;
const uint32_t LED_LIT_MICROS = 60000;
const uint8_t QUIETEST_VELOCITY = 1;
const uint8_t BUTTON_AGREEING = 10;
const uint8_t PLACE = 0;

Every sampleEvery(SAMPLE_MICROS);
Every controlsEvery(1000);
Every thresholdEvery(10000);
Steady switchedOn(BUTTON_AGREEING, HIGH);
Steady trigButton(BUTTON_AGREEING, HIGH);

float middle = MIDDLE;
float level = 0.0f;
float threshold = LOUDEST;
float peak = 0.0f;
Stage stage = LISTENING;
uint32_t stageMicros = 0;
uint32_t firedMicros = 0;
bool fired = false;

void fire(uint8_t velocity, uint32_t micros) {
    recall::oneshotDown(PLACE, velocity);
    recall::oneshotUp(PLACE);
    firedMicros = micros;
    fired = true;
}

uint8_t velocityOf(float loudest) {
    const float above = (loudest - threshold) / (LOUDEST - threshold);
    const float strength = constrain(above, 0.0f, 1.0f);
    return (uint8_t)(QUIETEST_VELOCITY + lroundf(strength * (recall::FULL_STRENGTH - QUIETEST_VELOCITY)));
}

void follow(float sample) {
    middle += (sample - middle) * MIDDLE_FOLLOWING;
    const float swing = fabsf(sample - middle);
    if (swing > level) level = swing;
    else level -= level * LEVEL_FALLING;
}

void listen(uint32_t micros) {
    switch (stage) {
        case LISTENING:
            if (switchedOn.value() != LOW || level <= threshold) return;
            stage = CATCHING_PEAK;
            stageMicros = micros;
            peak = level;
            return;
        case CATCHING_PEAK:
            if (level > peak) peak = level;
            if (micros - stageMicros < PEAK_MICROS) return;
            fire(velocityOf(peak), micros);
            stage = RESTING;
            stageMicros = micros;
            return;
        case RESTING:
            if (micros - stageMicros >= REST_MICROS && level <= threshold) stage = LISTENING;
            return;
    }
}

void readControls(uint32_t micros) {
    switchedOn.settle(digitalRead(pins::MIC_SWITCH));
    if (trigButton.settle(digitalRead(pins::TRIG_BUTTON)) && trigButton.value() == LOW) {
        fire(recall::FULL_STRENGTH, micros);
    }
}

}

namespace mic {

void begin() {
    pinMode(pins::MIC_SWITCH, INPUT_PULLUP);
    pinMode(pins::TRIG_BUTTON, INPUT_PULLUP);
    pinMode(pins::TRIG_LED, OUTPUT);
}

void update(uint32_t micros) {
    if (sampleEvery.due(micros)) {
        follow((float)analogRead(pins::MIC));
        listen(micros);
    }
    if (thresholdEvery.due(micros)) threshold = analogRead(pins::MIC_THRESHOLD) * LOUDEST / pins::ANALOG_FULL;
    if (controlsEvery.due(micros)) readControls(micros);
    digitalWrite(pins::TRIG_LED, fired && micros - firedMicros < LED_LIT_MICROS);
}

}
