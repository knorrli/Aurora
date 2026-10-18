#pragma once

#include <stdint.h>

#include "render.h"
#include "routes.h"

namespace render {

static const float MAX_SPEED_PIXELS_PER_BEAT = 60.0f;
static const uint8_t MAX_CHORD = 16;
static const float MAX_DELAY_STEPS = 2.0f;
static const float EDGE_REACH_PIXELS = PIXELS * 0.25f;

struct Envelope {
  float delay;
  float attack;
  float hold;
  float decay;
  float sustain;
  float release;
  bool stepped;
};

struct Cells {
  float rateBeats;
  uint8_t chord;
  float spread;
  bool outside;
  float delay;
  float gateBeats;
  uint8_t end;
  float width;
  float edge;
  float tailBeats;
  float position;
  float bend;
  float bendAt;
  float brightness;
  float curve;
  float curveAt;
  uint8_t direction;
  Envelope envelopes[AURORA_ENVELOPES];
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
  Cells cells;
  Field field;
  Flow flow;
  Core core;
  Scatter scatter;
  float lfoBeats;
  Hsv color;
};

float signedOf(float value);

float controlValue(uint8_t control, float value);

void readControls(const float *dialed, const Modulation *modulation, Reading &out);

void hurry(Reading &reading, float speed);

Hsv routedColor(const float *dialed, const Modulation *modulation);

}
