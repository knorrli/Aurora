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

static float bendAlong(const BendTable &table, float time) {
  const float wanted = clampUnit(time);
  uint8_t i = 0;
  while (i < BEND_STEPS - 1 && table.inverse[i + 1] < wanted) i++;
  const float span = table.inverse[i + 1] - table.inverse[i];
  const float t = (span > 0.0f) ? (wanted - table.inverse[i]) / span : 0.0f;
  return ((float)i + clampUnit(t)) / (float)BEND_STEPS;
}

float cellsAtStart(float count) { return 0.5f - 0.5f * count; }

static float cellAt(float cells, float count) {
  const float cell = floorf(cells);
  const float first = floorf(cellsAtStart(count));
  const float last = ceilf(cellsAtStart(count) + count) - 1.0f;
  if (cell < first) return first;
  if (cell > last) return last;
  return cell;
}

float bentCells(const BendTable *bend, bool perCell, float pixel, float cellLength, float count) {
  const float cells = cellsAtStart(count) + pixel / cellLength;
  if (!bend) return cells;
  if (!perCell) return cellsAtStart(count) + count * unbend(*bend, pixel / (float)PIXELS);
  const float cell = cellAt(cells, count);
  return cell + unbend(*bend, cells - cell);
}

float pixelOfCells(const BendTable *bend, bool perCell, float cells, float cellLength, float count) {
  const float fromStart = cells - cellsAtStart(count);
  if (!bend) return fromStart * cellLength;
  if (!perCell) return (float)PIXELS * bendAlong(*bend, fromStart / count);
  const float cell = cellAt(cells, count);
  return (cell - cellsAtStart(count) + bendAlong(*bend, cells - cell)) * cellLength;
}

void readBend(const BendTable *bend, bool perCell, float cellLength, float count, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    if (!bend) {
      out[i] = 1.0f;
      continue;
    }
    const float cells = cellsAtStart(count) + (float)i / cellLength;
    const float along = perCell ? cells - cellAt(cells, count) : (float)i / (float)PIXELS;
    out[i] = bendSpeed(along, bend->amount, bend->at) * bend->totalTime;
  }
}

}
