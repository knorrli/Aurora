#include "aurora.h"

// STROBE
#define STROBE_LENGTH 50

// RAIN


// RISE
#define RISE_LENGTH (PIXELS_PER_STRIP / 5)
#define RISE_SPACING 5
static PositionColor colorRise[NUMBER_OF_STRIPS];

void Fill(CHSV color) {
  strips.fill_solid(color);
}

void Gradient(CHSV color) {
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    fill_gradient(strip[stripIndex], PIXELS_PER_STRIP, color, CHSV((color.hue + 64), color.saturation, color.value), SHORTEST_HUES);
  }
}

// custom fill_rainbow implementation to allow hue and saturation values affecting the rainbow
void Rainbow(CHSV color) {
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    CHSV pixelColor = CHSV(color.hue, color.saturation, color.value);
    for (uint8_t pixel = 0; pixel < PIXELS_PER_STRIP; pixel++) {
      strip[stripIndex][pixel] = pixelColor;
      pixelColor.hue += (255 / PIXELS_PER_STRIP); // rainbow spread
    }
  }
}

void Strobe(CHSV color) {
  if (isTempoDivision(2)) {
    strips.fill_solid(color);
  }
}

void ColorRise(CHSV color) {
  if (tempoGate) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      colorRise[stripIndex].pixelIndex += 1;

      if (colorRise[stripIndex].pixelIndex == PIXELS_PER_STRIP) {
        colorRise[stripIndex].pixelIndex = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionColor rise = colorRise[stripIndex];
    for (uint8_t i = 0; i < RISE_LENGTH; i++) {
      strip[stripIndex][(rise.pixelIndex + i) % PIXELS_PER_STRIP] = color;
    }
    for (uint8_t i = RISE_LENGTH; i < RISE_LENGTH * 2; i++) {
      strip[stripIndex][(rise.pixelIndex + i + RISE_SPACING) % PIXELS_PER_STRIP] = CHSV(color.hue - 32, color.saturation, color.value);
    }
  }
}
