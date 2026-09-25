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

static float unbend(const BendTable &table, float along) {
  const float at = clampUnit(along) * (float)BEND_STEPS;
  const uint8_t i = (at >= (float)BEND_STEPS) ? BEND_STEPS - 1 : (uint8_t)at;
  const float t = at - (float)i;
  return table.inverse[i] + t * (table.inverse[i + 1] - table.inverse[i]);
}

static float cellAt(float cells, uint8_t count) {
  const float cell = floorf(cells);
  if (cell < 0.0f) return 0.0f;
  if (cell > (float)(count - 1)) return (float)(count - 1);
  return cell;
}

float bentCells(const BendTable *bend, bool perCell, float pixel, float cellLength, uint8_t count) {
  const float cells = pixel / cellLength;
  if (!bend) return cells;
  if (!perCell) return (float)count * unbend(*bend, pixel / (float)PIXELS);
  const float cell = cellAt(cells, count);
  return cell + unbend(*bend, cells - cell);
}

void readBend(const BendTable *bend, bool perCell, float cellLength, uint8_t count, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    if (!bend) {
      out[i] = 1.0f;
      continue;
    }
    const float cells = (float)i / cellLength;
    const float along = perCell ? cells - cellAt(cells, count) : (float)i / (float)PIXELS;
    out[i] = bendSpeed(along, bend->amount, bend->at) * bend->totalTime;
  }
}

}
