#pragma once

#include <stdint.h>

#include "reading.h"

namespace render {

enum Engine : uint8_t {
  ENGINE_SHAPE = 1,
  ENGINE_FIELD = 2,
  ENGINE_FLOW = 4,
  ENGINE_SCATTER = 8,
};

uint8_t engineOf(uint8_t cc);
bool showsEngine(uint8_t cc);
uint8_t hiddenEngines(const uint8_t *dialed);

bool fieldActive(const Reading &reading);
bool flowActive(const Reading &reading);
bool coreActive(const Reading &reading);
bool scatterActive(const Reading &reading, const uint8_t *dialed);

float fieldAtPosition(const Field &field, float u, float drift);
float fieldAt(const Field &field, uint8_t stripIndex, float alongPixels, float shapeAcross,
              float drift);

struct ScatterSpots {
  uint8_t count;
  float reach;
  float centers[MAX_COUNT];
  float widths[MAX_COUNT];
  float mixes[MAX_COUNT];
  float hues[MAX_COUNT];
  float saturations[MAX_COUNT];
  float values[MAX_COUNT];
  float lives[MAX_COUNT];
};

struct ScatterSample {
  float cover;
  float hue;
  float saturation;
  float value;
};

void placeScatter(const Scatter &scatter, const uint8_t *dialed, const SpotRoute *routes,
                  uint8_t routeCount, uint8_t stripIndex, const ScatterClock &now,
                  const ScatterClock &before, float *drifts, float elapsedCycles,
                  ScatterSpots &out);
ScatterSample scatterAt(const Scatter &scatter, const ScatterSpots &spots, float alongPixels);

Hsv tintAt(const Reading &reading, uint8_t stripIndex, uint8_t pixelIndex, float field,
           float shape, bool flowOn, float flowTime);

}
