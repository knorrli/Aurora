#include "routes.h"

#include <math.h>

#include "reading.h"
#include "render_math.h"

namespace render {

static const float SHORTEST_STAB_CYCLES = 0.06f;

static inline float raisedCosine(float x) {
  return 0.5f - 0.5f * cosf(0.5f * TURN * clampUnit(x));
}

float waveRise(uint8_t wave) {
  return (wave <= WAVE_SNAP) ? 1.0f - (float)wave / (float)WAVE_SNAP : 0.0f;
}

float lfoWave(float phase, uint8_t wave) {
  const float attack = waveRise(wave);
  float decay, hard;
  if (wave <= WAVE_SNAP) {
    decay = 1.0f - attack;
    hard = 0.0f;
  } else {
    const float toSquare = (float)(wave - WAVE_SNAP)
                         / (float)(WAVE_SQUARE - WAVE_SNAP);
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
    case CC_PAR_HUE_SOURCE:
    case CC_ARP_MODE:
    case CC_ARP_REVERSE:
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

static bool circular(uint8_t cc) {
  switch (cc) {
    case CC_HUE:
    case CC_PAR_HUE_OFFSET:
    case CC_SHAPE_POSITION:
    case CC_FAN_PHASE:
      return true;
    default:
      return false;
  }
}

static bool plainLfo(uint8_t cc) {
  switch (cc) {
    case CC_PAR_VALUE:
    case CC_PAR_HUE_OFFSET:
    case CC_PAR_SATURATION:
    case CC_PAR_HUE_RANGE:
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

static WaveIntegral integrals[AURORA_ROUTES];

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

static float swingReach(uint8_t cc, float amount) {
  const float share = (fabsf(amount) > 1.0f) ? 1.0f : fabsf(amount);
  const bool bipolarRate = controlValue(cc, 0) < 0.0f;
  const long byte = bipolarRate ? 64 + lroundf(share * 63.0f) : lroundf(share * 127.0f);
  return 2.0f * ((amount < 0.0f) ? -1.0f : 1.0f) * fabsf(controlValue(cc, (uint8_t)byte));
}

void gatherRoutes(const uint8_t *dialed, float beatsPerCycle, float plainPhase,
                  float stripPhase, Pushes &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    out.amount[i] = 0.0f;
    out.swing[i] = 0.0f;
    out.shift[i] = 0.0f;
  }

  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t aimedAt = dialed[aurora_route_cc(route, ROUTE_DESTINATION)];
    if (aurora_route_arp(aimedAt) != ARP_OFF) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (routeRefused(destination)) continue;

    const float amount = bipolarOf(dialed[aurora_route_cc(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;

    const uint8_t ratio = aurora_route_ratio(dialed[aurora_route_cc(route, ROUTE_RATIO)]);
    const uint8_t wave = dialed[aurora_route_cc(route, ROUTE_WAVE)];
    const float delay = (float)dialed[aurora_route_cc(route, ROUTE_PHASE)] / 128.0f;
    const float lfo = plainLfo(destination) ? plainPhase : stripPhase;
    const float phase = lfo * (float)ratio - delay;

    if (!swings(destination)) {
      out.amount[destination] += amount * lfoWave(phase, wave);
      continue;
    }

    const WaveIntegral &integral = integralOf(route, wave);
    const float reach = swingReach(destination, amount);
    out.swing[destination] += reach * (lfoWave(phase, wave) - integral.mean);
    out.shift[destination] += reach * totalAt(integral, phase) * beatsPerCycle / (float)ratio;
  }
}

static int16_t landing(uint8_t cc, uint8_t base, float amount) {
  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(cc)) {
    const float span = (cc == CC_SHAPE_POSITION) ? 128.0f : 64.0f;
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

uint8_t routeTarget(const uint8_t *dialed, uint8_t route) {
  return aurora_route_target(dialed[aurora_route_cc(route, ROUTE_DESTINATION)]);
}

bool routeAims(const uint8_t *dialed, uint8_t cc) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    if (routeTarget(dialed, route) != cc) continue;
    const float amount = bipolarOf(dialed[aurora_route_cc(route, ROUTE_AMOUNT)]);
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

uint8_t routedForDisplay(const uint8_t *dialed, const Pushes *pushes, uint8_t cc) {
  if (!pushes || !swings(cc)) return routed(dialed, pushes, cc);
  return nearestByte(cc, dialedValue(cc, dialed[cc]) + pushes->swing[cc]);
}

bool routeReach(const uint8_t *dialed, uint8_t cc, int16_t &low, int16_t &high) {
  low = high = dialed[cc];
  if (routeRefused(cc)) return false;

  float up = 0.0f;
  float down = 0.0f;
  bool aimed = false;
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    if (routeTarget(dialed, route) != cc) continue;
    const float amount = bipolarOf(dialed[aurora_route_cc(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    aimed = true;
    if (swings(cc)) {
      const float mean = waveMean(dialed[aurora_route_cc(route, ROUTE_WAVE)]);
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
    const float value = dialedValue(cc, dialed[cc]);
    low = nearestByte(cc, value + down);
    high = nearestByte(cc, value + up);
  } else {
    low = landing(cc, dialed[cc], down);
    high = landing(cc, dialed[cc], up);
  }
  return aimed;
}

}
