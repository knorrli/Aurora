#include "reading.h"

#include <math.h>

namespace render {


static const float FIELD_MAX_HUE = 128.0f;
static const float FIELD_MIN_COUNT = 0.5f;
static const float FLOW_MAX_HUE = 128.0f;
static const float SCATTER_MAX_HUE = 128.0f;
static const float CORE_MAX_HUE = 64.0f;

static const float FLOW_BEATS_PER_CYCLE[] = { 0.0f, 16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f };
static const uint8_t FLOW_RATE_STEPS = sizeof(FLOW_BEATS_PER_CYCLE) / sizeof(FLOW_BEATS_PER_CYCLE[0]);

static const float ENVELOPE_BEATS[] = {
  0.0f, 0.0625f, 0.125f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f,
};
static const uint8_t ENVELOPE_BEAT_STEPS = sizeof(ENVELOPE_BEATS) / sizeof(ENVELOPE_BEATS[0]);

static const float TAIL_BEATS[] = { 0.0f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, (float)TAIL_MAX_BEATS };
static const uint8_t TAIL_BEAT_STEPS = sizeof(TAIL_BEATS) / sizeof(TAIL_BEATS[0]);

static float steppedOf(float value, const float *steps, uint8_t count) {
  const uint8_t last = count - 1;
  const uint8_t step = (uint8_t)(((uint16_t)roundedControl(value) * last + 63) / 127);
  return steps[step > last ? last : step];
}

static float flowCyclesPerBeatOf(float value) {
  const float beats = steppedOf(value, FLOW_BEATS_PER_CYCLE, FLOW_RATE_STEPS);
  return beats > 0.0f ? 1.0f / beats : 0.0f;
}

static float tailBeatsOf(float value) { return steppedOf(value, TAIL_BEATS, TAIL_BEAT_STEPS); }

static float envelopeBeatsOf(float value) { return steppedOf(value, ENVELOPE_BEATS, ENVELOPE_BEAT_STEPS); }

static float gateBeatsOf(float value) { return aurora_lfo_period((uint8_t)(127 - roundedControl(value))); }

static float cellRateBeatsOf(float value) {
  const uint8_t last = AURORA_LFO_PERIOD_COUNT;
  const uint8_t step = (uint8_t)(((uint16_t)roundedControl(value) * last + 63) / 127);
  return step == 0 ? 0.0f : AURORA_LFO_PERIODS[(step > last ? last : step) - 1];
}

static float chordOf(float value) {
  return (float)(1 + ((uint16_t)roundedControl(value) * (MAX_CHORD - 1) + 63) / 127);
}

static bool envelopeTime(uint8_t control) {
  if (control < CONTROL_ENVELOPE_1_DELAY || control > CONTROL_ENVELOPE_3_RELEASE) return false;
  const uint8_t stage = (uint8_t)((control - CONTROL_ENVELOPE_1_DELAY) % (ENVELOPE_STAGES + 1));
  return stage != ENVELOPE_SUSTAIN && stage != ENVELOPE_STAGES;
}

static inline float unitOf(float value) { return value / 127.0f; }

float signedOf(float value) {
  return value < 64.0f ? (value - 64.0f) / 64.0f
                       : (value - 64.0f) / 63.0f;
}

static const float FULL_LEVEL = 255.0f;

static inline float levelOf(float value) { return value * FULL_LEVEL / 127.0f; }

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

static float positionOf(float value) {
  const float laps = floorf(value / 127.0f);
  return laps + 0.5f + 0.5f * signedOf(value - 127.0f * laps);
}

static float delayOf(float value) {
  const float steps = value / 127.0f;
  return steps < 0.0f ? 0.0f : (steps > MAX_DELAY_STEPS ? MAX_DELAY_STEPS : steps);
}

float controlValue(uint8_t control, float value) {
  if (envelopeTime(control)) return envelopeBeatsOf(value);
  switch (control) {
    case CONTROL_HUE: return hueOf(value);
    case CONTROL_SATURATION:
    case CONTROL_PAR_VALUE:
    case CONTROL_PAR_HUE_OFFSET:
    case CONTROL_PAR_SATURATION: return levelOf(value);
    case CONTROL_PAR_HUE_RANGE: return signedOf(value) * 128.0f;

    case CONTROL_SCATTER_COUNT: return smoothCountOf(value);
    case CONTROL_FIELD_COUNT: return fieldCountOf(value);

    case CONTROL_CELL_BEND:
    case CONTROL_CELL_CURVE: return signedOf(value);
    case CONTROL_ARP_SPREAD: return unitOf(value);
    case CONTROL_CELL_BEND_AT:
    case CONTROL_CELL_CURVE_AT: return 0.5f + 0.5f * signedOf(value);
    case CONTROL_CELL_POSITION: return positionOf(value);
    case CONTROL_CELL_DELAY: return delayOf(value);
    case CONTROL_CELL_WIDTH: return unitOf(value) * (float)PIXELS;
    case CONTROL_CELL_EDGE: return unitOf(value) * EDGE_REACH_PIXELS;
    case CONTROL_CELL_RATE: return cellRateBeatsOf(value);
    case CONTROL_CELL_GATE: return gateBeatsOf(value);
    case CONTROL_CELL_CHORD: return chordOf(value);
    case CONTROL_SCATTER_SPEED: return squaredRate(value, MAX_SPEED_PIXELS_PER_BEAT);
    case CONTROL_LFO_RATE: return aurora_lfo_period(roundedControl(value));

    case CONTROL_FIELD_HUE: return signedOf(value) * FIELD_MAX_HUE;
    case CONTROL_FIELD_SPEED: return lapsPerBeatOf(value);
    case CONTROL_FIELD_POSITION: return signedOf(value) * 0.5f;
    case CONTROL_FLOW_HUE: return signedOf(value) * FLOW_MAX_HUE;
    case CONTROL_FLOW_RATE: return flowCyclesPerBeatOf(value);
    case CONTROL_FLOW_DENSITY: return 0.12f * powf(180.0f, unitOf(value));
    case CONTROL_CORE_HUE: return signedOf(value) * CORE_MAX_HUE;
    case CONTROL_SCATTER_RATE: return 1.0f / aurora_lfo_period(roundedControl(value));
    case CONTROL_SCATTER_HUE: return signedOf(value) * SCATTER_MAX_HUE;

    case CONTROL_SCATTER_POSITION: return signedOf(value);

    case CONTROL_CELL_TAIL: return tailBeatsOf(value);

    case CONTROL_CELL_BRIGHTNESS:
    case CONTROL_CELL_SPREAD:
    case CONTROL_ENVELOPE_1_SUSTAIN:
    case CONTROL_ENVELOPE_2_SUSTAIN:
    case CONTROL_ENVELOPE_3_SUSTAIN:
    case CONTROL_FIELD_WIDTH:
    case CONTROL_FIELD_EDGE:
    case CONTROL_FIELD_WHITE:
    case CONTROL_FIELD_DARK:
    case CONTROL_FLOW_WHITE:
    case CONTROL_FLOW_DARK:
    case CONTROL_CORE_WHITE:
    case CONTROL_CORE_DARK:
    case CONTROL_SCATTER_SATURATION:
    case CONTROL_SCATTER_VALUE:
    case CONTROL_SCATTER_MIX:
    case CONTROL_SCATTER_WIDTH:
    case CONTROL_SCATTER_EDGE:
    case CONTROL_SCATTER_RANDOMIZE: return unitOf(value);

    default: return value;
  }
}

void readControls(const float *dialed, const Modulation *modulation, Reading &out) {
  auto at = [&](uint8_t control) { return controlValue(control, routed(dialed, modulation, control)); };

  Cells &cells = out.cells;
  cells.rateBeats = at(CONTROL_CELL_RATE);
  cells.chord = (uint8_t)at(CONTROL_CELL_CHORD);
  cells.spread = at(CONTROL_CELL_SPREAD);
  cells.outside = aurora_switch_is_on(roundedControl(dialed[CONTROL_CELL_ENDS]));
  cells.delay = at(CONTROL_CELL_DELAY);
  cells.gateBeats = at(CONTROL_CELL_GATE);
  cells.end = aurora_three_way_position(roundedControl(dialed[CONTROL_CELL_END]));
  cells.width = at(CONTROL_CELL_WIDTH);
  cells.edge = at(CONTROL_CELL_EDGE);
  cells.tailBeats = at(CONTROL_CELL_TAIL);
  cells.position = at(CONTROL_CELL_POSITION);
  cells.bend = at(CONTROL_CELL_BEND);
  cells.bendAt = at(CONTROL_CELL_BEND_AT);
  cells.brightness = at(CONTROL_CELL_BRIGHTNESS);
  cells.curve = at(CONTROL_CELL_CURVE);
  cells.curveAt = at(CONTROL_CELL_CURVE_AT);
  cells.direction = aurora_four_way_position(roundedControl(dialed[CONTROL_CELL_DIRECTION]));
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    Envelope &stages = cells.envelopes[envelope];
    stages.delay = at(aurora_envelope_control(envelope, ENVELOPE_DELAY));
    stages.attack = at(aurora_envelope_control(envelope, ENVELOPE_ATTACK));
    stages.hold = at(aurora_envelope_control(envelope, ENVELOPE_HOLD));
    stages.decay = at(aurora_envelope_control(envelope, ENVELOPE_DECAY));
    stages.sustain = at(aurora_envelope_control(envelope, ENVELOPE_SUSTAIN));
    stages.release = at(aurora_envelope_control(envelope, ENVELOPE_RELEASE));
    stages.stepped = aurora_switch_is_on(roundedControl(dialed[aurora_envelope_moves_control(envelope)]));
  }

  out.lfoBeats = at(CONTROL_LFO_RATE);

  Field &field = out.field;
  field.form = aurora_three_way_position(roundedControl(dialed[CONTROL_FIELD_FORM]));
  field.direction = aurora_four_way_position(roundedControl(dialed[CONTROL_FIELD_DIRECTION]));
  field.hue = at(CONTROL_FIELD_HUE);
  field.white = at(CONTROL_FIELD_WHITE);
  field.dark = at(CONTROL_FIELD_DARK);
  field.width = at(CONTROL_FIELD_WIDTH);
  field.edge = at(CONTROL_FIELD_EDGE);
  field.count = at(CONTROL_FIELD_COUNT);
  field.cellsPerBeat = at(CONTROL_FIELD_SPEED);
  field.position = at(CONTROL_FIELD_POSITION);

  Flow &flow = out.flow;
  flow.hue = at(CONTROL_FLOW_HUE);
  flow.white = at(CONTROL_FLOW_WHITE);
  flow.dark = at(CONTROL_FLOW_DARK);
  flow.cyclesPerBeat = at(CONTROL_FLOW_RATE);
  flow.density = at(CONTROL_FLOW_DENSITY);

  Core &core = out.core;
  core.hue = at(CONTROL_CORE_HUE);
  core.white = at(CONTROL_CORE_WHITE);
  core.dark = at(CONTROL_CORE_DARK);

  Scatter &scatter = out.scatter;
  scatter.rate = at(CONTROL_SCATTER_RATE);
  scatter.count = at(CONTROL_SCATTER_COUNT);
  scatter.width = at(CONTROL_SCATTER_WIDTH);
  scatter.edge = at(CONTROL_SCATTER_EDGE);
  scatter.randomize = at(CONTROL_SCATTER_RANDOMIZE);
  scatter.speed = at(CONTROL_SCATTER_SPEED);
  scatter.mix = at(CONTROL_SCATTER_MIX);

  out.color = routedColor(dialed, modulation);
}

Hsv routedColor(const float *dialed, const Modulation *modulation) {
  auto at = [&](uint8_t control) { return controlValue(control, routed(dialed, modulation, control)); };
  return { at(CONTROL_HUE), at(CONTROL_SATURATION), FULL_LEVEL };
}

void hurry(Reading &reading, float speed) {
  reading.lfoBeats /= speed;
  reading.field.cellsPerBeat *= speed;
  reading.flow.cyclesPerBeat *= speed;
  reading.scatter.rate *= speed;
  reading.scatter.speed *= speed;
}

}
