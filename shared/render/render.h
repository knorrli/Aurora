#pragma once

#include <stdint.h>

#include "aurora_protocol.h"
#include "color8.h"

namespace render {

static const uint8_t STRIPS = 5;
static const uint8_t PARS = 4;
static const uint8_t PIXELS = 45;
static const uint8_t MAX_COUNT = 20;
static const uint8_t MAX_CELLS = 64;
static const uint8_t SAMPLES_PER_PIXEL = 8;
static const uint16_t STRIP_SAMPLES = PIXELS * SAMPLES_PER_PIXEL;
static const uint8_t FAN_CURVE_STEPS_PER_STRIP = 24;
static const uint16_t FAN_CURVE_POINTS = (STRIPS - 1) * FAN_CURVE_STEPS_PER_STRIP + 1;
static const uint8_t BEND_POINTS = PIXELS + 1;
static const uint8_t FIELD_ACROSS_POINTS = (STRIPS - 1) * 12 + 1;

static const uint8_t RENDER_ROUTES = 2 * AURORA_ROUTES;
static const uint16_t RENDER_CONTROL_COUNT = AURORA_PATCH_CC_COUNT + AURORA_ROUTES * ROUTE_FIELDS;

static inline uint8_t routeByte(uint8_t route, uint8_t field) {
  if (route < AURORA_ROUTES) return aurora_route_cc(route, field);
  return (uint8_t)(AURORA_PATCH_CC_COUNT + (route - AURORA_ROUTES) * ROUTE_FIELDS + field);
}

static inline bool oneshotRoute(uint8_t route) { return route >= AURORA_ROUTES; }

static inline uint8_t roundedControl(float value) { return (uint8_t)(value + 0.5f); }

struct OneshotClock {
  float progress;
  float beats;
  bool staggered;
};

static const uint8_t TAIL_MAX_BEATS = 8;

struct Hsv {
  float h, s, v;
};

struct Clock {
  float offset;
  float rate;
};

struct ScatterClock {
  float time;
  float randomize;
};

static const uint8_t SWINGING_CONTROLS = 3;

struct PadFinger {
  bool playing = false;
  float x = 0.0f;
  float y = 0.0f;
  bool strips[STRIPS] = {};
  const float *morphed = nullptr;
};

struct Motion {
  Clock cells = {};
  float lastCellBeats = 0.0f;
  Clock lfo = {};
  Clock flow = {};
  Clock field = {};
  Clock scatter = {};
  float spotDrift[STRIPS][MAX_COUNT] = {};
  ScatterClock lastScatter[STRIPS] = {};
  float lastScatterBeats = 0.0f;
  float lastLfoBeats = 0.0f;
  float lastFieldBeats = 0.0f;
  float padShift[STRIPS][SWINGING_CONTROLS] = {};
};

struct Par {
  Rgb color;
  uint8_t value;
};

struct ParHold {
  bool lit = false;
  uint32_t sinceMilliseconds = 0;
  Par brightest = {};
  uint8_t brightestLevel = 0;
};

struct Wall {
  ParHold parHolds[PARS];
  bool restarting = false;
  float cellsStart = 0.0f;
  float lastBeats = 0.0f;
};

void restartCells(Wall &wall);

struct FanReading {
  float values[STRIPS];
  float curve[FAN_CURVE_POINTS];
  float turns;
  float spread;
  float lfo;
  float randomize;
};

struct Mark {
  float center;
  float width;
  float life;
};

struct StripMarks {
  uint8_t count;
  Mark marks[MAX_CELLS];
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
  float stripFanShift[STRIPS];
  StripMarks cells[STRIPS];
  StripMarks spots[STRIPS];
  float fieldLevels[STRIPS * PIXELS];
  bool curveShown;
  bool curveAcrossWall;
  float curve[STRIPS * BEND_POINTS];
  float curveAcross[FIELD_ACROSS_POINTS];
  float fieldAcross[FIELD_ACROSS_POINTS];
};

void renderFrame(const float *controls, float quarterNotes, uint32_t milliseconds,
                 const OneshotClock &oneshot, const PadFinger &pad, float speed, Motion &motion,
                 Wall &wall, Frame &out);

}
