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
static const uint8_t BEND_POINTS = PIXELS + 1;
static const uint8_t FIELD_ACROSS_POINTS = (STRIPS - 1) * 12 + 1;

static const uint8_t ECHO_ROUTE_FIRST = 2 * AURORA_ROUTES;
static const uint8_t RENDER_ROUTES = ECHO_ROUTE_FIRST + (AURORA_PATCH_LAYERS - 1) * AURORA_ROUTES;
static const uint16_t ROUTE_SHARE_FIRST = AURORA_PATCH_CONTROLS + (RENDER_ROUTES - AURORA_ROUTES) * ROUTE_FIELDS;
static const uint16_t RENDER_CONTROL_COUNT = ROUTE_SHARE_FIRST + RENDER_ROUTES;

static inline uint16_t routeControl(uint8_t route, uint8_t field) {
  if (route < AURORA_ROUTES) return aurora_route_control(route, field);
  return (uint16_t)(AURORA_PATCH_CONTROLS + (route - AURORA_ROUTES) * ROUTE_FIELDS + field);
}

static inline uint8_t echoRoute(uint8_t layer, uint8_t route) {
  return (uint8_t)(ECHO_ROUTE_FIRST + (layer - 1) * AURORA_ROUTES + route);
}

static inline uint16_t routeShare(uint8_t route) { return (uint16_t)(ROUTE_SHARE_FIRST + route); }

static inline bool oneshotRoute(uint8_t route) { return route >= AURORA_ROUTES && route < ECHO_ROUTE_FIRST; }

static inline uint8_t versionOf(uint8_t route) {
  return route < ECHO_ROUTE_FIRST ? route : (uint8_t)((route - ECHO_ROUTE_FIRST) % AURORA_ROUTES);
}

static inline uint8_t roundedControl(float value) { return (uint8_t)(value + 0.5f); }

struct OneshotClock {
  float progress;
  float beats;
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
  float bend[BEND_POINTS];
  float lfo;
  StripMarks cells[STRIPS];
  StripMarks spots[STRIPS];
  float fieldLevels[STRIPS * PIXELS];
  bool curveShown;
  bool curveHorizontal;
  float curve[STRIPS * BEND_POINTS];
  float curveAcross[FIELD_ACROSS_POINTS];
  float fieldAcross[FIELD_ACROSS_POINTS];
};

void renderFrame(const float *controls, float quarterNotes, uint32_t milliseconds,
                 const OneshotClock &oneshot, const PadFinger &pad, float speed, Motion &motion,
                 Wall &wall, Frame &out);

}
