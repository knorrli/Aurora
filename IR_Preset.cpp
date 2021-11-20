#include "aurora.h"


#define KEYPAD_DEBOUNCE_DELAY 100
static unsigned long lastKeyPadRead = 0;

void renderPreset(int8_t preset) {
  // render preset color indicator
  pixels[PIXEL_INDEX_PRESET_COLOR] = presetColor;

  switch (preset) {
    case 0:
      return;
    case 1:
      Fill(presetColor);
      return;
    case 2:
      Gradient(presetColor);
      break;
    case 3:
      Rainbow(presetColor);
      break;
    case 4:
      Stars(presetColor);
      break;
    case 5:
      Strobe(presetColor);
      break;
    case 6:
      Chaos(presetColor);
      break;
    case 7:
      Rain(presetColor);
      break;
    case 8:
      Invert(presetColor);
      break;
    case 9:
      XVision(presetColor);
      break;
  }
}

void readPreset() {
  if (currentMillis - lastKeyPadRead > KEYPAD_DEBOUNCE_DELAY) {
    uint8_t newPreset = readKeypad();
    if (currentPreset != newPreset) {
      lastPreset = currentPreset;
      currentPreset = newPreset;
    }
    lastKeyPadRead = currentMillis;
  }
}

uint8_t readKeypad() {
  byte pin_8_13 = PINB | 0b00100000;
  switch (pin_8_13) {
    case 0b00100001:
      return 1;
    case 0b00100011:
      return 2;
    case 0b00110001:
      return 3;
    case 0b00100101:
      return 4;
    case 0b00100111:
      return 5;
    case 0b00110101:
      return 6;
    case 0b00101001:
      return 7;
    case 0b00101011:
      return 8;
    case 0b00111001:
      return 9;
    case 0b00110011:
      return 0;
    default:
      return currentPreset;
  }
}
