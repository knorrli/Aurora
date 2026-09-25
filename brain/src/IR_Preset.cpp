#include "Aurora.h"

uint8_t selectedPreset = 0;

void renderPreset(uint8_t preset) {
  switch (preset) {
    case PRESET_GENERATOR:
      Generator();
      break;
    case PRESET_STRIP_ORDER:
      ShowStripOrder();
      break;
  }
}

void resetPreset(uint8_t preset) {
  if (preset == PRESET_GENERATOR) resetGenerator();
}
