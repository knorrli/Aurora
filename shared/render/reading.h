#pragma once

#include <stdint.h>

#include "render.h"
#include "routes.h"

namespace render {

static const float MAX_SPEED_PIXELS_PER_BEAT = 60.0f;

struct Shape {
  float width;
  float count;
  float edge;
  float tailBeats;
  float position;
  float lapsPerBeat;
  float bend;
  float bendAt;
  bool bounce;
};

struct Fan {
  float spread;
  float lfo;
  float speedPixels;
  float frequency;
  float phase;
  float randomize;
};

struct Field {
  uint8_t form;
  uint8_t direction;
  float hue;
  float white;
  float dark;
  float count;
  float width;
  float edge;
  float cellsPerBeat;
  float position;
};

struct Flow {
  float hue;
  float white;
  float dark;
  float cyclesPerBeat;
  float density;
};

struct Core {
  float hue;
  float white;
  float dark;
};

struct Scatter {
  float rate;
  float count;
  float width;
  float edge;
  float randomize;
  float speed;
  float mix;
};

struct Reading {
  Shape shape;
  Fan fan;
  Field field;
  Flow flow;
  Core core;
  Scatter scatter;
  float lfoBeats;
  Hsv color;
};

float signedOf(uint8_t value);

float controlValue(uint8_t cc, uint8_t value);

float lapPixels(const Shape &shape);

void readControls(const uint8_t *dialed, const Modulation *modulation, Reading &out);

Hsv routedColor(const uint8_t *dialed, const Modulation *modulation);

}
