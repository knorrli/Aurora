#pragma once

#include <stdint.h>

// Eight modulation routes, four CCs each: a destination named by its own CC
// number, an amount, a whole-number ratio against the one clock, and a wave.
// A push is a fraction of the distance left to a limit and the sign picks the
// limit, except on controls that wrap, which take a rotation instead. See
// docs/modulation.md.
namespace routes {

void gather(float plainPhase, float stripPhase);

// The dialed byte with the routes applied.
uint8_t value(uint8_t cc);

float pulseWave(float phase, uint8_t wave);

}  // namespace routes
