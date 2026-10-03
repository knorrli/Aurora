#include "pad.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "midi_link.h"
#include "pins.h"
#include "steady.h"

namespace {

const uint8_t TOP = 127;
const uint8_t ON = 127;
const uint8_t OFF = 0;
const uint8_t THREE_WAY_VALUES[] = { 0, 64, 127 };
const int16_t THREE_WAY_LOW_BELOW = 1600;
const int16_t THREE_WAY_HIGH_ABOVE = 4000;
const float X_LOWEST = 360.0f;
const float X_HIGHEST = 3720.0f;
const float Y_LOWEST = 880.0f;
const float Y_HIGHEST = 3440.0f;
const float STEP_HYSTERESIS = 0.7f;
const uint32_t SETTLE_MICROS = 20;
const uint8_t SAMPLES = 3;
const uint8_t TOUCH_AGREEING = 2;
const uint8_t LIFT_AGREEING = 3;
const uint8_t ROCKER_AGREEING = 10;

struct Rocker {
    uint8_t pin;
    uint8_t control;
    bool threeWay;
    Steady position;
};

Rocker rockers[] = {
    { pins::HOLD_ROCKER, CC_PAD_HOLD, false, Steady(ROCKER_AGREEING, 0) },
    { pins::PAD_MODE_ROCKER, CC_PAD_MODE, true, Steady(ROCKER_AGREEING, 0) },
    { pins::WIDTH_ROCKER, CC_PAD_WIDTH, true, Steady(ROCKER_AGREEING, PAD_WIDTH_ALL) },
    { pins::GAPS_ROCKER, CC_PAD_GAPS, false, Steady(ROCKER_AGREEING, 0) },
};
const uint8_t HOLD = 0;
const uint8_t MODE = 1;
const uint8_t WIDTH = 2;
const uint8_t GAPS = 3;
const uint8_t ROCKER_COUNT = sizeof(rockers) / sizeof(rockers[0]);

Every touchEvery(4000);
Every rockerEvery(2000);

uint8_t touchedScans = 0;
uint8_t liftedScans = 0;
bool down = false;
bool playingLatched = false;
float x = 0.0f;
float y = 0.0f;
uint8_t sentX = 0;
uint8_t sentY = 0;

uint16_t median(uint8_t pin) {
    uint16_t readings[SAMPLES];
    for (uint8_t i = 0; i < SAMPLES; i++) readings[i] = analogRead(pin);
    for (uint8_t i = 1; i < SAMPLES; i++) {
        for (uint8_t j = i; j > 0 && readings[j - 1] > readings[j]; j--) {
            const uint16_t swap = readings[j];
            readings[j] = readings[j - 1];
            readings[j - 1] = swap;
        }
    }
    return readings[SAMPLES / 2];
}

void release(uint8_t pin) { pinMode(pin, INPUT); }

void drive(uint8_t pin, uint8_t level) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, level);
}

bool pressedNow() {
    release(pins::PAD_XM);
    release(pins::PAD_YP);
    drive(pins::PAD_YM, LOW);
    pinMode(pins::PAD_XP, INPUT_PULLUP);
    delayMicroseconds(SETTLE_MICROS);
    return digitalRead(pins::PAD_XP) == LOW;
}

float readX() {
    release(pins::PAD_YP);
    release(pins::PAD_YM);
    drive(pins::PAD_XP, HIGH);
    drive(pins::PAD_XM, LOW);
    delayMicroseconds(SETTLE_MICROS);
    return (float)(pins::ANALOG_FULL - median(pins::PAD_YP));
}

float readY() {
    release(pins::PAD_XP);
    release(pins::PAD_XM);
    drive(pins::PAD_YP, HIGH);
    drive(pins::PAD_YM, LOW);
    delayMicroseconds(SETTLE_MICROS);
    return (float)(pins::ANALOG_FULL - median(pins::PAD_XM));
}

float scaled(float reading, float lowest, float highest) {
    return constrain((reading - lowest) / (highest - lowest), 0.0f, 1.0f) * TOP;
}

bool moved(float position, uint8_t &sent) {
    if (fabsf(position - sent) <= STEP_HYSTERESIS) return false;
    sent = (uint8_t)lroundf(position);
    return true;
}

bool holdOn() { return rockers[HOLD].position.value() != 0; }

void sendPosition(bool always) {
    if (moved(x, sentX) || always) midi_link::sendControl(CC_PAD_X, sentX);
    if (moved(y, sentY) || always) midi_link::sendControl(CC_PAD_Y, sentY);
}

void land() {
    down = true;
    playingLatched = false;
    sendPosition(true);
    midi_link::sendControl(CC_PAD_TOUCH, ON);
}

void lift() {
    down = false;
    playingLatched = holdOn();
    midi_link::sendControl(CC_PAD_TOUCH, OFF);
}

void readTouch() {
    if (!pressedNow()) {
        touchedScans = 0;
        if (down && ++liftedScans >= LIFT_AGREEING) lift();
        return;
    }
    const float readingX = readX();
    const float readingY = readY();
    if (!pressedNow()) return;
    liftedScans = 0;
    x = scaled(readingX, X_LOWEST, X_HIGHEST);
    y = scaled(readingY, Y_LOWEST, Y_HIGHEST);
    if (down) sendPosition(false);
    else if (++touchedScans >= TOUCH_AGREEING) land();
}

int16_t readRocker(const Rocker &rocker) {
    if (!rocker.threeWay) return digitalRead(rocker.pin) == LOW ? 1 : 0;
    const int16_t level = analogRead(rocker.pin);
    if (level < THREE_WAY_LOW_BELOW) return 0;
    if (level > THREE_WAY_HIGH_ABOVE) return 2;
    return 1;
}

uint8_t rockerValue(const Rocker &rocker) {
    const int16_t position = rocker.position.value();
    if (rocker.threeWay) return THREE_WAY_VALUES[position];
    return position ? ON : OFF;
}

void readRockers() {
    for (uint8_t i = 0; i < ROCKER_COUNT; i++) {
        Rocker &rocker = rockers[i];
        if (!rocker.position.settle(readRocker(rocker))) continue;
        midi_link::sendControl(rocker.control, rockerValue(rocker));
        if (i == HOLD && !holdOn()) playingLatched = false;
    }
}

}

namespace pad {

void begin() {
    pinMode(pins::HOLD_ROCKER, INPUT_PULLUP);
    pinMode(pins::GAPS_ROCKER, INPUT_PULLUP);
    for (uint8_t scan = 0; scan < ROCKER_AGREEING; scan++) {
        for (uint8_t i = 0; i < ROCKER_COUNT; i++) rockers[i].position.settle(readRocker(rockers[i]));
    }
}

void sendAll() {
    for (uint8_t i = 0; i < ROCKER_COUNT; i++) midi_link::sendControl(rockers[i].control, rockerValue(rockers[i]));
    sendPosition(true);
    midi_link::sendControl(CC_PAD_TOUCH, down ? ON : OFF);
}

void update(uint32_t micros) {
    if (rockerEvery.due(micros)) readRockers();
    if (touchEvery.due(micros)) readTouch();
}

uint8_t mode() { return (uint8_t)rockers[MODE].position.value(); }
uint8_t width() { return (uint8_t)rockers[WIDTH].position.value(); }
bool gaps() { return rockers[GAPS].position.value() != 0; }
bool touching() { return down; }
bool latched() { return playingLatched; }

uint8_t column() {
    const uint8_t at = (uint8_t)((uint16_t)sentX * COLUMNS / (TOP + 1));
    return at < COLUMNS ? at : COLUMNS - 1;
}

uint8_t row() {
    const uint8_t at = (uint8_t)((uint16_t)sentY * ROWS / (TOP + 1));
    return at < ROWS ? at : ROWS - 1;
}

}
