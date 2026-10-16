#include "bend.h"

#include <math.h>

#include "render.h"
#include "render_math.h"

namespace render {

static const float BEND_MAX = 0.95f;
static const uint8_t BEND_STEPS = 64;
static const uint8_t SLICES_PER_BEND_STEP = 4;

struct BendTable {
  float amount = 0.0f;
  float at = 0.0f;
  bool ready = false;
  float inverse[BEND_STEPS + 1];
  float totalTime;
};

static float bendSpeed(float along, float amount, float at) {
  const float reach = fmaxf(at, 1.0f - at);
  return 1.0f + amount * BEND_MAX * cosf(0.5f * TURN * fabsf(along - at) / reach);
}

float bendSpeedRatio(float amount) {
  const float reach = fabsf(amount) * BEND_MAX;
  return (1.0f + reach) / (1.0f - reach);
}

const BendTable *bendFor(float amount, float at) {
  if (fabsf(amount) <= 0.001f) return nullptr;
  static BendTable table;
  if (table.ready && table.amount == amount && table.at == at) return &table;
  table.amount = amount;
  table.at = at;
  table.ready = true;

  const float step = 1.0f / (float)(BEND_STEPS * SLICES_PER_BEND_STEP);
  float elapsed = 0.0f;
  table.inverse[0] = 0.0f;
  for (uint8_t i = 0; i < BEND_STEPS; i++) {
    for (uint8_t j = 0; j < SLICES_PER_BEND_STEP; j++) {
      const float along = ((float)(i * SLICES_PER_BEND_STEP + j) + 0.5f) * step;
      elapsed += step / bendSpeed(along, amount, at);
    }
    table.inverse[i + 1] = elapsed;
  }
  for (uint8_t i = 1; i <= BEND_STEPS; i++) table.inverse[i] /= elapsed;
  table.totalTime = elapsed;
  return &table;
}

float unbentAlong(const BendTable *bend, float along) {
  if (!bend) return along;
  const float at = clampUnit(along) * (float)BEND_STEPS;
  const uint8_t i = (at >= (float)BEND_STEPS) ? BEND_STEPS - 1 : (uint8_t)at;
  const float t = at - (float)i;
  return bend->inverse[i] + t * (bend->inverse[i + 1] - bend->inverse[i]);
}

float bentAlong(const BendTable *bend, float along) {
  if (!bend) return along;
  const float wanted = clampUnit(along);
  uint8_t i = 0;
  while (i < BEND_STEPS - 1 && bend->inverse[i + 1] < wanted) i++;
  const float span = bend->inverse[i + 1] - bend->inverse[i];
  const float t = (span > 0.0f) ? (wanted - bend->inverse[i]) / span : 0.0f;
  return ((float)i + clampUnit(t)) / (float)BEND_STEPS;
}

void readBend(const BendTable *bend, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    out[i] = bend ? bendSpeed((float)i / (float)PIXELS, bend->amount, bend->at) * bend->totalTime : 1.0f;
  }
}

}
