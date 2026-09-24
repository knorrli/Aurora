#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

// Eight modulation routes, four CCs each: a destination named by its own CC
// number, an amount, a whole-number ratio against the one clock, and a wave.
// See docs/modulation.md.
namespace render {

struct Pushes {
  float amount[AURORA_PATCH_CC_COUNT];
};

// Which phase a destination reads is its own property, not the route's, so
// both readings go in and the gather picks per destination.
void gatherRoutes(const uint8_t *dialed, float plainPhase, float stripPhase,
                  Pushes &out);

// The dialed byte with the pushes applied; null pushes is the dialed byte.
uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

// A rate would drift rather than return under a push, and tempo division is
// an index; CC 0 means a route aimed nowhere.
bool routeRefused(uint8_t cc);

// The furthest below and above its dialed value the routes aimed at a control
// can take it, unwrapped for a circular one. False if no route reaches it.
bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high);

float pulseWave(float phase, uint8_t wave);

}  // namespace render
