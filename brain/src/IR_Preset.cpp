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
    // --- Row 1: ambient ---
    case 1:
      // Solid / Starfield
      if (presetAltModeEnabled) {
        Starfield(presetColor);
      } else {
        FillStrips(presetColor);
      }
      return;
    case 2:
      // Breathe / Wave
      if (presetAltModeEnabled) {
        Wave(presetColor);
      } else {
        Breathe(presetColor);
      }
      break;
    case 3:
      // Plasma / Aurora
      if (presetAltModeEnabled) {
        Aurora(presetColor);
      } else {
        Plasma(presetColor);
      }
      break;
    // --- Row 2: groove ---
    case 4:
      // Pulse / Bars
      if (presetAltModeEnabled) {
        Bars(presetColor);
      } else {
        PulseFill(presetColor);
      }
      break;
    case 5:
      // Sweep / CrossSweep
      if (presetAltModeEnabled) {
        CrossSweep(presetColor);
      } else {
        Sweep(presetColor);
      }
      break;
    case 6:
      // Rain / Storm
      if (presetAltModeEnabled) {
        Storm(presetColor);
      } else {
        RainFall(presetColor);
      }
      break;
    // --- Row 3: intensity ---
    case 7:
      // Chase / Comet
      if (presetAltModeEnabled) {
        Comet(presetColor);
      } else {
        StripByStripOrdered(presetColor);
      }
      break;
    case 8:
      // Strobe / Stutter
      if (presetAltModeEnabled) {
        Stutter(presetColor);
      } else {
        StrobeStrips(presetColor);
      }
      break;
    case 9:
      // Chaos / Glitch
      if (presetAltModeEnabled) {
        Glitch(presetColor);
      } else {
        Chaos(presetColor);
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
    // Row 1 (ambient) — all stateless, nothing to reset.
    case 1:
    case 2:
    case 3:
      return;
    // Row 2 (groove)
    case 4:
      if (presetAltModeEnabled) {
        resetBars();
      } else {
        resetPulseFill();
      }
      break;
    case 5:
      if (presetAltModeEnabled) {
        resetCrossSweep();
      } else {
        resetMovingBlocks();
      }
      break;
    case 6:
      if (presetAltModeEnabled) {
        resetStorm();
      } else {
        resetRain();
      }
      break;
    // Row 3 (intensity)
    case 7:
      if (presetAltModeEnabled) {
        resetComet();
      } else {
        resetStripByStrip();
      }
      break;
    case 8:
      if (presetAltModeEnabled) {
        resetStutter();
      } else {
        resetStrobe();
      }
      break;
    case 9:
      // Glitch is stateless; only Chaos needs reset.
      if (!presetAltModeEnabled) {
        resetChaos();
      }
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
