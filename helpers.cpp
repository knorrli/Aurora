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

void readInputSwitches() {
  holdModeEnabled = digitalRead(PIN_HOLD_MODE);
  touchpadEffectMode = digitalRead(PIN_TOUCHPAD_EFFECT_MODE);

  uint16_t faderAndPresetModeValue = analogRead(PIN_FADER_AND_PRESET_MODE);
  if (faderAndPresetModeValue < 450) {
    faderAltModeEnabled = true;
    presetAltModeEnabled = false;
  } else if (faderAndPresetModeValue >= 450 && faderAndPresetModeValue < 600) {
    faderAltModeEnabled = false;
    presetAltModeEnabled = false;
  } else if (faderAndPresetModeValue >= 600 && faderAndPresetModeValue < 1000) {
    faderAltModeEnabled = true;
    presetAltModeEnabled = true;
  } else {
    faderAltModeEnabled = false;
    presetAltModeEnabled = true;
  }

  uint16_t touchpadStripModeValue = analogRead(PIN_TOUCHPAD_STRIP_MODE);
  if (touchpadStripModeValue > 400 && touchpadStripModeValue <= 1000) {
    touchpadStripMode = TOUCHPAD_STRIP_MODE_ALL;
  } else if (touchpadStripModeValue > 1000) {
    touchpadStripMode = TOUCHPAD_STRIP_MODE_MIRRORED_EXCLUSIVE;
  } else {
    touchpadStripMode = TOUCHPAD_STRIP_MODE_MIRRORED;
  }
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

void setBootSettings() {
  int8_t keyValue = readKeypad();
  if (keyValue == 0) {
    touchpadVerticalMode = TOUCHPAD_VERTICAL_MODE_COLOR;
    return;
  } else if (keyValue == -1) {
    brightness = MAX_BRIGHTNESS;
    return;
  } else {
    brightness = map(keyValue, 1, 9, 1, MAX_BRIGHTNESS / 2);
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
