#include "aurora.h"


#define KEYPAD_DEBOUNCE_DELAY 100
uint8_t selectedPreset = 0;
static unsigned long lastKeyPadRead = 0;

void renderPreset(uint8_t preset) {
  // render preset color indicator
  pixels[PIXEL_INDEX_PRESET_COLOR] = presetColor;

  switch (preset) {
    case 0:
      return;
    case 1:
      if (presetAltModeEnabled) {
        FillStars(presetColor);
      } else {
        FillStrips(presetColor);
      }
      return;
    case 2:
      if (presetAltModeEnabled) {
        Invert(presetColor);
      } else {
        Pulse(presetColor);
      }
      break;
    case 3:
      XFill(presetColor);
      break;
    case 4:
      if (presetAltModeEnabled) {
        RisingStars(presetColor);
      } else {
        RisingLines(presetColor);
      }
      break;
    case 5:
      if (presetAltModeEnabled) {
        FallingStars(presetColor);
      } else {
        FallingLines(presetColor);
      }
      break;
    case 6:
      if (presetAltModeEnabled) {
        RainBounce(presetColor);
      } else {
        RainFall(presetColor);
      }
      break;
    case 7:
      if (presetAltModeEnabled) {
        OneOnOneRandom(presetColor);
      } else {
        OneOnOneOrdered(presetColor);
      }
      break;
    case 8:
      if (presetAltModeEnabled) {
        StrobeMirrored(presetColor);
      } else {
        StrobeStrips(presetColor);
      }
      break;
    case 9:
      if (presetAltModeEnabled) {
        Chaos(presetColor);
      } else {
        StrobeUpDown(presetColor);
      }
      break;
  }
}

void readPreset() {
  if (currentMillis - lastKeyPadRead > KEYPAD_DEBOUNCE_DELAY) {
    int8_t newPreset = readKeypad();
    if (newPreset >= 0) {
      if (selectedPreset != newPreset) {
        selectedPreset = newPreset;
      }
      resetPreset(selectedPreset);
    }
    lastKeyPadRead = currentMillis;
  }
}

void resetPreset(uint8_t preset) {
  switch (preset) {
    case 0:
      return;
    case 1:
      return;
    case 2:
      resetPulse();
      break;
    case 3:
      resetXFill();
      break;
    case 4:
      resetRain();
      break;
    case 5:
      resetRise();
      break;
    case 6:
      resetInvert();
      break;
    case 7:
      resetOneOnOne();
      break;
    case 8:
      resetChaos();
      break;
    case 9:
      resetStrobe();
      break;
  }
}

int8_t readKeypad() {
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
      return -1;
  }
}
