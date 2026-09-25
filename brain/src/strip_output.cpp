#include "strip_output.h"

#include <FastLED.h>

namespace strip_output {

static const uint8_t DATA_PIN = 2;
static const uint16_t PIXEL_COUNT = render::STRIPS * render::PIXELS;
static const uint32_t POWER_SETTLE_MS = 500;

static CRGB leds[PIXEL_COUNT];

void begin() {
    delay(POWER_SETTLE_MS);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, PIXEL_COUNT);
}

void showStartupSequence() {
    static const CRGB sequence[] = { CRGB::Red, CRGB::Green, CRGB::Blue };
    for (const CRGB &color : sequence) {
        FastLED.clear(false);
        for (uint8_t strip = 0; strip < render::STRIPS; strip++) leds[strip * render::PIXELS] = color;
        FastLED.show();
        delay(200);
    }
    FastLED.clear(true);
}

void show(const render::Rgb *pixels, bool blackout) {
    for (uint16_t i = 0; i < PIXEL_COUNT; i++) {
        leds[i] = blackout ? CRGB::Black : CRGB(pixels[i].r, pixels[i].g, pixels[i].b);
    }
    FastLED.show();
}

}
