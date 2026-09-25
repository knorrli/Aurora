#include "routes.h"

#include <math.h>

#include "render.h"

namespace render {

static inline float fract(float x) { return x - floorf(x); }

static inline float raisedCosine(float x) {
  const float t = (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
  return 0.5f - 0.5f * cosf((float)M_PI * t);
}

// Saw down has to come before square, or the two blend into something that is
// neither. In this order it is one decay getting shorter and harder.
float lfoWave(float phase, uint8_t wave) {
  float attack, decay, hard;
  if (wave <= GEN_WAVE_SAW_DOWN) {
    decay = (float)wave / (float)GEN_WAVE_SAW_DOWN;
    attack = 1.0f - decay;
    hard = 0.0f;
  } else {
    attack = 0.0f;
    const float toSquare = (float)(wave - GEN_WAVE_SAW_DOWN)
                         / (float)(GEN_WAVE_SQUARE - GEN_WAVE_SAW_DOWN);
    hard = (toSquare > 1.0f) ? 1.0f : toSquare;
    decay = (wave <= GEN_WAVE_SQUARE)
        ? 1.0f - 0.5f * toSquare
        : 0.5f * powf(GEN_LFO_MIN_WIDTH / 0.5f,
                      (float)(wave - GEN_WAVE_SQUARE)
                          / (float)(127 - GEN_WAVE_SQUARE));
  }

  const float u = fract(phase);
  if (decay > 0.0f && u <= decay) {
    // Dividing by what is left of softness is what turns the decay into a
    // cliff: at the hard end everything above the floor saturates.
    const float soft = (1.0f - hard < 0.001f) ? 0.001f : 1.0f - hard;
    const float shaped = raisedCosine(1.0f - u / decay) / soft;
    return (shaped > 1.0f) ? 1.0f : shaped;
  }
  if (attack > 0.0f && u >= 1.0f - attack) {
    return raisedCosine((u - (1.0f - attack)) / attack);
  }
  return 0.0f;
}

static bool refused(uint8_t cc) {
  switch (cc) {
    case CC_TEMPO_DIVISION:
    case CC_GEN_LFO_RATE:
      return true;
    default:
      return false;
  }
}

bool routeRefused(uint8_t cc) { return cc == 0 || refused(cc); }

// A rate feeds a running total, so a one-way push would leave the wall
// somewhere else for good. These swing both ways and average to nothing.
static bool swings(uint8_t cc) {
  switch (cc) {
    case CC_PLACED_SPEED:
    case CC_WANDER_RATE:
    case CC_GEN_SPEED:
    case CC_GEN_FAN_RATE:
    case CC_SCATTER_RATE:
      return true;
    default:
      return false;
  }
}

// 0 and 127 are the same place, so there is no limit to travel toward.
static bool circular(uint8_t cc) {
  switch (cc) {
    case CC_HUE:
    case CC_WASH_HUE_OFFSET:
    case CC_GEN_POSITION:
    case CC_GEN_FAN_PHASE:
      return true;
    default:
      return false;
  }
}

// The fan's own controls shape the strips' shifted reading of the LFO, so a
// route aimed at one while reading that would need its own phase to compute
// what sets its own phase. Count sets the cell geometry and is read before the
// strip loop opens, and so is the bend. The washes have no strip to be offset
// from.
static bool plainLfo(uint8_t cc) {
  switch (cc) {
    case CC_WASH_LEVEL:
    case CC_WASH_HUE_OFFSET:
    case CC_WASH_SATURATION:
    case CC_GEN_COUNT:
    case CC_GEN_BEND:
    case CC_GEN_BEND_AT:
    case CC_GEN_FAN:
    case CC_GEN_FAN_FREQ:
    case CC_GEN_FAN_PHASE:
    case CC_GEN_FAN_RANDOM:
    case CC_GEN_FAN_RATE:
    case CC_GEN_FAN_LFO:
      return true;
    default:
      return false;
  }
}

static inline float bipolar(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

static const uint8_t INTEGRAL_STEPS = 128;

float waveMean(uint8_t wave) {
  float sum = 0.0f;
  for (uint8_t i = 0; i < INTEGRAL_STEPS; i++) {
    sum += lfoWave(((float)i + 0.5f) / (float)INTEGRAL_STEPS, wave);
  }
  return sum / (float)INTEGRAL_STEPS;
}

// A wave with its average taken off, and the running total of that over one
// cycle, centered so a still pattern swings around where it stands. Built once
// per wave byte a route holds. The total comes back to exactly zero at the end
// of the cycle, which is what returns a swung rate to where its tracker puts it.
struct WaveIntegral {
  int16_t wave = -1;
  float mean;
  float center;
  float total[INTEGRAL_STEPS + 1];
};

static WaveIntegral integrals[AURORA_ROUTES];

static const WaveIntegral &integralOf(uint8_t route, uint8_t wave) {
  WaveIntegral &in = integrals[route];
  if (in.wave == wave) return in;
  in.wave = wave;

  in.mean = waveMean(wave);
  in.total[0] = 0.0f;
  float area = 0.0f;
  for (uint8_t i = 0; i < INTEGRAL_STEPS; i++) {
    const float sample = lfoWave(((float)i + 0.5f) / (float)INTEGRAL_STEPS, wave);
    in.total[i + 1] = in.total[i] + (sample - in.mean) / (float)INTEGRAL_STEPS;
    area += 0.5f * (in.total[i] + in.total[i + 1]);
  }
  in.total[INTEGRAL_STEPS] = 0.0f;
  in.center = area / (float)INTEGRAL_STEPS;
  return in;
}

static float totalAt(const WaveIntegral &in, float phase) {
  const float at = fract(phase) * (float)INTEGRAL_STEPS;
  const uint8_t i = (uint8_t)at;
  if (i >= INTEGRAL_STEPS) return in.total[INTEGRAL_STEPS] - in.center;
  const float t = at - (float)i;
  return in.total[i] + t * (in.total[i + 1] - in.total[i]) - in.center;
}

// How far a rate swings for an amount: as far as the rate's own fader moves at
// that share of its throw, so the swing sits on the same curve as the rate.
// Doubled, so a sine at full amount swings the whole of it either way rather
// than half; the amount's sign picks which half of the cycle is faster.
static float swingReach(uint8_t cc, float amount) {
  const float share = (fabsf(amount) > 1.0f) ? 1.0f : fabsf(amount);
  const bool bipolarRate = convert(cc, 0) < 0.0f;
  const long byte = bipolarRate ? 64 + lroundf(share * 63.0f) : lroundf(share * 127.0f);
  return 2.0f * ((amount < 0.0f) ? -1.0f : 1.0f) * fabsf(convert(cc, (uint8_t)byte));
}

void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float plainPhase,
                  float stripPhase, Pushes &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    out.amount[i] = 0.0f;
    out.swing[i] = 0.0f;
    out.shift[i] = 0.0f;
  }

  for (uint8_t r = 0; r < AURORA_ROUTES; r++) {
    const uint8_t dest = dialed[aurora_route_cc(r, ROUTE_DESTINATION)];
    // CC 0 is never assigned, so it is free to mean "not aimed anywhere".
    if (routeRefused(dest)) continue;

    const float amount = bipolar(dialed[aurora_route_cc(r, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;

    const uint8_t ratio = aurora_route_ratio(dialed[aurora_route_cc(r, ROUTE_RATIO)]);
    const uint8_t wave = dialed[aurora_route_cc(r, ROUTE_WAVE)];
    const float delay = (float)dialed[aurora_route_cc(r, ROUTE_PHASE)] / 128.0f;
    const float lfo = plainLfo(dest) ? plainPhase : stripPhase;
    const float phase = lfo * (float)ratio - delay;

    if (!swings(dest)) {
      out.amount[dest] += amount * lfoWave(phase, wave);
      continue;
    }

    const WaveIntegral &in = integralOf(r, wave);
    const float reach = swingReach(dest, amount);
    out.swing[dest] += reach * (lfoWave(phase, wave) - in.mean);
    out.shift[dest] += reach * totalAt(in, phase) * beatsPerCycle / (float)ratio;
  }
}

// Unwrapped: a circular control may land below 0 or above 127, and routed()
// wraps it where routeReach() leaves it for the editor to draw both ends.
static int16_t landing(uint8_t cc, uint8_t base, float amount) {
  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(cc)) {
    // Half the wheel at a full amount, which on hue is the opposite color.
    // Position gets the whole cell, so a swipe from near the bottom can reach
    // the top before it snaps back.
    const float span = (cc == CC_GEN_POSITION) ? 128.0f : 64.0f;
    return (int16_t)base + (int16_t)lroundf(amount * span);
  }

  const float limit = (amount >= 0.0f) ? 127.0f : 0.0f;
  const long reached = lroundf((float)base + fabsf(amount) * (limit - (float)base));
  return (int16_t)(reached < 0 ? 0 : (reached > 127 ? 127 : reached));
}

uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc) {
  const uint8_t base = dialed[cc];
  if (!pushes || swings(cc)) return base;
  const float amount = pushes->amount[cc];
  if (amount > -0.001f && amount < 0.001f) return base;

  const int16_t landed = landing(cc, base, amount);
  return circular(cc) ? (uint8_t)((landed % 128 + 128) % 128) : (uint8_t)landed;
}

bool routeAims(const uint8_t *dialed, uint8_t cc) {
  for (uint8_t r = 0; r < AURORA_ROUTES; r++) {
    if (dialed[aurora_route_cc(r, ROUTE_DESTINATION)] != cc) continue;
    const float amount = bipolar(dialed[aurora_route_cc(r, ROUTE_AMOUNT)]);
    if (amount < -0.001f || amount > 0.001f) return true;
  }
  return false;
}

// The byte whose value is closest, so a rate swung past either end of its
// fader reads as that end.
static uint8_t nearestByte(uint8_t cc, float value) {
  uint8_t best = 0;
  float bestGap = fabsf(convert(cc, 0) - value);
  for (uint8_t v = 1; v < 128; v++) {
    const float gap = fabsf(convert(cc, v) - value);
    if (gap < bestGap) {
      best = v;
      bestGap = gap;
    }
  }
  return best;
}

uint8_t routedForDisplay(const uint8_t *dialed, const Pushes *pushes, uint8_t cc) {
  if (!pushes || !swings(cc)) return routed(dialed, pushes, cc);
  return nearestByte(cc, convert(cc, dialed[cc]) + pushes->swing[cc]);
}

// Every wave rests at zero and peaks at one, so the furthest a control goes
// up is every raising route at its peak at once, and the same going down. A
// rate's wave has its average taken off, so it reaches below as well.
bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high) {
  low = high = dialed[cc];
  if (routeRefused(cc)) return false;

  float up = 0.0f;
  float down = 0.0f;
  bool aimed = false;
  for (uint8_t r = 0; r < AURORA_ROUTES; r++) {
    if (dialed[aurora_route_cc(r, ROUTE_DESTINATION)] != cc) continue;
    const float amount = bipolar(dialed[aurora_route_cc(r, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    aimed = true;
    if (swings(cc)) {
      const float mean = waveMean(dialed[aurora_route_cc(r, ROUTE_WAVE)]);
      const float reach = swingReach(cc, amount);
      const float atTrough = -reach * mean;
      const float atPeak = reach * (1.0f - mean);
      up += (atPeak > atTrough) ? atPeak : atTrough;
      down += (atPeak > atTrough) ? atTrough : atPeak;
    } else if (amount > 0.0f) {
      up += amount;
    } else {
      down += amount;
    }
  }
  if (swings(cc)) {
    const float dialedValue = convert(cc, dialed[cc]);
    low = nearestByte(cc, dialedValue + down);
    high = nearestByte(cc, dialedValue + up);
  } else {
    low = landing(cc, dialed[cc], down);
    high = landing(cc, dialed[cc], up);
  }
  return aimed;
}

}  // namespace render
