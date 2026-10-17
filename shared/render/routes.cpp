#include "routes.h"

#include <math.h>

#include "reading.h"
#include "render_math.h"

namespace render {

static const float SHORTEST_STAB_CYCLES = 0.06f;
static const float ONCE_END = 0.9999f;

static OneshotClock oneshotClock = { 0.0f, 1.0f, false };

void setOneshotClock(const OneshotClock &clock) { oneshotClock = clock; }

RouteTiming routeTiming(const float *dialed, uint8_t route) {
  const uint8_t ratio = roundedControl(dialed[routeByte(route, ROUTE_RATIO)]);
  return { route, aurora_route_ratio(ratio), aurora_route_once(ratio),
           aurora_route_phase(roundedControl(dialed[routeByte(route, ROUTE_PHASE)])) };
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

Wave waveOf(const float *dialed, uint8_t route) {
  return { roundedControl(dialed[routeByte(route, ROUTE_WAVE)]), roundedControl(dialed[routeByte(route, ROUTE_SMOOTH)]) };
}

static inline float eased(float x, float smooth) {
  const float straight = clampUnit(x);
  return straight + (raisedCosine(x) - straight) * smooth;
}

float lfoWave(float phase, Wave shaped) {
  const uint8_t wave = shaped.shape;
  const float smooth = (float)shaped.smooth / 127.0f;
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
    const float falling = eased(1.0f - u / decay, smooth) / soft;
    return (falling > 1.0f) ? 1.0f : falling;
  }
  if (attack > 0.0f && u >= 1.0f - attack) {
    return eased((u - (1.0f - attack)) / attack, smooth);
  }
  return 0.0f;
}

static bool refused(uint8_t cc) {
  switch (cc) {
    case CC_TEMPO_DIVISION:
    case CC_PALETTE:
    case CC_CELL_END:
    case CC_CELL_ACROSS:
    case CC_ENVELOPE_1_MOVES:
    case CC_ENVELOPE_2_MOVES:
    case CC_ENVELOPE_3_MOVES:
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

bool cellControl(uint8_t cc) {
  switch (cc) {
    case CC_CELL_POSITION:
    case CC_CELL_WIDTH:
    case CC_CELL_EDGE:
    case CC_CELL_BRIGHTNESS:
    case CC_HUE:
    case CC_SATURATION:
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
    case CC_CELL_BEND:
    case CC_CELL_BEND_AT:
    case CC_FAN_SPREAD:
    case CC_FAN_FREQUENCY:
    case CC_FAN_PHASE:
    case CC_FAN_RANDOMIZE:
    case CC_FAN_LFO:
      return true;
    default:
      return false;
  }
}

static const uint8_t INTEGRAL_STEPS = 128;

float waveMean(Wave wave) {
  float sum = 0.0f;
  for (uint8_t i = 0; i < INTEGRAL_STEPS; i++) {
    sum += lfoWave(((float)i + 0.5f) / (float)INTEGRAL_STEPS, wave);
  }
  return sum / (float)INTEGRAL_STEPS;
}

struct WaveIntegral {
  int16_t wave = -1;
  int16_t smooth = -1;
  float mean;
  float center;
  float total[INTEGRAL_STEPS + 1];
};

static WaveIntegral integrals[RENDER_ROUTES];

static const WaveIntegral &integralOf(uint8_t route, Wave wave) {
  WaveIntegral &integral = integrals[route];
  if (integral.wave == wave.shape && integral.smooth == wave.smooth) return integral;
  integral.wave = wave.shape;
  integral.smooth = wave.smooth;

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

static uint8_t sourceOf(const float *dialed, uint8_t route) {
  return aurora_route_source(roundedControl(dialed[routeByte(route, ROUTE_RATIO)]));
}

static void addPadRoute(uint8_t aimedAt, uint8_t destination, float amount, float level,
                        Modulation &out) {
  if (aurora_route_bipolar(aimedAt)) {
    out.bipolar[destination] += bipolarReach(destination, amount) * (level - 0.5f);
    return;
  }
  if (!swings(destination)) {
    out.amount[destination] += amount * level;
    return;
  }
  const float push = swingReach(destination, amount) * 0.5f * level;
  out.swing[destination] += push;
  out.padSwing[destination] += push;
}

static const uint8_t SWINGING[SWINGING_CONTROLS] = {
    CC_FIELD_SPEED, CC_FLOW_RATE, CC_SCATTER_RATE,
};

void carryPadShift(float elapsedBeats, float *carried, Modulation &modulation) {
  for (uint8_t i = 0; i < SWINGING_CONTROLS; i++) {
    const uint8_t cc = SWINGING[i];
    carried[i] += modulation.padSwing[cc] * elapsedBeats;
    modulation.shift[cc] += carried[i];
  }
}

static bool heldRoute(const float *dialed, uint8_t route) {
  return aurora_route_held(roundedControl(dialed[routeByte(route, ROUTE_HOLD)]));
}

static bool playsPerCell(const float *dialed, uint8_t route, uint8_t destination, uint8_t source) {
  if (aurora_source_is_envelope(source)) return true;
  return source == ROUTE_SOURCE_LFO && cellControl(destination) && heldRoute(dialed, route);
}

static float clockBefore(uint8_t route, float lfo, float beatsPerCycle, float beatsAgo) {
  if (oneshotRoute(route)) return oneshotClock.progress - beatsAgo / oneshotClock.beats;
  return lfo - beatsAgo / beatsPerCycle;
}

static float staggerOf(uint8_t route, uint8_t destination, float fanShift) {
  const bool oneshotUnstaggered = oneshotRoute(route) && !oneshotClock.staggered;
  return unstaggered(destination) || oneshotUnstaggered ? 0.0f : fanShift;
}

void gatherRoutesBefore(const float *dialed, float beatsPerCycle, float lfo, float fanShift,
                        const PadLevels *pad, float beatsAgo, Modulation &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    out.amount[i] = 0.0f;
    out.swing[i] = 0.0f;
    out.shift[i] = 0.0f;
    out.bipolar[i] = 0.0f;
    out.padSwing[i] = 0.0f;
  }

  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t aimedAt = roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]);
    if (aurora_route_arp(aimedAt) != ARP_UNISON) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (routeRefused(destination)) continue;

    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;

    const uint8_t source = sourceOf(dialed, route);
    if (playsPerCell(dialed, route, destination, source)) continue;
    if (source != ROUTE_SOURCE_LFO) {
      if (pad) addPadRoute(aimedAt, destination, amount, source == ROUTE_SOURCE_PAD_X ? pad->x : pad->y, out);
      continue;
    }
    if (spotDestination(destination)) continue;

    const RouteTiming timing = routeTiming(dialed, route);
    const Wave wave = waveOf(dialed, route);
    const float shift = staggerOf(route, destination, fanShift);
    const float phase = turnsOn(timing, clockBefore(route, lfo, beatsPerCycle, beatsAgo), shift);
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

void gatherRoutes(const float *dialed, float beatsPerCycle, float lfo, float fanShift,
                  const PadLevels *pad, Modulation &out) {
  gatherRoutesBefore(dialed, beatsPerCycle, lfo, fanShift, pad, 0.0f, out);
}

uint8_t gatherCellRoutes(const float *dialed, float fanShift, CellRoute *out) {
  uint8_t count = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t aimedAt = roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]);
    if (aurora_route_arp(aimedAt) != ARP_UNISON) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (!cellControl(destination)) continue;
    const uint8_t source = sourceOf(dialed, route);
    if (!playsPerCell(dialed, route, destination, source)) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    const Wave wave = waveOf(dialed, route);
    const bool envelope = aurora_source_is_envelope(source);
    out[count++] = {
      destination,
      route,
      source,
      wave,
      aurora_route_bipolar(aimedAt),
      amount,
      envelope ? 0.0f : integralOf(route, wave).mean,
      staggerOf(route, destination, fanShift),
      routeTiming(dialed, route),
    };
  }
  return count;
}

float envelopeLevel(uint8_t source, const float *envelopes) {
  return envelopes[source - ROUTE_SOURCE_ENVELOPE_1];
}

float heldLevel(const CellRoute &route, float lfo, float beatsPerCycle, float beatsAgo) {
  return lfoWave(turnsOn(route.timing, clockBefore(route.route, lfo, beatsPerCycle, beatsAgo), route.shift), route.wave);
}

uint8_t envelopesUsed(const float *dialed) {
  uint8_t used = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t source = sourceOf(dialed, route);
    if (!aurora_source_is_envelope(source)) continue;
    if (!roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)])) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    used |= (uint8_t)(1 << (source - ROUTE_SOURCE_ENVELOPE_1));
  }
  return used;
}

static bool travels(uint8_t cc) { return cc == CC_CELL_POSITION; }

static float landing(uint8_t cc, float base, float amount) {
  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(cc)) return base + amount * 64.0f;
  if (travels(cc)) return base + amount * 127.0f;

  const float limit = (amount >= 0.0f) ? 127.0f : 0.0f;
  return fminf(fmaxf(base + fabsf(amount) * (limit - base), 0.0f), 127.0f);
}

static float settled(uint8_t cc, float landed) {
  if (circular(cc)) return 128.0f * fract(landed / 128.0f);
  return fminf(fmaxf(landed, 0.0f), 127.0f);
}

float cellRouted(uint8_t cc, float dialed, float amount, float bipolar, const CellRoute *routes,
                 uint8_t count, const float *levels) {
  for (uint8_t i = 0; i < count; i++) {
    if (routes[i].cc != cc) continue;
    if (routes[i].bipolar) bipolar += bipolarReach(cc, routes[i].amount) * (levels[i] - routes[i].mean);
    else amount += routes[i].amount * levels[i];
  }
  if (fabsf(amount) < 0.001f && fabsf(bipolar) < 0.001f) return dialed;
  const float landed = landing(cc, dialed, amount) + bipolar;
  return travels(cc) ? landed : settled(cc, landed);
}

float landedControl(uint8_t cc, float base, float amount) {
  if (amount > -0.001f && amount < 0.001f) return base;
  return settled(cc, landing(cc, base, amount));
}

uint8_t displayedByte(uint8_t cc, float value) {
  const uint8_t rounded = roundedControl(settled(cc, value));
  return circular(cc) ? (uint8_t)(rounded % 128) : rounded;
}

uint8_t gatherSpotRoutes(const float *dialed, SpotRoute *out) {
  uint8_t count = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t aimedAt = roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]);
    if (aurora_route_arp(aimedAt) != ARP_UNISON) continue;
    const uint8_t destination = aurora_route_target(aimedAt);
    if (!spotDestination(destination) || sourceOf(dialed, route) != ROUTE_SOURCE_LFO) continue;
    const float amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    out[count++] = {
      destination,
      amount,
      waveOf(dialed, route),
      routeTiming(dialed, route),
    };
  }
  return count;
}

float routed(const float *dialed, const Modulation *modulation, uint8_t cc) {
  const float base = dialed[cc];
  if (!modulation || swings(cc)) return base;
  const float amount = modulation->amount[cc];
  const float bipolar = modulation->bipolar[cc];
  if (fabsf(amount) < 0.001f && fabsf(bipolar) < 0.001f) return base;

  return settled(cc, landing(cc, base, amount) + bipolar);
}

uint8_t routeTarget(const float *dialed, uint8_t route) {
  return aurora_route_target(roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]));
}

bool routeAims(const float *dialed, uint8_t cc, uint8_t routes) {
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

uint8_t routedForDisplay(const float *dialed, const Modulation *modulation, uint8_t cc) {
  if (!modulation || !swings(cc)) return displayedByte(cc, routed(dialed, modulation, cc));
  return nearestByte(cc, controlValue(cc, dialed[cc]) + modulation->swing[cc]);
}

bool routeReach(const float *dialed, uint8_t cc, int16_t &low, int16_t &high) {
  low = high = displayedByte(cc, dialed[cc]);
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
    const uint8_t source = sourceOf(dialed, route);
    const bool fromPad = source == ROUTE_SOURCE_PAD_X || source == ROUTE_SOURCE_PAD_Y;
    const bool fromEnvelope = aurora_source_is_envelope(source);
    if (aurora_route_bipolar(roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]))) {
      const float mean = fromEnvelope ? 0.0f : fromPad ? 0.5f : waveMean(waveOf(dialed, route));
      const float reach = bipolarReach(cc, amount);
      bipolarUp += fmaxf(-reach * mean, reach * (1.0f - mean));
      bipolarDown += fminf(-reach * mean, reach * (1.0f - mean));
    } else if (swings(cc)) {
      const float reach = swingReach(cc, amount);
      const float mean = fromPad || fromEnvelope ? 0.0f : waveMean(waveOf(dialed, route));
      const float atTrough = -reach * mean;
      const float atPeak = reach * ((fromPad ? 0.5f : 1.0f) - mean);
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
    low = (int16_t)lroundf(landing(cc, dialed[cc], down) + bipolarDown);
    high = (int16_t)lroundf(landing(cc, dialed[cc], up) + bipolarUp);
    if (!circular(cc)) {
      low = low < 0 ? 0 : low;
      high = high > 127 ? 127 : high;
    }
  }
  return aimed;
}

}
