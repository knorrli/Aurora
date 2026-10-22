#include "routes.h"

#include <math.h>

#include "arp.h"
#include "reading.h"
#include "render_math.h"

namespace render {

static const float SHORTEST_STAB_CYCLES = 0.06f;
static const float ONCE_END = 0.9999f;

static OneshotClock oneshotClock = { 0.0f, 1.0f, 0 };

void setOneshotClock(const OneshotClock &clock) { oneshotClock = clock; }

static uint8_t fieldOf(const float *dialed, uint8_t route, uint8_t field) {
  return roundedControl(dialed[routeControl(route, field)]);
}

RouteTiming routeTiming(const float *dialed, uint8_t route) {
  return { route, aurora_route_ratio(fieldOf(dialed, route, ROUTE_RATIO)),
           aurora_route_once(fieldOf(dialed, route, ROUTE_CYCLE)),
           aurora_route_phase(fieldOf(dialed, route, ROUTE_PHASE)) };
}

uint8_t routeTarget(const float *dialed, uint8_t route) { return fieldOf(dialed, route, ROUTE_DESTINATION); }

bool routeArpeggiates(const float *dialed, uint8_t route) {
  return aurora_route_arp(fieldOf(dialed, route, ROUTE_ARP)) != ARP_UNISON
      && arpCapable(routeTarget(dialed, route));
}

uint32_t routeDraw(uint8_t route) { return oneshotRoute(route) ? oneshotClock.fire : 0; }

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
  return { roundedControl(dialed[routeControl(route, ROUTE_WAVE)]), roundedControl(dialed[routeControl(route, ROUTE_SMOOTH)]) };
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

static bool refused(uint8_t control) {
  switch (control) {
    case CONTROL_TEMPO_DIVISION:
    case CONTROL_PALETTE:
    case CONTROL_CELL_ENDS:
    case CONTROL_CELL_END:
    case CONTROL_CELL_RATE:
    case CONTROL_CELL_DIRECTION:
    case CONTROL_ENVELOPE_1_MOVES:
    case CONTROL_ENVELOPE_2_MOVES:
    case CONTROL_ENVELOPE_3_MOVES:
    case CONTROL_FIELD_FORM:
    case CONTROL_FIELD_DIRECTION:
    case CONTROL_LFO_RATE:
    case CONTROL_PAR_HUE_LAYOUT:
    case CONTROL_ARP_MODE:
    case CONTROL_ARP_DIRECTION:
      return true;
    default:
      return false;
  }
}

bool routeRefused(uint8_t control) { return control == 0 || control >= AURORA_FIRST_ROUTE_CONTROL || refused(control); }

static bool stripArpRefused(uint8_t control) {
  switch (control) {
    case CONTROL_PAR_HUE_RANGE:
    case CONTROL_ARP_SPREAD:
    case CONTROL_CELL_BEND:
    case CONTROL_CELL_BEND_AT:
    case CONTROL_CELL_DELAY:
      return true;
    default:
      return false;
  }
}

static bool swings(uint8_t control) {
  switch (control) {
    case CONTROL_FIELD_SPEED:
    case CONTROL_FLOW_RATE:
    case CONTROL_SCATTER_RATE:
      return true;
    default:
      return false;
  }
}

bool stripArpControl(uint8_t control) {
  return !routeRefused(control) && !aurora_par_arp_control(control) && !stripArpRefused(control) && !swings(control);
}

bool arpCapable(uint8_t control) { return aurora_par_arp_control(control) || stripArpControl(control); }

bool circular(uint8_t control) {
  switch (control) {
    case CONTROL_HUE:
    case CONTROL_PAR_HUE_OFFSET:
      return true;
    default:
      return false;
  }
}

bool spotDestination(uint8_t control) {
  switch (control) {
    case CONTROL_SCATTER_MIX:
    case CONTROL_SCATTER_VALUE:
    case CONTROL_SCATTER_HUE:
    case CONTROL_SCATTER_SATURATION:
    case CONTROL_SCATTER_WIDTH:
    case CONTROL_SCATTER_POSITION:
      return true;
    default:
      return false;
  }
}

bool cellControl(uint8_t control) {
  switch (control) {
    case CONTROL_CELL_POSITION:
    case CONTROL_CELL_DELAY:
    case CONTROL_CELL_WIDTH:
    case CONTROL_CELL_EDGE:
    case CONTROL_CELL_BRIGHTNESS:
    case CONTROL_HUE:
    case CONTROL_SATURATION:
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

static float bipolarReach(uint8_t control, float amount) {
  return amount * (circular(control) ? 128.0f : 127.0f);
}

static float swingReach(uint8_t control, float amount) {
  const float share = (fabsf(amount) > 1.0f) ? 1.0f : fabsf(amount);
  const bool signedRate = controlValue(control, 0) < 0.0f;
  const long byte = signedRate ? 64 + lroundf(share * 63.0f) : lroundf(share * 127.0f);
  return 2.0f * ((amount < 0.0f) ? -1.0f : 1.0f) * fabsf(controlValue(control, (uint8_t)byte));
}

static uint8_t sourceOf(const float *dialed, uint8_t route) {
  return aurora_route_source(fieldOf(dialed, route, ROUTE_SOURCE));
}

static void addSteadyRoute(bool bipolar, uint8_t destination, float amount, float level, Modulation &out) {
  if (bipolar) {
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
    CONTROL_FIELD_SPEED, CONTROL_FLOW_RATE, CONTROL_SCATTER_RATE,
};

void carryPadShift(float elapsedBeats, float *carried, Modulation &modulation) {
  for (uint8_t i = 0; i < SWINGING_CONTROLS; i++) {
    const uint8_t control = SWINGING[i];
    carried[i] += modulation.padSwing[control] * elapsedBeats;
    modulation.shift[control] += carried[i];
  }
}

static const uint8_t SHUFFLE_SALT = 53;
static const uint8_t BIRTH_SALT = 89;

static_assert(STRIPS <= MAX_CHORD, "");

static float clockBefore(uint8_t route, float lfo, float beatsPerCycle, float beatsAgo) {
  if (oneshotRoute(route)) return oneshotClock.progress - beatsAgo / oneshotClock.beats;
  return lfo - beatsAgo / beatsPerCycle;
}

static float stretched(uint8_t group, uint8_t groups) {
  return groups > 1 ? (float)group / (float)(groups - 1) : 0.5f;
}

static float pairsLevel(uint8_t index, uint8_t count) {
  if (count % 2 == 0) return stretched((uint8_t)(index / 2), (uint8_t)(count / 2));
  const uint8_t middle = (uint8_t)(count / 2);
  const uint8_t sideGroups = (uint8_t)((middle + 1) / 2);
  const uint8_t groups = (uint8_t)(2 * sideGroups + 1);
  if (index == middle) return stretched(sideGroups, groups);
  if (index < middle) return stretched((uint8_t)(index / 2), groups);
  return stretched((uint8_t)(groups - 1 - (count - 1 - index) / 2), groups);
}

static uint8_t shuffledRank(uint8_t index, uint8_t count, uint32_t salt) {
  if (count > MAX_CHORD) return index;
  uint8_t order[MAX_CHORD];
  for (uint8_t i = 0; i < count; i++) order[i] = i;
  for (uint8_t i = count - 1; i > 0; i--) {
    const uint8_t j = (uint8_t)(hash32(salt, i, SHUFFLE_SALT) % (i + 1));
    const uint8_t held = order[i];
    order[i] = order[j];
    order[j] = held;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (order[i] == index) return i;
  }
  return index;
}

float groupLevel(uint8_t pattern, uint8_t index, uint8_t count, uint32_t salt) {
  if (count == 0 || index >= count) return 0.5f;
  const uint8_t groups = (uint8_t)((count + 1) / 2);
  switch (pattern) {
    case GROUP_MIRROR: {
      const int16_t fromCenter = 2 * (int16_t)index - (count - 1);
      return stretched((uint8_t)((fromCenter < 0 ? -fromCenter : fromCenter) / 2), groups);
    }
    case GROUP_EVENS_ODDS: return (float)(index % 2);
    case GROUP_PAIRS: return pairsLevel(index, count);
    case GROUP_RANDOM: return stretched(shuffledRank(index, count, salt), count);
    default: return stretched(index, count);
  }
}

static float birthLevel(uint8_t pattern, const CellMember &cell, uint32_t salt) {
  switch (pattern) {
    case GROUP_EVENS_ODDS: return (float)(((cell.birthNumber % 2) + 2) % 2);
    case GROUP_RANDOM: return (float)(hash32((uint32_t)cell.birthNumber, salt, BIRTH_SALT) & 0xFF) / 255.0f;
    default: return groupLevel(pattern, cell.birthIndex, cell.birthsInCycle, salt);
  }
}

static uint32_t groupSalt(uint8_t route, uint32_t birth) {
  const uint32_t draw = routeDraw(route);
  return draw || birth ? hash32(route, draw, birth) : route;
}

static bool perCellSource(const RouteReading &reading) {
  return aurora_source_is_envelope(reading.source) || reading.source == ROUTE_SOURCE_NOTE
      || reading.source == ROUTE_SOURCE_BIRTH || (reading.source == ROUTE_SOURCE_LFO && reading.held);
}

static float sourceMean(uint8_t route, uint8_t source, Wave wave) {
  if (aurora_source_is_envelope(source)) return 0.0f;
  if (source == ROUTE_SOURCE_LFO) return integralOf(route, wave).mean;
  return 0.5f;
}

static uint8_t partnerBase(uint8_t route) { return oneshotRoute(route) ? AURORA_ROUTES : 0; }

void readRoutes(const float *dialed, Routes &out) {
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    RouteReading &reading = out.routes[route];
    const uint8_t target = routeTarget(dialed, route);
    uint8_t aimedRoute, aimedField;
    reading.aimsRoute = aurora_route_field_of(target, aimedRoute, aimedField)
                     && (aimedField == ROUTE_AMOUNT || aimedField == ROUTE_PHASE);
    reading.aimed = reading.aimsRoute ? (uint8_t)(partnerBase(route) + aimedRoute) : 0;
    reading.aimsPhase = reading.aimsRoute && aimedField == ROUTE_PHASE;
    const bool plays = !routeArpeggiates(dialed, route) && !routeRefused(target);
    reading.destination = plays ? target : 0;
    reading.source = sourceOf(dialed, route);
    reading.held = aurora_route_held(roundedControl(dialed[routeControl(route, ROUTE_HOLD)]));
    reading.bipolar = aurora_route_bipolar(roundedControl(dialed[routeControl(route, ROUTE_POLARITY)]))
                   && aurora_bipolar_capable(target);
    reading.amount = (plays || reading.aimsRoute) ? signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]) : 0.0f;
    reading.share = dialed[routeShare(route)];
    reading.wave = waveOf(dialed, route);
    reading.pattern = aurora_group_pattern(fieldOf(dialed, route, ROUTE_PATTERN));
    reading.mean = sourceMean(route, reading.source, reading.wave);
    reading.timing = routeTiming(dialed, route);
  }
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    RouteReading &reading = out.routes[route];
    reading.perCell = cellControl(reading.destination)
        && (reading.destination == CONTROL_CELL_DELAY || perCellSource(reading));
  }
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const RouteReading &aim = out.routes[route];
    if (!aim.aimsRoute || !perCellSource(aim)) continue;
    for (uint8_t version = 0; version < RENDER_ROUTES; version++) {
      RouteReading &aimed = out.routes[version];
      if (versionOf(version) == aim.aimed && cellControl(aimed.destination)) aimed.perCell = true;
    }
  }
}

static bool levelOf(const RouteReading &reading, uint8_t route, const RouteContext &context, float shift,
                    float &out) {
  const CellMember *cell = context.cell;
  switch (reading.source) {
    case ROUTE_SOURCE_LFO: {
      const float ago = reading.held && cell ? cell->age : context.beatsAgo;
      out = lfoWave(turnsOn(reading.timing, clockBefore(route, context.lfo, context.beatsPerCycle, ago), shift),
                    reading.wave);
      return true;
    }
    case ROUTE_SOURCE_PAD_X:
    case ROUTE_SOURCE_PAD_Y:
      if (!context.pad) return false;
      out = reading.source == ROUTE_SOURCE_PAD_X ? context.pad->x : context.pad->y;
      return true;
    case ROUTE_SOURCE_STRIP:
      if (context.strip < 0) return false;
      out = groupLevel(reading.pattern, (uint8_t)context.strip, STRIPS, groupSalt(route, 0));
      return true;
    case ROUTE_SOURCE_NOTE:
      if (!cell) return false;
      out = groupLevel(reading.pattern, cell->note, cell->chord, groupSalt(route, (uint32_t)cell->birthNumber));
      return true;
    case ROUTE_SOURCE_BIRTH:
      if (!cell) return false;
      out = birthLevel(reading.pattern, *cell, groupSalt(route, 0));
      return true;
    default:
      if (!cell || !cell->envelopes) return false;
      out = cell->envelopes[reading.source - ROUTE_SOURCE_ENVELOPE_1];
      return true;
  }
}

static void reverse(const RouteReading &reading, float &amount, float &level) {
  if (!aurora_source_is_group(reading.source) || amount >= 0.0f) return;
  amount = -amount;
  level = 1.0f - level;
}

static const float AMOUNT_AIM_REACH = 2.0f;

static float aimedAt(const Routes &routes, uint8_t target, bool phase, const RouteContext &context) {
  float sum = 0.0f;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const RouteReading &aim = routes.routes[route];
    if (!aim.aimsRoute || aim.aimed != versionOf(target) || aim.aimsPhase != phase) continue;
    float level;
    if (!levelOf(aim, route, context, 0.0f, level)) continue;
    float amount = aim.amount;
    reverse(aim, amount, level);
    sum += amount * (level - aim.mean) * (phase ? 1.0f : AMOUNT_AIM_REACH * routes.routes[target].share);
  }
  return sum;
}

static float amountOf(const Routes &routes, uint8_t route, const RouteContext &context);

float routeShiftAt(const Routes &routes, uint8_t route, const RouteContext &context) {
  return aimedAt(routes, route, true, context);
}

float routeAmountAt(const Routes &routes, uint8_t route, const RouteContext &context) {
  return amountOf(routes, route, context);
}

static float amountOf(const Routes &routes, uint8_t route, const RouteContext &context) {
  const float amount = routes.routes[route].amount + aimedAt(routes, route, false, context);
  return amount > 1.0f ? 1.0f : (amount < -1.0f ? -1.0f : amount);
}

static void addStripArpRoutes(const float *dialed, float beatsPerCycle, float lfo, uint8_t strip, float beatsAgo,
                              Modulation &out) {
  const Arp arp = readArp(dialed, &out, STRIPS);
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    ArpRoute arpRoute;
    if (!readArpRoute(dialed, route, arpRoute) || !stripArpControl(arpRoute.target)) continue;
    const float turns = turnsOn(arpRoute.timing, clockBefore(route, lfo, beatsPerCycle, beatsAgo), 0.0f);
    out.amount[arpRoute.target] += arpRoute.amount * arpLevel(arpRoute, arp, turns, strip);
  }
}

void gatherRoutesBefore(const float *dialed, float beatsPerCycle, float lfo, int8_t strip,
                        const PadLevels *pad, float beatsAgo, Modulation &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) {
    out.amount[i] = 0.0f;
    out.swing[i] = 0.0f;
    out.shift[i] = 0.0f;
    out.bipolar[i] = 0.0f;
    out.padSwing[i] = 0.0f;
  }

  static Routes routes;
  readRoutes(dialed, routes);
  const RouteContext context = { lfo, beatsPerCycle, beatsAgo, pad, strip, nullptr };
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const RouteReading &reading = routes.routes[route];
    const uint8_t destination = reading.destination;
    if (!destination || reading.perCell) continue;
    if (reading.source == ROUTE_SOURCE_LFO && spotDestination(destination)) continue;

    float amount = amountOf(routes, route, context);
    if (amount > -0.001f && amount < 0.001f) continue;

    if (reading.source != ROUTE_SOURCE_LFO) {
      float level;
      if (!levelOf(reading, route, context, 0.0f, level)) continue;
      reverse(reading, amount, level);
      addSteadyRoute(reading.bipolar, destination, amount, level, out);
      continue;
    }

    const float shift = aimedAt(routes, route, true, context);
    const float phase = turnsOn(reading.timing, clockBefore(route, lfo, beatsPerCycle, beatsAgo), shift);
    const float cycleBeats = oneshotRoute(route) ? oneshotClock.beats : beatsPerCycle;
    const float level = lfoWave(phase, reading.wave);

    if (reading.bipolar) {
      out.bipolar[destination] += bipolarReach(destination, amount) * (level - reading.mean);
      continue;
    }

    if (!swings(destination)) {
      out.amount[destination] += amount * level;
      continue;
    }

    const WaveIntegral &integral = integralOf(route, reading.wave);
    const float reach = swingReach(destination, amount);
    out.swing[destination] += reach * (level - integral.mean);
    out.shift[destination] += reach * totalAt(integral, phase) * cycleBeats / (float)reading.timing.ratio;
  }
  if (strip >= 0) addStripArpRoutes(dialed, beatsPerCycle, lfo, (uint8_t)strip, beatsAgo, out);
}

void gatherRoutes(const float *dialed, float beatsPerCycle, float lfo, int8_t strip,
                  const PadLevels *pad, Modulation &out) {
  gatherRoutesBefore(dialed, beatsPerCycle, lfo, strip, pad, 0.0f, out);
}

Push cellPush(const Routes &routes, uint8_t control, const RouteContext &context) {
  Push push = { 0.0f, 0.0f };
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const RouteReading &reading = routes.routes[route];
    if (!reading.perCell || reading.destination != control) continue;
    float amount = amountOf(routes, route, context);
    if (amount > -0.001f && amount < 0.001f) continue;
    float level;
    if (!levelOf(reading, route, context, aimedAt(routes, route, true, context), level)) continue;
    reverse(reading, amount, level);
    if (reading.bipolar) push.bipolar += bipolarReach(control, amount) * (level - reading.mean);
    else push.amount += amount * level;
  }
  return push;
}

uint8_t envelopesUsed(const float *dialed) {
  uint8_t used = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    const uint8_t source = sourceOf(dialed, route);
    if (!aurora_source_is_envelope(source)) continue;
    if (!roundedControl(dialed[routeControl(route, ROUTE_DESTINATION)])) continue;
    const float amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    used |= (uint8_t)(1 << (source - ROUTE_SOURCE_ENVELOPE_1));
  }
  return used;
}

static bool travels(uint8_t control) { return control == CONTROL_CELL_POSITION || control == CONTROL_CELL_DELAY; }

static float landing(uint8_t control, float base, float amount) {
  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(control)) return base + amount * 64.0f;
  if (travels(control)) return base + amount * 127.0f;

  const float limit = (amount >= 0.0f) ? 127.0f : 0.0f;
  return fminf(fmaxf(base + fabsf(amount) * (limit - base), 0.0f), 127.0f);
}

static float settled(uint8_t control, float landed) {
  if (circular(control)) return 128.0f * fract(landed / 128.0f);
  return fminf(fmaxf(landed, 0.0f), 127.0f);
}

float cellRouted(uint8_t control, float dialed, float amount, float bipolar, float chord) {
  if (fabsf(amount) < 0.001f && fabsf(bipolar) < 0.001f) return dialed;
  if (travels(control)) {
    const float divisor = control == CONTROL_CELL_POSITION && chord > 1.0f ? chord : 1.0f;
    return dialed + (amount * 127.0f + bipolar) / divisor;
  }
  return settled(control, landing(control, dialed, amount) + bipolar);
}

float landedControl(uint8_t control, float base, float amount) {
  if (amount > -0.001f && amount < 0.001f) return base;
  return settled(control, landing(control, base, amount));
}

uint8_t displayedByte(uint8_t control, float value) {
  const uint8_t rounded = roundedControl(settled(control, value));
  return circular(control) ? (uint8_t)(rounded % 128) : rounded;
}

uint8_t gatherSpotRoutes(const float *dialed, SpotRoute *out) {
  uint8_t count = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    if (routeArpeggiates(dialed, route)) continue;
    const uint8_t destination = routeTarget(dialed, route);
    if (!spotDestination(destination) || sourceOf(dialed, route) != ROUTE_SOURCE_LFO) continue;
    const float amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
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

float routed(const float *dialed, const Modulation *modulation, uint8_t control) {
  const float base = dialed[control];
  if (!modulation || swings(control)) return base;
  const float amount = modulation->amount[control];
  const float bipolar = modulation->bipolar[control];
  if (fabsf(amount) < 0.001f && fabsf(bipolar) < 0.001f) return base;

  return settled(control, landing(control, base, amount) + bipolar);
}

bool routeAims(const float *dialed, uint8_t control, uint8_t routes) {
  for (uint8_t route = 0; route < routes; route++) {
    if (routeTarget(dialed, route) != control) continue;
    const float amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
    if (amount < -0.001f || amount > 0.001f) return true;
  }
  return false;
}

static uint8_t nearestByte(uint8_t control, float value) {
  uint8_t best = 0;
  float bestGap = fabsf(controlValue(control, 0) - value);
  for (uint8_t candidate = 1; candidate < 128; candidate++) {
    const float gap = fabsf(controlValue(control, candidate) - value);
    if (gap < bestGap) {
      best = candidate;
      bestGap = gap;
    }
  }
  return best;
}

uint8_t routedForDisplay(const float *dialed, const Modulation *modulation, uint8_t control) {
  if (!modulation || !swings(control)) return displayedByte(control, routed(dialed, modulation, control));
  return nearestByte(control, controlValue(control, dialed[control]) + modulation->swing[control]);
}

bool routeReach(const float *dialed, uint8_t control, int16_t &low, int16_t &high) {
  low = high = displayedByte(control, dialed[control]);
  if (routeRefused(control)) return false;

  float up = 0.0f;
  float down = 0.0f;
  float bipolarUp = 0.0f;
  float bipolarDown = 0.0f;
  bool aimed = false;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    if (routeTarget(dialed, route) != control) continue;
    const float amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;
    aimed = true;
    const uint8_t source = sourceOf(dialed, route);
    const bool fromPad = source == ROUTE_SOURCE_PAD_X || source == ROUTE_SOURCE_PAD_Y;
    const bool fromEnvelope = aurora_source_is_envelope(source);
    const bool steady = fromPad || aurora_source_is_group(source);
    const bool bipolar = aurora_route_bipolar(roundedControl(dialed[routeControl(route, ROUTE_POLARITY)]))
                      && aurora_bipolar_capable(control);
    if (bipolar) {
      const float mean = fromEnvelope ? 0.0f : steady ? 0.5f : waveMean(waveOf(dialed, route));
      const float reach = bipolarReach(control, amount);
      bipolarUp += fmaxf(-reach * mean, reach * (1.0f - mean));
      bipolarDown += fminf(-reach * mean, reach * (1.0f - mean));
    } else if (swings(control)) {
      const float reach = swingReach(control, amount);
      const float mean = steady || fromEnvelope ? 0.0f : waveMean(waveOf(dialed, route));
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
  if (swings(control)) {
    const float value = controlValue(control, dialed[control]);
    low = nearestByte(control, value + down);
    high = nearestByte(control, value + up);
  } else {
    low = (int16_t)lroundf(landing(control, dialed[control], down) + bipolarDown);
    high = (int16_t)lroundf(landing(control, dialed[control], up) + bipolarUp);
    if (!circular(control)) {
      low = low < 0 ? 0 : low;
      high = high > 127 ? 127 : high;
    }
  }
  return aimed;
}

}
