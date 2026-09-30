#include "travel.h"

#include <math.h>

#include "clocks.h"
#include "render_math.h"

namespace render {

static const float POSITION_SETTLE_BEATS = 2.0f;

static inline float triangleSwing(float phase) {
  return (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
}

static float swingPhaseAt(float center, float halfCore, float swingSpan, float direction) {
  const float swing = clampUnit((swingSpan > 0.0001f) ? (center - halfCore) / swingSpan : 0.0f);
  return (direction >= 0.0f) ? (swing * 0.5f) : (1.0f - swing * 0.5f);
}

static void settle(float &anchorCells, float cellsTravelled, float elapsed) {
  if (elapsed <= 0.0f) return;
  const float travelled = cellsTravelled + anchorCells;
  const float drift = travelled - roundf(travelled);
  if (fabsf(drift) < 0.0001f) return;

  float pull = elapsed / POSITION_SETTLE_BEATS;
  if (pull > 1.0f) pull = 1.0f;
  anchorCells -= drift * pull;
}

static float wrapCenter(float cellsTravelled, Anchor &anchor, const Travel &travel,
                        const StripTravel &strip) {
  const float dialed = 0.5f + strip.positionCells + cellsTravelled
                     + strip.shiftLaps + strip.fanOffset;
  if (travel.flipped) anchor.travelCells = anchor.lastCenter - dialed;
  else if (strip.lapsPerBeat == 0.0f) settle(anchor.travelCells, cellsTravelled, travel.elapsed);
  else pullToWhole(anchor.travelCells, travel.elapsed, strip.lapsPerBeat);
  const float center = dialed + anchor.travelCells;
  if (!travel.walled) return center;

  const float lap = roundf(cellsTravelled + anchor.travelCells);
  const float halfCore = strip.width * 0.5f;
  return lap + fminf(fmaxf(center - lap, halfCore), 1.0f - halfCore);
}

static float bounceCenter(float swingCycles, Anchor &anchor, const Travel &travel,
                          const StripTravel &strip) {
  const float halfCore = strip.width * 0.5f;
  const float swingSpan = 1.0f - strip.width;
  const float direction = directionOf(strip.lapsPerBeat);
  const float cycles = swingCycles + direction * strip.shiftLaps + strip.fanOffset;
  if (travel.flipped) {
    anchor.swingCycles = swingPhaseAt(anchor.lastCenter, halfCore, swingSpan, direction) - cycles;
  } else {
    pullToWhole(anchor.swingCycles, travel.elapsed, strip.lapsPerBeat);
  }
  return halfCore + triangleSwing(fract(cycles + anchor.swingCycles)) * swingSpan;
}

float travelCenter(Clock &travelClock, Clock &swingClock, Anchor &anchor, const Travel &travel,
                   const StripTravel &strip) {
  const float cellsTravelled =
      anchoredPhase(travelClock, travel.beats, travel.elapsed, strip.lapsPerBeat);
  const float swingCycles =
      anchoredPhase(swingClock, travel.beats, travel.elapsed, fabsf(strip.lapsPerBeat));
  if (travel.bouncing) {
    const float center = bounceCenter(swingCycles, anchor, travel, strip);
    anchor.lastCenter = center;
    return center;
  }
  const float center = wrapCenter(cellsTravelled, anchor, travel, strip);
  anchor.lastCenter = fract(center);
  return center;
}

}
