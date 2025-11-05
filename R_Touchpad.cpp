#include "aurora.h"

#define NO_TOUCH_POSITION TSPoint()

#define X_AXIS_RESISTANCE 230

#define GRID_SIZE_X 5
#define GRID_SIZE_Y 2

#define STEP_SIZE 9

#define X_AXIS_VALUE_LOWER_BOUND 90
#define X_AXIS_VALUE_UPPER_BOUND 930
#define Y_AXIS_VALUE_LOWER_BOUND 220
#define Y_AXIS_VALUE_UPPER_BOUND 860

#define GRID_OFFSET_X ((X_AXIS_VALUE_UPPER_BOUND - X_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_X) / 2
#define GRID_OFFSET_Y ((Y_AXIS_VALUE_UPPER_BOUND - Y_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_Y) / 2

static TouchScreen touchScreen = TouchScreen(PIN_TOUCHPAD_XP, PIN_TOUCHPAD_YP, PIN_TOUCHPAD_XM, PIN_TOUCHPAD_YM, X_AXIS_RESISTANCE);
static TSPoint lastTouchPosition = NO_TOUCH_POSITION;
static TSPoint currentTouchPosition = NO_TOUCH_POSITION;
static TSPoint gridPosition = NO_TOUCH_POSITION;

void renderTouchAction() {
  TSPoint touchPosition = currentTouchPosition;
  if ((!isTouched()) && holdModeEnabled) {
    touchPosition = lastTouchPosition;
  }

  if (touchPosition != NO_TOUCH_POSITION) {
    if (touchpadStripMode == TOUCHPAD_STRIP_MODE_MIRRORED_EXCLUSIVE) {
      affectExclusive(touchPosition);
    } else if (touchpadStripMode == TOUCHPAD_STRIP_MODE_ALL) {
      affectAll();
    } else {
      affectMirrored(touchPosition);
    }
  }
  renderTouchpad();
}

void affectAll() {
  if (touchpadEffectMode == TOUCHPAD_EFFECT_MODE_FILL) {
    fillAll();
  } else {
    invertAll();
  }
}

void affectMirrored(TSPoint touchPosition) {
  if (touchpadEffectMode == TOUCHPAD_EFFECT_MODE_FILL) {
    fillMirrored(touchPosition);
  } else {
    invertMirrored(touchPosition);
  }
}

void affectExclusive(TSPoint touchPosition) {
  if (touchpadEffectMode == TOUCHPAD_EFFECT_MODE_FILL) {
    fillMirrored(touchPosition);
  } else {
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
        if (!strip[stripIndex][pixelIndex] == CRGB::Black) {
          strip[stripIndex][pixelIndex] = touchColor;
        }
      }
    }
  }
  // affectMirrored(touchPosition);
  clearUntouchedStrips(touchPosition);
}

void clearUntouchedStrips(TSPoint touchPosition) {
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    if (!((stripIndex == gridPosition.x) || (stripIndex == mirroredStrip(gridPosition.x)))) {
      fill_solid(strip[stripIndex], PIXELS_PER_STRIP, CRGB::Black);
    }
  }
}

void fillAll() {
  strips.fill_solid(touchColor);
}

void fillMirrored(TSPoint touchPosition) {
  uint8_t stripIndex = gridPosition.x;
  fill_solid(strip[stripIndex], PIXELS_PER_STRIP, touchColor);
  fill_solid(strip[mirroredStrip(stripIndex)], PIXELS_PER_STRIP, touchColor);
}

void invertAll() {
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    invertStrip(stripIndex);
  }
}

void invertMirrored(TSPoint touchPosition) {
  uint8_t stripIndex = gridPosition.x;
  invertStrip(stripIndex);
  if (!(stripIndex == ((NUMBER_OF_STRIPS - 1) / 2))) {
    invertStrip(mirroredStrip(stripIndex));
  }
}

void invertStrip(uint8_t stripIndex) {
  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
    if (!strip[stripIndex][pixelIndex] == CRGB::Black) {
      strip[stripIndex][pixelIndex] = CRGB::Black;
    } else {
      strip[stripIndex][pixelIndex] = touchColor;
    }
  }
}

void readTouchpad() {
  TSPoint touchPosition = touchScreen.getPoint();

  if (touchPosition.z > touchScreen.pressureThreshhold) {
    currentTouchPosition = touchPosition;
    gridPosition = calculateGridPosition(touchPosition);
    lastTouchPosition = touchPosition;
  } else {
    currentTouchPosition = NO_TOUCH_POSITION;
  }

  setTouchColor();
}

void setTouchColor() {
  int16_t yValue = constrain(map(lastTouchPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, -255, 255), -255, 255);
  touchColor = CHSV(presetColor.hue, 255, 255);
  if (yValue < 0) {
    touchColor.value = max(0, 255 + yValue);
  } else {
    if (touchpadVerticalMode == TOUCHPAD_VERTICAL_MODE_SATURATION) {
      touchColor.saturation = 255 - yValue;
    } else {
      touchColor.hue += (yValue / 3);
    }
  }
}

TSPoint calculateGridPosition(TSPoint touchPosition) {
  uint8_t gridX = constrain(map(touchPosition.x + GRID_OFFSET_X, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, 0, GRID_SIZE_X - 1), 0, GRID_SIZE_X - 1);
  uint8_t gridY = constrain(map(touchPosition.y + (GRID_OFFSET_Y * 2), Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 0, GRID_SIZE_Y - 1), 0, GRID_SIZE_Y - 1);

  return TSPoint(gridX, gridY, touchPosition.z);
}

void renderTouchpad() {
  for (uint8_t gridX = 0; gridX < GRID_SIZE_X; gridX++) {
    CRGB upperPixelColor;
    CRGB lowerPixelColor;
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      nblend(upperPixelColor, strip[gridX][(PIXELS_PER_STRIP - 1) - pixelIndex], PIXELS_PER_STRIP);
      nblend(lowerPixelColor, strip[gridX][pixelIndex], PIXELS_PER_STRIP);
    }
    touchpad[gridX] = lowerPixelColor;
    touchpad[(PIXEL_INDEX_TOUCHPAD_END - 1) - gridX] = upperPixelColor;
  }
}

bool isTouched() {
  return currentTouchPosition != NO_TOUCH_POSITION;
}

uint8_t mirroredStrip(uint8_t stripIndex) {
  return (NUMBER_OF_STRIPS - 1) - stripIndex;
}
