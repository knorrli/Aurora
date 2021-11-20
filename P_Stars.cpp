#include "aurora.h"

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
  }

  for (uint8_t starIndex = 0; starIndex < NUMBER_OF_STARS; starIndex++) {
    if (isTempoDivision(15)) {
      stars[starIndex].color.fadeToBlackBy(1);
    }
    PositionColor star = stars[starIndex];
    strip[star.stripIndex][star.pixelIndex] = star.color;
  }
}
