#include "aurora.h"

#define FADER_MIN_VAL 10
#define FADER_MAX_VAL 1020
#define MIN_HUE 0
#define MAX_HUE 250
#define MIN_SATURATION 0
#define MAX_SATURATION 255
#define MIN_VALUE 0
#define MAX_VALUE 255

static uint8_t altColorModFactor = 0;
static uint8_t altColorModDirection = 1;

void setCurrentColor() {
  uint8_t currentHue = readHue();
  uint8_t currentSaturation = readSaturation();
  uint8_t currentValue = readValue();
  if (isFaderAlternativeMode()) {
    altColorModFactor = altColorModFactor + (altColorModFactor * )
    uint8_t modifiedHue = currentHue + altColorModFactor; // hue + (FADER_LEFT (speed))
    presetColor = CHSV(currentHue, MAX_SATURATION, MAX_VALUE);
  } else {
    altColorModFactor = 0;
    presetColor = CHSV(currentHue, currentSaturation, currentValue);
  }
  touchColor = CHSV(presetColor.hue + (MAX_HUE/2), presetColor.saturation, presetColor.value);
}

uint8_t readHue() {
  uint8_t hueValue = constrain(map((1023 - analogRead(PIN_FADER_HUE)), FADER_MIN_VAL, FADER_MAX_VAL, MIN_HUE, MAX_HUE), MIN_HUE, MAX_HUE);
  return hueValue;
}

uint8_t readSaturation() {
  return MAX_SATURATION - constrain(map((1023 - analogRead(PIN_FADER_SATURATION)), FADER_MIN_VAL, FADER_MAX_VAL, MIN_SATURATION, MAX_SATURATION), MIN_SATURATION, MAX_SATURATION);
}


uint8_t readValue() {
  return constrain(map((1023 - analogRead(PIN_FADER_VALUE)), FADER_MIN_VAL, FADER_MAX_VAL, MIN_VALUE, MAX_VALUE), MIN_VALUE, MAX_VALUE);
}
