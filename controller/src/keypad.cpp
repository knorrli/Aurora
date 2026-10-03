#include "keypad.h"

#include <Arduino.h>

#include "pins.h"
#include "recall.h"
#include "steady.h"

namespace {

const int8_t HOOK = 10;
const int8_t NOTHING = 11;
const int8_t UNREADABLE = 12;

const int8_t PRESSED_BY_CODE[16] = {
    1, 2, 4, 5, 7, 8, UNREADABLE, UNREADABLE,
    3, 0, 6, 1, 9, UNREADABLE, NOTHING, HOOK,
};

const uint8_t AGREEING = 5;

Every every(1000);
Steady pressed(AGREEING, NOTHING);
int8_t down = NOTHING;

uint8_t readCode() {
    uint8_t code = 0;
    for (uint8_t line = 0; line < pins::KEYPAD_LINE_COUNT; line++) {
        code |= (uint8_t)(digitalRead(pins::KEYPAD_LINES[line]) << line);
    }
    return code;
}

void press(int8_t what) {
    if (what == HOOK) recall::oneshotDown(1, recall::FULL_STRENGTH);
    else recall::keyDown((uint8_t)what);
}

void release(int8_t what) {
    if (what == HOOK) recall::oneshotUp(1);
    else recall::keyUp();
}

}

namespace keypad {

void begin() {
    for (uint8_t line = 0; line < pins::KEYPAD_LINE_COUNT; line++) pinMode(pins::KEYPAD_LINES[line], INPUT_PULLUP);
}

void update(uint32_t micros) {
    if (!every.due(micros)) return;
    const int8_t reading = PRESSED_BY_CODE[readCode()];
    if (reading == UNREADABLE || !pressed.settle(reading)) return;
    const int8_t now = pressed.value();
    if (down == NOTHING) {
        down = now;
        press(down);
    } else if (now == NOTHING) {
        release(down);
        down = NOTHING;
    }
}

}
