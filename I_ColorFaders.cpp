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
  if (isFaderAlternativeMode()) {
    presetColor = CHSV(currentHue, MAX_SATURATION, MAX_VALUE);
  } else {
    presetColor = CHSV(currentHue, currentSaturation, currentValue);
  }
  touchColor = CHSV(presetColor.hue + (MAX_HUE/2), presetColor.saturation, presetColor.value);
}

void applyRainbow() {
  uint8_t xValue = constrain(map((1023 - analogRead(PIN_FADER_VALUE)), FADER_MIN_VAL, FADER_MAX_VAL, 0, (255 / (NUMBER_OF_STRIPS-1))), 0, (255 / (NUMBER_OF_STRIPS-1)));
  uint8_t yValue = constrain(map((1023 - analogRead(PIN_FADER_SATURATION)), FADER_MIN_VAL, FADER_MAX_VAL, 0, 255), 0, 255);
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t stripHue = presetColor.hue;
    stripHue += (xValue * (stripIndex - ((NUMBER_OF_STRIPS - 1) / 2)));

    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      if (!strip[stripIndex][pixelIndex] == CRGB::Black) {
        int8_t offsetPixelIndex = pixelIndex - (PIXELS_PER_STRIP / 2);
        CHSV pixelColor = CHSV((stripHue + ((offsetPixelIndex * yValue) / PIXELS_PER_STRIP)), presetColor.saturation, presetColor.value);
        strip[stripIndex][pixelIndex] = blend(strip[stripIndex][pixelIndex], pixelColor, 255);
      }
    }
  }
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
