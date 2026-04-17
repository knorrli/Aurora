#include "aurora.h"

static unsigned long lastStrobeTrigger = 0;

/////////////////////////////////
// STRIP BY STRIP
/////////////////////////////////
static uint8_t stripByStripOrderedOrder[] = { 2, 3, 4, 0, 1};
static uint8_t stripByStripIndex = 0;

void StripByStripOrdered(CHSV color) {
  StripByStrip(color, stripByStripOrderedOrder);
}

void StripByStrip(CHSV color, uint8_t order[]) {
  fill_solid(strip[order[stripByStripIndex]], PIXELS_PER_STRIP, color);
  if (tempoGate) {
    stripByStripIndex += 1;
    if (stripByStripIndex > (NUMBER_OF_STRIPS - 1))
    {
      stripByStripIndex = 0;
    }
  }
}

/////////////////////////////////
// STRIP BY STRIP MIRRORED
/////////////////////////////////
uint8_t strobeMirroredStripIndex = (NUMBER_OF_STRIPS / 2);
void StripByStripMirrored(CHSV color) {
  if (tempoGate) {
    lastStrobeTrigger = currentMillis;
    strobeMirroredStripIndex += 1;
    if (strobeMirroredStripIndex == NUMBER_OF_STRIPS) {
      strobeMirroredStripIndex = 1;
    }

  }
  fill_solid(strip[strobeMirroredStripIndex], PIXELS_PER_STRIP, color);
  fill_solid(strip[(NUMBER_OF_STRIPS - 1) - strobeMirroredStripIndex], PIXELS_PER_STRIP, color);
}

void resetStripByStrip() {
  stripByStripIndex = 0;
  strobeMirroredStripIndex = (NUMBER_OF_STRIPS / 2);
  return;
}

/////////////////////////////////
// STROBE STRIPS
/////////////////////////////////
#define STROBE_SPEED 1
#define STROBE_TEMPO_FACTOR 4 // how long per tempo step
#define MINIMUM_STROBE_LENGTH 20
#define MAXIMUM_STROBE_LENGTH 200
void StrobeStrips(CHSV color) {
  if (tempoGate) {
    lastStrobeTrigger = currentMillis;
  }
  if ((currentMillis - lastStrobeTrigger) < min(max((currentTempo / STROBE_TEMPO_FACTOR), MINIMUM_STROBE_LENGTH), MAXIMUM_STROBE_LENGTH)) {
    strips.fill_solid(color);
  }
}


/////////////////////////////////
// STROBE_UP_DOWN
/////////////////////////////////
uint8_t strobeUpDownDirection = DOWN;
void StrobeUpDown(CHSV color) {
  if (tempoGate) {
    lastStrobeTrigger = currentMillis;
    strobeUpDownDirection *= -1;
  }
  if (strobeUpDownDirection == UP)
  {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
    {
      fill_solid(strip[stripIndex] + (PIXELS_PER_STRIP / 2), (PIXELS_PER_STRIP / 2) + 1, color);
    }
  }
  else
  {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
    {
      fill_solid(strip[stripIndex], (PIXELS_PER_STRIP / 2) + 1, color);
    }
  }
}

void resetStrobe() {
strobeUpDownDirection = DOWN;
}

/////////////////////////////////
// STUTTER — half-wall strobe alternating top/bottom each beat
/////////////////////////////////
static uint8_t stutterHalf = 0;
static unsigned long lastStutterTrigger = 0;

void Stutter(CHSV color) {
  if (tempoGate) {
    lastStutterTrigger = currentMillis;
    stutterHalf = 1 - stutterHalf;
  }
  unsigned long duration = min(max((currentTempo / STROBE_TEMPO_FACTOR), MINIMUM_STROBE_LENGTH), MAXIMUM_STROBE_LENGTH);
  if ((currentMillis - lastStutterTrigger) < duration) {
    uint8_t halfLen = PIXELS_PER_STRIP / 2;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      if (stutterHalf == 0) {
        fill_solid(strip[stripIndex], halfLen, color);
      } else {
        fill_solid(strip[stripIndex] + halfLen, PIXELS_PER_STRIP - halfLen, color);
      }
    }
  }
}

void resetStutter() {
  stutterHalf = 0;
  lastStutterTrigger = currentMillis;
}

/////////////////////////////////
// CHAOS
/////////////////////////////////
#define CHAOS_STEPS_PER_GATE 2
#define CHAOS_LENGTH 100
#define CHAOS_BLOCK_SIZE 10
static uint8_t chaosGateCounter = 0;
static unsigned long lastChaosGate = 0;
static uint8_t chaosStripPosition[NUMBER_OF_STRIPS];
void Chaos(CHSV color) {
  if (tempoGate) {
    chaosGateCounter = 0;
  }
  if ((chaosGateCounter < CHAOS_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastChaosGate + (currentTempo / CHAOS_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    chaosGateCounter += 1;
    lastChaosGate = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      chaosStripPosition[stripIndex] = (chaosStripPosition[stripIndex] + (PIXELS_PER_STRIP / 3) + random8(PIXELS_PER_STRIP / 3)) % (PIXELS_PER_STRIP - CHAOS_BLOCK_SIZE);
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    fill_solid(strip[stripIndex] + chaosStripPosition[stripIndex], CHAOS_BLOCK_SIZE, color);
  }
}

void resetChaos() {
  chaosGateCounter = 0;
  lastChaosGate = currentMillis;
  return;
}

/////////////////////////////////
// GLITCH — random pixels flash white or preset color every frame
/////////////////////////////////
#define GLITCH_PIXELS_PER_FRAME 12
#define GLITCH_WHITE_CHANCE 77 // out of 255
void Glitch(CHSV color) {
  for (uint8_t i = 0; i < GLITCH_PIXELS_PER_FRAME; i++) {
    uint8_t stripIndex = random8(NUMBER_OF_STRIPS);
    uint8_t pixelIndex = random8(PIXELS_PER_STRIP);
    if (random8() < GLITCH_WHITE_CHANCE) {
      strip[stripIndex][pixelIndex] = CRGB::White;
    } else {
      strip[stripIndex][pixelIndex] = CHSV(color.hue, color.saturation, color.value);
    }
  }
}

// Retired: StripByStripRandom. See P_Retired.cpp.
