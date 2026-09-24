#pragma once

#include <stdint.h>

// Eight modulation routes, four CCs each: a destination named by its own CC
// number, an amount, a whole-number ratio against the one clock, and a wave.
// See docs/modulation.md.
namespace routes {

// Which phase a destination reads is its own property, not the route's, so
// both readings go in and gather() picks per destination.
void gather(float plainPhase, float stripPhase);

// The dialed byte with the routes applied.
uint8_t value(uint8_t cc);

float pulseWave(float phase, uint8_t wave);

}  // namespace routes
