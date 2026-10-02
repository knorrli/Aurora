#include "render.h"

#include <math.h>

#include "bend.h"
#include "clocks.h"
#include "fan.h"
#include "engines.h"
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
static const float STILL_PIXELS_PER_BEAT = 0.05f;

struct FrameContext {
  const float *dialed;
  uint8_t palette;
  float beats;
  float lfo;
  Reading wallReading;
  float cellLength;
  const BendTable *bend;
  Travel travel;
  TailFrame tail;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  SpotRoute spotRoutes[RENDER_ROUTES];
  uint8_t spotRouteCount;
  float stripLaps[STRIPS];
  const PadFinger *pad;
  PadLevels padLevels;
  float padElapsed;
};

struct StripContext {
  uint8_t index;
  Reading reading;
  float fanShift;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  ScatterSpots scatterSpots;
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

struct PixelEngines {
  bool fieldOn;
  bool flowOn;
  bool scatterOn;
  bool flat;
};

static float beatsAt(float quarterNotes, uint8_t division) {
  return quarterNotes * (float)AURORA_TICKS_PER_BEAT / (float)aurora_ticks_per_division(division);
}

static void startTravel(FrameContext &context, const Wall &wall) {
  const Reading &wallReading = context.wallReading;
  bool anyMoving =
      routeAims(context.dialed, CC_SHAPE_SPEED, RENDER_ROUTES)
      || routeAims(context.dialed, CC_FAN_SPEED, RENDER_ROUTES);
  const float lap = lapPixels(wallReading.shape);
  const float fanLaps = lap > 0.0001f ? wallReading.fan.speedPixels / lap : 0.0f;
  for (uint8_t i = 0; i < STRIPS; i++) {
    context.stripLaps[i] = wallReading.shape.lapsPerBeat + fanLaps * fanWave(wallReading.fan, i);
    if (fabsf(context.stripLaps[i] * lap) < STILL_PIXELS_PER_BEAT) context.stripLaps[i] = 0.0f;
    if (context.stripLaps[i] != 0.0f) anyMoving = true;
  }

  Travel &travel = context.travel;
  travel.beats = context.beats;
  travel.bouncing = wallReading.shape.bounce && anyMoving;
  travel.walled = wallReading.shape.bounce;
  travel.flipped = travel.bouncing != wall.lastBouncing;
  travel.elapsed = context.beats - wall.lastBeats;
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

static const PadLevels *padReaching(const FrameContext &context, int8_t strip) {
  if (!context.pad->playing) return nullptr;
  if (strip >= 0 && !context.pad->strips[strip]) return nullptr;
  return &context.padLevels;
}

static void readStrip(const FrameContext &context, uint8_t index, float *padShift,
                      Modulation &modulation, StripContext &strip, Frame &out) {
  const Fan &fan = context.wallReading.fan;
  const float wave = fanWave(fan, index);
  strip.index = index;
  strip.fanShift = fan.lfo * wave;
  out.stripFanShift[index] = strip.fanShift;

  gatherRoutes(context.dialed, context.wallReading.lfoBeats, context.lfo, strip.fanShift,
               padReaching(context, (int8_t)index), modulation);
  carryPadShift(context.padElapsed, padShift, modulation);
  readControls(context.dialed, &modulation, strip.reading);

  strip.flowTime = context.flowTime + modulation.shift[CC_FLOW_RATE];
  strip.fieldDrift = context.fieldDrift + modulation.shift[CC_FIELD_SPEED];
  strip.scatterTime = context.scatterTime + modulation.shift[CC_SCATTER_RATE];

  const float lap = lapPixels(context.wallReading.shape);
  strip.travel.lapsPerBeat = context.stripLaps[index];
  strip.travel.shiftLaps = modulation.shift[CC_SHAPE_SPEED]
      + (lap > 0.0001f ? modulation.shift[CC_FAN_SPEED] * wave / lap : 0.0f);
  strip.travel.positionCells =
      (strip.reading.shape.position - 0.5f) * (1.0f - strip.reading.shape.width);
  const bool restsWalled = context.travel.walled && !context.travel.bouncing;
  const float freeSpace = restsWalled ? 1.0f - strip.reading.shape.width : 1.0f;
  strip.travel.fanOffset = fan.spread * wave * freeSpace;
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
      ? tail.direction : directionOf(strip.travel.lapsPerBeat);
  const float edgeSpread = shape.edge * (1.0f - shape.width) * 0.5f;
  look.tailCells = look.tailing ? tail.lengthCells : 0.0f;
  look.lead = look.halfWidth + edgeSpread;
  look.trail = look.halfWidth + fmaxf(edgeSpread, look.tailCells);
  return look;
}

static bool nearestOffset(float cells, float centerInCell, float direction, bool bounce,
                          float count, float &out) {
  const float firstImage = centerInCell + floorf(cells - centerInCell);
  bool found = false;
  for (uint8_t image = 0; image < 2; image++) {
    const float imageCells = firstImage + (float)image;
    if (bounce && (imageCells < cellsAtStart(count) || imageCells > cellsAtStart(count) + count)) continue;
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
  const Shape &wallShape = context.wallReading.shape;
  float nearest = 0.0f;
  const bool onShape =
      nearestOffset(cells, look.centerInCell, look.direction, wallShape.bounce, wallShape.count,
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
                     const ShapeLook &look, const Tail &tail, const PixelEngines &engines,
                     uint8_t pixelIndex, float &fieldLevel) {
  const Reading &reading = strip.reading;
  const Shape &wallShape = context.wallReading.shape;
  float shapeTotal = 0.0f;
  float fieldTotal = 0.0f;
  float scatterCover = 0.0f;
  ScatterSample spot = { 0.0f, 0.0f, 0.0f, 0.0f };
  for (uint8_t sampleIndex = 0; sampleIndex < SAMPLES_PER_PIXEL; sampleIndex++) {
    const float acrossPixel = ((float)sampleIndex + 0.5f) / (float)SAMPLES_PER_PIXEL - 0.5f;
    const float cells = bentCells(context.bend, wallShape.bounce,
                                  (float)pixelIndex + 0.5f + acrossPixel, context.cellLength,
                                  wallShape.count);
    const ShapeSample sample = sampleShape(context, look, tail, cells);
    shapeTotal += sample.level;

    if (engines.fieldOn) {
      fieldTotal += fieldAt(reading.field, strip.index, (float)pixelIndex + acrossPixel,
                            clampUnit(sample.across), strip.fieldDrift);
    }
    if (engines.scatterOn) {
      const ScatterSample here = scatterAt(reading.scatter, strip.scatterSpots,
                                           (float)pixelIndex + 0.5f + acrossPixel);
      scatterCover += here.cover;
      if (here.cover > spot.cover) spot = here;
    }
  }

  const float shape = shapeTotal / (float)SAMPLES_PER_PIXEL;
  const float cover = scatterCover / (float)SAMPLES_PER_PIXEL;
  fieldLevel = fieldTotal / (float)SAMPLES_PER_PIXEL;
  const bool baseLit = shape > DARKEST_DRAWN;
  if (!baseLit && cover <= DARKEST_DRAWN) return { 0, 0, 0 };

  const Hsv tint = (engines.flat || !baseLit)
      ? reading.color
      : tintAt(reading, strip.index, pixelIndex, fieldTotal / (float)SAMPLES_PER_PIXEL, shape,
               engines.flowOn, strip.flowTime);
  const float baseValue = baseLit ? tint.v * shape : 0.0f;
  const Rgb base = scaleVideo(paletteColor(context.palette, tint.h, tint.s), baseValue);
  if (cover <= DARKEST_DRAWN) return base;

  const Rgb painted = scaleVideo(
      paletteColor(context.palette, reading.color.h + spot.hue, spot.saturation * 255.0f),
      spot.value * 255.0f);
  const auto mixed = [cover](uint8_t below, uint8_t above) {
    return (uint8_t)lroundf((float)below + ((float)above - (float)below) * cover);
  };
  return { mixed(base.r, painted.r), mixed(base.g, painted.g), mixed(base.b, painted.b) };
}

static void drawStrip(const FrameContext &context, const StripContext &strip,
                      const TailHistory &history, Rgb *pixels, float *fieldLevels) {
  Tail tail;
  const ShapeLook look = lookOf(context, strip, history, tail);

  const Reading &reading = strip.reading;
  PixelEngines engines;
  engines.fieldOn = fieldActive(reading, context.dialed, RENDER_ROUTES);
  engines.flowOn = flowActive(reading);
  engines.scatterOn = scatterActive(reading, context.dialed, RENDER_ROUTES);
  engines.flat =
      !engines.fieldOn && !engines.flowOn && !coreActive(reading);

  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
    pixels[pixelIndex] = drawPixel(context, strip, look, tail, engines, pixelIndex, fieldLevels[pixelIndex]);
  }
}

void renderFrame(const float *controls, float quarterNotes, uint32_t milliseconds,
                 const OneshotClock &oneshot, const PadFinger &pad, Motion &motion, Wall &wall,
                 Frame &out) {
  setOneshotClock(oneshot);
  FrameContext context;
  context.dialed = controls;
  context.pad = &pad;
  context.padLevels = { pad.x, pad.y };
  context.palette = roundedControl(controls[CC_PALETTE]);
  context.beats = beatsAt(quarterNotes, roundedControl(controls[CC_TEMPO_DIVISION]));

  readControls(controls, nullptr, context.wallReading);
  context.lfo = anchoredLfoPhase(motion, context.beats, 1.0f / context.wallReading.lfoBeats);
  out.lfo = context.lfo;

  Modulation modulation;
  gatherRoutes(controls, context.wallReading.lfoBeats, context.lfo, 0.0f, padReaching(context, -1),
               modulation);
  readControls(controls, &modulation, context.wallReading);
  readPars(controls, modulation, context.lfo, out);
  holdParPulses(milliseconds, wall, out);
  readFan(context.wallReading, out.fan);

  const Shape &shape = context.wallReading.shape;
  context.cellLength = (float)PIXELS / shape.count;
  context.bend = bendFor(shape.bend, shape.bendAt);
  readBend(context.bend, shape.bounce, context.cellLength, shape.count, out.bend);

  startTravel(context, wall);
  const float sinceLastFrame = context.beats - wall.lastBeats;
  context.padElapsed = (sinceLastFrame > 0.0f && sinceLastFrame < (float)TAIL_MAX_BEATS) ? sinceLastFrame : 0.0f;
  startTails(context, wall, quarterNotes);
  context.flowTime = clockPhase(motion.flow, context.beats, context.wallReading.flow.cyclesPerBeat);
  const float fieldElapsed = context.beats - motion.lastFieldBeats;
  motion.lastFieldBeats = context.beats;
  context.fieldDrift =
      anchoredPhase(motion.field, context.beats, fieldElapsed, context.wallReading.field.cellsPerBeat);
  const bool fieldOn = fieldActive(context.wallReading, context.dialed, RENDER_ROUTES);
  for (uint8_t i = 0; i < FIELD_ACROSS_POINTS; i++) {
    const float u = (float)i / (float)(FIELD_ACROSS_POINTS - 1);
    out.fieldAcross[i] = fieldOn ? fieldAtPosition(context.wallReading.field, u, context.fieldDrift) : 0.0f;
  }
  float scatterElapsed = context.beats - motion.lastScatterBeats;
  motion.lastScatterBeats = context.beats;
  context.scatterTime =
      anchoredPhase(motion.scatter, context.beats, scatterElapsed, context.wallReading.scatter.rate);
  context.spotRouteCount = gatherSpotRoutes(controls, context.spotRoutes);
  if (scatterElapsed < 0.0f) {
    for (auto &drifts : motion.spotDrift) {
      for (float &drift : drifts) drift = 0.0f;
    }
    scatterElapsed = 0.0f;
  }
  const float scatterCycles = scatterElapsed * context.wallReading.scatter.rate;

  for (uint8_t index = 0; index < STRIPS; index++) {
    StripContext strip;
    readStrip(context, index, motion.padShift[index], modulation, strip, out);
    const float randomize = strip.reading.scatter.randomize;
    const ScatterClock scatterNow = { strip.scatterTime, randomize };
    placeScatter(strip.reading.scatter, context.dialed, context.spotRoutes, context.spotRouteCount,
                 index, scatterNow, motion.lastScatter[index], motion.spotDrift[index], scatterCycles,
                 strip.scatterSpots);
    motion.lastScatter[index] = scatterNow;
    const ScatterSpots &spots = strip.scatterSpots;
    out.spots[index].count = spots.count;
    for (uint8_t i = 0; i < spots.count; i++) {
      out.spots[index].marks[i] = { spots.centers[i], spots.widths[i] * spots.reach, spots.lives[i] };
    }
    strip.center = travelCenter(motion.travel[index], motion.swing[index], wall.anchors[index],
                                context.travel, strip.travel);
    TailHistory &history = wall.tails.strips[index];
    strip.tailCenter =
        recordTail(history, context.tail, strip.center, placementOf(context, strip));
    out.centers[index] = pixelOfCells(context.bend, shape.bounce, fract(strip.center),
                                      context.cellLength, shape.count);
    drawStrip(context, strip, history, out.pixels + index * PIXELS, out.fieldLevels + index * PIXELS);
  }

  wall.lastBouncing = context.travel.bouncing;
  wall.lastBeats = context.beats;
  wall.lastQuarterNotes = quarterNotes;
  wall.tails.lastStep = context.tail.step;
  wall.tails.empty = false;
}

}
