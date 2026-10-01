#include "engines.h"

#include <math.h>

#include "render_math.h"

namespace render {

static const float DARK_FLOOR = 0.02f;
static const float GOLDEN_RATIO_CONJUGATE = 0.6180339887f;

static bool pushesColor(float hue, float white, float dark) {
  return fabsf(hue) > 0.5f || fabsf(white) > 0.001f || fabsf(dark) > 0.001f;
}

uint8_t engineOf(uint8_t cc) {
  if (cc >= CC_SHAPE_COUNT && cc <= CC_SHAPE_BEND_AT) return ENGINE_SHAPE;
  if (cc >= CC_CORE_HUE && cc <= CC_CORE_DARK) return ENGINE_SHAPE;
  if (cc >= CC_SCATTER_COUNT && cc <= CC_SCATTER_VALUE) return ENGINE_SCATTER;
  if (cc == CC_SCATTER_MIX) return ENGINE_SCATTER;
  if (cc >= CC_FIELD_FORM && cc <= CC_FIELD_DARK) return ENGINE_FIELD;
  if (cc >= CC_FLOW_DENSITY && cc <= CC_FLOW_DARK) return ENGINE_FLOW;
  return 0;
}

bool showsEngine(uint8_t cc) {
  switch (cc) {
    case CC_SHAPE_WIDTH:
    case CC_SHAPE_EDGE:
    case CC_SCATTER_MIX:
    case CC_FIELD_HUE:
    case CC_FIELD_WHITE:
    case CC_FIELD_DARK:
    case CC_FLOW_HUE:
    case CC_FLOW_WHITE:
    case CC_FLOW_DARK:
      return true;
    default:
      return false;
  }
}

uint8_t hiddenEngines(const uint8_t *dialed) {
  Reading reading;
  readControls(dialed, nullptr, reading);
  uint8_t hidden = 0;
  if (reading.shape.width <= 0.0001f && reading.shape.edge <= 0.0001f) hidden |= ENGINE_SHAPE;
  if (!fieldActive(reading)) hidden |= ENGINE_FIELD;
  if (!flowActive(reading)) hidden |= ENGINE_FLOW;
  if (!scatterActive(reading, dialed, AURORA_ROUTES)) hidden |= ENGINE_SCATTER;
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t target = aurora_route_target(dialed[aurora_route_cc(route, ROUTE_DESTINATION)]);
    const float amount = signedOf(dialed[aurora_route_cc(route, ROUTE_AMOUNT)]);
    if (fabsf(amount) > 0.001f && showsEngine(target)) hidden &= (uint8_t)~engineOf(target);
  }
  return hidden;
}

bool fieldActive(const Reading &reading) {
  return pushesColor(reading.field.hue, reading.field.white, reading.field.dark);
}

bool flowActive(const Reading &reading) {
  return pushesColor(reading.flow.hue, reading.flow.white, reading.flow.dark);
}

bool coreActive(const Reading &reading) {
  return pushesColor(reading.core.hue, reading.core.white, reading.core.dark);
}

bool scatterActive(const Reading &reading, const uint8_t *dialed, uint8_t routes) {
  return reading.scatter.mix > 0.001f || routeAims(dialed, CC_SCATTER_MIX, routes);
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
  return fieldAtPosition(field, positionAlongFieldDirection(field, stripIndex, alongPixels, shapeAcross), drift);
}

float fieldAtPosition(const Field &field, float u, float drift) {
  if (field.form == FIELD_FORM_GRADIENT) {
    const float along = fract((u - 0.5f) * field.count * 0.5f + 0.25f + drift - field.position);
    return along < 0.5f ? 4.0f * along - 1.0f : 3.0f - 4.0f * along;
  }
  const float cell = (u - 0.5f) * field.count + 0.5f + drift - field.position;
  const float bump = bumpAt(fract(cell) - 0.5f, field.width, field.edge);
  return field.form == FIELD_FORM_ALL_BUT_REGION ? 1.0f - bump : bump;
}

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

static const float DRIFT_PULL_PER_CYCLE = 0.5f;
static const float POSITION_REACH_PIXELS = (float)PIXELS * 0.5f;

static float pulledToWholePulse(float drift, float rateSpread, float randomize, float elapsedCycles) {
  drift += rateSpread * randomize * elapsedCycles;
  const float whole = roundf(drift);
  return whole + (drift - whole) * expf(-DRIFT_PULL_PER_CYCLE * (1.0f - randomize) * fabsf(elapsedCycles));
}

static float spotClock(const ScatterClock &clock, float drift, float phaseOffset) {
  return clock.time + drift + clock.randomize * phaseOffset;
}

static float spotWave(const SpotRoute &route, float clock, float before) {
  const float phase = turnsOn(route.timing, clock, 0.0f);
  const float now = lfoWave(phase, route.wave);
  const bool brightens = route.amount > 0.0f
      && (route.cc == CC_SCATTER_MIX || route.cc == CC_SCATTER_VALUE);
  const float earlier = turnsOn(route.timing, before, 0.0f);
  if (!brightens || earlier >= phase) return now;
  if (phase - earlier >= 1.0f || floorf(phase) > floorf(earlier)) return 1.0f;
  return fmaxf(now, lfoWave(earlier, route.wave));
}

static float spotControl(uint8_t cc, const uint8_t *dialed, const SpotRoute *routes,
                         uint8_t routeCount, float clock, float before) {
  float amount = 0.0f;
  for (uint8_t i = 0; i < routeCount; i++) {
    if (routes[i].cc == cc) amount += routes[i].amount * spotWave(routes[i], clock, before);
  }
  return controlValue(cc, landedByte(cc, dialed[cc], amount));
}

void placeScatter(const Scatter &scatter, const uint8_t *dialed, const SpotRoute *routes,
                  uint8_t routeCount, uint8_t stripIndex, const ScatterClock &now,
                  const ScatterClock &before, float *drifts, float elapsedCycles,
                  ScatterSpots &out) {
  out.count = 0;
  out.reach = (float)PIXELS / scatter.count;
  for (uint8_t place = 0; place < MAX_COUNT; place++) {
    const float rateSpread = (float)hash8(stripIndex, place, 17) / 255.0f - 0.5f;
    const float phaseOffset = (float)hash8(stripIndex, place, 43) / 255.0f;
    const float earlier = spotClock(before, drifts[place], phaseOffset);
    drifts[place] = pulledToWholePulse(drifts[place], rateSpread, now.randomize, elapsedCycles);
    const float clock = spotClock(now, drifts[place], phaseOffset);

    const float lit = clampUnit(scatter.count - (float)placeRank(stripIndex, place));
    if (lit <= 0.0f) continue;
    const uint32_t life = (uint32_t)(int32_t)floorf(clock);
    if (lit < 1.0f && (float)hash8(stripIndex, place * 131u + life, 97) >= lit * 256.0f) continue;

    const auto control = [&](uint8_t cc) {
      return spotControl(cc, dialed, routes, routeCount, clock, earlier);
    };
    const float mix = control(CC_SCATTER_MIX);
    if (mix <= 0.001f) continue;

    const float landing = (float)hash8(stripIndex, place * 131u + life, 61) / 255.0f * (float)PIXELS;
    const float beatsSinceLanding = (fabsf(scatter.rate) > 0.0001f) ? fract(clock) / fabsf(scatter.rate) : 0.0f;
    out.centers[out.count] = landing + scatter.speed * beatsSinceLanding + control(CC_SCATTER_POSITION) * POSITION_REACH_PIXELS;
    out.widths[out.count] = fmaxf(control(CC_SCATTER_WIDTH), 1.0f / out.reach);
    out.mixes[out.count] = mix;
    out.hues[out.count] = control(CC_SCATTER_HUE);
    out.saturations[out.count] = control(CC_SCATTER_SATURATION);
    out.values[out.count] = control(CC_SCATTER_VALUE);
    out.lives[out.count] = fract(clock);
    out.count++;
  }
}

ScatterSample scatterAt(const Scatter &scatter, const ScatterSpots &spots, float alongPixels) {
  ScatterSample sample = { 0.0f, 0.0f, 0.0f, 0.0f };
  for (uint8_t i = 0; i < spots.count; i++) {
    const float offset = (alongPixels - spots.centers[i]) / spots.reach;
    if (fabsf(offset) >= 0.5f) continue;
    const float cover = spots.mixes[i] * bumpAt(offset, spots.widths[i], scatter.edge);
    if (cover <= sample.cover) continue;
    sample = { cover, spots.hues[i], spots.saturations[i], spots.values[i] };
  }
  return sample;
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
           float shape, bool flowOn, float flowTime) {
  const float along = (float)pixelIndex / (float)(PIXELS - 1);
  const float flow = flowOn ? flowAt(reading.flow, stripIndex, along, flowTime) : 0.0f;

  const float fieldAway = fabsf(field);
  const float flowAway = (flow > 0.0f) ? flow : 0.0f;
  return pushed(reading.color,
      field * reading.field.hue + flow * reading.flow.hue
          + shape * reading.core.hue,
      fieldAway * reading.field.white + flowAway * reading.flow.white
          + shape * reading.core.white,
      fieldAway * reading.field.dark + flowAway * reading.flow.dark
          + shape * reading.core.dark);
}

}
