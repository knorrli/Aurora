#include "aurora.h"

void renderColorIndicators() {
  pixels[PIXEL_INDEX_PRESET_COLOR] = presetColor;
  pixels[PIXEL_INDEX_PRESET_COLOR].fadeLightBy(128);
  pixels[PIXEL_INDEX_TOUCH_COLOR] = touchColor;
  pixels[PIXEL_INDEX_TOUCH_COLOR].fadeLightBy(128);
}

//uint8_t mapToTouchPadPixelIndex(TSPoint gridPosition) {
//  uint8_t touchPadPixelIndex = (gridPosition.y == 2) ? gridPosition.x : PIXEL_INDEX_TOUCHPAD_END - (gridPosition.x - 1);
//  return touchPadPixelIndex;
//}

CHSV randomColor() {
  return CHSV(random8(), 255, 255);
}

bool readTempoGate() {
  if (!tempoGate && (currentMillis - lastGateMillis > TEMPO_GATE_READ_DURATION)) {
    if (digitalRead(PIN_TEMPO)) {
      currentTempo = currentMillis - lastGateMillis;
      lastGateMillis = currentMillis;
      return HIGH;
    }
  }
  return LOW;
}

bool isTempoDivision(uint8_t division) {
  bool gate = tempoGate;
  gate = (currentTick % ((ticks / division) + 1) == 0);
  return gate;
}

void renderTempo() {
  if (tempoGate || (currentMillis - lastGateMillis < TEMPO_GATE_DURATION)) {
    digitalWrite(PIN_TEMPO_LED, HIGH);
  } else {
    digitalWrite(PIN_TEMPO_LED, LOW);
  }
}

void showBootIndicatorReady() {
  pixels[PIXEL_INDEX_TOUCH_COLOR] = CRGB::Red;
  pixels[PIXEL_INDEX_PRESET_COLOR] = CRGB::Red;
  FastLED.show();
  delay(300);
  pixels[PIXEL_INDEX_TOUCH_COLOR] = CRGB::Green;
  pixels[PIXEL_INDEX_PRESET_COLOR] = CRGB::Green;
  FastLED.show();
  delay(300);
  pixels[PIXEL_INDEX_TOUCH_COLOR] = CRGB::Blue;
  pixels[PIXEL_INDEX_PRESET_COLOR] = CRGB::Blue;
  FastLED.show();
  delay(300);
  FastLED.clear();
}
