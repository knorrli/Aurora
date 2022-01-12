#include "aurora.h"

/////////////////////////////////
// STARS
/////////////////////////////////
#define STARS_SPEED 8
#define NUMBER_OF_STARS 30
static PositionColor stars[NUMBER_OF_STARS];
static uint8_t changingStarIndex = 0;
void Stars(CHSV color) {
  if (isTempoDivision(STARS_SPEED)) {
    if (changingStarIndex == NUMBER_OF_STARS) {
      changingStarIndex = 0;
    }
    stars[changingStarIndex] = { random8(NUMBER_OF_STRIPS), random8(PIXELS_PER_STRIP), color };
    changingStarIndex += 1;

    for (uint8_t starIndex = 0; starIndex < NUMBER_OF_STARS; starIndex++) {
      stars[starIndex].color.fadeToBlackBy(min((NUMBER_OF_STARS / ticks), 1));
    }

  }

  for (uint8_t starIndex = 0; starIndex < NUMBER_OF_STARS; starIndex++) {
    uint8_t tickFadeAmount = ((NUMBER_OF_STARS * STARS_SPEED) / ticks);
    stars[starIndex].color.fadeToBlackBy(max(1, tickFadeAmount));
    PositionColor star = stars[starIndex];
    strip[star.stripIndex][star.pixelIndex] = star.color;
  }
}

void resetStars() {
  return;
}

/////////////////////////////////
// CHAOS
/////////////////////////////////
#define CHAOS_SPEED 3
#define CHAOS_LENGTH 100
#define CHAOS_BLOCK_SIZE 10
static PositionColor chaos[NUMBER_OF_STRIPS];
void Chaos(CHSV color) {
  if (tempoGate) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      chaos[stripIndex] = { stripIndex, random8(PIXELS_PER_STRIP - CHAOS_BLOCK_SIZE) };
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    fill_solid(strip[stripIndex] + chaos[stripIndex].pixelIndex, CHAOS_BLOCK_SIZE, color);
  }
}

void resetChaos() {
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
  if (isTempoDivision(STROBE_SPEED)) {
    lastStrobeTrigger = currentMillis;
  }
  if ((currentMillis - lastStrobeTrigger) < min(max((currentTempo / STROBE_TEMPO_FACTOR), MINIMUM_STROBE_LENGTH), MAXIMUM_STROBE_LENGTH)) {
    strips.fill_solid(color);
  }
}

void resetStrobe() {
  return;
}
