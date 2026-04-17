#include "aurora.h"

/////////////////////////////////
// ===== RETIRED PRESETS =====
//
// These presets are kept as reference only. The whole file is wrapped
// in #if 0 so the preprocessor strips it before compilation — zero
// flash, zero SRAM. Flip to #if 1 (and restore the matching extern
// declarations in aurora.h + the dispatch in IR_Preset.cpp) to revive
// any of them.
/////////////////////////////////

#if 0

/////////////////////////////////
// FILL_STARS — superseded by Starfield (P_Fills.cpp)
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
// X_FILL — concept contributed by band member; superseded by CrossSweep (P_Movements.cpp)
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

/////////////////////////////////
// RISE_LINES — superseded by Sweep (P_Movements.cpp)
/////////////////////////////////
#define RISING_LINES_LINE_LENGTH (PIXELS_PER_STRIP / 6)
#define RISING_LINES_GAP (RISING_LINES_LINE_LENGTH + 1)
#define RISING_LINES_DIRECTION UP
void RisingBlocks(CHSV color) {
  MovingBlocks(color, RISING_LINES_LINE_LENGTH, RISING_LINES_GAP, RISING_LINES_DIRECTION);
}

/////////////////////////////////
// RISE_STARS — superseded by Wave (P_Fills.cpp)
/////////////////////////////////
#define RISING_STARS_LINE_LENGTH 1
#define RISING_STARS_GAP 4
#define RISING_STARS_DIRECTION UP
void RisingStars(CHSV color) {
  MovingBlocks(color, RISING_STARS_LINE_LENGTH, RISING_STARS_GAP, RISING_STARS_DIRECTION);
}

/////////////////////////////////
// FALL_LINES — superseded by Plasma (P_Fills.cpp)
/////////////////////////////////
#define FALLING_LINES_LINE_LENGTH (PIXELS_PER_STRIP / 6)
#define FALLING_LINES_GAP (FALLING_LINES_LINE_LENGTH + 1)
#define FALLING_LINES_DIRECTION DOWN
void FallingBlocks(CHSV color) {
  MovingBlocks(color, FALLING_LINES_LINE_LENGTH, FALLING_LINES_GAP, FALLING_LINES_DIRECTION);
}

/////////////////////////////////
// FALL_STARS — superseded by Aurora (P_Fills.cpp)
/////////////////////////////////
#define FALLING_STARS_LINE_LENGTH 1
#define FALLING_STARS_GAP 4
#define FALLING_STARS_DIRECTION DOWN
void FallingStars(CHSV color) {
  MovingBlocks(color, FALLING_STARS_LINE_LENGTH, FALLING_STARS_GAP, FALLING_STARS_DIRECTION);
}

/////////////////////////////////
// RAIN_BOUNCE — superseded by Storm (P_Movements.cpp)
/////////////////////////////////
void RainBounce(CHSV color)
{
  Rain(color, true);
}

/////////////////////////////////
// INVERT — superseded by Sweep/CrossSweep (P_Movements.cpp)
/////////////////////////////////
#define INVERT_STEPS_PER_GATE 11
#define BREAK_POSITION ((PIXELS_PER_STRIP / 4))
static int8_t invertDirection = UP;
static uint8_t invertPosition = 0;
static uint8_t invertGateCounter = 0;
static unsigned long lastInvertGate = 0;

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

/////////////////////////////////
// STRIP_BY_STRIP_RANDOM — superseded by Comet (P_Movements.cpp)
/////////////////////////////////
static uint8_t stripByStripRandomOrder[] = { 2, 0, 3, 1, 4 };

void StripByStripRandom(CHSV color) {
  StripByStrip(color, stripByStripRandomOrder);
}

#endif
