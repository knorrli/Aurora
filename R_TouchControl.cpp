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
#define Y_AXIS_VALUE_LOWER_BOUND 220
#define Y_AXIS_VALUE_UPPER_BOUND 860

#define GRID_OFFSET_X ((X_AXIS_VALUE_UPPER_BOUND - X_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_X) / 2
#define GRID_OFFSET_Y ((Y_AXIS_VALUE_UPPER_BOUND - Y_AXIS_VALUE_LOWER_BOUND) / GRID_SIZE_Y) / 2

static bool holdModeEnabled = false;
static bool presetModeEnabled = false;
static bool variationModeEnabled = false;

static TouchScreen touchScreen = TouchScreen(PIN_TOUCHPAD_XP, PIN_TOUCHPAD_YP, PIN_TOUCHPAD_XM, PIN_TOUCHPAD_YM, X_AXIS_RESISTANCE);
static TSPoint lastTouchPosition = NO_TOUCH_POSITION;
static TSPoint currentTouchPosition = NO_TOUCH_POSITION;
static TSPoint gridPosition = NO_TOUCH_POSITION;

void renderTouchAction() {
  TSPoint currentPosition = currentTouchPosition;
  if ((currentPosition == NO_TOUCH_POSITION) && holdModeEnabled) {
    currentPosition = lastTouchPosition;
  }

  if (currentPosition != NO_TOUCH_POSITION) {
    if (presetModeEnabled) {
      affectPresetColor(currentPosition);
    } else {
      if (variationModeEnabled) {
        renderStripMode(currentPosition);
      } else {
        renderPointMode(currentPosition);
      }
    }
    renderTouchPosition(currentPosition);
  }
  renderTouchpad();
}

void renderStripMode(TSPoint currentPosition) {
  int16_t yValue = constrain(map(currentPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, -255, 255), -255, 255);
  uint8_t stripIndex = gridPosition.x;
  //  CHSV color = touchColor;
  if (yValue < 0) {
    touchColor.value = max(0, 255 + yValue);
  } else {
    touchColor.saturation = 255 - yValue;
  }
  fill_solid(strip[stripIndex], PIXELS_PER_STRIP, touchColor);
  fill_solid(strip[(NUMBER_OF_STRIPS - 1) - stripIndex], PIXELS_PER_STRIP, touchColor);
}

void affectPresetColor(TSPoint currentPosition) {
  uint8_t xIndex = constrain(map(currentPosition.x, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, -(255 / NUMBER_OF_STRIPS), (255 / NUMBER_OF_STRIPS)), -(255 / NUMBER_OF_STRIPS), (255 / NUMBER_OF_STRIPS));
  // TODO: DONT MAP TO PIXELS, MAP TO COLORS DIRECTLY!
  int8_t yValue = constrain(map(currentPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, -128, 127), -128, 127);
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t stripHue = presetColor.hue;
    if (variationModeEnabled) {
      if (stripIndex <= ((NUMBER_OF_STRIPS / 2) )) {
        stripHue += (xIndex * (-(NUMBER_OF_STRIPS / 2) + stripIndex));
      } else {
        stripHue += (xIndex * (-(NUMBER_OF_STRIPS / 2) + ((NUMBER_OF_STRIPS - 1) - stripIndex)));
      }
    } else {
      stripHue += (xIndex * (-(NUMBER_OF_STRIPS / 2) + stripIndex));
    }
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      if (!strip[stripIndex][pixelIndex] == CRGB::Black) {
        int8_t offsetPixelIndex = pixelIndex - (PIXELS_PER_STRIP / 2);
        CHSV pixelColor = CHSV((stripHue + ((offsetPixelIndex * yValue) / PIXELS_PER_STRIP)), presetColor.saturation, presetColor.value);
        strip[stripIndex][pixelIndex] = blend(strip[stripIndex][pixelIndex], pixelColor, 255);
      }
    }
  }
}

void renderPointMode(TSPoint currentPosition) {
  uint8_t pixelIndex = constrain(map(currentPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 0, PIXELS_PER_STRIP) - 1, 0, PIXELS_PER_STRIP - 1);
  if (variationModeEnabled) {
    uint8_t stripIndex = gridPosition.x;
    if (stripIndex > (NUMBER_OF_STRIPS / 2)) {
      stripIndex = (NUMBER_OF_STRIPS - 1) - stripIndex;
    }
    fill_solid(strip[stripIndex], PIXELS_PER_STRIP, CRGB::Black);
    strip[stripIndex][pixelIndex] = touchColor;
  } else {
    uint8_t width = constrain(map(currentPosition.x, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, 1, PIXELS_PER_STRIP - 1), 1, PIXELS_PER_STRIP - 1);
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

void readTouchInputs() {
  // TODO: FIX PIN NAMING!
  holdModeEnabled = (analogRead(PIN_EFFECTS_MODE) > 511);
  presetModeEnabled = digitalRead(PIN_TOUCH_MODE);
  variationModeEnabled = digitalRead(PIN_LINK_MODE);
  TSPoint touchPosition = touchScreen.getPoint();
  if (touchPosition.z > touchScreen.pressureThreshhold) {
    currentTouchPosition = touchPosition;
    lastTouchPosition = touchPosition;
    gridPosition = calculateGridPosition(currentTouchPosition);
  } else {
    currentTouchPosition = NO_TOUCH_POSITION;
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
  //  touchpad.fadeLightBy(192); // dim touchpad
}

void renderTouchPosition(TSPoint touchPosition) {
  uint8_t yValue = constrain(map(touchPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 0, 255), 0, 255);
  uint8_t upperPixelIndex = gridPosition.x;
  uint8_t lowerPixelIndex = (PIXEL_INDEX_TOUCHPAD_END - 1) - upperPixelIndex;
  //  if (presetModeEnabled) {
  //    touchpad[upperPixelIndex] = blend(touchpad[upperPixelIndex], touchColor, min(yValue * 2, 255));
  //    touchpad[lowerPixelIndex] = blend(touchpad[lowerPixelIndex], touchColor, min((255 - yValue) * 2, 255));
  //    if (variationModeEnabled) {
  //      uint8_t upperMirrorPixelIndex = (GRID_SIZE_X - 1) - gridPosition.x;
  //      uint8_t lowerMirrorPixelIndex = (PIXEL_INDEX_TOUCHPAD_END - 1) - upperMirrorPixelIndex;
  //      touchpad[upperMirrorPixelIndex] = touchpad[upperPixelIndex];
  //      touchpad[lowerMirrorPixelIndex] = touchpad[lowerPixelIndex];
  //    }
  //  } else {
  //    touchpad[upperPixelIndex] = touchColor;
  //    touchpad[lowerPixelIndex] = touchColor;
  //  }
}
