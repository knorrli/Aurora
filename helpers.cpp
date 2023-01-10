#include "aurora.h"

void renderColorIndicators() {
  pixels[PIXEL_INDEX_PRESET_COLOR] = CHSV(presetColor.hue, presetColor.saturation, 255);
  pixels[PIXEL_INDEX_PRESET_COLOR].fadeLightBy(128);
  pixels[PIXEL_INDEX_TOUCH_COLOR] = touchColor;
  pixels[PIXEL_INDEX_TOUCH_COLOR].fadeLightBy(128);
}

CHSV randomColor() {
  return CHSV(random8(), 255, 255);
}

bool isFaderAlternativeMode() {
  return (analogRead(PIN_FADER_MODE) > 511);
}

void readTempoGates() {
  if (!tempoGate && ((currentMillis - lastGateMillis) > TEMPO_GATE_READ_DURATION)) {
    if (digitalRead(PIN_TEMPO)) {
      tempoGate = HIGH;
      return;
    }
  }
  tempoGate = LOW;
  return;
}

void renderTempo() {
  if (tempoGate || ((currentMillis - lastGateMillis) < TEMPO_GATE_DURATION)) {
    digitalWrite(PIN_TEMPO_LED, HIGH);
  } else {
    digitalWrite(PIN_TEMPO_LED, LOW);
  }
}

uint8_t readBrightness() {
  uint8_t brightnessFactor = readKeypad();
  if (brightnessFactor == 0 || brightnessFactor == -1) {
    return MAX_BRIGHTNESS;
  } else {
    return map(brightnessFactor, 1, 9, 1, MAX_BRIGHTNESS / 2);
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
