#include "aurora.h"

void fillTouchPad(CRGB color) {
  touchpad.fill_solid(color);
  touchpad.fadeLightBy(218);
}

void setStripPixelColor(uint8_t stripIndex, uint8_t pixelIndex, CRGB color) {
  strip[stripIndex][pixelIndex] = color;
}

CHSV randomColor() {
  return CHSV(random8(), 255, 255);
}

bool readTempoGate() {
  if (!tempoGate && (currentMillis - lastGateMillis > TEMPO_GATE_DURATION)) {
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
  gate = (currentTick % ((ticks / division)+1) == 0);
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
