#include "aurora.h"

/////////////////////////////////
// RISE_LINES
/////////////////////////////////
#define RISE_LINES_LINE_LENGTH (PIXELS_PER_STRIP / 6)
#define RISE_LINES_GAP (RISE_LINES_LINE_LENGTH + 1)
#define RISE_LINES_DIRECTION UP
void RiseLines(CHSV color) {
  Rise(color, RISE_LINES_LINE_LENGTH, RISE_LINES_GAP, RISE_LINES_DIRECTION);
}

/////////////////////////////////
// RISE_STARS
/////////////////////////////////
#define RISE_STARS_LINE_LENGTH 1
#define RISE_STARS_GAP 4
#define RISE_STARS_DIRECTION DOWN
void RiseStars(CHSV color) {
  Rise(color, RISE_STARS_LINE_LENGTH, RISE_STARS_GAP, RISE_STARS_DIRECTION);
}


/////////////////////////////////
// RISE
/////////////////////////////////
#define RISE_STEPS_PER_GATE 4
static int8_t risePosition;
uint8_t riseGateCounter = 0;
unsigned long lastRiseGate = 0;

void Rise(CHSV color, uint8_t fillLength, uint8_t gap, int8_t direction = UP) {
  if (tempoGate) {
    riseGateCounter = 0;
  }
  
  if ((riseGateCounter < RISE_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastRiseGate + (currentTempo / RISE_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    riseGateCounter += 1;
    lastRiseGate = currentMillis;
    risePosition += direction;
    if (risePosition < 0)
    {
      risePosition = (PIXELS_PER_STRIP - 1);
    }
    if (risePosition == PIXELS_PER_STRIP)
    {
      risePosition = 0;
    }
    
    
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    for (uint8_t fillStartIndex = 0; fillStartIndex < PIXELS_PER_STRIP; fillStartIndex += (fillLength + gap))
    {
      for (uint8_t pixelIndex = 0; pixelIndex < fillLength; pixelIndex++)
      {
        uint8_t offsetPixelIndex = (fillStartIndex + pixelIndex + risePosition) % PIXELS_PER_STRIP;
        strip[stripIndex][offsetPixelIndex] = color;
      }
    }
  }
}

void resetRise()
{
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    risePosition = 0;
  }
  riseGateCounter = 0;
  lastRiseGate = currentMillis;
}

/////////////////////////////////
// RAIN_FALL
/////////////////////////////////
void RainFall(CHSV color)
{
  Rain(color, false);
}

/////////////////////////////////
// RAIN_BOUNCE
/////////////////////////////////
void RainBounce(CHSV color)
{
  Rain(color, true);
}

/////////////////////////////////
// RAIN
/////////////////////////////////
#define RAIN_LENGTH 20
#define RAIN_STEPS_PER_GATE 4
static PositionDirection rain[] = {
  { 0, 20, DOWN },
  { 1, 28, DOWN },
  { 2, 34, DOWN },
  { 3, 28, DOWN },
  { 4, 20, DOWN }
};
uint8_t rainGateCounter = 0;
unsigned long lastRainGate = 0;

void Rain(CHSV color, bool changeDirectionOnEnds) {
  if (tempoGate) {
    rainGateCounter = 0;
  }
  if ((rainGateCounter < RAIN_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastRainGate + (currentTempo / RAIN_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    rainGateCounter += 1;
    lastRainGate = currentMillis;

    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rain[stripIndex].pixelIndex += rain[stripIndex].direction;

      if ((rain[stripIndex].pixelIndex == 0) || (rain[stripIndex].pixelIndex == (PIXELS_PER_STRIP - 1))) {
        if (changeDirectionOnEnds) {
          rain[stripIndex].direction *= -1;
        } else {
          if ((rain[stripIndex].direction == DOWN) && (rain[stripIndex].pixelIndex == 0)) {
            rain[stripIndex].pixelIndex = (PIXELS_PER_STRIP - 1);
          }
          if ((rain[stripIndex].direction == UP) && (rain[stripIndex].pixelIndex == (PIXELS_PER_STRIP - 1))) {
            rain[stripIndex].pixelIndex = 0;
          }
        }
      }
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    PositionDirection raindrop = rain[stripIndex];
    for (uint8_t index = 0; index < RAIN_LENGTH; index++) {
      int8_t pixelIndex = raindrop.pixelIndex - (((RAIN_LENGTH - 1) - index) * raindrop.direction);
      if (changeDirectionOnEnds) {
        if (pixelIndex < 0) {
          pixelIndex = 0 - pixelIndex;
        }
        if (pixelIndex > (PIXELS_PER_STRIP - 1)) {
          pixelIndex = (PIXELS_PER_STRIP - 1) - (pixelIndex - PIXELS_PER_STRIP);
        }
      } else {
        if (pixelIndex < 0) {
          pixelIndex = (PIXELS_PER_STRIP - 1) + pixelIndex;
        }
        if (pixelIndex > (PIXELS_PER_STRIP - 1)) {
          pixelIndex = pixelIndex - PIXELS_PER_STRIP;
        }
      }

      strip[stripIndex][pixelIndex] = color;
      uint8_t raindropFadeAmount = 255 - (255 / (RAIN_LENGTH - (index)));
      strip[stripIndex][pixelIndex].fadeToBlackBy(raindropFadeAmount);
    }
  }
}

void resetRain() {
  rain[0] = { 0, 20, DOWN };
  rain[1] = { 1, 28, DOWN };
  rain[2] = { 2, 34, DOWN };
  rain[3] = { 3, 28, DOWN };
  rain[4] = { 4, 20, DOWN };
  rainGateCounter = 0;
  lastRainGate = 0;
  return;
}

/////////////////////////////////
// INVERT
/////////////////////////////////
#define INVERT_STEPS_PER_GATE 11
#define BREAK_POSITION ((PIXELS_PER_STRIP / 4))
int8_t invertDirection = UP;
uint8_t invertPosition = 0;
uint8_t invertGateCounter = 0;
unsigned long lastInvertGate = 0;

void Invert(CHSV color)
{
  if (tempoGate)
  {
    invertGateCounter = 0;
  }

  if ((invertGateCounter < INVERT_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastInvertGate + (currentTempo / INVERT_STEPS_PER_GATE) - (elapsedLoopTime / 2))))))
  {
    invertGateCounter += 1;
    lastInvertGate = currentMillis;
    invertPosition += invertDirection;
    if ((invertPosition <= 0) || (invertPosition >= ((PIXELS_PER_STRIP - 1) / 2)))
    {
      invertDirection *= -1;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    uint8_t startIndex = min(invertPosition, BREAK_POSITION);
    uint8_t endIndex = max(invertPosition, BREAK_POSITION);
    if (stripIndex % 2 == 0)
    {
      startIndex = min((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), BREAK_POSITION);
      endIndex = max((((PIXELS_PER_STRIP - 1) / 2) - invertPosition), BREAK_POSITION);
    }
    for (uint8_t pixelIndex = startIndex; pixelIndex <= endIndex; pixelIndex++)
    {
      strip[stripIndex][pixelIndex] = color;
      strip[stripIndex][(PIXELS_PER_STRIP - 1) - pixelIndex] = color;
    }
  }
}

void resetInvert()
{
  invertDirection = UP;
  invertPosition = 0;
  invertGateCounter = 0;
  lastInvertGate = currentMillis;
}
