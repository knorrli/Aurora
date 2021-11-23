#include "aurora.h"

/////////////////////////////////
// FILL
/////////////////////////////////
void Fill(CHSV color) {
  strips.fill_solid(color);
}


/////////////////////////////////
// PULSE
/////////////////////////////////

uint8_t pulsePosition = 0;
uint8_t pulseDirection = 1;

void Pulse(CHSV color) {
  if (isTempoDivision(4)) {
    pulsePosition += pulseDirection;
    if ((pulsePosition > ((PIXELS_PER_STRIP / 2) - 1)) || (pulsePosition <= 0)) {
      pulseDirection *= -1;
    }
  }
  
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = pulsePosition; pixelIndex <= ((PIXELS_PER_STRIP-1)-pulsePosition); pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
    }
  }
}

/////////////////////////////////
// XVISION
/////////////////////////////////
#define XVISION_STEPS ((PIXELS_PER_STRIP-1) / (NUMBER_OF_STRIPS-1))
struct xVisionPositions {
  uint8_t startPixelIndex;
  uint8_t endPixelIndex;
};
static xVisionPositions xVision[] = {
  { (0 * XVISION_STEPS), (0 * XVISION_STEPS) },
  { (1 * XVISION_STEPS), (1 * XVISION_STEPS) },
  { (2 * XVISION_STEPS), (2 * XVISION_STEPS) },
  { (3 * XVISION_STEPS), (3 * XVISION_STEPS) },
  { (4 * XVISION_STEPS), (4 * XVISION_STEPS) },
};

static uint8_t xVisionDirection = 0;
static uint8_t xVisionStep = 0;

void XVision(CHSV color) {
  if (isTempoDivision(4)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      uint8_t xVisionStep = (NUMBER_OF_STRIPS - 1) - stripIndex;
      switch (xVisionDirection) {
        case 0:
          xVision[stripIndex].endPixelIndex += (xVisionStep);
          xVision[stripIndex].startPixelIndex -= (NUMBER_OF_STRIPS - 1) - xVisionStep;
          break;
        case 1:
          xVision[stripIndex].endPixelIndex -= (NUMBER_OF_STRIPS - 1) - xVisionStep;
          xVision[stripIndex].startPixelIndex += xVisionStep;
          break;
        case 2:
          xVision[stripIndex].endPixelIndex += (NUMBER_OF_STRIPS - 1) - xVisionStep;
          xVision[stripIndex].startPixelIndex -= xVisionStep;
          break;
        case 3:
          xVision[stripIndex].endPixelIndex -= xVisionStep;
          xVision[stripIndex].startPixelIndex += (NUMBER_OF_STRIPS - 1) - xVisionStep;
      }
    }

    bool stripEmpty = (xVision[0].endPixelIndex == xVision[0].startPixelIndex);
    bool stripFull = ((xVision[0].endPixelIndex - xVision[0].startPixelIndex) == (PIXELS_PER_STRIP - 1));

    if (stripEmpty || stripFull) {
      xVisionDirection += 1;
      if (xVisionDirection > 3) {
        xVisionDirection = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = xVision[stripIndex].startPixelIndex; pixelIndex <= xVision[stripIndex].endPixelIndex; pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
    }
  }
}
