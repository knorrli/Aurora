#pragma once

#include <stdint.h>

#include "reading.h"

namespace render {

bool fieldActive(const Reading &reading);
bool flowActive(const Reading &reading);
bool lightActive(const Reading &reading);
bool scatterTints(const Reading &reading);
bool scatterActive(const Reading &reading);

float fieldAt(const Field &field, uint8_t stripIndex, float alongPixels, float shapeAcross,
              float drift);

float scatterAt(const Scatter &scatter, uint8_t stripIndex, float alongPixels, float time);

Hsv tintAt(const Reading &reading, uint8_t stripIndex, uint8_t pixelIndex, float field,
           float shape, float scatter, bool flowOn, float flowTime);

}
