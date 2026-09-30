#pragma once

#include <stdint.h>

#include "color8.h"

namespace render {

static const uint8_t STRIPS = 5;
static const uint8_t PARS = 4;
static const uint8_t PIXELS = 45;
static const uint8_t MAX_COUNT = 20;
static const uint8_t FAN_CURVE_STEPS_PER_STRIP = 24;
static const uint16_t FAN_CURVE_POINTS = (STRIPS - 1) * FAN_CURVE_STEPS_PER_STRIP + 1;
static const uint8_t BEND_POINTS = PIXELS + 1;
static const uint8_t FIELD_ACROSS_POINTS = (STRIPS - 1) * 12 + 1;

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

struct ScatterClock {
  float time;
  float randomize;
};

struct Motion {
  Clock travel[STRIPS] = {};
  Clock swing[STRIPS] = {};
  Clock lfo = {};
  Clock flow = {};
  Clock field = {};
  Clock scatter = {};
  float spotDrift[STRIPS][MAX_COUNT] = {};
  ScatterClock lastScatter[STRIPS] = {};
  float lastScatterBeats = 0.0f;
  float lastLfoBeats = 0.0f;
  float lastFieldBeats = 0.0f;
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

struct SpotMark {
  float center;
  float width;
  float life;
};

struct StripSpots {
  uint8_t count;
  SpotMark marks[MAX_COUNT];
};

struct Frame {
  Rgb pixels[STRIPS * PIXELS];
  Par pars[PARS];
  float parHuePlaces[PARS];
  uint8_t stripsHue;
  uint8_t parHues[PARS];
  FanReading fan;
  float bend[BEND_POINTS];
  float lfo;
  float stripLfo[STRIPS];
  float centers[STRIPS];
  StripSpots spots[STRIPS];
  float fieldLevels[STRIPS * PIXELS];
  float fieldAcross[FIELD_ACROSS_POINTS];
};

void renderFrame(const uint8_t *controls, float quarterNotes, Motion &motion, Wall &wall,
                 Frame &out);

Hsv dialedColor(const uint8_t *controls);

}
