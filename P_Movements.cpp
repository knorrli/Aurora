#include "aurora.h"

/////////////////////////////////
// RAIN
/////////////////////////////////
#define RAIN_LENGTH 20
#define RAIN_SPEED 8
static PositionOrientation rain[] = {
  { 0, 20, -1 },
  { 1, 25, -1 },
  { 2, 30, -1 },
  { 3, 25, -1 },
  { 4, 20, -1 }
};
static unsigned long rainTempoGate = 0;

void Rain(CHSV color) {
  rainTempoGate += 1;
  if (divisionGate) {
    rainTempoGate = 0;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rain[stripIndex].pixelIndex += rain[stripIndex].orientation;

      if ((rain[stripIndex].pixelIndex == 0) || (rain[stripIndex].pixelIndex == (PIXELS_PER_STRIP - 1))) {
        rain[stripIndex].orientation *= -1;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionOrientation raindrop = rain[stripIndex];
    for (uint8_t index = 0; index < RAIN_LENGTH; index++) {
      int8_t pixelIndex = raindrop.pixelIndex - (((RAIN_LENGTH - 1) - index) * raindrop.orientation);
      if (pixelIndex < 0) {
        pixelIndex = 0 - pixelIndex;
      }
      if (pixelIndex > (PIXELS_PER_STRIP - 1)) {
        pixelIndex = (PIXELS_PER_STRIP - 1) - (pixelIndex - PIXELS_PER_STRIP);
      }
      strip[stripIndex][pixelIndex] = color;
      uint8_t raindropFadeAmount = 255 - (255 / (RAIN_LENGTH - (index)));
      strip[stripIndex][pixelIndex].fadeToBlackBy(raindropFadeAmount);
    }
  }
}

void resetRain() {
  rain[0] = { 0, 20, -1 };
  rain[1] = { 1, 25, -1 };
  rain[2] = { 2, 30, -1 };
  rain[3] = { 3, 25, -1 };
  rain[4] = { 4, 20, -1 };
  return;
}

/////////////////////////////////
// RISE
/////////////////////////////////
#define RISE_SPEED 4
#define RISE_LENGTH (PIXELS_PER_STRIP / 3)
#define RISE_SPACING RISE_LENGTH
static PositionColor rise[NUMBER_OF_STRIPS];

void Rise(CHSV color) {
  if (divisionGate) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rise[stripIndex].pixelIndex += 1;

      if (rise[stripIndex].pixelIndex == PIXELS_PER_STRIP) {
        rise[stripIndex].pixelIndex = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionColor stripRise = rise[stripIndex];
    uint8_t stripOffset = (stripIndex % 2 == 0) ? 0 : (RISE_LENGTH / 2);
    for (uint8_t index = 0; index < PIXELS_PER_STRIP; index += RISE_LENGTH) {
      for (uint8_t risePixel = 0; risePixel < (RISE_LENGTH / 2); risePixel++) {
        uint8_t pixelIndex = ((index + stripRise.pixelIndex + risePixel) + stripOffset) % (PIXELS_PER_STRIP - 1);
        strip[stripIndex][pixelIndex] = color;
      }
    }
  }
}

void resetRise() {
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    rise[stripIndex].pixelIndex = 0;
  }
}

/////////////////////////////////
// INVERT
/////////////////////////////////
#define INVERT_SPEED 6
#define BREAK_POSITION ((PIXELS_PER_STRIP / 4) - 1)
int8_t invertDirection = 1;
uint8_t invertPosition = 0;

void Invert(CHSV color) {
  if (divisionGate) {
    invertPosition += invertDirection;
    if ((invertPosition <= 0) || (invertPosition >= ((PIXELS_PER_STRIP - 1) / 2))) {
      invertDirection *= -1;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t startIndex = min(invertPosition, BREAK_POSITION);
    uint8_t endIndex = max(invertPosition, BREAK_POSITION);
    if (stripIndex % 2 == 0) {
      startIndex = min((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), BREAK_POSITION);
      endIndex = max((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), BREAK_POSITION);
    }
    for (uint8_t pixelIndex = startIndex; pixelIndex <= endIndex; pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
      strip[stripIndex][(PIXELS_PER_STRIP - 1) - pixelIndex] = color;
    }
  }
}

void resetInvert() {
  invertDirection = 1;
  invertPosition = 0;
}
