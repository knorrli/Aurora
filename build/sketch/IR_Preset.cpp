#line 1 "/Users/igor/Projects/Aurora/IR_Preset.cpp"
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
      // FillBlocks
      if (presetAltModeEnabled) {
        FillStars(presetColor);
      } else {
        FillStrips(presetColor);
      }
      return;
    case 2:
      // DispersingBlocks
      if (presetAltModeEnabled) {
        RisingStars(presetColor);
      } else {
        RisingBlocks(presetColor);
      }
      break;
    case 3:
      // ConvergingBlocks
      if (presetAltModeEnabled) {
        FallingStars(presetColor);
      } else {
        FallingBlocks(presetColor);
      }
      break;
    case 4:
      // TODO
      if (presetAltModeEnabled) {
        Bars(presetColor);
      } else {
        PulseFill(presetColor);
      }
      break;
    case 5:
      // TODO
      if (presetAltModeEnabled) {
        XFill(presetColor);
      } else {
        Invert(presetColor);
      }
      break;
    case 6:
      // TODO
      if (presetAltModeEnabled) {
        RainBounce(presetColor);
      } else {
        RainFall(presetColor);
      }
      break;
    case 7:
      // StripByStripMirrored(presetColor);
      if (presetAltModeEnabled) {
        StripByStripRandom(presetColor);
      } else {
        StripByStripOrdered(presetColor);
      }
      break;
    case 8:
      // StrobeUpDown(presetColor);
      if (presetAltModeEnabled) {
        Chaos(presetColor);
      } else {
        StrobeStrips(presetColor);
      }
      break;
    case 9:
      if (presetAltModeEnabled) {
      } else {
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
      resetMovingBlocks();
      break;
    case 3:
      resetMovingBlocks();
      break;
    case 4:
      if (presetAltModeEnabled) {
        resetBars();
      } else {
        resetPulseFill();
      }
      break;
    case 5:
      if (presetAltModeEnabled) {
        resetXFill();
      } else {
        resetInvert();
      }
      break;
    case 6:
      resetRain();
      break;
    case 7:
      resetStripByStrip();
      break;
    case 8:
      if (presetAltModeEnabled) {
        resetChaos();
      } else {
        resetStrobe();
      }
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
