#include "aurora.h"

/////////////////////////////////
// MOVE_FILL — shared helper for block scrolling
/////////////////////////////////
#define MOVE_STEPS_PER_GATE 4
static int8_t movePosition = 0;
static uint8_t moveGateCounter = 0;
static unsigned long lastMoveGate = 0;

void MovingBlocks(CHSV color, uint8_t fillLength, uint8_t gap, int8_t direction = UP) {
  if (tempoGate) {
    moveGateCounter = 0;
  }

  if ((moveGateCounter < MOVE_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastMoveGate + (currentTempo / MOVE_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    moveGateCounter += 1;
    lastMoveGate = currentMillis;
    movePosition += direction;
    if (movePosition < 0)
    {
      movePosition = (PIXELS_PER_STRIP - 1);
    }
    if (movePosition == PIXELS_PER_STRIP)
    {
      movePosition = 0;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    for (uint8_t fillStartIndex = 0; fillStartIndex < PIXELS_PER_STRIP; fillStartIndex += (fillLength + gap))
    {
      for (uint8_t pixelIndex = 0; pixelIndex < fillLength; pixelIndex++)
      {
        uint8_t offsetPixelIndex = (fillStartIndex + pixelIndex + movePosition) % PIXELS_PER_STRIP;
        strip[stripIndex][offsetPixelIndex] = color;
      }
    }
  }
}

void resetMovingBlocks()
{
  movePosition = 0;
  moveGateCounter = 0;
  lastMoveGate = currentMillis;
}

/////////////////////////////////
// SWEEP — single fat block rising across all strips in sync
/////////////////////////////////
#define SWEEP_BLOCK_LENGTH 12
void Sweep(CHSV color) {
  MovingBlocks(color, SWEEP_BLOCK_LENGTH, PIXELS_PER_STRIP - SWEEP_BLOCK_LENGTH, UP);
}

/////////////////////////////////
// CROSS_SWEEP — even strips rise, odd strips fall (interlocking)
/////////////////////////////////
#define CROSS_SWEEP_STEPS_PER_GATE 4
#define CROSS_SWEEP_BLOCK_LENGTH 12
static int8_t crossSweepPosition = 0;
static uint8_t crossSweepGateCounter = 0;
static unsigned long lastCrossSweepGate = 0;

void CrossSweep(CHSV color) {
  if (tempoGate) {
    crossSweepGateCounter = 0;
  }
  if ((crossSweepGateCounter < CROSS_SWEEP_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastCrossSweepGate + (currentTempo / CROSS_SWEEP_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    crossSweepGateCounter += 1;
    lastCrossSweepGate = currentMillis;
    crossSweepPosition += 1;
    if (crossSweepPosition >= PIXELS_PER_STRIP) {
      crossSweepPosition = 0;
    }
  }
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    bool evenStrip = (stripIndex % 2 == 0);
    int8_t basePos = evenStrip ? crossSweepPosition : ((PIXELS_PER_STRIP - 1) - crossSweepPosition);
    for (uint8_t i = 0; i < CROSS_SWEEP_BLOCK_LENGTH; i++) {
      int8_t pixelIndex = evenStrip ? (basePos + i) : (basePos - i);
      if (pixelIndex < 0) pixelIndex += PIXELS_PER_STRIP;
      if (pixelIndex >= PIXELS_PER_STRIP) pixelIndex -= PIXELS_PER_STRIP;
      strip[stripIndex][pixelIndex] = color;
    }
  }
}

void resetCrossSweep() {
  crossSweepPosition = 0;
  crossSweepGateCounter = 0;
  lastCrossSweepGate = currentMillis;
}

/////////////////////////////////
// BARS — continuous-phase prototype
//
// Block travels bottom→top over BARS_BEATS_PER_BAR beats, then top→bottom
// over the next BARS_BEATS_PER_BAR beats (full cycle = 2 bars). Position
// is computed every frame from tempo-anchored phase — no step counter —
// so the block glides smoothly and auto-corrects when tempo drifts.
// Sub-pixel edges are rendered at fractional brightness to hide integer-
// pixel jumps.
/////////////////////////////////
#define BARS_BAR_LENGTH 9
#define BARS_BEATS_PER_BAR 4
#define BARS_CYCLE_BEATS (BARS_BEATS_PER_BAR * 2)
#define BARS_HALF_CYCLE_PHASE (BARS_BEATS_PER_BAR * 1000UL)
#define BARS_FULL_CYCLE_PHASE (BARS_CYCLE_BEATS * 1000UL)

static int8_t barsBeatIndex = -1;

void Bars(CHSV color) {
  if (tempoGate) {
    barsBeatIndex = (barsBeatIndex + 1) % BARS_CYCLE_BEATS;
  }
  if (barsBeatIndex < 0) {
    // no beat received yet — park at the bottom
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      fill_solid(strip[stripIndex], BARS_BAR_LENGTH, color);
    }
    return;
  }

  // phase within the current beat, 0..1000
  uint32_t timeSinceBeat = currentMillis - lastGateMillis;
  uint16_t beatPhase = (timeSinceBeat * 1000UL) / (currentTempo > 0 ? currentTempo : 1);
  if (beatPhase > 1000) beatPhase = 1000;

  // phase within the full cycle, 0..BARS_FULL_CYCLE_PHASE
  uint16_t cyclePhase = (uint16_t)barsBeatIndex * 1000 + beatPhase;

  // triangle wave: rising then falling
  uint16_t upPhase = (cyclePhase < BARS_HALF_CYCLE_PHASE)
                     ? cyclePhase
                     : (uint16_t)(BARS_FULL_CYCLE_PHASE - cyclePhase);

  // position in tenths of a pixel
  uint8_t travel = PIXELS_PER_STRIP - BARS_BAR_LENGTH;
  uint16_t position_x10 = (uint32_t)travel * 10 * upPhase / BARS_HALF_CYCLE_PHASE;

  uint8_t integerPos = position_x10 / 10;
  uint8_t fracPos = position_x10 % 10;

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    if (fracPos == 0) {
      fill_solid(strip[stripIndex] + integerPos, BARS_BAR_LENGTH, color);
    } else {
      // trailing edge, dims as block moves away
      uint8_t trailValue = (uint16_t)color.value * (10 - fracPos) / 10;
      strip[stripIndex][integerPos] = CHSV(color.hue, color.saturation, trailValue);
      // solid body
      fill_solid(strip[stripIndex] + integerPos + 1, BARS_BAR_LENGTH - 1, color);
      // leading edge, brightens as block moves into it
      if (integerPos + BARS_BAR_LENGTH < PIXELS_PER_STRIP) {
        uint8_t leadValue = (uint16_t)color.value * fracPos / 10;
        strip[stripIndex][integerPos + BARS_BAR_LENGTH] = CHSV(color.hue, color.saturation, leadValue);
      }
    }
  }
}

void resetBars() {
  barsBeatIndex = -1;
}

/////////////////////////////////
// BARS — previous discrete-step implementation (kept for reference)
/////////////////////////////////
#if 0
#define BARS_STEPS_PER_GATE 9
static int8_t barsPosition = 0;
static int8_t barsDirection = DOWN;
static uint8_t barsGateCounter = 0;
static unsigned long lastBarsGate = 0;

void Bars(CHSV color) {
  if (tempoGate) {
    barsGateCounter = 0;
  }

  if ((barsGateCounter < BARS_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastBarsGate + (currentTempo / BARS_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    barsGateCounter += 1;
    lastBarsGate = currentMillis;
    if (barsPosition == 0)
    {
      barsDirection *= -1;
    }
    if (barsPosition >= ((PIXELS_PER_STRIP) - BARS_BAR_LENGTH))
    {
      barsDirection *= -1;
    }
    barsPosition += barsDirection;
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    fill_solid(strip[stripIndex] + barsPosition, BARS_BAR_LENGTH, color);
  }
}

void resetBars()
{
  barsDirection = DOWN;
  barsPosition = 0;
  lastBarsGate = currentMillis;
}
#endif

/////////////////////////////////
// RAIN_FALL
/////////////////////////////////
void RainFall(CHSV color)
{
  Rain(color, false);
}

/////////////////////////////////
// RAIN — shared helper (fall + bounce)
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
static uint8_t rainGateCounter = 0;
static unsigned long lastRainGate = 0;

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
// STORM — rain plus occasional white-wall lightning flash
/////////////////////////////////
#define STORM_LIGHTNING_DURATION 60
#define STORM_LIGHTNING_CHANCE 32
static bool stormLightningActive = false;
static unsigned long stormLightningStart = 0;

void Storm(CHSV color) {
  Rain(color, false);

  if (tempoGate && (random8() < STORM_LIGHTNING_CHANCE)) {
    stormLightningActive = true;
    stormLightningStart = currentMillis;
  }

  if (stormLightningActive) {
    if ((currentMillis - stormLightningStart) < STORM_LIGHTNING_DURATION) {
      strips.fill_solid(CRGB::White);
    } else {
      stormLightningActive = false;
    }
  }
}

void resetStorm() {
  stormLightningActive = false;
  stormLightningStart = 0;
  resetRain();
}

/////////////////////////////////
// COMET — bright head + fading trail, hops across strips
/////////////////////////////////
#define COMET_LENGTH 15
#define COMET_STEPS_PER_GATE 6
static uint8_t cometOrder[] = { 0, 2, 4, 1, 3 };
static uint8_t cometOrderIndex = 0;
static int8_t cometPosition = 0;
static int8_t cometDirection = UP;
static uint8_t cometGateCounter = 0;
static unsigned long lastCometGate = 0;

void Comet(CHSV color) {
  if (tempoGate) {
    cometGateCounter = 0;
  }
  if ((cometGateCounter < COMET_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastCometGate + (currentTempo / COMET_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    cometGateCounter += 1;
    lastCometGate = currentMillis;
    cometPosition += cometDirection;
    if (cometPosition >= PIXELS_PER_STRIP || cometPosition < 0) {
      cometOrderIndex = (cometOrderIndex + 1) % NUMBER_OF_STRIPS;
      cometDirection = -cometDirection;
      cometPosition = (cometDirection == UP) ? 0 : (PIXELS_PER_STRIP - 1);
    }
  }

  uint8_t activeStrip = cometOrder[cometOrderIndex];
  for (uint8_t i = 0; i < COMET_LENGTH; i++) {
    int8_t trailPos = cometPosition - (i * cometDirection);
    if (trailPos < 0 || trailPos >= PIXELS_PER_STRIP) continue;
    uint8_t brightness = 255 - ((uint16_t)i * 255 / COMET_LENGTH);
    strip[activeStrip][trailPos] = CHSV(color.hue, color.saturation, scale8(color.value, brightness));
  }
}

void resetComet() {
  cometOrderIndex = 0;
  cometPosition = 0;
  cometDirection = UP;
  cometGateCounter = 0;
  lastCometGate = currentMillis;
}

// Retired: RisingBlocks, RisingStars, FallingBlocks, FallingStars, RainBounce, Invert. See P_Retired.cpp.
