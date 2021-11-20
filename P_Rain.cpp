#include "aurora.h"

#define RAIN_LENGTH 15
#define RAINDROPS_PER_STRIP 1
static PositionDirection rain[] = {
  { 0, 10, 1 },
  { 1, 15, 1 },
  { 2, 20, 1 },
  { 3, 25, 1 },
  { 4, 30, 1 }
};

void Rain(CHSV color) {
  if (isTempoDivision(2)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rain[stripIndex].pixelIndex += 1;

      if ((rain[stripIndex].pixelIndex == (PIXELS_PER_STRIP - 1))) {
        rain[stripIndex].orientation *= -1;
        rain[stripIndex].pixelIndex = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionDirection raindrop = rain[stripIndex];

    uint8_t startIndex = raindrop.pixelIndex;
    int8_t endIndex = startIndex - (RAIN_LENGTH * raindrop.orientation);

    for (int8_t rainIndex = min(startIndex, endIndex); rainIndex < max(startIndex, endIndex); rainIndex++) {
      uint8_t pixelIndex;
      if (rainIndex < 0) {
        pixelIndex = abs(rainIndex);
      }
      if (rainIndex > (PIXELS_PER_STRIP - 1)) {
        pixelIndex = (PIXELS_PER_STRIP - 1) - (rainIndex - (PIXELS_PER_STRIP - 1));
      }
      CHSV raindropColor = CHSV(color.hue, color.saturation, color.value);
      if (raindrop.orientation > 0) {
        raindropColor.value -= ((RAIN_LENGTH - rainIndex) * 5);
      } else {
        raindropColor.value -= (rainIndex * 5);
      }
      strip[stripIndex][pixelIndex] = raindropColor;
    }
  }
}
