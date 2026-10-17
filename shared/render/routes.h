#pragma once

#include <stdint.h>

#include "aurora_protocol.h"
#include "render.h"

namespace render {
struct Modulation {
  float amount[AURORA_PATCH_CC_COUNT];
  float swing[AURORA_PATCH_CC_COUNT];
  float shift[AURORA_PATCH_CC_COUNT];
  float bipolar[AURORA_PATCH_CC_COUNT];
  float padSwing[AURORA_PATCH_CC_COUNT];
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
  uint8_t cc;
  float amount;
  Wave wave;
  RouteTiming timing;
};

struct CellRoute {
  uint8_t cc;
  uint8_t route;
  uint8_t source;
  Wave wave;
  bool bipolar;
  float amount;
  float mean;
  float shift;
  RouteTiming timing;
};

void setOneshotClock(const OneshotClock &clock);

RouteTiming routeTiming(const float *dialed, uint8_t route);

float turnsOn(const RouteTiming &timing, float clock, float shift);

float routeTurns(const RouteTiming &timing, float lfo, float shift);

bool spotDestination(uint8_t cc);

bool circular(uint8_t cc);

uint8_t gatherSpotRoutes(const float *dialed, SpotRoute *out);

float landedControl(uint8_t cc, float base, float amount);

void gatherRoutes(const float *dialed, float beatsPerCycle, float lfo, float fanShift,
                  const PadLevels *pad, Modulation &out);

void gatherRoutesBefore(const float *dialed, float beatsPerCycle, float lfo, float fanShift,
                        const PadLevels *pad, float beatsAgo, Modulation &out);

bool cellControl(uint8_t cc);

uint8_t gatherCellRoutes(const float *dialed, float fanShift, CellRoute *out);

float envelopeLevel(uint8_t source, const float *envelopes);

float heldLevel(const CellRoute &route, float lfo, float beatsPerCycle, float beatsAgo);

uint8_t envelopesUsed(const float *dialed);

float cellRouted(uint8_t cc, float dialed, float amount, float bipolar, const CellRoute *routes,
                 uint8_t count, const float *levels);

void carryPadShift(float elapsedBeats, float *carried, Modulation &modulation);

float routed(const float *dialed, const Modulation *modulation, uint8_t cc);

uint8_t displayedByte(uint8_t cc, float value);

uint8_t routedForDisplay(const float *dialed, const Modulation *modulation, uint8_t cc);

bool routeRefused(uint8_t cc);

uint8_t routeTarget(const float *dialed, uint8_t route);

bool routeAims(const float *dialed, uint8_t cc, uint8_t routes);

bool routeReach(const float *dialed, uint8_t cc, int16_t &low, int16_t &high);

float waveRise(uint8_t wave);

Wave waveOf(const float *dialed, uint8_t route);

float lfoWave(float phase, Wave wave);

float waveMean(Wave wave);

}
