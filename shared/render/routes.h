#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace render {
struct Pushes {
  float amount[AURORA_PATCH_CC_COUNT];
  float swing[AURORA_PATCH_CC_COUNT];
  float shift[AURORA_PATCH_CC_COUNT];
};

void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float plainPhase,
                  float stripPhase, Pushes &out);

uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

uint8_t routedForDisplay(const uint8_t *dialed, const Pushes *pushes, uint8_t cc);

bool routeRefused(uint8_t cc);

bool routeAims(const uint8_t *dialed, uint8_t cc);

bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high);

float lfoWave(float phase, uint8_t wave);

float waveMean(uint8_t wave);

}
