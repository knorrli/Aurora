#include "routes.h"

#include <math.h>

#include "reading.h"
#include "render_math.h"

namespace render {

static const float SHORTEST_STAB_CYCLES = 0.06f;
static const float ONCE_END = 0.9999f;

static OneshotClock oneshotClock = { 0.0f, 1.0f, false };

void setOneshotClock(const OneshotClock &clock) { oneshotClock = clock; }

RouteTiming routeTiming(const uint8_t *dialed, uint8_t route) {
  const uint8_t ratio = dialed[routeByte(route, ROUTE_RATIO)];
  return { route, aurora_route_ratio(ratio), aurora_route_once(ratio),
           aurora_route_phase(dialed[routeByte(route, ROUTE_PHASE)]) };
}

float turnsOn(const RouteTiming &timing, float clock, float shift) {
  if (!timing.once) return clock * (float)timing.ratio + shift - timing.phase;
  const float turns = (fract(clock) - timing.phase) * (float)timing.ratio + shift;
  return turns < 0.0f ? 0.0f : (turns > ONCE_END ? ONCE_END : turns);
}

float routeTurns(const RouteTiming &timing, float lfo, float shift) {
  return turnsOn(timing, oneshotRoute(timing.route) ? oneshotClock.progress : lfo, shift);
}

static inline float raisedCosine(float x) {
  return 0.5f - 0.5f * cosf(0.5f * TURN * clampUnit(x));
}

float waveRise(uint8_t wave) {
  return (wave <= WAVE_FALL) ? 1.0f - (float)wave / (float)WAVE_FALL : 0.0f;
}

float lfoWave(float phase, uint8_t wave) {
  const float attack = waveRise(wave);
  float decay, hard;
  if (wave <= WAVE_FALL) {
    decay = 1.0f - attack;
    hard = 0.0f;
  } else {
    const float toSquare = (float)(wave - WAVE_FALL)
                         / (float)(WAVE_SQUARE - WAVE_FALL);
    hard = (toSquare > 1.0f) ? 1.0f : toSquare;
    decay = (wave <= WAVE_SQUARE)
        ? 1.0f - 0.5f * toSquare
        : 0.5f * powf(SHORTEST_STAB_CYCLES / 0.5f,
                      (float)(wave - WAVE_SQUARE)
                          / (float)(127 - WAVE_SQUARE));
  }

  const float u = fract(phase);
  if (decay > 0.0f && u <= decay) {
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
    case CC_PALETTE:
    case CC_SHAPE_BOUNCE:
    case CC_FIELD_FORM:
    case CC_FIELD_DIRECTION:
    case CC_LFO_RATE:
    case CC_PAR_HUE_LAYOUT:
    case CC_ARP_MODE:
      return true;
    default:
      return false;
  }
}

bool routeRefused(uint8_t cc) { return cc == 0 || refused(cc); }

static bool swings(uint8_t cc) {
  switch (cc) {
    case CC_FIELD_SPEED:
    case CC_FLOW_RATE:
    case CC_SHAPE_SPEED:
    case CC_FAN_SPEED:
    case CC_SCATTER_RATE:
      return true;
    default:
      return false;
  }
}

bool circular(uint8_t cc) {
  switch (cc) {
    case CC_HUE:
    case CC_PAR_HUE_OFFSET:
    case CC_FAN_PHASE:
      return true;
    default:
      return false;
  }
}

bool spotDestination(uint8_t cc) {
  switch (cc) {
    case CC_SCATTER_MIX:
    case CC_SCATTER_VALUE:
    case CC_SCATTER_HUE:
    case CC_SCATTER_SATURATION:
    case CC_SCATTER_WIDTH:
    case CC_SCATTER_POSITION:
      return true;
    default:
      return false;
  }
}

static bool unstaggered(uint8_t cc) {
  switch (cc) {
    case CC_PAR_VALUE:
    case CC_PAR_HUE_OFFSET:
    case CC_PAR_SATURATION:
    case CC_PAR_HUE_RANGE:
    case CC_ARP_SPREAD:
    case CC_SHAPE_COUNT:
    case CC_SHAPE_BEND:
    case CC_SHAPE_BEND_AT:
    case CC_FAN_SPREAD:
    case CC_FAN_FREQUENCY:
    case CC_FAN_PHASE:
    case CC_FAN_RANDOMIZE:
    case CC_FAN_SPEED:
    case CC_FAN_LFO:
      return true;
    default:
      return false;
  }
}

static const uint8_t INTEGRAL_STEPS = 128;

float waveMean(uint8_t wave) {
  float sum = 0.0f;
  for (uint8_t i = 0; i < INTEGRAL_STEPS; i++) {
    sum += lfoWave(((float)i + 0.5f) / (float)INTEGRAL_STEPS, wave);
  }
  return sum / (float)INTEGRAL_STEPS;
}

struct WaveIntegral {
  int16_t wave = -1;
  float mean;
  float center;
  float total[INTEGRAL_STEPS + 1];
};

static WaveIntegral integrals[RENDER_ROUTES];

static const WaveIntegral &integralOf(uint8_t route, uint8_t wave) {
  WaveIntegral &integral = integrals[route];
  if (integral.wave == wave) return integral;
  integral.wave = wave;

  integral.mean = waveMean(wave);
  integral.total[0] = 0.0f;
  float area = 0.0f;
  for (uint8_t i = 0; i < INTEGRAL_STEPS; i++) {
    const float sample = lfoWave(((float)i + 0.5f) / (float)INTEGRAL_STEPS, wave);
    integral.total[i + 1] = integral.total[i] + (sample - integral.mean) / (float)INTEGRAL_STEPS;
    area += 0.5f * (integral.total[i] + integral.total[i + 1]);
  }
  integral.total[INTEGRAL_STEPS] = 0.0f;
  integral.center = area / (float)INTEGRAL_STEPS;
  return integral;
}

static float totalAt(const WaveIntegral &integral, float phase) {
  const float at = fract(phase) * (float)INTEGRAL_STEPS;
  const uint8_t i = (uint8_t)at;
  if (i >= INTEGRAL_STEPS) return integral.total[INTEGRAL_STEPS] - integral.center;
  const float t = at - (float)i;
  return integral.total[i] + t * (integral.total[i + 1] - integral.total[i]) - integral.center;
}

static float bipolarReach(uint8_t cc, float amount) {
  return amount * (circular(cc) ? 128.0f : 127.0f);
}

static float swingReach(uint8_t cc, float amount) {
  const float share = (fabsf(amount) > 1.0f) ? 1.0f : fabsf(amount);
  const bool signedRate = controlValue(cc, 0) < 0.0f;
  const long byte = signedRate ? 64 + lroundf(share * 63.0f) : lroundf(share * 127.0f);
  return 2.0f * ((amount < 0.0f) ? -1.0f : 1.0f) * fabsf(controlValue(cc, (uint8_t)byte));
}

void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float lfo, float fanShift,
                  Modulation &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    out.amount[i] = 0.0f;
    out.swing[i] = 0.0f;
    out.shift[i] = 0.0f;
    out.bipolar[i] = 0.0f;
  }

  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t aimedAt = dialed[routeByte(route, ROUTE_DESTINATION)];
    if (aurora_route_arp(aimedAt) != ARP_UNISON) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (routeRefused(destination) || spotDestination(destination)) continue;

    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;

    const RouteTiming timing = routeTiming(dialed, route);
    const uint8_t wave = dialed[routeByte(route, ROUTE_WAVE)];
    const bool oneshotUnstaggered = oneshotRoute(route) && !oneshotClock.staggered;
    const float shift = unstaggered(destination) || oneshotUnstaggered ? 0.0f : fanShift;
    const float phase = routeTurns(timing, lfo, shift);
    const float cycleBeats = oneshotRoute(route) ? oneshotClock.beats : beatsPerCycle;

    if (aurora_route_bipolar(aimedAt)) {
      const float mean = integralOf(route, wave).mean;
      out.bipolar[destination] += bipolarReach(destination, amount) * (lfoWave(phase, wave) - mean);
      continue;
    }

    if (!swings(destination)) {
      out.amount[destination] += amount * lfoWave(phase, wave);
      continue;
    }

    const WaveIntegral &integral = integralOf(route, wave);
    const float reach = swingReach(destination, amount);
    out.swing[destination] += reach * (lfoWave(phase, wave) - integral.mean);
    out.shift[destination] += reach * totalAt(integral, phase) * cycleBeats / (float)timing.ratio;
  }
}

static int16_t landing(uint8_t cc, uint8_t base, float amount) {
  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(cc)) {
    return (int16_t)base + (int16_t)lroundf(amount * 64.0f);
  }

  const float limit = (amount >= 0.0f) ? 127.0f : 0.0f;
  const long reached = lroundf((float)base + fabsf(amount) * (limit - (float)base));
  return (int16_t)(reached < 0 ? 0 : (reached > 127 ? 127 : reached));
}

static uint8_t settled(uint8_t cc, int16_t landed) {
  if (circular(cc)) return (uint8_t)((landed % 128 + 128) % 128);
  return (uint8_t)(landed < 0 ? 0 : (landed > 127 ? 127 : landed));
}

uint8_t landedByte(uint8_t cc, uint8_t base, float amount) {
  if (amount > -0.001f && amount < 0.001f) return base;
  return settled(cc, landing(cc, base, amount));
}

uint8_t gatherSpotRoutes(const uint8_t *dialed, SpotRoute *out) {
  uint8_t count = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t aimedAt = dialed[routeByte(route, ROUTE_DESTINATION)];
    if (aurora_route_arp(aimedAt) != ARP_UNISON) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (!spotDestination(destination)) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    out[count++] = {
      destination,
      amount,
      dialed[routeByte(route, ROUTE_WAVE)],
      routeTiming(dialed, route),
    };
  }
  return count;
}

uint8_t routed(const uint8_t *dialed, const Modulation *modulation, uint8_t cc) {
  const uint8_t base = dialed[cc];
  if (!modulation || swings(cc)) return base;
  const float amount = modulation->amount[cc];
  const float bipolar = modulation->bipolar[cc];
  if (fabsf(amount) < 0.001f && fabsf(bipolar) < 0.001f) return base;

  return settled(cc, landing(cc, base, amount) + (int16_t)lroundf(bipolar));
}

uint8_t routeTarget(const uint8_t *dialed, uint8_t route) {
  return aurora_route_target(dialed[routeByte(route, ROUTE_DESTINATION)]);
}

bool routeAims(const uint8_t *dialed, uint8_t cc, uint8_t routes) {
  for (uint8_t route = 0; route < routes; route++) {
    if (routeTarget(dialed, route) != cc) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount < -0.001f || amount > 0.001f) return true;
  }
  return false;
}

static uint8_t nearestByte(uint8_t cc, float value) {
  uint8_t best = 0;
  float bestGap = fabsf(controlValue(cc, 0) - value);
  for (uint8_t candidate = 1; candidate < 128; candidate++) {
    const float gap = fabsf(controlValue(cc, candidate) - value);
    if (gap < bestGap) {
      best = candidate;
      bestGap = gap;
    }
  }
  return best;
}

uint8_t routedForDisplay(const uint8_t *dialed, const Modulation *modulation, uint8_t cc) {
  if (!modulation || !swings(cc)) return routed(dialed, modulation, cc);
  return nearestByte(cc, controlValue(cc, dialed[cc]) + modulation->swing[cc]);
}

bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high) {
  low = high = dialed[cc];
  if (routeRefused(cc)) return false;

  float up = 0.0f;
  float down = 0.0f;
  float bipolarUp = 0.0f;
  float bipolarDown = 0.0f;
  bool aimed = false;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    if (routeTarget(dialed, route) != cc) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    aimed = true;
    if (aurora_route_bipolar(dialed[routeByte(route, ROUTE_DESTINATION)])) {
      const float mean = waveMean(dialed[routeByte(route, ROUTE_WAVE)]);
      const float reach = bipolarReach(cc, amount);
      bipolarUp += fmaxf(-reach * mean, reach * (1.0f - mean));
      bipolarDown += fminf(-reach * mean, reach * (1.0f - mean));
    } else if (swings(cc)) {
      const float mean = waveMean(dialed[routeByte(route, ROUTE_WAVE)]);
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
    const float value = controlValue(cc, dialed[cc]);
    low = nearestByte(cc, value + down);
    high = nearestByte(cc, value + up);
  } else {
    low = landing(cc, dialed[cc], down) + (int16_t)lroundf(bipolarDown);
    high = landing(cc, dialed[cc], up) + (int16_t)lroundf(bipolarUp);
    if (!circular(cc)) {
      low = low < 0 ? 0 : low;
      high = high > 127 ? 127 : high;
    }
  }
  return aimed;
}

}
