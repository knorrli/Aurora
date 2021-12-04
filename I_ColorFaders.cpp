#include "aurora.h"

#define FADER_MIN_VAL 10
#define FADER_MAX_VAL 1020
#define MIN_HUE 0
#define MAX_HUE 250
#define MIN_SATURATION 0
#define MAX_SATURATION 255
#define MIN_VALUE 0
#define MAX_VALUE 255

void setCurrentColor() {
  uint8_t currentHue = readHue();
  uint8_t currentSaturation = readSaturation();
  uint8_t currentValue = readValue();
  // manual touch color
  if ((analogRead(PIN_FADER_MODE) > 511)) {
    if (isTouched()) {
      touchColor = CHSV(currentHue, currentSaturation, currentValue);
    } else {
      presetColor = CHSV(currentHue, currentSaturation, currentValue);
    }
  } else {
    presetColor = CHSV(readHue(), readSaturation(), readValue());
    touchColor = CHSV(presetColor.hue + 128, presetColor.saturation, presetColor.value);
  }
}

uint8_t readHue() {
  uint16_t hueFaderValue = analogRead(PIN_FADER_HUE);
  uint8_t hueValue = constrain(map((1023 - hueFaderValue), FADER_MIN_VAL, FADER_MAX_VAL, MIN_HUE, MAX_HUE), MIN_HUE, MAX_HUE);
  //  if (analogRead(PIN_FADER_MODE) < 511) { // Continuous Colors
  return hueValue;
  //  } else { // binned colors
  //    uint8_t binnedValue = constrain(map(hueValue, MIN_HUE, MAX_HUE, 0, COLOR_BINS), 0, COLOR_BINS);
  //    return binnedValue * ((256 - COLOR_BINS) / COLOR_BINS);
  //  }
}

uint8_t readSaturation() {
  return MAX_SATURATION - constrain(map((1023 - analogRead(PIN_FADER_SATURATION)), FADER_MIN_VAL, FADER_MAX_VAL, MIN_SATURATION, MAX_SATURATION), MIN_SATURATION, MAX_SATURATION);
}


uint8_t readValue() {
  return constrain(map((1023 - analogRead(PIN_FADER_VALUE)), FADER_MIN_VAL, FADER_MAX_VAL, MIN_VALUE, MAX_VALUE), MIN_VALUE, MAX_VALUE);
}
