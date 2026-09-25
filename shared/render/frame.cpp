#include "render.h"

#include <math.h>

#include "bend.h"
#include "clocks.h"
#include "fan.h"
#include "layers.h"
#include "palettes.h"
#include "pars.h"
#include "reading.h"
#include "render_math.h"
#include "routes.h"
#include "tails.h"
#include "travel.h"

namespace render {

static const uint8_t SAMPLES_PER_PIXEL = 8;
static const float TAIL_GAP_QUARTER_NOTES = 0.5f;
static const float DARKEST_DRAWN = 0.002f;

struct FrameContext {
  const uint8_t *dialed;
  float beats;
  float lfo;
  Reading plain;
  float cellLength;
  const BendTable *bend;
  Travel travel;
  TailFrame tail;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  float stripSpeeds[STRIPS];
};

struct StripContext {
  uint8_t index;
  Reading reading;
  float lfo;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  StripTravel travel;
  float center;
  float tailCenter;
};

struct ShapeLook {
  float centerInCell;
  float direction;
  float width;
  float halfWidth;
  float edge;
  bool tailing;
  float tailBeats;
  float tailCells;
  float lead;
  float trail;
};

struct ShapeSample {
  float level;
  float across;
};

struct PixelLayers {
  bool fieldOn;
  bool flowOn;
  bool scatterOn;
  bool flat;
};

static inline float pushToward(float base, float push, float low, float high) {
  const float limit = (push >= 0.0f) ? high : low;
  return base + fabsf(push) * (limit - base);
}

static float beatsAt(float quarterNotes, uint8_t division) {
  return quarterNotes * (float)AURORA_TICKS_PER_BEAT / (float)aurora_ticks_per_division(division);
}

static void startTravel(FrameContext &context, const Wall &wall) {
  const Reading &plain = context.plain;
  bool anyMoving =
      routeAims(context.dialed, CC_SHAPE_SPEED) || routeAims(context.dialed, CC_FAN_SPEED);
  for (uint8_t i = 0; i < STRIPS; i++) {
    context.stripSpeeds[i] =
        snapToStill(plain.shape.speedPixels + plain.fan.speedPixels * fanWave(plain.fan, i));
    if (context.stripSpeeds[i] != 0.0f) anyMoving = true;
  }

  Travel &travel = context.travel;
  travel.beats = context.beats;
  travel.bouncing = plain.shape.bounce && anyMoving;
  travel.flipped = travel.bouncing != wall.lastBouncing;
  travel.elapsed = context.beats - wall.lastBeats;
  travel.cellLength = context.cellLength;
}

static void startTails(FrameContext &context, const Wall &wall, float quarterNotes) {
  TailFrame &tail = context.tail;
  tail.beats = context.beats;
  tail.lastBeats = wall.lastBeats;
  tail.step = (int32_t)floorf(context.beats * (float)TAIL_STEPS_PER_BEAT);
  tail.lastStep = wall.tails.lastStep;
  tail.fresh = wall.tails.empty || context.beats < wall.lastBeats
            || context.beats - wall.lastBeats > (float)TAIL_MAX_BEATS
            || quarterNotes - wall.lastQuarterNotes > TAIL_GAP_QUARTER_NOTES;
  tail.bouncing = context.travel.bouncing;
  tail.flipped = context.travel.flipped;
}

static void readStrip(const FrameContext &context, uint8_t index, Pushes &pushes,
                      StripContext &strip, Frame &out) {
  const Fan &fan = context.plain.fan;
  const float wave = fanWave(fan, index);
  strip.index = index;
  strip.lfo = context.lfo + fan.lfo * wave;
  out.stripLfo[index] = strip.lfo;

  gatherRoutes(context.dialed, context.plain.lfoBeats, context.lfo, strip.lfo, pushes);
  readControls(context.dialed, &pushes, strip.reading);

  strip.flowTime = context.flowTime + pushes.shift[CC_FLOW_RATE];
  strip.fieldDrift = context.fieldDrift + pushes.shift[CC_FIELD_SPEED];
  strip.scatterTime = context.scatterTime + pushes.shift[CC_SCATTER_RATE];

  strip.travel.speedPixels = context.stripSpeeds[index];
  strip.travel.shiftPixels = pushes.shift[CC_SHAPE_SPEED] + pushes.shift[CC_FAN_SPEED] * wave;
  strip.travel.positionCells = strip.reading.shape.positionCells;
  strip.travel.fanOffset = fan.spread * wave;
  strip.travel.width = strip.reading.shape.width;
}

static float placementOf(const FrameContext &context, const StripContext &strip) {
  return (context.travel.bouncing ? 0.0f : strip.travel.positionCells) + strip.travel.fanOffset;
}

static ShapeLook lookOf(const FrameContext &context, const StripContext &strip,
                        const TailHistory &history, Tail &tail) {
  const Shape &shape = strip.reading.shape;
  ShapeLook look;
  look.centerInCell = fract(strip.center);
  look.width = shape.width;
  look.halfWidth = shape.width * 0.5f;
  look.edge = shape.edge;
  look.tailBeats = shape.tailBeats;
  look.tailing = shape.tailBeats > 0.0001f;
  if (look.tailing) {
    walkTail(history, context.tail.step, context.beats, strip.tailCenter, look.halfWidth,
             shape.tailBeats, tail);
  }

  look.direction = (look.tailing && tail.direction != 0.0f)
      ? tail.direction : directionOf(strip.travel.speedPixels);
  const float edgeSpread = shape.edge * (1.0f - shape.width) * 0.5f;
  look.tailCells = look.tailing ? tail.lengthCells : 0.0f;
  look.lead = look.halfWidth + edgeSpread;
  look.trail = look.halfWidth + fmaxf(edgeSpread, look.tailCells);
  return look;
}

static bool nearestOffset(float cells, float centerInCell, float direction, bool bounce,
                          float countCells, float &out) {
  const float firstImage = centerInCell + floorf(cells - centerInCell);
  bool found = false;
  for (uint8_t image = 0; image < 2; image++) {
    const float imageCells = firstImage + (float)image;
    if (bounce && (imageCells < 0.0f || imageCells > countCells)) continue;
    const float offset = -direction * (cells - imageCells);
    if (!found || fabsf(offset) < fabsf(out)) {
      out = offset;
      found = true;
    }
  }
  return found;
}

static ShapeSample sampleShape(const FrameContext &context, const ShapeLook &look,
                               const Tail &tail, float cells) {
  const Shape &plain = context.plain.shape;
  float nearest = 0.0f;
  const bool onShape =
      nearestOffset(cells, look.centerInCell, look.direction, plain.bounce, (float)plain.count,
                    nearest);
  const float coreLevel = onShape ? bumpAt(nearest, look.width, look.edge) : 0.0f;
  const float age = look.tailing ? tailAgeAt(tail, cells) : -1.0f;
  float tailLevel = 0.0f;
  if (age >= 0.0f && age < look.tailBeats) {
    const float left = 1.0f - age / look.tailBeats;
    tailLevel = left * left;
  }

  ShapeSample sample = { fmaxf(coreLevel, tailLevel), 0.5f };
  if (tailLevel > coreLevel && look.trail > 0.0001f) {
    sample.across =
        0.5f + 0.5f * (look.halfWidth + (age / look.tailBeats) * look.tailCells) / look.trail;
  } else if (onShape) {
    const float reach = (nearest < 0.0f) ? look.lead : look.trail;
    if (reach > 0.0001f) sample.across = 0.5f + 0.5f * nearest / reach;
  }
  return sample;
}

static Rgb drawPixel(const FrameContext &context, const StripContext &strip,
                     const ShapeLook &look, const Tail &tail, const PixelLayers &layers,
                     uint8_t pixelIndex) {
  const Reading &reading = strip.reading;
  const Shape &plain = context.plain.shape;
  float shapeTotal = 0.0f;
  float fieldTotal = 0.0f;
  float scatterTotal = 0.0f;
  for (uint8_t sampleIndex = 0; sampleIndex < SAMPLES_PER_PIXEL; sampleIndex++) {
    const float acrossPixel = ((float)sampleIndex + 0.5f) / (float)SAMPLES_PER_PIXEL - 0.5f;
    const float cells = bentCells(context.bend, plain.bounce,
                                  (float)pixelIndex + 0.5f + acrossPixel, context.cellLength,
                                  plain.count);
    const ShapeSample sample = sampleShape(context, look, tail, cells);
    shapeTotal += sample.level;

    if (layers.fieldOn) {
      fieldTotal += fieldAt(reading.field, strip.index, (float)pixelIndex + acrossPixel,
                            clampUnit(sample.across), strip.fieldDrift);
    }
    if (layers.scatterOn) {
      scatterTotal += scatterAt(reading.scatter, strip.index,
                                (float)pixelIndex + 0.5f + acrossPixel, strip.scatterTime);
    }
  }

  const float shape = shapeTotal / (float)SAMPLES_PER_PIXEL;
  const float scatter = scatterTotal / (float)SAMPLES_PER_PIXEL;

  float intensity = shape;
  if (layers.scatterOn) {
    intensity = pushToward(intensity, scatter * reading.scatter.value, 0.0f, 1.0f);
  }
  if (intensity <= DARKEST_DRAWN) return { 0, 0, 0 };

  const Hsv tint = layers.flat
      ? reading.color
      : tintAt(reading, strip.index, pixelIndex, fieldTotal / (float)SAMPLES_PER_PIXEL, shape,
               scatter, layers.flowOn, strip.flowTime);
  return scaleVideo(paletteColor(context.dialed[CC_PALETTE], tint.h, tint.s),
                    (uint8_t)((float)tint.v * intensity));
}

static void drawStrip(const FrameContext &context, const StripContext &strip,
                      const TailHistory &history, Rgb *pixels) {
  Tail tail;
  const ShapeLook look = lookOf(context, strip, history, tail);

  const Reading &reading = strip.reading;
  PixelLayers layers;
  layers.fieldOn = fieldActive(reading);
  layers.flowOn = flowActive(reading);
  layers.scatterOn = scatterActive(reading);
  layers.flat =
      !layers.fieldOn && !layers.flowOn && !lightActive(reading) && !scatterTints(reading);

  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
    pixels[pixelIndex] = drawPixel(context, strip, look, tail, layers, pixelIndex);
  }
}

void renderFrame(const uint8_t *controls, float quarterNotes, Motion &motion, Wall &wall,
                 Frame &out) {
  FrameContext context;
  context.dialed = controls;
  context.beats = beatsAt(quarterNotes, controls[CC_TEMPO_DIVISION]);

  readControls(controls, nullptr, context.plain);
  context.lfo = anchoredLfoPhase(motion, context.beats, 1.0f / context.plain.lfoBeats);
  out.lfo = context.lfo;

  Pushes pushes;
  gatherRoutes(controls, context.plain.lfoBeats, context.lfo, context.lfo, pushes);
  readControls(controls, &pushes, context.plain);
  readPars(controls, pushes, context.beats, context.lfo, context.plain.lfoBeats, out);
  readFan(context.plain, out.fan);

  const Shape &shape = context.plain.shape;
  context.cellLength = (float)PIXELS / (float)shape.count;
  context.bend = bendFor(shape.bend, shape.bendAt);
  readBend(context.bend, shape.bounce, context.cellLength, shape.count, out.bend);

  startTravel(context, wall);
  startTails(context, wall, quarterNotes);
  context.flowTime = clockPhase(motion.flow, context.beats, context.plain.flow.cyclesPerBeat);
  context.fieldDrift = clockPhase(motion.field, context.beats, context.plain.field.cellsPerBeat);
  context.scatterTime = clockPhase(motion.scatter, context.beats, context.plain.scatter.rate);

  for (uint8_t index = 0; index < STRIPS; index++) {
    StripContext strip;
    readStrip(context, index, pushes, strip, out);
    strip.center = travelCenter(motion.travel[index], motion.swing[index], wall.anchors[index],
                                context.travel, strip.travel);
    TailHistory &history = wall.tails.strips[index];
    strip.tailCenter =
        recordTail(history, context.tail, strip.center, placementOf(context, strip));
    drawStrip(context, strip, history, out.pixels + index * PIXELS);
  }

  wall.lastBouncing = context.travel.bouncing;
  wall.lastBeats = context.beats;
  wall.lastQuarterNotes = quarterNotes;
  wall.tails.lastStep = context.tail.step;
  wall.tails.empty = false;
}

}
