#include "render.h"

#include <math.h>

#include "bend.h"
#include "cells.h"
#include "clocks.h"
#include "fan.h"
#include "engines.h"
#include "palettes.h"
#include "pars.h"
#include "reading.h"
#include "render_math.h"
#include "routes.h"

namespace render {

static const float DARKEST_DRAWN = 0.002f;

struct FrameContext {
  const float *dialed;
  uint8_t palette;
  float beats;
  float lfo;
  Reading wallReading;
  CellsFrame cells;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  SpotRoute spotRoutes[RENDER_ROUTES];
  uint8_t spotRouteCount;
  const float *stripDialed[STRIPS];
  const PadFinger *pad;
  float speed;
  PadLevels padLevels;
  float padElapsed;
};

struct StripContext {
  uint8_t index;
  Reading reading;
  float fanWave;
  float fanShift;
  float flowTime;
  float fieldDrift;
  float scatterTime;
  ScatterSpots scatterSpots;
};

struct PixelEngines {
  bool fieldOn;
  bool flowOn;
  bool scatterOn;
  bool flat;
};

void restartCells(Wall &wall) { wall.restarting = true; }

static float beatsAt(float quarterNotes, uint8_t division) {
  return quarterNotes * (float)AURORA_TICKS_PER_BEAT / (float)aurora_ticks_per_division(division);
}

static const PadLevels *padReaching(const FrameContext &context, int8_t strip) {
  if (!context.pad->playing) return nullptr;
  if (strip >= 0 && !context.pad->strips[strip]) return nullptr;
  return &context.padLevels;
}

static void readStrip(const FrameContext &context, uint8_t index, float *padShift,
                      Modulation &modulation, StripContext &strip, Frame &out) {
  const Fan &fan = context.wallReading.fan;
  strip.index = index;
  strip.fanWave = fanWave(fan, index);
  strip.fanShift = fan.lfo * strip.fanWave;
  out.stripFanShift[index] = strip.fanShift;

  const float *dialed = context.stripDialed[index];
  gatherRoutes(dialed, context.wallReading.lfoBeats, context.lfo, strip.fanShift,
               padReaching(context, (int8_t)index), modulation);
  carryPadShift(context.padElapsed, padShift, modulation);
  readControls(dialed, &modulation, strip.reading);
  hurry(strip.reading, context.speed);

  strip.flowTime = context.flowTime + modulation.shift[CC_FLOW_RATE];
  strip.fieldDrift = context.fieldDrift + modulation.shift[CC_FIELD_SPEED];
  strip.scatterTime = context.scatterTime + modulation.shift[CC_SCATTER_RATE];
}

static Rgb drawPixel(const FrameContext &context, const StripContext &strip, const DrawnCells &cells,
                     const PixelEngines &engines, uint8_t pixelIndex, float &fieldLevel) {
  const Reading &reading = strip.reading;
  float levelTotal = 0.0f;
  float bodyTotal = 0.0f;
  float fieldTotal = 0.0f;
  float scatterCover = 0.0f;
  float brightest = 0.0f;
  uint8_t leading = NO_CELL;
  ScatterSample spot = { 0.0f, 0.0f, 0.0f, 0.0f };
  for (uint8_t sampleIndex = 0; sampleIndex < SAMPLES_PER_PIXEL; sampleIndex++) {
    const float acrossPixel = ((float)sampleIndex + 0.5f) / (float)SAMPLES_PER_PIXEL - 0.5f;
    const CellSample &sample = cells.samples[pixelIndex * SAMPLES_PER_PIXEL + sampleIndex];
    levelTotal += sample.level;
    bodyTotal += sample.body;
    if (sample.level > brightest) {
      brightest = sample.level;
      leading = sample.cell;
    }

    if (engines.fieldOn) {
      fieldTotal += fieldAt(reading.field, strip.index, (float)pixelIndex + acrossPixel,
                            clampUnit(sample.across), sample.place, strip.fieldDrift);
    }
    if (engines.scatterOn) {
      const ScatterSample here = scatterAt(reading.scatter, strip.scatterSpots,
                                           (float)pixelIndex + 0.5f + acrossPixel);
      scatterCover += here.cover;
      if (here.cover > spot.cover) spot = here;
    }
  }

  const float level = levelTotal / (float)SAMPLES_PER_PIXEL;
  const float body = bodyTotal / (float)SAMPLES_PER_PIXEL;
  const float cover = scatterCover / (float)SAMPLES_PER_PIXEL;
  fieldLevel = fieldTotal / (float)SAMPLES_PER_PIXEL;
  const bool baseLit = level > DARKEST_DRAWN;
  if (!baseLit && cover <= DARKEST_DRAWN) return { 0, 0, 0 };

  const Hsv color = leading == NO_CELL ? reading.color : cells.colors[leading];
  const Hsv tint = (engines.flat || !baseLit)
      ? color
      : tintAt(reading, color, strip.index, pixelIndex, fieldLevel, body, engines.flowOn, strip.flowTime);
  const float baseValue = baseLit ? tint.v * level : 0.0f;
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

static void drawStrip(const FrameContext &context, const StripContext &strip, const DrawnCells &cells,
                      Rgb *pixels, float *fieldLevels) {
  const Reading &reading = strip.reading;
  PixelEngines engines;
  const float *dialed = context.stripDialed[strip.index];
  engines.fieldOn = fieldActive(reading, dialed, RENDER_ROUTES);
  engines.flowOn = flowActive(reading);
  engines.scatterOn = scatterActive(reading, dialed, RENDER_ROUTES);
  engines.flat = !engines.fieldOn && !engines.flowOn && !coreActive(reading);

  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
    pixels[pixelIndex] = drawPixel(context, strip, cells, engines, pixelIndex, fieldLevels[pixelIndex]);
  }
}

static float cellClock(Motion &motion, float beats, float speed) {
  const float elapsed = beats - motion.lastCellBeats;
  motion.lastCellBeats = beats;
  return anchoredPhase(motion.cells, beats, elapsed, speed);
}

void renderFrame(const float *controls, float quarterNotes, uint32_t milliseconds,
                 const OneshotClock &oneshot, const PadFinger &pad, float speed, Motion &motion,
                 Wall &wall, Frame &out) {
  setOneshotClock(oneshot);
  FrameContext context;
  bool everyStrip = true;
  for (uint8_t i = 0; i < STRIPS; i++) everyStrip = everyStrip && pad.strips[i];
  const float *wallControls = pad.morphed && everyStrip ? pad.morphed : controls;
  context.dialed = wallControls;
  for (uint8_t i = 0; i < STRIPS; i++) {
    context.stripDialed[i] = pad.morphed && pad.strips[i] ? pad.morphed : wallControls;
  }
  context.pad = &pad;
  context.speed = speed;
  context.padLevels = { pad.x, pad.y };
  context.palette = roundedControl(wallControls[CC_PALETTE]);
  context.beats = beatsAt(quarterNotes, roundedControl(wallControls[CC_TEMPO_DIVISION]));

  readControls(wallControls, nullptr, context.wallReading);
  hurry(context.wallReading, speed);
  context.lfo = anchoredLfoPhase(motion, context.beats, 1.0f / context.wallReading.lfoBeats);
  out.lfo = context.lfo;

  Modulation modulation;
  gatherRoutes(wallControls, context.wallReading.lfoBeats, context.lfo, 0.0f, padReaching(context, -1),
               modulation);
  readControls(wallControls, &modulation, context.wallReading);
  hurry(context.wallReading, speed);
  if (pad.morphed && wallControls != pad.morphed) {
    Modulation parModulation;
    gatherRoutes(pad.morphed, context.wallReading.lfoBeats, context.lfo, 0.0f, padReaching(context, -1),
                 parModulation);
    readPars(pad.morphed, parModulation, context.lfo, out);
  } else {
    readPars(wallControls, modulation, context.lfo, out);
  }
  holdParPulses(milliseconds, wall, out);
  readFan(context.wallReading, out.fan);

  const float cellBeats = cellClock(motion, context.beats, speed);
  if (wall.restarting) {
    wall.cellsStart = cellBeats;
    wall.restarting = false;
  }
  startCells(cellBeats, wall.cellsStart, context.lfo, context.wallReading.lfoBeats, context.wallReading.cells,
             context.cells);
  readBend(context.cells.bend, out.bend);
  out.curveShown = context.wallReading.cells.direction != DIRECTION_CELL;
  out.curveHorizontal = context.wallReading.cells.direction == DIRECTION_HORIZONTAL;
  for (uint8_t i = 0; i < FIELD_ACROSS_POINTS; i++) {
    out.curveAcross[i] = curveLevel(context.wallReading.cells, (float)i / (float)(FIELD_ACROSS_POINTS - 1));
  }

  const float sinceLastFrame = context.beats - wall.lastBeats;
  context.padElapsed = (sinceLastFrame > 0.0f && sinceLastFrame < (float)TAIL_MAX_BEATS) ? sinceLastFrame : 0.0f;
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
  context.spotRouteCount = gatherSpotRoutes(wallControls, context.spotRoutes);
  if (scatterElapsed < 0.0f) {
    for (auto &drifts : motion.spotDrift) {
      for (float &drift : drifts) drift = 0.0f;
    }
    scatterElapsed = 0.0f;
  }
  const float scatterCycles = scatterElapsed * context.wallReading.scatter.rate;

  static DrawnCells drawn;
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

    const CellsStrip cellsStrip = {
      index, context.stripDialed[index], &strip.reading, &modulation, padReaching(context, (int8_t)index),
      strip.fanWave, strip.fanShift,
    };
    drawCells(context.cells, cellsStrip, drawn);
    out.cells[index] = drawn.marks;
    readCurve(strip.reading.cells, index, out.curve + index * BEND_POINTS);
    drawStrip(context, strip, drawn, out.pixels + index * PIXELS, out.fieldLevels + index * PIXELS);
  }

  wall.lastBeats = context.beats;
}

}
