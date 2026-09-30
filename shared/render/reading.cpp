#include "reading.h"

#include <math.h>

namespace render {


static const float FAN_FREQUENCY_STEPS = 16.0f;
static const float FAN_MAX_CYCLES_PER_STRIP = 0.5f;

static const float FIELD_MAX_HUE = 128.0f;
static const float FLOW_MAX_HUE = 128.0f;
static const float SCATTER_MAX_HUE = 128.0f;
static const float LIGHT_MAX_HUE = 64.0f;

static const float FLOW_MAX_CYCLES_PER_BEAT = 0.5f;

static inline float unitOf(uint8_t value) { return (float)value / 127.0f; }

float bipolarOf(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

static inline uint8_t byteOf(uint8_t value) {
  return (uint8_t)(((uint16_t)value * 255 + 63) / 127);
}

static inline uint8_t hueOf(uint8_t value) {
  return (uint8_t)(((uint16_t)value * 256 + 63) / 127);
}

static float smoothCountOf(uint8_t value) {
  return powf((float)MAX_COUNT, unitOf(value));
}

static uint8_t countOf(uint8_t value) {
  const long count = lroundf(powf((float)MAX_COUNT, unitOf(value)));
  return (uint8_t)(count < 1 ? 1 : count > MAX_COUNT ? MAX_COUNT : count);
}

static float squaredRate(uint8_t value, float max) {
  const float x = fmaxf(((float)value - 64.0f) / 63.0f, -1.0f);
  return (x < 0.0f ? -1.0f : 1.0f) * x * x * max;
}

static inline float squaredUnit(uint8_t value, float max) {
  const float x = unitOf(value);
  return x * x * max;
}

static float lapsPerBeatOf(uint8_t value) {
  const long step = lroundf(bipolarOf(value) * (float)AURORA_LFO_PERIOD_COUNT);
  if (step == 0) return 0.0f;
  return (step < 0 ? -1.0f : 1.0f) / AURORA_LFO_PERIODS[(step < 0 ? -step : step) - 1];
}

float lapPixels(const Shape &shape) {
  const float cellLength = (float)PIXELS / (float)shape.count;
  return shape.bounce ? 2.0f * (1.0f - shape.width) * cellLength : cellLength;
}

static float fanFrequencyOf(uint8_t value) {
  const long step = lroundf((float)value * FAN_FREQUENCY_STEPS / 127.0f);
  return (float)step * (FAN_MAX_CYCLES_PER_STRIP / FAN_FREQUENCY_STEPS);
}

float controlValue(uint8_t cc, uint8_t value) {
  switch (cc) {
    case CC_HUE: return hueOf(value);
    case CC_SATURATION:
    case CC_VALUE:
    case CC_PAR_VALUE:
    case CC_PAR_HUE_OFFSET:
    case CC_PAR_SATURATION: return byteOf(value);
    case CC_PAR_HUE_RANGE: return bipolarOf(value) * 128.0f;

    case CC_SHAPE_COUNT:
    case CC_FIELD_COUNT: return countOf(value);
    case CC_SCATTER_COUNT: return smoothCountOf(value);

    case CC_SHAPE_BEND:
    case CC_ARP_SPREAD: return bipolarOf(value);
    case CC_SHAPE_BEND_AT:
    case CC_SHAPE_POSITION: return 0.5f + 0.5f * bipolarOf(value);
    case CC_FAN_SPREAD:
    case CC_FAN_LFO: return bipolarOf(value) * 0.5f;
    case CC_SHAPE_SPEED: return lapsPerBeatOf(value);
    case CC_SCATTER_SPEED:
    case CC_FAN_SPEED: return squaredRate(value, MAX_SPEED_PIXELS_PER_BEAT);
    case CC_FAN_FREQUENCY: return fanFrequencyOf(value);
    case CC_FAN_PHASE: return (float)value / 128.0f;
    case CC_LFO_RATE: return aurora_lfo_period(value);

    case CC_FIELD_HUE: return bipolarOf(value) * FIELD_MAX_HUE;
    case CC_FIELD_SPEED: return lapsPerBeatOf(value);
    case CC_FLOW_HUE: return bipolarOf(value) * FLOW_MAX_HUE;
    case CC_FLOW_RATE: return squaredUnit(value, FLOW_MAX_CYCLES_PER_BEAT);
    case CC_FLOW_DENSITY: return 0.12f * powf(180.0f, unitOf(value));
    case CC_LIGHT_HUE: return bipolarOf(value) * LIGHT_MAX_HUE;
    case CC_SCATTER_RATE: return 1.0f / aurora_lfo_period(value);
    case CC_SCATTER_HUE: return bipolarOf(value) * SCATTER_MAX_HUE;

    case CC_SCATTER_POSITION: return bipolarOf(value);

    case CC_SHAPE_TAIL: return squaredUnit(value, (float)TAIL_MAX_BEATS);

    case CC_SHAPE_WIDTH:
    case CC_SHAPE_EDGE:
    case CC_FAN_RANDOMIZE:
    case CC_FIELD_WIDTH:
    case CC_FIELD_EDGE:
    case CC_FIELD_WHITE:
    case CC_FIELD_DARK:
    case CC_FLOW_WHITE:
    case CC_FLOW_DARK:
    case CC_LIGHT_WHITE:
    case CC_LIGHT_DARK:
    case CC_SCATTER_SATURATION:
    case CC_SCATTER_VALUE:
    case CC_SCATTER_MIX:
    case CC_SCATTER_WIDTH:
    case CC_SCATTER_EDGE:
    case CC_SCATTER_RANDOMIZE: return unitOf(value);

    default: return (float)value;
  }
}

void readControls(const uint8_t *dialed, const Pushes *pushes, Reading &out) {
  auto at = [&](uint8_t cc) { return controlValue(cc, routed(dialed, pushes, cc)); };

  Shape &shape = out.shape;
  shape.width = at(CC_SHAPE_WIDTH);
  shape.edge = at(CC_SHAPE_EDGE);
  shape.tailBeats = at(CC_SHAPE_TAIL);
  shape.count = (uint8_t)at(CC_SHAPE_COUNT);
  shape.position = at(CC_SHAPE_POSITION);
  shape.lapsPerBeat = at(CC_SHAPE_SPEED);
  shape.bend = at(CC_SHAPE_BEND);
  shape.bendAt = at(CC_SHAPE_BEND_AT);
  shape.bounce = aurora_switch_is_on(dialed[CC_SHAPE_BOUNCE]);

  Fan &fan = out.fan;
  fan.spread = at(CC_FAN_SPREAD);
  fan.lfo = at(CC_FAN_LFO);
  fan.speedPixels = at(CC_FAN_SPEED);
  fan.frequency = at(CC_FAN_FREQUENCY);
  fan.phase = at(CC_FAN_PHASE);
  fan.randomize = at(CC_FAN_RANDOMIZE);

  out.lfoBeats = at(CC_LFO_RATE);

  Field &field = out.field;
  field.form = aurora_three_way_position(dialed[CC_FIELD_FORM]);
  field.direction = aurora_three_way_position(dialed[CC_FIELD_DIRECTION]);
  field.hue = at(CC_FIELD_HUE);
  field.white = at(CC_FIELD_WHITE);
  field.dark = at(CC_FIELD_DARK);
  field.width = at(CC_FIELD_WIDTH);
  field.edge = at(CC_FIELD_EDGE);
  field.count = (uint8_t)at(CC_FIELD_COUNT);
  field.cellsPerBeat = at(CC_FIELD_SPEED);

  Flow &flow = out.flow;
  flow.hue = at(CC_FLOW_HUE);
  flow.white = at(CC_FLOW_WHITE);
  flow.dark = at(CC_FLOW_DARK);
  flow.cyclesPerBeat = at(CC_FLOW_RATE);
  flow.density = at(CC_FLOW_DENSITY);

  Light &light = out.light;
  light.hue = at(CC_LIGHT_HUE);
  light.white = at(CC_LIGHT_WHITE);
  light.dark = at(CC_LIGHT_DARK);

  Scatter &scatter = out.scatter;
  scatter.rate = at(CC_SCATTER_RATE);
  scatter.count = at(CC_SCATTER_COUNT);
  scatter.width = at(CC_SCATTER_WIDTH);
  scatter.edge = at(CC_SCATTER_EDGE);
  scatter.randomize = at(CC_SCATTER_RANDOMIZE);
  scatter.speed = at(CC_SCATTER_SPEED);
  scatter.mix = at(CC_SCATTER_MIX);

  out.color = routedColor(dialed, pushes);
}

Hsv routedColor(const uint8_t *dialed, const Pushes *pushes) {
  auto at = [&](uint8_t cc) { return (uint8_t)controlValue(cc, routed(dialed, pushes, cc)); };
  return { at(CC_HUE), at(CC_SATURATION), at(CC_VALUE) };
}

Hsv dialedColor(const uint8_t *controls) { return routedColor(controls, nullptr); }

}
