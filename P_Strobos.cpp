#include "aurora.h"

/////////////////////////////////
// ONE ON ONE
/////////////////////////////////
#define ONE_ON_ONE_SPEED 1
static uint8_t one_on_one_order[] = { 2, 0, 3, 1, 4 };
static uint8_t one_on_one_index = 0;
void OneOnOne(CHSV color) {
  fill_solid(strip[one_on_one_order[one_on_one_index]], PIXELS_PER_STRIP, color);
  if (tempoGate) {
    one_on_one_index += 1;
    if (one_on_one_index > (NUMBER_OF_STRIPS - 1)) {
      one_on_one_index = 0;
    }
  }
}

void resetOneOnOne() {
  one_on_one_index = 0;
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
