#include "cells.h"

#include <math.h>

#include "render_math.h"

namespace render {

static const uint8_t TRAIL_STEPS = 24;
static const uint16_t MAX_BIRTHS_SCANNED = 256;
static const float FOREVER = 1.0e9f;
static const float FULL_LEVEL = 255.0f;

static const uint8_t PER_CELL[] = {
  CC_CELL_POSITION, CC_CELL_WIDTH, CC_CELL_EDGE, CC_CELL_BRIGHTNESS, CC_HUE, CC_SATURATION,
};
static const uint8_t PER_CELL_COUNT = sizeof(PER_CELL) / sizeof(PER_CELL[0]);

enum PerCell : uint8_t { POSITION, WIDTH, EDGE, BRIGHTNESS, HUE, SATURATION };

struct Live {
  float amount[PER_CELL_COUNT];
  float bipolar[PER_CELL_COUNT];
  float speedShift;
  float fanSpeedShift;
  float speedSwing;
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
  CellRoute routes[RENDER_ROUTES];
  uint8_t routeCount;
  Life life;
  float baseSpeed;
  float fanOffset;
  float padSpeedShift;
  float padFanSpeedShift;
  Live past[TRAIL_STEPS + 1];
};

struct Birth {
  float age;
  float held[RENDER_ROUTES];
  float speedShift;
  float fanSpeedShift;
  float chordOffset;
};

struct CellState {
  bool exists;
  float center;
  float halfWidth;
  float edge;
  float brightness;
  float hue;
  float saturation;
  float velocity;
};

static float stagesAt(const Envelope &envelope, float time) {
  if (time < envelope.attack) return time / envelope.attack;
  time -= envelope.attack;
  if (time < envelope.hold) return 1.0f;
  time -= envelope.hold;
  if (time < envelope.decay) return 1.0f + (envelope.sustain - 1.0f) * time / envelope.decay;
  return envelope.sustain;
}

static float stagesArea(const Envelope &envelope, float time) {
  const float attack = fminf(time, envelope.attack);
  float area = envelope.attack > 0.0f ? attack * attack / (2.0f * envelope.attack) : 0.0f;
  time -= envelope.attack;
  if (time <= 0.0f) return area;
  const float hold = fminf(time, envelope.hold);
  area += hold;
  time -= envelope.hold;
  if (time <= 0.0f) return area;
  const float decay = fminf(time, envelope.decay);
  if (decay > 0.0f) area += decay * (2.0f + (envelope.sustain - 1.0f) * decay / envelope.decay) * 0.5f;
  time -= envelope.decay;
  if (time <= 0.0f) return area;
  return area + time * envelope.sustain;
}

static float envelopeAt(const Envelope &envelope, const Life &life, float age) {
  if (life.end == CELL_END_STAY) return stagesAt(envelope, age);
  if (life.end == CELL_END_START_OVER) return stagesAt(envelope, age - life.gateBeats * floorf(age / life.gateBeats));
  if (age < life.gateBeats) return stagesAt(envelope, age);
  if (envelope.release <= 0.0f) return 0.0f;
  return stagesAt(envelope, life.gateBeats) * fmaxf(0.0f, 1.0f - (age - life.gateBeats) / envelope.release);
}

static const uint16_t MAX_STEPS_SUMMED = 512;

static bool steps(const Envelope &envelope, const Life &life) { return envelope.stepped && life.stepBeats > 0.0f; }

static float steppedAt(const Envelope &envelope, const Life &life, float age) {
  if (!steps(envelope, life)) return envelopeAt(envelope, life, age);
  return envelopeAt(envelope, life, floorf(age / life.stepBeats) * life.stepBeats);
}

static float glidingArea(const Envelope &envelope, const Life &life, float age) {
  if (life.end == CELL_END_STAY) return stagesArea(envelope, age);
  const float lives = floorf(age / life.gateBeats);
  if (life.end == CELL_END_START_OVER) {
    return lives * stagesArea(envelope, life.gateBeats) + stagesArea(envelope, age - lives * life.gateBeats);
  }
  if (age < life.gateBeats) return stagesArea(envelope, age);
  const float released = fminf(age - life.gateBeats, envelope.release);
  const float tail = envelope.release > 0.0f ? released - released * released / (2.0f * envelope.release) : 0.0f;
  return stagesArea(envelope, life.gateBeats) + stagesAt(envelope, life.gateBeats) * tail;
}

static float envelopeArea(const Envelope &envelope, const Life &life, float age) {
  if (!steps(envelope, life)) return glidingArea(envelope, life, age);
  const float whole = floorf(age / life.stepBeats);
  const uint16_t summed = whole < (float)MAX_STEPS_SUMMED ? (uint16_t)whole : MAX_STEPS_SUMMED;
  float area = 0.0f;
  for (uint16_t step = 0; step < summed; step++) area += envelopeAt(envelope, life, (float)step * life.stepBeats);
  area *= life.stepBeats;
  return area + envelopeAt(envelope, life, whole * life.stepBeats) * (age - whole * life.stepBeats);
}

float envelopeLevelAt(const Envelope &envelope, float gateBeats, uint8_t end, float stepBeats, float age) {
  return steppedAt(envelope, { gateBeats, end, FOREVER, stepBeats }, age);
}

static float lastLife(const Cells &cells, const float *dialed) {
  if (cells.end != CELL_END_GONE) return FOREVER;
  const uint8_t used = envelopesUsed(dialed);
  float release = 0.0f;
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    if (used & (1 << envelope)) release = fmaxf(release, cells.envelopes[envelope].release);
  }
  return cells.gateBeats + release;
}

static void liveOf(const Modulation &modulation, float fanWave, Live &out) {
  for (uint8_t i = 0; i < PER_CELL_COUNT; i++) {
    out.amount[i] = modulation.amount[PER_CELL[i]];
    out.bipolar[i] = modulation.bipolar[PER_CELL[i]];
  }
  out.speedShift = modulation.shift[CC_CELL_SPEED];
  out.fanSpeedShift = modulation.shift[CC_FAN_SPEED];
  out.speedSwing = modulation.swing[CC_CELL_SPEED] + modulation.swing[CC_FAN_SPEED] * fanWave;
}

static float speedShiftBefore(const StripLife &life, uint8_t cc, float beatsAgo) {
  const CellsStrip &strip = *life.strip;
  return swingShiftAt(strip.dialed, cc, life.frame->lfoBeats, life.frame->lfo, strip.fanShift, beatsAgo);
}

static void readPast(StripLife &life) {
  const CellsStrip &strip = *life.strip;
  liveOf(*strip.live, strip.fanWave, life.past[0]);
  life.padSpeedShift = strip.live->shift[CC_CELL_SPEED] - speedShiftBefore(life, CC_CELL_SPEED, 0.0f);
  life.padFanSpeedShift = strip.live->shift[CC_FAN_SPEED] - speedShiftBefore(life, CC_FAN_SPEED, 0.0f);
  if (life.cells->tailBeats <= 0.0f) return;
  static Modulation before;
  const float step = life.cells->tailBeats / (float)TRAIL_STEPS;
  for (uint8_t i = 1; i <= TRAIL_STEPS; i++) {
    gatherRoutesBefore(strip.dialed, life.frame->lfoBeats, life.frame->lfo, strip.fanShift, strip.pad,
                       step * (float)i, before);
    liveOf(before, strip.fanWave, life.past[i]);
    life.past[i].speedShift += life.padSpeedShift;
    life.past[i].fanSpeedShift += life.padFanSpeedShift;
  }
}

static CellState evaluate(const StripLife &life, const Birth &birth, float beatsAgo, const Live &live) {
  CellState state = {};
  const float age = birth.age - beatsAgo;
  if (age < 0.0f || age >= life.life.lastBeats) return state;
  state.exists = true;

  float envelopes[AURORA_ENVELOPES];
  float areas[AURORA_ENVELOPES];
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    envelopes[envelope] = steppedAt(life.cells->envelopes[envelope], life.life, age);
    areas[envelope] = envelopeArea(life.cells->envelopes[envelope], life.life, age);
  }
  float levels[RENDER_ROUTES];
  for (uint8_t i = 0; i < life.routeCount; i++) {
    const uint8_t source = life.routes[i].source;
    levels[i] = aurora_source_is_envelope(source) ? envelopeLevel(source, envelopes) : birth.held[i];
  }

  const float *dialed = life.strip->dialed;
  const auto value = [&](uint8_t which) {
    const uint8_t cc = PER_CELL[which];
    return controlValue(cc, cellRouted(cc, dialed[cc], live.amount[which], live.bipolar[which],
                                       life.routes, life.routeCount, levels));
  };

  float travel = life.baseSpeed * age + (live.speedShift - birth.speedShift)
               + (live.fanSpeedShift - birth.fanSpeedShift) * life.strip->fanWave;
  state.velocity = life.baseSpeed + live.speedSwing;
  for (uint8_t i = 0; i < life.routeCount; i++) {
    const CellRoute &route = life.routes[i];
    if (route.cc != CC_CELL_SPEED) continue;
    const float reach = cellSwingReach(route);
    if (aurora_source_is_envelope(route.source)) {
      travel += reach * areas[route.source - ROUTE_SOURCE_ENVELOPE_1];
      state.velocity += reach * levels[i];
    } else {
      travel += reach * (levels[i] - route.mean) * age;
      state.velocity += reach * (levels[i] - route.mean);
    }
  }

  state.center = value(POSITION) + birth.chordOffset + life.fanOffset + travel;
  state.halfWidth = value(WIDTH) * 0.5f;
  state.edge = value(EDGE);
  state.brightness = value(BRIGHTNESS);
  state.hue = value(HUE);
  state.saturation = value(SATURATION);
  return state;
}

static float curveOf(float along, float amount, float at) {
  if (fabsf(amount) < 0.001f) return 1.0f;
  const float reach = fmaxf(at, 1.0f - at);
  const float distance = reach > 0.0f ? clampUnit(fabsf(along - at) / reach) : 0.0f;
  const float near = 0.5f + 0.5f * cosf(0.5f * TURN * distance);
  return amount > 0.0f ? 1.0f - amount * (1.0f - near) : 1.0f + amount * near;
}

static float curveAtSample(const StripLife &life, uint16_t sample, float across, float center) {
  const Cells &cells = *life.cells;
  float along = 0.0f;
  switch (cells.across) {
    case CELL_ACROSS_STRIP: along = ((float)sample + 0.5f) / (float)STRIP_SAMPLES; break;
    case CELL_ACROSS_WALL: along = (float)life.strip->index / (float)(STRIPS - 1); break;
    case CELL_ACROSS_CELL_BODY: along = across; break;
    default: along = bentAlong(life.frame->bend, center / (float)PIXELS); break;
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

static float directionOf(float velocity) { return velocity >= 0.0f ? 1.0f : -1.0f; }

static void keep(CellSample &sample, float level, float body, float across, uint8_t cell) {
  if (level <= sample.level) return;
  sample = { level, body, across, cell };
}

static void stampBody(const StripLife &life, const CellState &state, uint8_t cell, DrawnCells &out) {
  const float *logical = life.frame->logical;
  const float reach = state.halfWidth + state.edge;
  const float direction = directionOf(state.velocity);
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
    const float level = body * state.brightness * curveAtSample(life, i, across, state.center);
    keep(out.samples[i], level, body, across, cell);
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
  const float direction = directionOf(head.velocity);
  float length = head.halfWidth;
  for (uint8_t j = newest; j < TRAIL_STEPS && states[j + 1].exists; j++) {
    length += fabsf(states[j + 1].center - states[j].center);
  }

  const float *logical = life.frame->logical;
  for (uint8_t j = newest; j < TRAIL_STEPS; j++) {
    const CellState &newer = states[j];
    const CellState &older = states[j + 1];
    if (!older.exists) break;
    const float newerLevel = newer.brightness * faded(step * (float)j, tailBeats);
    const float olderLevel = older.brightness * faded(step * (float)(j + 1), tailBeats);
    const float walked = older.center - newer.center;
    const float low = fminf(newer.center, older.center) - newer.halfWidth;
    const float high = fmaxf(newer.center, older.center) + newer.halfWidth;
    for (uint16_t i = firstSampleFrom(logical, low); i < STRIP_SAMPLES && logical[i] <= high; i++) {
      const float t = fabsf(walked) > 0.0001f ? clampUnit((logical[i] - newer.center) / walked) : 0.0f;
      const float trail = newerLevel + (olderLevel - newerLevel) * t;
      const float behind = -direction * (logical[i] - head.center);
      const float across = 0.5f + 0.5f * clampUnit(behind / length);
      const float level = trail * curveAtSample(life, i, across, newer.center);
      keep(out.samples[i], level, level, across, cell);
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

void readCurve(const Cells &cells, uint8_t strip, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    const float along = cells.across == CELL_ACROSS_WALL ? (float)strip / (float)(STRIPS - 1) : (float)i / (float)PIXELS;
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

void drawCells(const CellsFrame &frame, const CellsStrip &strip, DrawnCells &out) {
  for (CellSample &sample : out.samples) sample = { 0.0f, 0.0f, 0.5f, NO_CELL };
  out.marks.count = 0;

  static StripLife life;
  const Cells &cells = strip.reading->cells;
  life.frame = &frame;
  life.strip = &strip;
  life.cells = &cells;
  life.routeCount = gatherCellRoutes(strip.dialed, strip.fanShift, life.routes);
  life.life = { cells.gateBeats, cells.end, lastLife(cells, strip.dialed), cells.rateBeats };
  life.baseSpeed = controlValue(CC_CELL_SPEED, strip.dialed[CC_CELL_SPEED])
                 + controlValue(CC_FAN_SPEED, strip.dialed[CC_FAN_SPEED]) * strip.fanWave;
  life.fanOffset = strip.reading->fan.spread * strip.fanWave * (float)PIXELS;
  readPast(life);

  const float tailBeats = cells.tailBeats;
  const bool once = cells.rateBeats <= 0.0f;
  const float latest = once ? 0.0f : floorf(frame.beats / cells.rateBeats);
  const float spacing = cells.spread * (float)PIXELS / (float)cells.chord;
  uint8_t drawn = 0;

  for (uint16_t scanned = 0; scanned < MAX_BIRTHS_SCANNED && drawn < MAX_CELLS; scanned++) {
    if (once && scanned > 0) break;
    const float birthBeat = once ? frame.start : (latest - (float)scanned) * cells.rateBeats;
    Birth birth;
    birth.age = frame.beats - birthBeat;
    if (birth.age < 0.0f) continue;
    if (birth.age > life.life.lastBeats + tailBeats) break;
    for (uint8_t i = 0; i < life.routeCount; i++) {
      birth.held[i] = heldLevel(life.routes[i], frame.beats / frame.lfoBeats, frame.lfoBeats, birth.age);
    }
    birth.speedShift = speedShiftBefore(life, CC_CELL_SPEED, birth.age);
    birth.fanSpeedShift = speedShiftBefore(life, CC_FAN_SPEED, birth.age);

    for (uint8_t note = 0; note < cells.chord && drawn < MAX_CELLS; note++) {
      birth.chordOffset = ((float)note - (float)(cells.chord - 1) * 0.5f) * spacing;
      CellState states[TRAIL_STEPS + 1];
      states[0] = evaluate(life, birth, 0.0f, life.past[0]);
      if (states[0].exists && !reachesStrip(states[0], tailBeats)) continue;

      const uint8_t cell = drawn++;
      if (states[0].exists) {
        out.colors[cell] = { states[0].hue, states[0].saturation, FULL_LEVEL };
        stampBody(life, states[0], cell, out);
        markCell(life, states[0], birth.age, out);
      }
      if (tailBeats <= 0.0f) continue;
      const float step = tailBeats / (float)TRAIL_STEPS;
      for (uint8_t j = 1; j <= TRAIL_STEPS; j++) states[j] = evaluate(life, birth, step * (float)j, life.past[j]);
      if (!states[0].exists) {
        uint8_t first = 1;
        while (first <= TRAIL_STEPS && !states[first].exists) first++;
        if (first > TRAIL_STEPS) continue;
        out.colors[cell] = { states[first].hue, states[first].saturation, FULL_LEVEL };
      }
      stampTrail(life, states, cell, out);
    }
  }
}

}
