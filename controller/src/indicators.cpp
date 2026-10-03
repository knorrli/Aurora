#include "indicators.h"

#include <Arduino.h>
#include <FastLED.h>

#include "aurora_protocol.h"
#include "faders.h"
#include "pad.h"
#include "pins.h"
#include "steady.h"

namespace {

const uint8_t PIXEL_COUNT = 12;
const uint8_t RIGHT_INDICATOR = 0;
const uint8_t GRID_FIRST = 1;
const uint8_t GRID_LAST = 10;
const uint8_t LEFT_INDICATOR = 11;

const uint8_t LIT = 64;
const uint8_t DIM = 13;
const uint8_t UNDER_FINGER = 255;
const uint32_t BLINK_MICROS = 250000;
const uint32_t CUE_ALTERNATING_MICROS = 500000;

const CRGB MODE_COLORS[] = {
    CRGB(255, 255, 255),
    CRGB(140, 0, 255),
    CRGB(255, 200, 0),
};

const CRGB EFFECT_COLORS[PAD_EFFECTS] = {
    CRGB(0, 255, 255),
    CRGB(0, 0, 255),
    CRGB(0, 255, 0),
    CRGB(255, 80, 0),
    CRGB(255, 0, 0),
};

Every every(20000);
CRGB pixels[PIXEL_COUNT];

CRGB at(CRGB color, uint8_t brightness) { return color.nscale8_video(brightness); }

bool phase(uint32_t micros, uint32_t half) { return (micros / half) % 2 == 0; }

CRGB fadersMix(uint8_t (*level)(faders::Fader)) {
    return CRGB(level(faders::COLOR) * 2, level(faders::MOTION) * 2, level(faders::EXTENT) * 2);
}

CRGB leftIndicator(uint32_t micros) {
    const CRGB playing = fadersMix(faders::playing);
    if (!faders::cueing() || phase(micros, CUE_ALTERNATING_MICROS)) return at(playing, LIT);
    const CRGB standing = fadersMix(faders::standing);
    if (standing == playing) return at(CRGB::White, DIM);
    return at(standing, LIT);
}

CRGB rightIndicator(uint32_t micros) {
    if (pad::latched() && !phase(micros, BLINK_MICROS)) return CRGB::Black;
    return at(MODE_COLORS[pad::mode()], LIT);
}

uint8_t gridPixel(uint8_t column, uint8_t row) {
    return row == 0 ? GRID_FIRST + column : GRID_LAST - column;
}

void paintGrid() {
    const bool effects = pad::mode() == PAD_MODE_EFFECTS;
    for (uint8_t column = 0; column < pad::COLUMNS; column++) {
        const CRGB color = effects ? EFFECT_COLORS[column] : MODE_COLORS[pad::mode()];
        const bool marked = aurora_pad_marks(pad::width(), pad::gaps(), column, pad::COLUMNS);
        for (uint8_t row = 0; row < pad::ROWS; row++) {
            const bool fingered = pad::touching() && pad::column() == column && pad::row() == row;
            const uint8_t brightness = fingered ? UNDER_FINGER : marked ? LIT : DIM;
            pixels[gridPixel(column, row)] = at(color, brightness);
        }
    }
}

}

namespace indicators {

void begin() {
    FastLED.addLeds<WS2812, pins::PIXELS, GRB>(pixels, PIXEL_COUNT);
    FastLED.clear(true);
}

void update(uint32_t micros) {
    if (!every.due(micros)) return;
    pixels[LEFT_INDICATOR] = leftIndicator(micros);
    pixels[RIGHT_INDICATOR] = rightIndicator(micros);
    paintGrid();
    FastLED.show();
}

}
