#pragma once

#include <stdint.h>

#include "render.h"
#include "routes.h"

namespace render {

static const float MAX_SPEED_PIXELS_PER_BEAT = 60.0f;

struct Shape {
  float width;
  uint8_t count;
  float edge;
  float tailBeats;
  float positionCells;
  float speedPixels;
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
  uint8_t count;
  float width;
  float edge;
  float cellsPerBeat;
};

struct Flow {
  float hue;
  float white;
  float dark;
  float cyclesPerBeat;
  float density;
};

struct Light {
  float hue;
  float white;
  float dark;
};

struct Scatter {
  float rate;
  uint8_t count;
  float width;
  float edge;
  float randomize;
  float slide;
  float spread;
  float value;
  float hue;
  float white;
};

struct Reading {
  Shape shape;
  Fan fan;
  Field field;
  Flow flow;
  Light light;
  Scatter scatter;
  float lfoBeats;
  Hsv color;
};

float bipolarOf(uint8_t value);

float controlValue(uint8_t cc, uint8_t value);

float snapToStill(float pixels);

void readControls(const uint8_t *dialed, const Pushes *pushes, Reading &out);

Hsv routedColor(const uint8_t *dialed, const Pushes *pushes);

}
