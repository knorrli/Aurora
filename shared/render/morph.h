#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace render {

void blendPatches(const uint8_t *from, const uint8_t *to, float position, const uint8_t *switches,
                  bool switchesFromStart, uint8_t *out);

void mixLayers(const uint8_t *base, const uint8_t (*layers)[AURORA_PATCH_CC_COUNT],
               const float *positions, uint8_t *out);

}
