#include "Aurora.h"

CHSV randomColor() {
  return CHSV(random8(), 255, 255);
}

void renderTempo() {
  digitalWrite(PIN_TEMPO_LED, (currentMillis - lastGateMillis) < TEMPO_LED_PULSE_MS);
}

void showBootIndicatorReady() {
  static const CRGB sequence[] = { CRGB::Red, CRGB::Green, CRGB::Blue };

  // One pixel per strip rather than a full-brightness fill: on the bench
  // the strips may be running off the Teensy's own supply.
  for (uint8_t step = 0; step < 3; step++) {
    FastLED.clear(false);
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      strip[stripIndex][0] = sequence[step];
    }
    FastLED.show();
    delay(200);
  }
  FastLED.clear(true);
}

uint8_t mirroredStrip(uint8_t stripIndex) {
  return (NUMBER_OF_STRIPS - 1) - stripIndex;
}

// Paints each strip one flat hue so the data-chain order can be read off the
// wall while rigging. Hues run red → blue in index order, spaced widely
// enough to stay distinguishable at a distance and through a phone camera.
void ShowStripOrder() {
  static const uint8_t hues[NUMBER_OF_STRIPS] = { 0, 40, 96, 130, 165 };

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      strip[stripIndex][pixelIndex] = CHSV(hues[stripIndex], 255, 200);
    }
  }
}
