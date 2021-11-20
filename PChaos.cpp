#include "aurora.h"

// CHAOS
#define CHAOS_LENGTH 100
#define CHAOS_BLOCK_SIZE 10
static PositionColor chaos[NUMBER_OF_STRIPS];

void Chaos(CHSV color) {
  if (isTempoDivision(3)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      chaos[stripIndex] = { stripIndex, random8(PIXELS_PER_STRIP - CHAOS_BLOCK_SIZE) };
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    fill_solid(strip[stripIndex] + chaos[stripIndex].pixelIndex, CHAOS_BLOCK_SIZE, color);
  }
}
