#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace render {

void controlsOf(const uint8_t *bytes, float *out);

void blendPatches(const float *from, const float *to, float position, const float *switches,
                  bool switchesFromStart, float *out);

void composeOneshot(const float *live, const uint8_t *oneshot, const uint8_t *marks, float *out);

void mixLayers(const float *live, const uint8_t (*layers)[AURORA_PATCH_CC_COUNT],
               const float *positions, float *out);

}
