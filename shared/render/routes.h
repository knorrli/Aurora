#pragma once

#include <stdint.h>

#include "aurora_protocol.h"
#include "render.h"

namespace render {
struct Pushes {
  float amount[AURORA_PATCH_CC_COUNT];
  float swing[AURORA_PATCH_CC_COUNT];
  float shift[AURORA_PATCH_CC_COUNT];
  float bipolar[AURORA_PATCH_CC_COUNT];
};

struct RouteTiming {
  uint8_t route;
  uint8_t ratio;
  bool once;
  float delay;
};

struct SpotRoute {
  uint8_t cc;
  float amount;
  uint8_t wave;
  RouteTiming timing;
};

void setOneshotClock(float progress, float beats);

RouteTiming routeTiming(const uint8_t *dialed, uint8_t route);

float turnsOn(const RouteTiming &timing, float clock, float shift);

float routeTurns(const RouteTiming &timing, float lfo, float shift);

bool spotDestination(uint8_t cc);

bool circular(uint8_t cc);

uint8_t gatherSpotRoutes(const uint8_t *dialed, SpotRoute *out);

uint8_t landedByte(uint8_t cc, uint8_t base, float amount);

void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float lfo, float fanShift,
                  Pushes &out);

uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

uint8_t routedForDisplay(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

bool routeRefused(uint8_t cc);

uint8_t routeTarget(const uint8_t *dialed, uint8_t route);

bool routeAims(const uint8_t *dialed, uint8_t cc, uint8_t routes);

bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high);

float waveRise(uint8_t wave);

float lfoWave(float phase, uint8_t wave);

float waveMean(uint8_t wave);

}
