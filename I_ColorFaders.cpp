#include "aurora.h"

#define FADER_MIN_VAL 10
#define FADER_MAX_VAL 1020
#define MOD_SLOW_FACTOR 2
#define MIN_COLOR_MOD_INTERVAL 1
#define MAX_COLOR_MOD_INTERVAL 50
#define MIN_COLOR_MOD_RANGE 20
#define MAX_COLOR_MOD_RANGE MAX_HUE
#define MIN_HUE 0
#define MAX_HUE 250
#define MIN_SATURATION 0
#define MAX_SATURATION 255
#define MIN_VALUE 0
#define MAX_VALUE 255

static uint8_t altColorModFactor = 7;
static int8_t altColorModDirection = 1;
static unsigned long color_mod_counter = 0;

void setCurrentColor() {
  uint8_t currentHue = readHue();
  if (isFaderAlternativeMode()) {
    color_mod_counter += 1;
    uint8_t currentModRange = constrain(map(readValue(), MIN_VALUE, MAX_VALUE, MIN_COLOR_MOD_RANGE, MAX_COLOR_MOD_RANGE), MIN_COLOR_MOD_RANGE, MAX_COLOR_MOD_RANGE);
    uint8_t currentModInterval = constrain(map(readSaturation(), MIN_SATURATION, MAX_SATURATION, MIN_COLOR_MOD_INTERVAL, MAX_COLOR_MOD_INTERVAL), MIN_COLOR_MOD_INTERVAL, MAX_COLOR_MOD_INTERVAL);
    if (color_mod_counter > currentModInterval) {
      altColorModFactor += (altColorModDirection);
      color_mod_counter = 0;
    }
    if (((altColorModFactor + (altColorModDirection * currentModInterval)) < 1) || ((altColorModFactor + (altColorModDirection * currentModInterval)) > currentModRange)) {
      altColorModDirection *= -1;
    }
    if ((altColorModFactor > currentModRange)) {
      altColorModFactor = currentModRange;
    }
    uint8_t modifiedHue = currentHue + altColorModFactor;
    presetColor = CHSV(modifiedHue, MAX_SATURATION, MAX_VALUE);
    Serial.print("Counter: ");
    Serial.print(color_mod_counter);
    Serial.print(", Interval: ");
    Serial.print(currentModInterval);
    Serial.print(", Range: ");
    Serial.print(currentModRange);
    Serial.print(", Direction: ");
    Serial.print(altColorModDirection);
    Serial.print(", modFactor: ");
    Serial.print(altColorModFactor);
    Serial.print(", Hue: ");
    Serial.print(currentHue);
    Serial.print(", Mod Hue: ") + Serial.println(modifiedHue);
  } else {
    uint8_t currentSaturation = readSaturation(); // invert fader
    uint8_t currentValue = readValue();
    altColorModFactor = 0;
    presetColor = CHSV(currentHue, currentSaturation, currentValue);
  }
  touchColor = CHSV(presetColor.hue + (MAX_HUE / 2), presetColor.saturation, presetColor.value);
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
