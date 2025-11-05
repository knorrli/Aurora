#include "aurora.h"

/////////////////////////////////
// FILL_STRIPS
/////////////////////////////////
void FillStrips(CHSV color) {
  strips.fill_solid(color);
}

/////////////////////////////////
// FILL_STARS
/////////////////////////////////
#define FILL_STARS_LENGTH 1
#define FILL_STARS_GAP 4
void FillStars(CHSV color) {
  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex += (FILL_STARS_LENGTH + FILL_STARS_GAP)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      fill_solid(strip[stripIndex] + pixelIndex, FILL_STARS_LENGTH, color);
    }
  }
}

/////////////////////////////////
// PULSE
/////////////////////////////////
#define PULSE_STEPS_PER_GATE 11
static uint8_t pulsePosition = 0;
static uint8_t pulseDirection = UP;
static uint8_t pulseGateCounter = 0;
static unsigned long lastPulseGate = 0;

void PulseFill(CHSV color) {
  if (tempoGate) {
    pulseGateCounter = 0;
  }
  if ((pulseGateCounter < PULSE_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastPulseGate + (currentTempo / PULSE_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    pulseGateCounter += 1;
    lastPulseGate = currentMillis;
    pulsePosition += pulseDirection;
    if ((pulsePosition > ((PIXELS_PER_STRIP / 2) - 1)) || (pulsePosition <= 0)) {
      pulseDirection *= -1;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = pulsePosition; pixelIndex <= ((PIXELS_PER_STRIP - 1) - pulsePosition); pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
    }
  }
}

void resetPulseFill() {
  pulseGateCounter = 0;
  lastPulseGate = currentMillis;
  pulsePosition = 0;
  pulseDirection = UP;
  return;
}

/////////////////////////////////
// X_FILL
/////////////////////////////////
#define X_FILL_STEPS_PER_GATE 11
#define X_FILL_STEPS ((PIXELS_PER_STRIP-1) / (NUMBER_OF_STRIPS-1))
struct xFillPositions {
  uint8_t startPixelIndex;
  uint8_t endPixelIndex;
};
static xFillPositions xFill[] = {
  { (0 * X_FILL_STEPS), (0 * X_FILL_STEPS) },
  { (1 * X_FILL_STEPS), (1 * X_FILL_STEPS) },
  { (2 * X_FILL_STEPS), (2 * X_FILL_STEPS) },
  { (3 * X_FILL_STEPS), (3 * X_FILL_STEPS) },
  { (4 * X_FILL_STEPS), (4 * X_FILL_STEPS) },
};

static uint8_t xFillOrientation = 0;
static uint8_t xFillStep = 0;
static uint8_t xFillGateCounter = 0;
unsigned long lastXFillGate = 0;

void XFill(CHSV color) {
  if (tempoGate) {
    xFillGateCounter = 0;
  }
  if ((xFillGateCounter < X_FILL_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastXFillGate + (currentTempo / X_FILL_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    xFillGateCounter += 1;
    lastXFillGate = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      uint8_t xFillStep = (NUMBER_OF_STRIPS - 1) - stripIndex;
      switch (xFillOrientation) {
        case 0:
          xFill[stripIndex].endPixelIndex += (xFillStep);
          xFill[stripIndex].startPixelIndex -= (NUMBER_OF_STRIPS - 1) - xFillStep;
          break;
        case 1:
          xFill[stripIndex].endPixelIndex -= (NUMBER_OF_STRIPS - 1) - xFillStep;
          xFill[stripIndex].startPixelIndex += xFillStep;
          break;
        case 2:
          xFill[stripIndex].endPixelIndex += (NUMBER_OF_STRIPS - 1) - xFillStep;
          xFill[stripIndex].startPixelIndex -= xFillStep;
          break;
        case 3:
          xFill[stripIndex].endPixelIndex -= xFillStep;
          xFill[stripIndex].startPixelIndex += (NUMBER_OF_STRIPS - 1) - xFillStep;
      }
    }

    bool stripEmpty = (xFill[0].endPixelIndex == xFill[0].startPixelIndex);
    bool stripFull = ((xFill[0].endPixelIndex - xFill[0].startPixelIndex) == (PIXELS_PER_STRIP - 1));

    if (stripEmpty || stripFull) {
      xFillOrientation += 1;
      if (xFillOrientation > 3) {
        xFillOrientation = 0;
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = xFill[stripIndex].startPixelIndex; pixelIndex <= xFill[stripIndex].endPixelIndex; pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
    }
  }
}

void resetXFill() {
  xFill[0] = { (0 * X_FILL_STEPS), (0 * X_FILL_STEPS) };
  xFill[1] = { (1 * X_FILL_STEPS), (1 * X_FILL_STEPS) };
  xFill[2] = { (2 * X_FILL_STEPS), (2 * X_FILL_STEPS) };
  xFill[3] = { (3 * X_FILL_STEPS), (3 * X_FILL_STEPS) };
  xFill[4] = { (4 * X_FILL_STEPS), (4 * X_FILL_STEPS) };

  xFillOrientation = 0;
  xFillStep = 0;
  xFillGateCounter = 0;
  lastXFillGate = currentMillis;
  return;
}
