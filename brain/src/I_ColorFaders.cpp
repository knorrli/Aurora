#include "Aurora.h"

#define MOD_SLOW_FACTOR 2
#define MIN_COLOR_MOD_INTERVAL 1
#define MAX_COLOR_MOD_INTERVAL 50
#define MIN_COLOR_MOD_RANGE 5
#define MAX_COLOR_MOD_RANGE 125
#define MIN_HUE 0
#define MAX_HUE 250
#define MIN_SATURATION 0
#define MAX_SATURATION 255
#define MIN_VALUE 0
#define MAX_VALUE 255

static uint8_t hue = MIN_HUE;
static uint8_t saturation = MAX_SATURATION;
static uint8_t value = MAX_VALUE;

static int8_t altColorModFactor = 0;
static int8_t altColorModDirection = UP;
static unsigned long color_mod_counter = 0;

void setHueFromCC(uint8_t ccValue) {
  hue = map(ccValue, 0, 127, MIN_HUE, MAX_HUE);
}

void setSaturationFromCC(uint8_t ccValue) {
  saturation = map(ccValue, 0, 127, MIN_SATURATION, MAX_SATURATION);
}

void setValueFromCC(uint8_t ccValue) {
  value = map(ccValue, 0, 127, MIN_VALUE, MAX_VALUE);
}

void setCurrentColor() {
  if (faderAltModeEnabled) {
    color_mod_counter += 1;
    uint8_t currentModInterval = constrain(map(saturation, MIN_SATURATION, MAX_SATURATION, MIN_COLOR_MOD_INTERVAL, MAX_COLOR_MOD_INTERVAL), MIN_COLOR_MOD_INTERVAL, MAX_COLOR_MOD_INTERVAL);
    uint8_t currentModRange = constrain(map(value, MIN_VALUE, MAX_VALUE, MIN_COLOR_MOD_RANGE, MAX_COLOR_MOD_RANGE), MIN_COLOR_MOD_RANGE, MAX_COLOR_MOD_RANGE);

    if (color_mod_counter > currentModInterval) {
      altColorModFactor += (altColorModDirection);
      color_mod_counter = 0;
    }

    int8_t modRangeLowerBound = -(currentModRange/2)-1;
    int8_t modRangeUpperBound = (currentModRange/2);

    if (altColorModFactor - ((altColorModDirection * currentModInterval)) < (modRangeLowerBound-1)) {
      altColorModFactor = modRangeLowerBound;
      altColorModDirection *= -1;
    }

    if (altColorModFactor + (altColorModDirection * currentModInterval) > modRangeUpperBound) {
      altColorModFactor = modRangeUpperBound;
      altColorModDirection *= -1;
    }

    uint8_t modifiedHue = hue + altColorModFactor;
    presetColor = CHSV(modifiedHue, MAX_SATURATION, MAX_VALUE);
  } else {
    altColorModFactor = 0;
    presetColor = CHSV(hue, saturation, value);
  }
}
