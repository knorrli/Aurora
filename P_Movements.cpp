#include "aurora.h"

/////////////////////////////////
// RAIN
/////////////////////////////////
#define RAIN_LENGTH 20
#define RAIN_SPEED 4
static PositionDirection rain[] = {
  { 0, 10, -1 },
  { 1, 20, -1 },
  { 2, 30, -1 },
  { 3, 20, -1 },
  { 4, 10, -1 }
};
static unsigned long rainTempoGate = 0;

void Rain(CHSV color) {
  rainTempoGate += 1;
  if (isTempoDivision(RAIN_SPEED)) {
    rainTempoGate = 0;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rain[stripIndex].pixelIndex += rain[stripIndex].orientation;

      if ((rain[stripIndex].pixelIndex == 0) || (rain[stripIndex].pixelIndex == (PIXELS_PER_STRIP - 1))) {
        rain[stripIndex].orientation *= -1;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionDirection raindrop = rain[stripIndex];
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

/////////////////////////////////
// RISE
/////////////////////////////////
#define RISE_LENGTH (PIXELS_PER_STRIP / 5)
#define RISE_SPACING 5
static PositionColor rise[NUMBER_OF_STRIPS];

void Rise(CHSV color) {
  if (isTempoDivision(4)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rise[stripIndex].pixelIndex += 1;

      if (rise[stripIndex].pixelIndex == PIXELS_PER_STRIP) {
        rise[stripIndex].pixelIndex = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionColor stripRise = rise[stripIndex];
    for (uint8_t i = 0; i < RISE_LENGTH; i++) {
      strip[stripIndex][(stripRise.pixelIndex + i) % PIXELS_PER_STRIP] = color;
    }
    for (uint8_t i = RISE_LENGTH; i < RISE_LENGTH * 2; i++) {
      strip[stripIndex][(stripRise.pixelIndex + i + RISE_SPACING) % PIXELS_PER_STRIP] = color;
    }
  }
}

/////////////////////////////////
// INVERT
/////////////////////////////////
int8_t invertDirection = 1;
uint8_t invertPosition = 0;
uint8_t breakPosition = (PIXELS_PER_STRIP / 4) - 1;

void Invert(CHSV color) {
  if (isTempoDivision(3)) {
    invertPosition += invertDirection;
    if ((invertPosition <= 0) || (invertPosition >= ((PIXELS_PER_STRIP - 1) / 2))) {
      invertDirection *= -1;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t startIndex = min(invertPosition, breakPosition);
    uint8_t endIndex = max(invertPosition, breakPosition);
    if (stripIndex % 2 == 0) {
      startIndex = min((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), breakPosition);
      endIndex = max((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), breakPosition);
    }
    for (uint8_t pixelIndex = startIndex; pixelIndex <= endIndex; pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
      strip[stripIndex][(PIXELS_PER_STRIP - 1) - pixelIndex] = color;
    }
  }
}
