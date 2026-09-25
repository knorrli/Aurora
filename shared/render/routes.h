#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

// Eight modulation routes, five CCs each: a destination named by its own CC
// number, an amount, a whole-number ratio against the LFO, a wave, and
// how far into its cycle the wave is delayed. See docs/modulation.md.
namespace render {

// A rate takes no push. Its swing arrives as how far it stands from its
// dialed value right now, in its own units per beat, and as how far that has
// carried what it moves, in its own units: the second is what the renderer
// adds, and it returns to zero once a route cycle.
struct Pushes {
  float amount[AURORA_PATCH_CC_COUNT];
  float swing[AURORA_PATCH_CC_COUNT];
  float shift[AURORA_PATCH_CC_COUNT];
};

// Which phase a destination reads is its own property, not the route's, so
// both readings go in and the gather picks per destination.
void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float plainPhase,
                  float stripPhase, Pushes &out);

// The dialed byte with the pushes applied; null pushes is the dialed byte. A
// rate reads as dialed: its swing is carried by the shift.
uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

// Where a control stands this instant, for drawing: a rate at the byte
// nearest its swung value, anything else as routed().
uint8_t routedForDisplay(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

// The LFO's own rate, because every route reads it, and tempo division,
// because it is an index; CC 0 means a route aimed nowhere.
bool routeRefused(uint8_t cc);

// Whether any route with an amount is aimed at a control.
bool routeAims(const uint8_t *dialed, uint8_t cc);

// The furthest below and above its dialed value the routes aimed at a control
// can take it, unwrapped for a circular one and held to 0-127 for a rate.
// False if no route reaches it.
bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high);

float lfoWave(float phase, uint8_t wave);

// A wave's average over one cycle: what a rate's swing takes off so it adds
// nothing over a cycle.
float waveMean(uint8_t wave);

}  // namespace render
