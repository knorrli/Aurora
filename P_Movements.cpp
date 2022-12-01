#include "aurora.h"

/////////////////////////////////
// RAIN
/////////////////////////////////
#define RAIN_LENGTH 20
#define RAIN_STEPS_PER_GATE 4
static PositionOrientation rain[] = {
  { 0, 20, -1 },
  { 1, 28, -1 },
  { 2, 34, -1 },
  { 3, 28, -1 },
  { 4, 20, -1 }
};
uint8_t rainGateCounter = 0;
unsigned long lastRainGate = 0;

void Rain(CHSV color) {
  if (tempoGate) {
    rainGateCounter = 0;
  }
  if ((rainGateCounter < RAIN_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastRainGate + (currentTempo / RAIN_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    rainGateCounter += 1;
    lastRainGate = currentMillis;
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
  rain[1] = { 1, 28, -1 };
  rain[2] = { 2, 34, -1 };
  rain[3] = { 3, 28, -1 };
  rain[4] = { 4, 20, -1 };
  rainGateCounter = 0;
  lastRainGate = 0;
  return;
}

/////////////////////////////////
// RISE
/////////////////////////////////
#define RISE_STEPS_PER_GATE 4
#define RISE_LENGTH (PIXELS_PER_STRIP / 3)
#define RISE_SPACING RISE_LENGTH
static PositionColor rise[NUMBER_OF_STRIPS];
uint8_t riseGateCounter = 0;
unsigned long lastRiseGate = 0;

void Rise(CHSV color) {
  if (tempoGate) {
    riseGateCounter = 0;
  }
  
  if ((riseGateCounter < RISE_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastRiseGate + (currentTempo / RISE_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    riseGateCounter += 1;
    lastRiseGate = currentMillis;
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
  riseGateCounter = 0;
  lastRiseGate = currentMillis;
}

/////////////////////////////////
// INVERT
/////////////////////////////////
#define INVERT_STEPS_PER_GATE 11
#define BREAK_POSITION ((PIXELS_PER_STRIP / 4))
int8_t invertDirection = 1;
uint8_t invertPosition = 0;
uint8_t invertGateCounter = 0;
unsigned long lastInvertGate = 0;

void Invert(CHSV color) {
  if (tempoGate) {
    invertGateCounter = 0;
  }
  
  if ((invertGateCounter < INVERT_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastInvertGate + (currentTempo / INVERT_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    invertGateCounter += 1;
    lastInvertGate = currentMillis;
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
  invertGateCounter = 0;
  lastInvertGate = currentMillis;
}
