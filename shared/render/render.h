#pragma once

#include <stdint.h>

#include "color8.h"

namespace render {

static const uint8_t STRIPS = 5;
static const uint8_t PARS = 4;
static const uint8_t PIXELS = 45;
static const uint8_t FAN_CURVE_STEPS_PER_STRIP = 24;
static const uint16_t FAN_CURVE_POINTS = (STRIPS - 1) * FAN_CURVE_STEPS_PER_STRIP + 1;
static const uint8_t BEND_POINTS = PIXELS + 1;

static const uint8_t TAIL_MAX_BEATS = 8;
static const uint8_t TAIL_STEPS_PER_BEAT = 64;
static const uint16_t TAIL_STEPS = TAIL_MAX_BEATS * TAIL_STEPS_PER_BEAT;

struct Hsv {
  uint8_t h, s, v;
};

struct Clock {
  float offset;
  float rate;
};

struct Motion {
  Clock travel[STRIPS] = {};
  Clock swing[STRIPS] = {};
  Clock lfo = {};
  Clock flow = {};
  Clock field = {};
  Clock scatter = {};
  float lastLfoBeats = 0.0f;
};

struct TailHistory {
  float centers[TAIL_STEPS];
  float lastCenter;
  int32_t firstStep;
  float unwrapCells;
  float lastPlacement;
};

struct Tails {
  TailHistory strips[STRIPS];
  int32_t lastStep;
  bool empty = true;
};

struct Anchor {
  float lastCenter = 0.5f;
  float travelCells = 0.0f;
  float swingCycles = 0.0f;
};

struct Wall {
  Tails tails;
  Anchor anchors[STRIPS];
  bool lastBouncing = false;
  float lastBeats = 0.0f;
  float lastQuarterNotes = 0.0f;
};

void clearTails(Wall &wall);

struct Par {
  Rgb color;
  uint8_t value;
};

struct FanReading {
  float values[STRIPS];
  float curve[FAN_CURVE_POINTS];
  float turns;
  float stillAt;
  float spread;
  float speed;
  float lfo;
  float randomize;
};

struct Frame {
  Rgb pixels[STRIPS * PIXELS];
  Par pars[PARS];
  FanReading fan;
  float bend[BEND_POINTS];
  float lfo;
  float stripLfo[STRIPS];
  float parLfo[PARS];
};

void renderFrame(const uint8_t *controls, float quarterNotes, Motion &motion, Wall &wall,
                 Frame &out);

float dialedValue(uint8_t cc, uint8_t value);

Hsv dialedColor(const uint8_t *controls);

}
