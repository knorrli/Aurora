#include "aurora.h"

// INVERT
int8_t invertDirection = 1;
uint8_t invertPosition = 0;
uint8_t breakPosition = (PIXELS_PER_STRIP / 3) - 1;

void Invert(CHSV color) {
  if (isTempoDivision(3)) {
    invertPosition += invertDirection;
    if ((invertPosition <= 0) || (invertPosition >= ((PIXELS_PER_STRIP - 1) / 2))) {
      invertDirection *= -1;
    }
  }

  uint8_t startIndex = min(invertPosition, breakPosition);
  uint8_t endIndex = max(invertPosition, breakPosition);
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = startIndex; pixelIndex <= endIndex; pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
      strip[stripIndex][(PIXELS_PER_STRIP - 1) - pixelIndex] = color;
    }
  }
}
