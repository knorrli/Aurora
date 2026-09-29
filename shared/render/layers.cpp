#include "layers.h"

#include <math.h>

#include "render_math.h"

namespace render {

static const float DARK_FLOOR = 0.02f;
static const float GOLDEN_RATIO_CONJUGATE = 0.6180339887f;

static bool pushesColor(float hue, float white, float dark) {
  return fabsf(hue) > 0.5f || fabsf(white) > 0.001f || fabsf(dark) > 0.001f;
}

bool fieldActive(const Reading &reading) {
  return pushesColor(reading.field.hue, reading.field.white, reading.field.dark);
}

bool flowActive(const Reading &reading) {
  return pushesColor(reading.flow.hue, reading.flow.white, reading.flow.dark);
}

bool lightActive(const Reading &reading) {
  return pushesColor(reading.light.hue, reading.light.white, reading.light.dark);
}

bool scatterTints(const Reading &reading) {
  return fabsf(reading.scatter.hue) > 0.5f || fabsf(reading.scatter.white) > 0.001f;
}

bool scatterActive(const Reading &reading) {
  return fabsf(reading.scatter.value) > 0.001f || scatterTints(reading);
}

static float flowAt(const Flow &flow, uint8_t stripIndex, float along, float time) {
  const float acrossFromCenter =
      ((float)stripIndex - (float)(STRIPS - 1) * 0.5f) / (float)(STRIPS - 1);
  const float cyclesAlong = flow.density;
  float cyclesAcross = cyclesAlong * 0.3f;
  if (cyclesAcross > 1.4f) cyclesAcross = 1.4f;

  const float first = sinf(TURN
      * (cyclesAlong * along + cyclesAcross * acrossFromCenter + time));
  const float second = sinf(TURN
      * (cyclesAlong * GOLDEN_RATIO_CONJUGATE * along - cyclesAcross * 1.37f * acrossFromCenter
         + time * GOLDEN_RATIO_CONJUGATE));
  return (first + second) * 0.5f;
}

static float positionAlongFieldDirection(const Field &field, uint8_t stripIndex, float alongPixels,
                        float shapeAcross) {
  if (field.direction == FIELD_DIRECTION_HORIZONTAL) return (float)stripIndex / (float)(STRIPS - 1);
  if (field.direction == FIELD_DIRECTION_SHAPE) return shapeAcross;
  return alongPixels / (float)(PIXELS - 1);
}

float fieldAt(const Field &field, uint8_t stripIndex, float alongPixels, float shapeAcross,
              float drift) {
  const float u = positionAlongFieldDirection(field, stripIndex, alongPixels, shapeAcross);
  if (field.form == FIELD_FORM_GRADIENT) return (u - 0.5f) * 2.0f;
  const float cell = u * (float)field.count + drift;
  const float bump = bumpAt(fract(cell) - 0.5f, field.width, field.edge);
  return field.form == FIELD_FORM_ALL_BUT_REGION ? 1.0f - bump : bump;
}

static const float PLACE_JITTER = 0.7f;

static uint8_t placeRank(uint8_t stripIndex, uint8_t place) {
  static uint8_t ranks[STRIPS][MAX_COUNT];
  static bool ranked = false;
  if (!ranked) {
    for (uint8_t strip = 0; strip < STRIPS; strip++) {
      for (uint8_t k = 0; k < MAX_COUNT; k++) {
        const uint8_t order = hash8(strip, k, 7);
        uint8_t rank = 0;
        for (uint8_t j = 0; j < MAX_COUNT; j++) {
          const uint8_t other = hash8(strip, j, 7);
          if (other < order || (other == order && j < k)) rank++;
        }
        ranks[strip][k] = rank;
      }
    }
    ranked = true;
  }
  return ranks[stripIndex][place];
}

void placeScatter(const Scatter &scatter, uint8_t stripIndex, float time, float spreadTime,
                  ScatterSpots &out) {
  out.count = 0;
  out.reach = (float)PIXELS / scatter.count;
  for (uint8_t place = 0; place < MAX_COUNT; place++) {
    const float lit = clampUnit(scatter.count - (float)placeRank(stripIndex, place));
    if (lit <= 0.0f) continue;

    const float rateSpread = (float)hash8(stripIndex, place, 17) / 255.0f - 0.5f;
    const float phaseOffset = (float)hash8(stripIndex, place, 43) / 255.0f;
    const float clock = time + rateSpread * spreadTime + scatter.randomize * phaseOffset;
    const float age = fract(clock);
    const float alive = bumpAt(age - 0.5f, scatter.width, scatter.edge);
    if (alive <= 0.0001f) continue;

    const uint32_t life = (uint32_t)(int32_t)floorf(clock);
    if (lit < 1.0f && (float)hash8(stripIndex, place * 131u + life, 97) >= lit * 256.0f) continue;
    const float jitter = ((float)hash8(stripIndex, place, 29) / 255.0f - 0.5f) * PLACE_JITTER;
    const float home = ((float)place + 0.5f + jitter) / (float)MAX_COUNT * (float)PIXELS;
    const float landing = scatter.spread * out.reach
        * ((float)hash8(stripIndex, place * 131u + life, 61) / 255.0f * 2.0f - 1.0f);
    out.centers[out.count] = home + landing + scatter.slide * out.reach * (age - 0.5f);
    out.levels[out.count] = alive;
    out.count++;
  }
}

float scatterAt(const Scatter &scatter, const ScatterSpots &spots, float alongPixels) {
  float level = 0.0f;
  for (uint8_t i = 0; i < spots.count; i++) {
    const float offset = (alongPixels - spots.centers[i]) / spots.reach;
    if (fabsf(offset) >= 0.5f) continue;
    level = fmaxf(level, spots.levels[i] * bumpAt(offset, scatter.width, scatter.edge));
  }
  return level;
}

static float valueLeftAfterDark(float dark) { return powf(DARK_FLOOR, dark); }

static Hsv pushed(Hsv base, float hue, float white, float dark) {
  if (white > 1.0f) white = 1.0f;
  if (dark > 1.0f) dark = 1.0f;

  const float saturation = (float)base.s * (1.0f - white);
  const float value = (float)base.v * valueLeftAfterDark(dark);

  return { (uint8_t)(base.h + lroundf(hue)), (uint8_t)saturation, (uint8_t)value };
}

Hsv tintAt(const Reading &reading, uint8_t stripIndex, uint8_t pixelIndex, float field,
           float shape, float scatter, bool flowOn, float flowTime) {
  const float along = (float)pixelIndex / (float)(PIXELS - 1);
  const float flow = flowOn ? flowAt(reading.flow, stripIndex, along, flowTime) : 0.0f;

  const float fieldAway = fabsf(field);
  const float flowAway = (flow > 0.0f) ? flow : 0.0f;
  return pushed(reading.color,
      field * reading.field.hue + flow * reading.flow.hue
          + shape * reading.light.hue + scatter * reading.scatter.hue,
      fieldAway * reading.field.white + flowAway * reading.flow.white
          + shape * reading.light.white + scatter * reading.scatter.white,
      fieldAway * reading.field.dark + flowAway * reading.flow.dark
          + shape * reading.light.dark);
}

}
