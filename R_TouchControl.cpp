#include "aurora.h"

#define NO_TOUCH_POSITION TSPoint()

#define X_AXIS_RESISTANCE 230
#define PIN_TOUCHPAD_YP A4
#define PIN_TOUCHPAD_XM A4
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
static bool effectsModeEnabled;
static bool touchModeEnabled;
static TSPoint currentPosition;
static TSPoint gridPosition;


void renderTouchColorIndicator() {
  pixels[PIXEL_INDEX_TOUCH_COLOR] = touchColor;
}

void renderTouchPosition(uint8_t touchPadPixelIndex) {
  pixels[touchPadPixelIndex] = touchColor;
}

void renderTouchControl() {
  renderTouchColorIndicator();
  fillTouchPad(touchColor);

  if (currentPosition != NO_TOUCH_POSITION) {
    uint8_t touchPadPixelIndex = mapToTouchPadPixelIndex(gridPosition);
    renderTouchPosition(touchPadPixelIndex);

    if (effectsModeEnabled) {
      renderEffect(touchPadPixelIndex);
    } else {
      renderTouchAction(touchPadPixelIndex);
    }
  }
}

void renderTouchAction(uint8_t touchPadPixelIndex) {
  uint8_t selectedStrip = gridPosition.x - 1;

  uint16_t yPos = constrain(map(currentPosition.y, Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 1, PIXELS_PER_STRIP) - 1, 0, PIXELS_PER_STRIP - 1);
//  setStripPixelColor(selectedStrip, yPos, touchColor);
}

void renderEffect(uint8_t touchPadPixelIndex) {
  switch (touchPadPixelIndex) {
    case 10:
      renderPreset(lastPreset);
      return;
    case 9:
      Rain(touchColor);
      return;
    case 8:
      Stars(touchColor);
      return;
    case 7:
      Strobe(touchColor);
      return;
    case 6:
      Chaos(touchColor);
      return;
    case 1:
      //      renderLastPreset(gate, randomColor());
      return;
    case 2:
      //      renderRain(gate, randomColor());
      return;
    case 3:
      //      renderStars(gate, randomColor());
      return;
    case 4:
      //      renderStrobe(gate, randomColor());
      return;
    case 5:
    //      renderChaos(gate, randomColor());
    default:
      return;
  }
}

uint8_t mapToTouchPadPixelIndex(TSPoint gridPosition) {
  uint8_t touchPadPixelIndex = (gridPosition.y == 2) ? gridPosition.x : PIXEL_INDEX_TOUCHPAD_END - (gridPosition.x - 1);
  return touchPadPixelIndex;
}

void readTouchInputs() {
  effectsModeEnabled = (analogRead(PIN_EFFECTS_MODE) > 511);
  touchModeEnabled = digitalRead(PIN_TOUCH_MODE);
  linkModeEnabled = digitalRead(PIN_LINK_MODE);
  TSPoint position = touchScreen.getPoint();
  if (position.z > touchScreen.pressureThreshhold) {
    currentPosition = position;
    gridPosition = calculateGridPosition(currentPosition);
  } else {
    currentPosition = NO_TOUCH_POSITION;
    gridPosition = NO_TOUCH_POSITION;
  }
}

TSPoint calculateGridPosition(TSPoint position) {
  uint8_t gridX = constrain(map(position.x + GRID_OFFSET_X, X_AXIS_VALUE_LOWER_BOUND, X_AXIS_VALUE_UPPER_BOUND, 1, GRID_SIZE_X), 1, GRID_SIZE_X);
  uint8_t gridY = constrain(map(position.y + (GRID_OFFSET_Y * 2), Y_AXIS_VALUE_LOWER_BOUND, Y_AXIS_VALUE_UPPER_BOUND, 1, GRID_SIZE_Y), 1, GRID_SIZE_Y);

  return TSPoint(gridX, gridY, position.z);
}
