#pragma once

#include <stdint.h>

namespace pins {

const uint8_t PIXELS = 2;
const uint8_t KEYPAD_LINES[] = { 3, 4, 5, 6 };
const uint8_t KEYPAD_LINE_COUNT = sizeof(KEYPAD_LINES) / sizeof(KEYPAD_LINES[0]);
const uint8_t PAD_XP = 7;
const uint8_t PAD_YM = 8;
const uint8_t MIC_SWITCH = 9;
const uint8_t TRIG_BUTTON = 10;
const uint8_t TRIG_LED = 11;
const uint8_t TAP_BUTTON = 12;
const uint8_t TAP_LED = 13;
const uint8_t PAD_YP = 14;
const uint8_t PAD_XM = 15;
const uint8_t FADERS[] = { 16, 17, 18 };
const uint8_t ROTARY = 19;
const uint8_t PEDAL_TIP = 20;
const uint8_t PEDAL_RING = 21;
const uint8_t MIC = 22;
const uint8_t MIC_THRESHOLD = 23;
const uint8_t PAD_MODE_ROCKER = 24;
const uint8_t WIDTH_ROCKER = 25;
const uint8_t HOLD_ROCKER = 26;
const uint8_t GAPS_ROCKER = 27;
const uint8_t CUE_ROCKER = 28;

const uint16_t ANALOG_FULL = 4095;

}
