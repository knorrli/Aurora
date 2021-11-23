#include "aurora.h"

#define NO_TOUCH_POSITION TSPoint()

#define X_AXIS_RESISTANCE 230
#define PIN_TOUCHPAD_YP A4
#define PIN_TOUCHPAD_XM A5
#define PIN_TOUCHPAD_YM 7
#define PIN_TOUCHPAD_XP 6

#define GRID_SIZE_X 5
#define GRID_SIZE_Y 2

#define X_AXIS_VALUE_LOWER_BOUND 90
#define X_AXIS_VALUE_UPPER_BOUND 930
#define Y_AXIS_VALUE_LOWER_BOUND 190
#define Y_AXIS_VALUE_UPPER_BOUND 860

#define GRID_OFFSET_X ((X_AXIS_VALUE_UPPER_BOUND - X_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_X) / 2
#define GRID_OFFSET_Y ((Y_AXIS_VALUE_UPPER_BOUND - Y_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_Y) / 2


static TouchScreen touchScreen = TouchScreen(PIN_TOUCHPAD_XP, PIN_TOUCHPAD_YP, PIN_TOUCHPAD_XM, PIN_TOUCHPAD_YM, X_AXIS_RESISTANCE);
static TSPoint currentTouchPosition;
static TSPoint gridPosition;

void renderTouchAction() {
  if (currentTouchPosition != NO_TOUCH_POSITION) {
    uint8_t touchpadPixelIndex = mapToTouchPadPixelIndex(gridPosition);
    renderTouchPosition(touchpadPixelIndex);

    if (presetModeEnabled) {
      if (touchModeEnabled) {
        affectPresetColor();
      } else {
        affectPresetValue();
      }
    } else {
      if (touchModeEnabled) {
        renderTouchMode();
      } else {
        renderStripMode();
      }
    }
  }
}

void affectPresetColor() {
  uint8_t xIndex = constrain(map(currentTouchPosition.x, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, -(255 / NUMBER_OF_STRIPS), (255 / NUMBER_OF_STRIPS)), -(255 / NUMBER_OF_STRIPS), (255 / NUMBER_OF_STRIPS));
  uint8_t yIndex = constrain(map(currentTouchPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, -(255 / (PIXELS_PER_STRIP - 1)), (255 / (PIXELS_PER_STRIP - 1))), -(255 / (PIXELS_PER_STRIP - 1)), (255 / (PIXELS_PER_STRIP - 1)));
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t stripHue = presetColor.hue + (xIndex * (-(NUMBER_OF_STRIPS / 2) + stripIndex));
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      if (!strip[stripIndex][pixelIndex] == CRGB::Black) {
        CHSV pixelColor = CHSV(stripHue + (yIndex * (-((PIXELS_PER_STRIP - 1) / 2) + pixelIndex)), presetColor.saturation, presetColor.value);
        strip[stripIndex][pixelIndex] = blend(strip[stripIndex][pixelIndex], pixelColor, 255);
      }
    }
  }
}

void affectPresetValue() {
}

void renderTouchMode() {
  uint8_t pixelIndex = constrain(map(currentTouchPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 0, PIXELS_PER_STRIP) - 1, 0, PIXELS_PER_STRIP - 1);
  if (linkModeEnabled) {
    uint8_t stripIndex = (gridPosition.x - 1);
    if (stripIndex > (NUMBER_OF_STRIPS / 2)) {
      stripIndex = (NUMBER_OF_STRIPS - 1) - stripIndex;
    }
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, CRGB::Black);
    strip[stripIndex][pixelIndex] = touchColor;
  } else {
    uint8_t width = constrain(map(currentTouchPosition.x, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, 1, PIXELS_PER_STRIP - 1), 1, PIXELS_PER_STRIP - 1);
    if (pixelIndex + width > (PIXELS_PER_STRIP - 1)) {
      pixelIndex -= ((pixelIndex + width) - (PIXELS_PER_STRIP - 1));
    }
    strips.fill_solid(CRGB::Black);
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      for (uint8_t startPixel = 0; startPixel < width; startPixel++) {
        strip[stripIndex][startPixel + pixelIndex] = touchColor;
      }
    }
  }
}

void renderStripMode() {
  uint8_t pixelIndex = constrain(map(currentTouchPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 1, PIXELS_PER_STRIP) - 1, 0, PIXELS_PER_STRIP - 1);
  uint8_t stripIndex = (gridPosition.x - 1);
  if (linkModeEnabled) {
    if (stripIndex > (NUMBER_OF_STRIPS / 2)) {
      stripIndex = (NUMBER_OF_STRIPS - 1) - stripIndex;
    }
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, touchColor);
    fadeToBlackBy(strip[stripIndex], PIXELS_PER_STRIP, map(pixelIndex, 0, PIXELS_PER_STRIP, 255, 0));
  } else {
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, touchColor);
    fadeToBlackBy(strip[stripIndex], PIXELS_PER_STRIP, map(pixelIndex, 0, PIXELS_PER_STRIP, 255, 0));
  }
}

void readTouchInputs() {
  presetModeEnabled = (analogRead(PIN_EFFECTS_MODE) > 511);
  touchModeEnabled = digitalRead(PIN_TOUCH_MODE);
  linkModeEnabled = digitalRead(PIN_LINK_MODE);
  TSPoint touchPosition = touchScreen.getPoint();
  if (touchPosition.z > touchScreen.pressureThreshhold) {
    currentTouchPosition = touchPosition;
    gridPosition = calculateGridPosition(currentTouchPosition);
  } else {
    currentTouchPosition = NO_TOUCH_POSITION;
    gridPosition = NO_TOUCH_POSITION;
  }
}

TSPoint calculateGridPosition(TSPoint touchPosition) {
  uint8_t gridX = constrain(map(touchPosition.x + GRID_OFFSET_X, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, 1, GRID_SIZE_X), 1, GRID_SIZE_X);
  uint8_t gridY = constrain(map(touchPosition.y + (GRID_OFFSET_Y * 2), Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 1, GRID_SIZE_Y), 1, GRID_SIZE_Y);

  return TSPoint(gridX, gridY, touchPosition.z);
}
