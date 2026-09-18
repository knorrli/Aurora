#include "Aurora.h"

uint8_t selectedPreset = 0;
uint8_t previousPreset = 0;

void renderPreset(uint8_t preset) {
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
    case PRESET_GENERATOR:
      Generator(presetColor);
      break;
    case PRESET_STRIP_ORDER:
      ShowStripOrder();
      break;
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
    // Both derive everything they show from scratch each frame.
    case PRESET_GENERATOR:
    case PRESET_STRIP_ORDER:
      return;
  }
}
