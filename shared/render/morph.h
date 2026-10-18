#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace render {

void controlsOf(const uint8_t *bytes, float *out);

void silenceRenderRoutes(float *out);

void usedControls(const uint8_t *bytes, bool *out);

void blendPatches(const float *from, const float *to, float position, const float *switches,
                  bool switchesFromStart, float *out);

void composeOneshot(const float *live, const uint8_t *oneshot, const uint8_t *marks, float strength,
                    float *out);

void morphCorners(const float *live, const uint8_t *const *controls, const uint8_t *const *marks,
                  const float *weights, int8_t switchCorner, float *out);

void mixLayers(const float *live, const uint8_t (*layers)[AURORA_PATCH_CONTROLS],
               const float *positions, float *out);

}
