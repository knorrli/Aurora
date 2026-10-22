#include "arp.h"

#include <math.h>

#include "aurora_protocol.h"
#include "reading.h"
#include "render.h"
#include "render_math.h"

namespace render {

static const uint32_t SHUFFLE_SALT = 71;
static const uint32_t DRAW_STRIDE = 256;

Arp readArp(const float *dialed, const Modulation *modulation, uint8_t members) {
  return { aurora_arp_mode(roundedControl(dialed[CONTROL_ARP_MODE])),
           controlValue(CONTROL_ARP_SPREAD, routed(dialed, modulation, CONTROL_ARP_SPREAD)),
           aurora_switch_is_on(roundedControl(dialed[CONTROL_ARP_DIRECTION])), members, 0 };
}

static bool hasDirection(uint8_t mode) {
  return mode != ARP_MODE_RANDOM;
}

uint8_t arpGroupCount(const Arp &arp) {
  switch (arp.mode) {
    case ARP_MODE_EVENS_ODDS: return arp.members > 1 ? 2 : 1;
    case ARP_MODE_PAIRS:
    case ARP_MODE_MIRROR: return (uint8_t)((arp.members + 1) / 2);
    default: return arp.members;
  }
}

uint8_t arpGroupOf(const Arp &arp, uint8_t member) {
  switch (arp.mode) {
    case ARP_MODE_EVENS_ODDS: return member % 2;
    case ARP_MODE_PAIRS: return member / 2;
    case ARP_MODE_MIRROR: {
      const int16_t fromCenter = 2 * (int16_t)member - (arp.members - 1);
      return (uint8_t)((fromCenter < 0 ? -fromCenter : fromCenter) / 2);
    }
    default: return member;
  }
}

uint8_t passLength(const Arp &arp) {
  if (arp.mode == ARP_MODE_BOUNCE) return arp.members > 1 ? (uint8_t)(2 * arp.members - 2) : 1;
  return arpGroupCount(arp);
}

bool reversed(const Arp &arp) { return arp.reverse && hasDirection(arp.mode); }

int32_t turnsPerPass(const Arp &arp) {
  const long turns = lroundf(fabsf(arp.spread) * (float)passLength(arp));
  return turns < 1 ? 1 : (int32_t)turns;
}

static int32_t wrapped(int32_t value, int32_t length) {
  const int32_t rest = value % length;
  return rest < 0 ? rest + length : rest;
}

static void shuffledPass(const Arp &arp, int32_t pass, uint8_t *order) {
  for (uint8_t i = 0; i < arp.members; i++) order[i] = i;
  for (uint8_t i = arp.members - 1; i > 0; i--) {
    const uint8_t j = (uint8_t)(hash32((uint32_t)pass, i, SHUFFLE_SALT + arp.draw * DRAW_STRIDE) % (i + 1));
    const uint8_t held = order[i];
    order[i] = order[j];
    order[j] = held;
  }
}

static void randomPass(const Arp &arp, int32_t pass, uint8_t *order) {
  shuffledPass(arp, pass, order);
  if (arp.members < 3) return;
  uint8_t before[MAX_ARP_MEMBERS];
  shuffledPass(arp, pass - 1, before);
  const uint8_t last = before[arp.members - 1];
  if (order[0] != last) return;
  order[0] = order[1];
  order[1] = last;
}

static uint8_t groupAtStep(const Arp &arp, uint8_t step) {
  const uint8_t group =
      (arp.mode == ARP_MODE_BOUNCE && step >= arp.members) ? (uint8_t)(2 * arp.members - 2 - step) : step;
  return reversed(arp) ? (uint8_t)(arpGroupCount(arp) - 1 - group) : group;
}

static bool stepLights(const Arp &arp, const uint8_t *order, uint8_t step, uint8_t member) {
  return arp.mode == ARP_MODE_RANDOM ? order[step] == member
                                     : groupAtStep(arp, step) == arpGroupOf(arp, member);
}

static bool turnLights(const Arp &arp, int32_t turn, uint8_t member) {
  const uint8_t steps = passLength(arp);
  const int32_t turns = turnsPerPass(arp);
  const int32_t place = wrapped(turn, turns);
  uint8_t order[MAX_ARP_MEMBERS];
  if (arp.mode == ARP_MODE_RANDOM) randomPass(arp, (turn - place) / turns, order);
  for (uint8_t step = 0; step < steps; step++) {
    if ((int32_t)step * turns / steps != place) continue;
    if (stepLights(arp, order, step, member)) return true;
  }
  return false;
}

static bool lastTurn(const Arp &arp, float turns, uint8_t member, Pulse &out) {
  const int32_t turn = (int32_t)floorf(turns);
  const int32_t reach = 2 * (int32_t)passLength(arp);
  for (int32_t back = 0; back < reach; back++) {
    if (!turnLights(arp, turn - back, member)) continue;
    out = { (float)(turn - back), 1.0f, (uint32_t)(turn - back) };
    return true;
  }
  return false;
}

static bool lastRipple(const Arp &arp, float turns, uint8_t member, Pulse &out) {
  const uint8_t steps = passLength(arp);
  const float apart = fabsf(arp.spread);
  const int32_t pass = (int32_t)floorf(turns / (float)steps);
  bool found = false;
  for (int32_t which = pass; which >= pass - 1; which--) {
    uint8_t order[MAX_ARP_MEMBERS];
    if (arp.mode == ARP_MODE_RANDOM) randomPass(arp, which, order);
    for (uint8_t step = 0; step < steps; step++) {
      if (!stepLights(arp, order, step, member)) continue;
      const float start = (float)(which * steps) + (float)step * apart;
      if (start > turns || (found && start <= out.start)) continue;
      out = { start, (float)steps, (uint32_t)(which * steps + step) };
      found = true;
    }
  }
  return found;
}

uint8_t livePulses(const Arp &arp, bool ripple, float turns, uint8_t member, Pulse *out) {
  if (!ripple) {
    Pulse pulse;
    if (!lastTurn(arp, turns, member, pulse) || turns - pulse.start >= 1.0f) return 0;
    out[0] = pulse;
    return 1;
  }
  const uint8_t steps = passLength(arp);
  const float apart = fabsf(arp.spread);
  const int32_t pass = (int32_t)floorf(turns / (float)steps);
  uint8_t count = 0;
  for (int32_t which = pass; which >= pass - 1; which--) {
    uint8_t order[MAX_ARP_MEMBERS];
    if (arp.mode == ARP_MODE_RANDOM) randomPass(arp, which, order);
    for (uint8_t step = 0; step < steps && count < LIVE_PULSES; step++) {
      if (!stepLights(arp, order, step, member)) continue;
      const float start = (float)(which * steps) + (float)step * apart;
      if (start > turns || turns - start >= (float)steps) continue;
      out[count++] = { start, (float)steps, (uint32_t)(which * steps + step) };
    }
  }
  return count;
}

bool lastPulse(const Arp &arp, bool ripple, float turns, uint8_t member, Pulse &out) {
  return ripple ? lastRipple(arp, turns, member, out) : lastTurn(arp, turns, member, out);
}

bool readArpRoute(const float *dialed, uint8_t route, ArpRoute &out) {
  if (!routeArpeggiates(dialed, route)) return false;
  out.arp = aurora_route_arp(roundedControl(dialed[routeControl(route, ROUTE_ARP)]));
  out.target = routeTarget(dialed, route);
  if (aurora_route_source(roundedControl(dialed[routeControl(route, ROUTE_SOURCE)])) != ROUTE_SOURCE_LFO) return false;
  out.amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
  if (fabsf(out.amount) < 0.001f) return false;
  out.wave = waveOf(dialed, route);
  out.timing = routeTiming(dialed, route);
  return true;
}

Arp drawnFor(const Arp &arp, uint8_t route) {
  Arp drawn = arp;
  drawn.draw = routeDraw(route);
  return drawn;
}

float arpLevel(const ArpRoute &route, const Arp &arp, float turns, uint8_t member) {
  Pulse pulses[LIVE_PULSES];
  const uint8_t count =
      livePulses(drawnFor(arp, route.timing.route), route.arp == ARP_RIPPLE, turns, member, pulses);
  float level = 0.0f;
  for (uint8_t i = 0; i < count; i++) {
    const float into = (turns - pulses[i].start) / pulses[i].length;
    level = fmaxf(level, lfoWave(into - waveRise(route.wave.shape), route.wave));
  }
  return level;
}

static void mark(ArpPass &out, float start, uint8_t par) {
  if (out.count >= PASS_MARKS) return;
  out.starts[out.count] = start;
  out.pars[out.count] = par;
  out.count++;
}

void passAt(const Arp &arp, bool ripple, float turns, ArpPass &out) {
  const uint8_t steps = passLength(arp);
  const int32_t length = ripple ? steps : turnsPerPass(arp);
  const int32_t pass = (int32_t)floorf(turns / (float)length);
  out.turns = (float)length;
  out.at = turns - (float)(pass * length);
  out.length = ripple ? (float)steps : 1.0f;
  out.count = 0;

  uint8_t order[MAX_ARP_MEMBERS];
  if (arp.mode == ARP_MODE_RANDOM) randomPass(arp, pass, order);
  for (uint8_t step = 0; step < steps; step++) {
    const float start = ripple ? (float)step * fabsf(arp.spread)
                               : (float)((int32_t)step * length / steps);
    for (uint8_t member = 0; member < arp.members; member++) {
      if (stepLights(arp, order, step, member)) mark(out, start, member);
    }
  }
}

}
