#pragma once

#include <stdint.h>

#include "color8.h"
#include "routes.h"

// The wall, drawn from a patch's control bytes and the musical position. The
// brain wraps it with FastLED and DMX; the editor runs the same source as
// WebAssembly, through tools/preview.js. Nothing in here may reach for
// Arduino, FastLED or a clock: everything arrives as an argument.
namespace render {

static const uint8_t STRIPS = 5;
static const uint8_t PIXELS = 45;
static const uint8_t FAN_CURVE_STEPS_PER_STRIP = 24;
static const uint16_t FAN_CURVE_POINTS = (STRIPS - 1) * FAN_CURVE_STEPS_PER_STRIP + 1;

struct Hsv {
  uint8_t h, s, v;
};

struct Tracker {
  float offset;
  float rate;
};

// Everything that has to remember where it was between frames. One per wall
// being drawn: two walls sharing one would advance it at one wall's rates and
// read it at the other's.
struct Motion {
  Tracker travel[STRIPS] = {};
  Tracker pulse = {};
  Tracker wander = {};
  Tracker placed = {};
  Tracker scatter = {};
  float lastCoreCells[STRIPS] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
  float lastPulseBeats = 0.0f;
  float lastTravelBeats = 0.0f;
  bool lastBouncing = false;
};

// The color at full value, with the light carried apart for the fixture's own
// dimmer.
struct Wash {
  Rgb color;
  uint8_t level;
};

// What the fan is doing, for the editor's overlay: the wave between the
// strips is something five strips cannot show.
struct FanReading {
  float values[STRIPS];
  float curve[FAN_CURVE_POINTS];
  float turns;
  // Where the wave has to read for a strip to cancel Speed and stand still.
  // Outside -1..1, no strip can.
  float stillAt;
  // How much of the wave each of the three amounts spends, -1..1.
  float position;
  float rate;
  float pulse;
  float scrambled;
};

struct Frame {
  Rgb pixels[STRIPS * PIXELS];  // strip by strip, pixel 0 first
  Wash wash;
  FanReading fan;
};

void renderGenerator(const uint8_t *dialed, float beats, Motion &motion, Frame &out);

// A control's byte as the renderer uses it: pixels a beat, shapes, beats a
// cycle, a signed reach. Every conversion the renderer makes goes through
// here, which is what lets the editor label a fader in the renderer's own
// units without a second copy of the curves.
float convert(uint8_t cc, uint8_t value);

// The fraction of its light a pixel keeps under a darkening push, 0 to -1.
float lightLeft(float dark);

// Each strip one flat color in data-chain order, for reading the rig's order
// off the wall.
void renderStripOrder(Rgb *pixels);

// The three faders as the strips take them.
Hsv colorFrom(const uint8_t *dialed, const Pushes *pushes);

// Null pushes is the washes with nothing modulating them, which is what every
// pattern but the generator gives them.
Wash washFrom(const uint8_t *dialed, const Pushes *pushes);

}  // namespace render
