#include "aurora.h"

/////////////////////////////////
// ONE ON ONE RANDOM
/////////////////////////////////
#define ONE_ON_ONE_SPEED 1
static uint8_t oneOnOneOrderedOrder[] = { 2, 3, 4, 0, 1};
static uint8_t oneOnOneRandomOrder[] = { 2, 0, 3, 1, 4 };
static uint8_t oneOnOneIndex = 0;

void OneOnOneOrdered(CHSV color) {
  OneOnOne(color, oneOnOneOrderedOrder);
}

void OneOnOneRandom(CHSV color) {
  OneOnOne(color, oneOnOneRandomOrder);
}

void OneOnOne(CHSV color, uint8_t order[]) {
  fill_solid(strip[order[oneOnOneIndex]], PIXELS_PER_STRIP, color);
  if (tempoGate) {
    oneOnOneIndex += 1;
    if (oneOnOneIndex > (NUMBER_OF_STRIPS - 1))
    {
      oneOnOneIndex = 0;
    }
  }
}

void resetOneOnOne() {
  oneOnOneIndex = 0;
  return;
}

/////////////////////////////////
// STROBE
/////////////////////////////////
#define STROBE_SPEED 1
#define STROBE_TEMPO_FACTOR 4 // how long per tempo step
#define MINIMUM_STROBE_LENGTH 20
#define MAXIMUM_STROBE_LENGTH 200
static unsigned long lastStrobeTrigger = 0;
void Strobe(CHSV color) {
  if (tempoGate) {
    lastStrobeTrigger = currentMillis;
  }
  if ((currentMillis - lastStrobeTrigger) < min(max((currentTempo / STROBE_TEMPO_FACTOR), MINIMUM_STROBE_LENGTH), MAXIMUM_STROBE_LENGTH)) {
    strips.fill_solid(color);
  }
}

void resetStrobe() {
  return;
}

/////////////////////////////////
// CHAOS
/////////////////////////////////
#define CHAOS_STEPS_PER_GATE 2
#define CHAOS_LENGTH 100
#define CHAOS_BLOCK_SIZE 10
uint8_t chaosGateCounter = 0;
unsigned long lastChaosGate = 0;
static PositionColor chaos[NUMBER_OF_STRIPS];
void Chaos(CHSV color) {
  if (tempoGate) {
    chaosGateCounter = 0;
  }
  if ((chaosGateCounter < CHAOS_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastChaosGate + (currentTempo / CHAOS_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    chaosGateCounter += 1;
    lastChaosGate = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      chaos[stripIndex] = { stripIndex, random8(PIXELS_PER_STRIP - CHAOS_BLOCK_SIZE) };
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    fill_solid(strip[stripIndex] + chaos[stripIndex].pixelIndex, CHAOS_BLOCK_SIZE, color);
  }
}

void resetChaos() {
  chaosGateCounter = 0;
  lastChaosGate = currentMillis;
  return;
}
