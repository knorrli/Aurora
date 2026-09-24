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

void showRendered(const render::Rgb *rendered) {
  for (uint16_t i = 0; i < NUM_PIXELS_TOTAL; i++) {
    pixels[i] = CRGB(rendered[i].r, rendered[i].g, rendered[i].b);
  }
}

void ShowStripOrder() {
  static render::Rgb rendered[render::STRIPS * render::PIXELS];
  render::renderStripOrder(rendered);
  showRendered(rendered);
}
