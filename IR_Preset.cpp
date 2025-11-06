#include "aurora.h"


#define KEYPAD_DEBOUNCE_DELAY 100
uint8_t selectedPreset = 0;
uint8_t previousPreset = 0;
static unsigned long lastKeyPadRead = 0;

void renderPreset(uint8_t preset) {
  // render preset color indicator
  pixels[PIXEL_INDEX_PRESET_COLOR] = CHSV(presetColor.hue, 255, 255);

  switch (preset) {
    case 0:
      return;
    case 1:
      AlbtraumBreath();
      return;
    case 2:
      AlbtraumParty();
      break;
    case 3:
      AlbtraumChaossaft();
      break;
    case 4:
      TransitionRainbow();
      break;
    case 5:
      TransitionStrobo();
    case 6:
      break;
    case 7:
      ErnstBreath();
      break;
    case 8:
      ErnstParty();
      break;
    case 9:
      ErnstDark();
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
      break;
    case 3:
      break;
    case 4:
      break;
    case 5:
      break;
    case 6:
      break;
    case 7:
      break;
    case 8:
      break;
    case 9:
      break;
  }
}

int8_t readKeypad() {
  byte pin_8_13 = PINB | 0b00100000;
  if (pin_8_13 == 0b00111111) {
    muted = true;
  } else {
    muted = false;
  }

  if (pin_8_13 == 0b00111101) {
    return previousPreset;
  }

  switch (pin_8_13) {
    case 0b00100001:
    case 0b00110111:
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
