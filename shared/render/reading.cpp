#include "reading.h"

#include <math.h>

namespace render {


static const float FAN_FREQUENCY_STEPS = 16.0f;
static const float FAN_MAX_CYCLES_PER_STRIP = 0.5f;

static const float FIELD_MAX_HUE = 128.0f;
static const float FIELD_MIN_COUNT = 0.5f;
static const float FLOW_MAX_HUE = 128.0f;
static const float SCATTER_MAX_HUE = 128.0f;
static const float CORE_MAX_HUE = 64.0f;

static const float FLOW_BEATS_PER_CYCLE[] = { 0.0f, 16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f };
static const uint8_t FLOW_RATE_STEPS = sizeof(FLOW_BEATS_PER_CYCLE) / sizeof(FLOW_BEATS_PER_CYCLE[0]);

static const float TAIL_BEATS[] = { 0.0f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, (float)TAIL_MAX_BEATS };
static const uint8_t TAIL_BEAT_STEPS = sizeof(TAIL_BEATS) / sizeof(TAIL_BEATS[0]);

static float flowCyclesPerBeatOf(float value) {
  const uint8_t last = FLOW_RATE_STEPS - 1;
  const uint8_t step = (uint8_t)(((uint16_t)roundedControl(value) * last + 63) / 127);
  const float beats = FLOW_BEATS_PER_CYCLE[step > last ? last : step];
  return beats > 0.0f ? 1.0f / beats : 0.0f;
}

static float tailBeatsOf(float value) {
  const uint8_t last = TAIL_BEAT_STEPS - 1;
  const uint8_t step = (uint8_t)(((uint16_t)roundedControl(value) * last + 63) / 127);
  return TAIL_BEATS[step > last ? last : step];
}

static inline float unitOf(float value) { return value / 127.0f; }

float signedOf(float value) {
  return value < 64.0f ? (value - 64.0f) / 64.0f
                       : (value - 64.0f) / 63.0f;
}

static inline float levelOf(float value) { return value * 255.0f / 127.0f; }

static inline float hueOf(float value) { return value * 256.0f / 127.0f; }

static float smoothCountOf(float value) {
  return powf((float)MAX_COUNT, unitOf(value));
}

static float fieldCountOf(float value) {
  return FIELD_MIN_COUNT * powf((float)MAX_COUNT / FIELD_MIN_COUNT, unitOf(value));
}

static float squaredRate(float value, float max) {
  const float x = fmaxf((value - 64.0f) / 63.0f, -1.0f);
  return (x < 0.0f ? -1.0f : 1.0f) * x * x * max;
}

static float lapsPerBeatOf(float value) {
  const long step = lroundf(signedOf(value) * (float)AURORA_LFO_PERIOD_COUNT);
  if (step == 0) return 0.0f;
  return (step < 0 ? -1.0f : 1.0f) / AURORA_LFO_PERIODS[(step < 0 ? -step : step) - 1];
}

float lapPixels(const Shape &shape) {
  const float cellLength = (float)PIXELS / shape.count;
  return shape.bounce ? 2.0f * (1.0f - shape.width) * cellLength : cellLength;
}

static float fanFrequencyOf(float value) {
  const long step = lroundf(value * FAN_FREQUENCY_STEPS / 127.0f);
  return (float)step * (FAN_MAX_CYCLES_PER_STRIP / FAN_FREQUENCY_STEPS);
}

float controlValue(uint8_t cc, float value) {
  switch (cc) {
    case CC_HUE: return hueOf(value);
    case CC_SATURATION:
    case CC_VALUE:
    case CC_PAR_VALUE:
    case CC_PAR_HUE_OFFSET:
    case CC_PAR_SATURATION: return levelOf(value);
    case CC_PAR_HUE_RANGE: return signedOf(value) * 128.0f;

    case CC_SHAPE_COUNT:
    case CC_SCATTER_COUNT: return smoothCountOf(value);
    case CC_FIELD_COUNT: return fieldCountOf(value);

    case CC_SHAPE_BEND:
    case CC_ARP_SPREAD: return signedOf(value);
    case CC_SHAPE_BEND_AT:
    case CC_SHAPE_POSITION: return 0.5f + 0.5f * signedOf(value);
    case CC_FAN_SPREAD:
    case CC_FAN_LFO: return signedOf(value) * 0.5f;
    case CC_SHAPE_SPEED: return lapsPerBeatOf(value);
    case CC_SCATTER_SPEED:
    case CC_FAN_SPEED: return squaredRate(value, MAX_SPEED_PIXELS_PER_BEAT);
    case CC_FAN_FREQUENCY: return fanFrequencyOf(value);
    case CC_FAN_PHASE: return value / 128.0f;
    case CC_LFO_RATE: return aurora_lfo_period(roundedControl(value));

    case CC_FIELD_HUE: return signedOf(value) * FIELD_MAX_HUE;
    case CC_FIELD_SPEED: return lapsPerBeatOf(value);
    case CC_FIELD_POSITION: return signedOf(value) * 0.5f;
    case CC_FLOW_HUE: return signedOf(value) * FLOW_MAX_HUE;
    case CC_FLOW_RATE: return flowCyclesPerBeatOf(value);
    case CC_FLOW_DENSITY: return 0.12f * powf(180.0f, unitOf(value));
    case CC_CORE_HUE: return signedOf(value) * CORE_MAX_HUE;
    case CC_SCATTER_RATE: return 1.0f / aurora_lfo_period(roundedControl(value));
    case CC_SCATTER_HUE: return signedOf(value) * SCATTER_MAX_HUE;

    case CC_SCATTER_POSITION: return signedOf(value);

    case CC_SHAPE_TAIL: return tailBeatsOf(value);

    case CC_SHAPE_WIDTH:
    case CC_SHAPE_EDGE:
    case CC_FAN_RANDOMIZE:
    case CC_FIELD_WIDTH:
    case CC_FIELD_EDGE:
    case CC_FIELD_WHITE:
    case CC_FIELD_DARK:
    case CC_FLOW_WHITE:
    case CC_FLOW_DARK:
    case CC_CORE_WHITE:
    case CC_CORE_DARK:
    case CC_SCATTER_SATURATION:
    case CC_SCATTER_VALUE:
    case CC_SCATTER_MIX:
    case CC_SCATTER_WIDTH:
    case CC_SCATTER_EDGE:
    case CC_SCATTER_RANDOMIZE: return unitOf(value);

    default: return value;
  }
}

void readControls(const float *dialed, const Modulation *modulation, Reading &out) {
  auto at = [&](uint8_t cc) { return controlValue(cc, routed(dialed, modulation, cc)); };

  Shape &shape = out.shape;
  shape.width = at(CC_SHAPE_WIDTH);
  shape.edge = at(CC_SHAPE_EDGE);
  shape.tailBeats = at(CC_SHAPE_TAIL);
  shape.count = at(CC_SHAPE_COUNT);
  shape.position = at(CC_SHAPE_POSITION);
  shape.lapsPerBeat = at(CC_SHAPE_SPEED);
  shape.bend = at(CC_SHAPE_BEND);
  shape.bendAt = at(CC_SHAPE_BEND_AT);
  shape.bounce = aurora_switch_is_on(roundedControl(dialed[CC_SHAPE_BOUNCE]));

  Fan &fan = out.fan;
  fan.spread = at(CC_FAN_SPREAD);
  fan.lfo = at(CC_FAN_LFO);
  fan.speedPixels = at(CC_FAN_SPEED);
  fan.frequency = at(CC_FAN_FREQUENCY);
  fan.phase = at(CC_FAN_PHASE);
  fan.randomize = at(CC_FAN_RANDOMIZE);

  out.lfoBeats = at(CC_LFO_RATE);

  Field &field = out.field;
  field.form = aurora_three_way_position(roundedControl(dialed[CC_FIELD_FORM]));
  field.direction = aurora_three_way_position(roundedControl(dialed[CC_FIELD_DIRECTION]));
  field.hue = at(CC_FIELD_HUE);
  field.white = at(CC_FIELD_WHITE);
  field.dark = at(CC_FIELD_DARK);
  field.width = at(CC_FIELD_WIDTH);
  field.edge = at(CC_FIELD_EDGE);
  field.count = at(CC_FIELD_COUNT);
  field.cellsPerBeat = at(CC_FIELD_SPEED);
  field.position = at(CC_FIELD_POSITION);

  Flow &flow = out.flow;
  flow.hue = at(CC_FLOW_HUE);
  flow.white = at(CC_FLOW_WHITE);
  flow.dark = at(CC_FLOW_DARK);
  flow.cyclesPerBeat = at(CC_FLOW_RATE);
  flow.density = at(CC_FLOW_DENSITY);

  Core &core = out.core;
  core.hue = at(CC_CORE_HUE);
  core.white = at(CC_CORE_WHITE);
  core.dark = at(CC_CORE_DARK);

  Scatter &scatter = out.scatter;
  scatter.rate = at(CC_SCATTER_RATE);
  scatter.count = at(CC_SCATTER_COUNT);
  scatter.width = at(CC_SCATTER_WIDTH);
  scatter.edge = at(CC_SCATTER_EDGE);
  scatter.randomize = at(CC_SCATTER_RANDOMIZE);
  scatter.speed = at(CC_SCATTER_SPEED);
  scatter.mix = at(CC_SCATTER_MIX);

  out.color = routedColor(dialed, modulation);
}

Hsv routedColor(const float *dialed, const Modulation *modulation) {
  auto at = [&](uint8_t cc) { return controlValue(cc, routed(dialed, modulation, cc)); };
  return { at(CC_HUE), at(CC_SATURATION), at(CC_VALUE) };
}

void hurry(Reading &reading, float speed) {
  reading.lfoBeats /= speed;
  reading.shape.lapsPerBeat *= speed;
  reading.shape.tailBeats /= speed;
  reading.fan.speedPixels *= speed;
  reading.field.cellsPerBeat *= speed;
  reading.flow.cyclesPerBeat *= speed;
  reading.scatter.rate *= speed;
  reading.scatter.speed *= speed;
}

}
