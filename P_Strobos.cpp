#include "aurora.h"

/////////////////////////////////
// STROBE
/////////////////////////////////
#define STROBE_LENGTH 20
static unsigned long lastStrobeTrigger = 0;
void Strobe(CHSV color) {
  if (isTempoDivision(1)) {
    lastStrobeTrigger = currentMillis;
  }
  if ((currentMillis - lastStrobeTrigger < STROBE_LENGTH)) {
    strips.fill_solid(color);
  }
}

/////////////////////////////////
// STARS
/////////////////////////////////
#define NUMBER_OF_STARS 30
#define STARS_SPEED 4
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

/////////////////////////////////
// CHAOS
/////////////////////////////////
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
