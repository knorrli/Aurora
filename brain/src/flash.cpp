#include "flash.h"

#include <Arduino.h>

namespace flash {

static const uint32_t IGNORE_REFIRE_MS = 300;
static const uint32_t FADE_MS = 500;
static const uint32_t DARK_AFTER_FADE_MS = 200;

static bool fired = false;
static uint32_t firedAtMs = 0;
static uint8_t flashHue = 0;

void fire(uint8_t hue) {
    const uint32_t now = millis();
    if (fired && now - firedAtMs <= IGNORE_REFIRE_MS) return;
    fired = true;
    firedAtMs = now;
    flashHue = hue;
}

void drawOver(render::Rgb *pixels) {
    if (!fired) return;
    const uint32_t elapsed = millis() - firedAtMs;
    if (elapsed >= FADE_MS + DARK_AFTER_FADE_MS) {
        fired = false;
        return;
    }
    const uint8_t progress = elapsed < FADE_MS ? (uint8_t)(elapsed * 255 / FADE_MS) : 255;
    const render::Rgb color = render::withSaturationAndValue(render::rainbowRgb(flashHue), progress, 255 - progress);
    for (uint16_t i = 0; i < render::STRIPS * render::PIXELS; i++) pixels[i] = color;
}

}
