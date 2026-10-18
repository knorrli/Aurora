#include "cells.h"

#include <math.h>

#include "render_math.h"

namespace render {

static const uint8_t TRAIL_STEPS = 24;
static const uint16_t MAX_BIRTHS_SCANNED = 256;
static const float FOREVER = 1.0e9f;
static const float FULL_LEVEL = 255.0f;
static const float VELOCITY_BEATS = 1.0f / 64.0f;
static const float WRAP_SLACK = 0.02f;

static const uint8_t PER_CELL[] = {
  CONTROL_CELL_POSITION, CONTROL_CELL_WIDTH, CONTROL_CELL_EDGE, CONTROL_CELL_BRIGHTNESS, CONTROL_HUE, CONTROL_SATURATION,
};
static const uint8_t PER_CELL_COUNT = sizeof(PER_CELL) / sizeof(PER_CELL[0]);

enum PerCell : uint8_t { POSITION, WIDTH, EDGE, BRIGHTNESS, HUE, SATURATION };

struct Live {
  float amount[PER_CELL_COUNT];
  float bipolar[PER_CELL_COUNT];
};

struct Life {
  float gateBeats;
  uint8_t end;
  float lastBeats;
  float stepBeats;
};

struct StripLife {
  const CellsFrame *frame;
  const CellsStrip *strip;
  const Cells *cells;
  Routes routes;
  Life life;
  bool wraps;
  Live recent;
  Live past[TRAIL_STEPS + 1];
};

struct Birth {
  float age;
  CellMember member;
  float chordOffset;
};

struct CellState {
  bool exists;
  float along;
  float start;
  float travel;
  float center;
  float halfWidth;
  float edge;
  float brightness;
  float hue;
  float saturation;
  float velocity;
};

static float stagesAt(const Envelope &envelope, float time) {
  time -= envelope.delay;
  if (time < 0.0f) return 0.0f;
  if (time < envelope.attack) return time / envelope.attack;
  time -= envelope.attack;
  if (time < envelope.hold) return 1.0f;
  time -= envelope.hold;
  if (time < envelope.decay) return 1.0f + (envelope.sustain - 1.0f) * time / envelope.decay;
  return envelope.sustain;
}

static float envelopeAt(const Envelope &envelope, const Life &life, float age) {
  if (life.end == CELL_END_STAY) return stagesAt(envelope, age);
  if (life.end == CELL_END_START_OVER) return stagesAt(envelope, age - life.gateBeats * floorf(age / life.gateBeats));
  if (age < life.gateBeats) return stagesAt(envelope, age);
  if (envelope.release <= 0.0f) return 0.0f;
  return stagesAt(envelope, life.gateBeats) * fmaxf(0.0f, 1.0f - (age - life.gateBeats) / envelope.release);
}

static bool steps(const Envelope &envelope, const Life &life) { return envelope.stepped && life.stepBeats > 0.0f; }

static float steppedAt(const Envelope &envelope, const Life &life, float age) {
  if (!steps(envelope, life)) return envelopeAt(envelope, life, age);
  return envelopeAt(envelope, life, floorf(age / life.stepBeats) * life.stepBeats);
}

float envelopeLevelAt(const Envelope &envelope, float gateBeats, uint8_t end, float stepBeats, float age) {
  return steppedAt(envelope, { gateBeats, end, FOREVER, stepBeats }, age);
}

static bool bornOnce(const Cells &cells) { return cells.rateBeats <= 0.0f; }

static float lastLife(const Cells &cells, const float *dialed) {
  if (cells.end != CELL_END_GONE) return FOREVER;
  const uint8_t used = envelopesUsed(dialed);
  float release = 0.0f;
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    if (used & (1 << envelope)) release = fmaxf(release, cells.envelopes[envelope].release);
  }
  return cells.gateBeats + release;
}

static void liveOf(const Modulation &modulation, Live &out) {
  for (uint8_t i = 0; i < PER_CELL_COUNT; i++) {
    out.amount[i] = modulation.amount[PER_CELL[i]];
    out.bipolar[i] = modulation.bipolar[PER_CELL[i]];
  }
}

static void liveBefore(const StripLife &life, float beatsAgo, Live &out) {
  static Modulation before;
  const CellsStrip &strip = *life.strip;
  gatherRoutesBefore(strip.dialed, life.frame->lfoBeats, life.frame->lfo, (int8_t)strip.index, strip.pad, beatsAgo,
                     before);
  liveOf(before, out);
}

static void readPast(StripLife &life) {
  liveOf(*life.strip->live, life.past[0]);
  liveBefore(life, VELOCITY_BEATS, life.recent);
  if (life.cells->tailBeats <= 0.0f) return;
  const float step = life.cells->tailBeats / (float)TRAIL_STEPS;
  for (uint8_t i = 1; i <= TRAIL_STEPS; i++) liveBefore(life, step * (float)i, life.past[i]);
}

static RouteContext contextOf(const StripLife &life, float beatsAgo, const CellMember &member) {
  return { life.frame->lfo, life.frame->lfoBeats, beatsAgo, life.strip->pad, (int8_t)life.strip->index, &member };
}

static float travelOf(const StripLife &life, float reach) {
  return (float)PIXELS + (life.cells->outside ? 2.0f : -2.0f) * reach;
}

static void place(const StripLife &life, CellState &state) {
  const float reach = state.halfWidth + state.edge;
  state.start = life.cells->outside ? -reach : reach;
  state.travel = travelOf(life, reach);
  state.center = state.start + state.along * state.travel;
}

static CellState evaluate(const StripLife &life, const Birth &birth, float beatsAgo, const Live &live) {
  CellState state = {};
  const float age = birth.age - beatsAgo;
  if (age < 0.0f || age >= life.life.lastBeats) return state;
  state.exists = true;

  float envelopes[AURORA_ENVELOPES];
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    envelopes[envelope] = steppedAt(life.cells->envelopes[envelope], life.life, age);
  }
  CellMember member = birth.member;
  member.envelopes = envelopes;
  const RouteContext context = contextOf(life, beatsAgo, member);

  const float *dialed = life.strip->dialed;
  const float chord = (float)life.cells->chord;
  const auto value = [&](uint8_t which) {
    const uint8_t control = PER_CELL[which];
    const Push push = cellPush(life.routes, control, context);
    return controlValue(control, cellRouted(control, dialed[control], live.amount[which] + push.amount,
                                       live.bipolar[which] + push.bipolar, chord));
  };

  state.along = value(POSITION) + birth.chordOffset;
  state.halfWidth = value(WIDTH) * 0.5f;
  state.edge = value(EDGE);
  state.brightness = value(BRIGHTNESS);
  state.hue = value(HUE);
  state.saturation = value(SATURATION);
  place(life, state);
  return state;
}

static float delayOf(const StripLife &life, const CellMember &member) {
  const RouteContext context = contextOf(life, 0.0f, member);
  const Push push = cellPush(life.routes, CONTROL_CELL_DELAY, context);
  const float dialed = life.strip->dialed[CONTROL_CELL_DELAY];
  return controlValue(CONTROL_CELL_DELAY, cellRouted(CONTROL_CELL_DELAY, dialed, push.amount, push.bipolar, 1.0f));
}

static float curveOf(float along, float amount, float at) {
  if (fabsf(amount) < 0.001f) return 1.0f;
  const float reach = fmaxf(at, 1.0f - at);
  const float distance = reach > 0.0f ? clampUnit(fabsf(along - at) / reach) : 0.0f;
  const float near = 0.5f + 0.5f * cosf(0.5f * TURN * distance);
  return amount > 0.0f ? 1.0f - amount * (1.0f - near) : 1.0f + amount * near;
}

static float placeOf(const StripLife &life, float center) {
  return bentAlong(life.frame->bend, center / (float)PIXELS);
}

static float curveAtSample(const StripLife &life, uint16_t sample, float across, float center) {
  const Cells &cells = *life.cells;
  float along = 0.0f;
  switch (cells.direction) {
    case DIRECTION_VERTICAL: along = ((float)sample + 0.5f) / (float)STRIP_SAMPLES; break;
    case DIRECTION_HORIZONTAL: along = (float)life.strip->index / (float)(STRIPS - 1); break;
    case DIRECTION_CELL: along = across; break;
    default: along = placeOf(life, center); break;
  }
  return curveOf(along, cells.curve, cells.curveAt);
}

static uint16_t firstSampleFrom(const float *logical, float from) {
  uint16_t low = 0;
  uint16_t high = STRIP_SAMPLES;
  while (low < high) {
    const uint16_t middle = (uint16_t)((low + high) / 2);
    if (logical[middle] < from) low = (uint16_t)(middle + 1);
    else high = middle;
  }
  return low;
}

static float seen(float presence) { return presence * presence; }

static float directionOf(float velocity) { return velocity >= 0.0f ? 1.0f : -1.0f; }

static void keep(CellSample &sample, float level, float body, float across, float place, uint8_t cell) {
  if (level <= sample.level) return;
  sample = { level, body, across, place, cell };
}

static void stampBody(const StripLife &life, const CellState &state, uint8_t cell, DrawnCells &out) {
  const float *logical = life.frame->logical;
  const float reach = state.halfWidth + state.edge;
  const float direction = directionOf(state.velocity);
  const float place = placeOf(life, state.center);
  for (uint16_t i = firstSampleFrom(logical, state.center - reach);
       i < STRIP_SAMPLES && logical[i] <= state.center + reach; i++) {
    const float offset = logical[i] - state.center;
    const float distance = fabsf(offset);
    float body = 1.0f;
    if (distance > state.halfWidth) {
      if (state.edge <= 0.0001f) continue;
      const float k = 1.0f - (distance - state.halfWidth) / state.edge;
      if (k <= 0.0f) continue;
      body = k * k * (3.0f - 2.0f * k);
    }
    const float across = reach > 0.0001f ? 0.5f - 0.5f * direction * offset / reach : 0.5f;
    const float level = body * seen(state.brightness * curveAtSample(life, i, across, state.center));
    keep(out.samples[i], level, body, across, place, cell);
  }
}

static float faded(float beatsAgo, float tailBeats) {
  const float left = 1.0f - beatsAgo / tailBeats;
  return left > 0.0f ? left * left : 0.0f;
}

static void stampTrail(const StripLife &life, const CellState *states, uint8_t cell, DrawnCells &out) {
  const float tailBeats = life.cells->tailBeats;
  const float step = tailBeats / (float)TRAIL_STEPS;
  uint8_t newest = 0;
  while (newest <= TRAIL_STEPS && !states[newest].exists) newest++;
  if (newest > TRAIL_STEPS) return;
  const CellState &head = states[newest];
  const bool followed = newest < TRAIL_STEPS && states[newest + 1].exists
                     && fabsf(head.center - states[newest + 1].center) > 0.0001f;
  const float direction = directionOf(followed ? head.center - states[newest + 1].center : head.velocity);
  float length = head.halfWidth;
  for (uint8_t j = newest; j < TRAIL_STEPS && states[j + 1].exists; j++) {
    length += fabsf(states[j + 1].center - states[j].center);
  }

  const float place = placeOf(life, head.center);
  const float *logical = life.frame->logical;
  for (uint8_t j = newest; j < TRAIL_STEPS; j++) {
    const CellState &newer = states[j];
    const CellState &older = states[j + 1];
    if (!older.exists) break;
    const float newerLevel = seen(newer.brightness) * faded(step * (float)j, tailBeats);
    const float olderLevel = seen(older.brightness) * faded(step * (float)(j + 1), tailBeats);
    const float walked = older.center - newer.center;
    const float low = fminf(newer.center, older.center) - newer.halfWidth;
    const float high = fmaxf(newer.center, older.center) + newer.halfWidth;
    for (uint16_t i = firstSampleFrom(logical, low); i < STRIP_SAMPLES && logical[i] <= high; i++) {
      const float t = fabsf(walked) > 0.0001f ? clampUnit((logical[i] - newer.center) / walked) : 0.0f;
      const float trail = newerLevel + (olderLevel - newerLevel) * t;
      const float behind = -direction * (logical[i] - head.center);
      const float across = 0.5f + 0.5f * clampUnit(behind / length);
      const float level = trail * seen(curveAtSample(life, i, across, newer.center));
      keep(out.samples[i], level, level, across, place, cell);
    }
  }
}

static float lifeShare(const Life &life, float age) {
  if (life.end == CELL_END_START_OVER) return fract(age / life.gateBeats);
  return clampUnit(age / life.gateBeats);
}

static bool reachesStrip(const CellState &state, float tailBeats) {
  const float reach = state.halfWidth + state.edge + fabsf(state.velocity) * tailBeats;
  return state.center + reach >= 0.0f && state.center - reach <= (float)PIXELS;
}

static void markCell(const StripLife &life, const CellState &state, float age, DrawnCells &out) {
  StripMarks &marks = out.marks;
  if (marks.count >= MAX_CELLS) return;
  if (state.center + state.halfWidth < 0.0f || state.center - state.halfWidth > (float)PIXELS) return;
  const bool onStrip = state.center >= 0.0f && state.center <= (float)PIXELS;
  const float center = onStrip ? bentAlong(life.frame->bend, state.center / (float)PIXELS) * (float)PIXELS
                               : state.center;
  marks.marks[marks.count++] = { center, state.halfWidth * 2.0f, lifeShare(life.life, age) };
}

static void stamp(const StripLife &life, const CellState *states, uint8_t cell, bool trailed, DrawnCells &out) {
  if (states[0].exists) stampBody(life, states[0], cell, out);
  if (trailed) stampTrail(life, states, cell, out);
}

static void lap(CellState *states, float by) {
  for (uint8_t j = 0; j <= TRAIL_STEPS; j++) {
    states[j].along += by;
    states[j].center = states[j].start + states[j].along * states[j].travel;
  }
}

float curveLevel(const Cells &cells, float along) { return curveOf(along, cells.curve, cells.curveAt); }

void readCurve(const Cells &cells, uint8_t strip, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    const float along = cells.direction == DIRECTION_HORIZONTAL ? (float)strip / (float)(STRIPS - 1) : (float)i / (float)PIXELS;
    out[i] = curveOf(along, cells.curve, cells.curveAt);
  }
}

void startCells(float beats, float start, float lfo, float lfoBeats, const Cells &cells, CellsFrame &out) {
  out.beats = beats;
  out.start = start;
  out.lfo = lfo;
  out.lfoBeats = lfoBeats;
  out.bend = bendFor(cells.bend, cells.bendAt);
  for (uint16_t i = 0; i < STRIP_SAMPLES; i++) {
    const float along = ((float)i + 0.5f) / (float)STRIP_SAMPLES;
    out.logical[i] = unbentAlong(out.bend, along) * (float)PIXELS;
  }
}

static void birthsInCycle(const CellsFrame &frame, const Cells &cells, bool once, float undelayedAge,
                          CellMember &out) {
  if (once) {
    out.birthIndex = 0;
    out.birthsInCycle = 1;
    return;
  }
  const float perCycle = frame.lfoBeats / cells.rateBeats;
  const float counted = floorf(perCycle + 0.001f);
  out.birthsInCycle = (uint8_t)(counted < 1.0f ? 1.0f : (counted > 255.0f ? 255.0f : counted));
  const float into = fract(frame.lfo - undelayedAge / frame.lfoBeats + 0.0001f);
  const float index = floorf(into * perCycle + 0.001f);
  out.birthIndex = (uint8_t)fminf(index, (float)(out.birthsInCycle - 1));
}

void drawCells(const CellsFrame &frame, const CellsStrip &strip, DrawnCells &out) {
  for (CellSample &sample : out.samples) sample = { 0.0f, 0.0f, 0.5f, 0.5f, NO_CELL };
  out.marks.count = 0;

  static StripLife life;
  const Cells &cells = strip.reading->cells;
  life.frame = &frame;
  life.strip = &strip;
  life.cells = &cells;
  readRoutes(strip.dialed, life.routes);
  life.life = { cells.gateBeats, cells.end, lastLife(cells, strip.dialed), cells.rateBeats };
  life.wraps = bornOnce(cells);
  readPast(life);

  const float tailBeats = cells.tailBeats;
  const bool once = bornOnce(cells);
  const float step = once ? frame.lfoBeats : cells.rateBeats;
  const float latest = once ? 0.0f : floorf(frame.beats / step);
  const float reachBack = life.life.lastBeats + tailBeats + MAX_DELAY_STEPS * step;
  const float spacing = cells.spread / (float)cells.chord;
  uint8_t drawn = 0;

  for (uint16_t scanned = 0; scanned < MAX_BIRTHS_SCANNED && drawn < MAX_CELLS; scanned++) {
    if (once && scanned > 0) break;
    const float number = latest - (float)scanned;
    const float undelayedAge = frame.beats - (once ? frame.start : number * step);
    if (undelayedAge < 0.0f) continue;
    if (undelayedAge > reachBack) break;

    Birth birth;
    birth.member.chord = cells.chord;
    birth.member.birthNumber = (int32_t)number;
    birth.member.envelopes = nullptr;
    birthsInCycle(frame, cells, once, undelayedAge, birth.member);

    for (uint8_t note = 0; note < cells.chord && drawn < MAX_CELLS; note++) {
      birth.member.note = note;
      birth.member.age = undelayedAge;
      birth.age = undelayedAge - delayOf(life, birth.member) * step;
      if (birth.age < 0.0f || birth.age > life.life.lastBeats + tailBeats) continue;
      birth.member.age = birth.age;
      birth.chordOffset = ((float)note - (float)(cells.chord - 1) * 0.5f) * spacing;
      CellState states[TRAIL_STEPS + 1];
      states[0] = evaluate(life, birth, 0.0f, life.past[0]);
      if (states[0].exists) {
        const CellState earlier = evaluate(life, birth, VELOCITY_BEATS, life.recent);
        if (earlier.exists) states[0].velocity = (states[0].center - earlier.center) / VELOCITY_BEATS;
        if (!life.wraps && !reachesStrip(states[0], tailBeats)) continue;
      }
      const bool trailed = tailBeats > 0.0f;
      if (trailed) {
        const float trailStep = tailBeats / (float)TRAIL_STEPS;
        for (uint8_t j = 1; j <= TRAIL_STEPS; j++) states[j] = evaluate(life, birth, trailStep * (float)j, life.past[j]);
      }
      uint8_t first = 0;
      while (first <= (trailed ? TRAIL_STEPS : 0) && !states[first].exists) first++;
      if (first > (trailed ? TRAIL_STEPS : 0)) continue;

      const uint8_t cell = drawn++;
      out.colors[cell] = { states[first].hue, states[first].saturation, FULL_LEVEL };
      if (!life.wraps) {
        stamp(life, states, cell, trailed, out);
        if (states[0].exists) markCell(life, states[0], birth.age, out);
        continue;
      }
      const float along = states[first].along;
      if (along < -WRAP_SLACK || along > 1.0f + WRAP_SLACK) lap(states, -floorf(along));
      if (states[0].exists) markCell(life, states[0], birth.age, out);
      if (!cells.outside) {
        stamp(life, states, cell, trailed, out);
        continue;
      }
      lap(states, -1.0f);
      for (uint8_t round = 0; round < 3; round++) {
        stamp(life, states, cell, trailed, out);
        lap(states, 1.0f);
      }
    }
  }
}

}
