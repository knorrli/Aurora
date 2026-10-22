#pragma once

#include <stdint.h>

#include "aurora_protocol.h"
#include "render.h"

namespace render {
struct Modulation {
  float amount[AURORA_PATCH_CONTROLS];
  float swing[AURORA_PATCH_CONTROLS];
  float shift[AURORA_PATCH_CONTROLS];
  float bipolar[AURORA_PATCH_CONTROLS];
  float padSwing[AURORA_PATCH_CONTROLS];
};

struct PadLevels {
  float x;
  float y;
};

struct RouteTiming {
  uint8_t route;
  uint8_t ratio;
  bool once;
  float phase;
};

struct Wave {
  uint8_t shape;
  uint8_t smooth;
};

struct SpotRoute {
  uint8_t control;
  float amount;
  Wave wave;
  RouteTiming timing;
};

struct RouteReading {
  uint8_t destination;
  bool aimsRoute;
  uint8_t aimed;
  bool aimsPhase;
  uint8_t source;
  bool held;
  bool bipolar;
  bool perCell;
  uint8_t pattern;
  float amount;
  float share;
  float mean;
  Wave wave;
  RouteTiming timing;
};

struct Routes {
  RouteReading routes[RENDER_ROUTES];
};

struct CellMember {
  uint8_t note;
  uint8_t chord;
  uint8_t birthIndex;
  uint8_t birthsInCycle;
  int32_t birthNumber;
  float age;
  const float *envelopes;
};

struct RouteContext {
  float lfo;
  float beatsPerCycle;
  float beatsAgo;
  const PadLevels *pad;
  int8_t strip;
  const CellMember *cell;
};

struct Push {
  float amount;
  float bipolar;
};

void setOneshotClock(const OneshotClock &clock);

RouteTiming routeTiming(const float *dialed, uint8_t route);

float turnsOn(const RouteTiming &timing, float clock, float shift);

float routeTurns(const RouteTiming &timing, float lfo, float shift);

bool spotDestination(uint8_t control);

bool circular(uint8_t control);

uint8_t gatherSpotRoutes(const float *dialed, SpotRoute *out);

float landedControl(uint8_t control, float base, float amount);

void gatherRoutes(const float *dialed, float beatsPerCycle, float lfo, int8_t strip,
                  const PadLevels *pad, Modulation &out);

void gatherRoutesBefore(const float *dialed, float beatsPerCycle, float lfo, int8_t strip,
                        const PadLevels *pad, float beatsAgo, Modulation &out);

bool cellControl(uint8_t control);

void readRoutes(const float *dialed, Routes &out);

Push cellPush(const Routes &routes, uint8_t control, const RouteContext &context);

float groupLevel(uint8_t pattern, uint8_t index, uint8_t count, uint32_t salt);

float routeAmountAt(const Routes &routes, uint8_t route, const RouteContext &context);

float routeShiftAt(const Routes &routes, uint8_t route, const RouteContext &context);

uint8_t envelopesUsed(const float *dialed);

float cellRouted(uint8_t control, float dialed, float amount, float bipolar, float chord);

void carryPadShift(float elapsedBeats, float *carried, Modulation &modulation);

float routed(const float *dialed, const Modulation *modulation, uint8_t control);

uint8_t displayedByte(uint8_t control, float value);

uint8_t routedForDisplay(const float *dialed, const Modulation *modulation, uint8_t control);

bool routeRefused(uint8_t control);

uint8_t routeTarget(const float *dialed, uint8_t route);

bool routeArpeggiates(const float *dialed, uint8_t route);

bool stripArpControl(uint8_t control);

bool arpCapable(uint8_t control);

uint32_t routeDraw(uint8_t route);

bool routeAims(const float *dialed, uint8_t control, uint8_t routes);

bool routeReach(const float *dialed, uint8_t control, int16_t &low, int16_t &high);

float waveRise(uint8_t wave);

Wave waveOf(const float *dialed, uint8_t route);

float lfoWave(float phase, Wave wave);

float waveMean(Wave wave);

}
