#include "fl/fill.h"
#include "aurora.h"


#define ALBTRAUM_BREATH_MIN_HUE 128
#define ALBTRAUM_BREATH_MAX_HUE 180
static uint8_t albtraumBreathHue = 160;
void AlbtraumBreath() {
  Breath(albtraumBreathHue, ALBTRAUM_BREATH_MIN_HUE, ALBTRAUM_BREATH_MAX_HUE);
}

#define ERNST_BREATH_MIN_HUE 64
#define ERNST_BREATH_MAX_HUE 116
static uint8_t ernstBreathHue = 96;
void ErnstBreath() {
  Breath(ernstBreathHue, ERNST_BREATH_MIN_HUE, ERNST_BREATH_MAX_HUE);
}

/////////////////////////////////
// BREATH
/////////////////////////////////
#define BREATH_MIN_VALUE 50
#define BREATH_MAX_VALUE 255
static unsigned long lastBreathGate = 0;
static uint8_t breathStripDelta[] = { 50, 200, 100, 0, 150 };
void Breath(uint8_t &hue, uint8_t min_hue, uint8_t max_hue) {
  if ((currentMillis - lastBreathGate) > 50) {
    hue++;
    lastBreathGate = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      breathStripDelta[stripIndex]++;
    }
  }
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t breathValue = constrain(triwave8(breathStripDelta[stripIndex]), BREATH_MIN_VALUE, BREATH_MAX_VALUE);
    CHSV breathColor = CHSV(map(triwave8(hue), 0, 255, min_hue, max_hue), 255, breathValue);
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, breathColor);
  }
}

/////////////////////////////////
// ALBTRAUM_CHAOSSAFT
/////////////////////////////////
static uint8_t chaossaftValueDelta = 0;
void AlbtraumChaossaft() {
  chaossaftValueDelta += 5;

  CHSV chaossaftColor = CHSV(0, 255, cubicwave8(chaossaftValueDelta));
  strips.fill_solid(chaossaftColor);
}

/////////////////////////////////
// ALBTRAUM_PARTY
/////////////////////////////////
#define MINIMUM_PARTY_LENGTH 20
#define MAXIMUM_PARTY_LENGTH 200
static uint8_t albtraumPartyStripHues[] = { 0, 50, 100, 150, 200 };
void AlbtraumParty() {
  if (tempoGate) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      albtraumPartyStripHues[stripIndex] += 90;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    CHSV partyColor = CHSV(albtraumPartyStripHues[stripIndex], 255, 255);
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, partyColor);
  }
}

/////////////////////////////////
// ERNST_PARTY
/////////////////////////////////
#define MINIMUM_PARTY_LENGTH 20
#define MAXIMUM_PARTY_LENGTH 200
static unsigned long lastPartyTrigger = 0;
static uint8_t ernstPartyStripHues[] = { 0, 50, 100, 150, 200 };
void ErnstParty() {
  if (tempoGate) {
    lastPartyTrigger = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      ernstPartyStripHues[stripIndex] += 90;
    }
  }

  if ((currentMillis - lastPartyTrigger) < constrain(currentTempo, MINIMUM_PARTY_LENGTH, MAXIMUM_PARTY_LENGTH)) {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      CHSV partyColor = CHSV(ernstPartyStripHues[stripIndex], 255, 255);
      fill_solid(strip[stripIndex], PIXELS_PER_STRIP, partyColor);
    }
  }
}

/////////////////////////////////
// TRANSITION_RAINBOW
/////////////////////////////////
static uint8_t rainbowDelta = 0;
static unsigned long lastRainbowGate = 0;
void TransitionRainbow() {
  if ((currentMillis - lastRainbowGate) > 100) {
    lastRainbowGate = currentMillis;
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      rainbowDelta++;
    }
  }

  fill_rainbow(strip[0], NUM_PIXELS_STRIP, rainbowDelta, 255 / NUM_PIXELS_STRIP);
}

/////////////////////////////////
// TRANSITION_STROBO
/////////////////////////////////
#define STROBO_BLOCK_SIZE 10
static uint8_t randomStrip = 0;
static unsigned long lastStroboGate = 0;
static uint8_t stroboStripPosition[NUMBER_OF_STRIPS];
void TransitionStrobo() {
  if ((currentMillis - lastStroboGate) > 50) {
    lastStroboGate = currentMillis;
    randomStrip = random8(NUMBER_OF_STRIPS);

    // for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    stroboStripPosition[randomStrip] = (stroboStripPosition[randomStrip] + (PIXELS_PER_STRIP / 3) + random8(PIXELS_PER_STRIP / 3)) % (PIXELS_PER_STRIP - STROBO_BLOCK_SIZE);
    // }
  }

  // for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
  fill_solid(strip[randomStrip] + stroboStripPosition[randomStrip], STROBO_BLOCK_SIZE, CHSV(100, 0, 255));
  // }
}

/////////////////////////////////
// ERNST_DARK
/////////////////////////////////
static uint8_t darkStripIndex = 0;
void ErnstDark() {
  if (tempoGate) {
    darkStripIndex++;
    if (darkStripIndex >= NUMBER_OF_STRIPS) {
      darkStripIndex = 0;
    }
  }

  fill_solid(strip[darkStripIndex], PIXELS_PER_STRIP, CHSV(0, 255, 255));
}
