#include "arp.h"

#include <math.h>

#include "aurora_protocol.h"
#include "reading.h"
#include "render.h"
#include "render_math.h"

namespace render {

static const uint8_t SHUFFLE_SALT = 71;

Arp readArp(const uint8_t *dialed, const Pushes *pushes) {
  return { aurora_arp_mode(dialed[CC_ARP_MODE]),
           dialedValue(CC_ARP_SPREAD, routed(dialed, pushes, CC_ARP_SPREAD)) };
}

static bool hasDirection(uint8_t mode) {
  return mode != ARP_MODE_TOGETHER && mode != ARP_MODE_RANDOM;
}

uint8_t arpGroupCount(const Arp &arp) {
  switch (arp.mode) {
    case ARP_MODE_TOGETHER: return 1;
    case ARP_MODE_EVENS_ODDS: return PARS > 1 ? 2 : 1;
    case ARP_MODE_PAIRS:
    case ARP_MODE_MIRROR: return (PARS + 1) / 2;
    default: return PARS;
  }
}

uint8_t arpGroupOf(const Arp &arp, uint8_t par) {
  switch (arp.mode) {
    case ARP_MODE_TOGETHER: return 0;
    case ARP_MODE_EVENS_ODDS: return par % 2;
    case ARP_MODE_PAIRS: return par / 2;
    case ARP_MODE_MIRROR: {
      const int16_t fromCenter = 2 * (int16_t)par - (PARS - 1);
      return (uint8_t)((fromCenter < 0 ? -fromCenter : fromCenter) / 2);
    }
    default: return par;
  }
}

static uint8_t passLength(const Arp &arp) {
  if (arp.mode == ARP_MODE_BOUNCE) return PARS > 1 ? 2 * PARS - 2 : 1;
  return arpGroupCount(arp);
}

static bool reversed(const Arp &arp) { return arp.spread < 0.0f && hasDirection(arp.mode); }

static int32_t turnsPerPass(const Arp &arp) {
  const long turns = lroundf(fabsf(arp.spread) * (float)passLength(arp));
  return turns < 1 ? 1 : (int32_t)turns;
}

static int32_t wrapped(int32_t value, int32_t length) {
  const int32_t rest = value % length;
  return rest < 0 ? rest + length : rest;
}

static void shuffledPass(int32_t pass, uint8_t *order) {
  for (uint8_t i = 0; i < PARS; i++) order[i] = i;
  for (uint8_t i = PARS - 1; i > 0; i--) {
    const uint8_t j = (uint8_t)(hash32((uint32_t)pass, i, SHUFFLE_SALT) % (i + 1));
    const uint8_t held = order[i];
    order[i] = order[j];
    order[j] = held;
  }
}

static void randomPass(int32_t pass, uint8_t *order) {
  shuffledPass(pass, order);
  if (PARS < 3) return;
  uint8_t before[PARS];
  shuffledPass(pass - 1, before);
  if (order[0] != before[PARS - 1]) return;
  order[0] = order[1];
  order[1] = before[PARS - 1];
}

static uint8_t groupAtStep(const Arp &arp, uint8_t step) {
  const uint8_t group =
      (arp.mode == ARP_MODE_BOUNCE && step >= PARS) ? (uint8_t)(2 * PARS - 2 - step) : step;
  return reversed(arp) ? (uint8_t)(arpGroupCount(arp) - 1 - group) : group;
}

static bool stepLights(const Arp &arp, const uint8_t *order, uint8_t step, uint8_t par) {
  return arp.mode == ARP_MODE_RANDOM ? order[step] == par
                                     : groupAtStep(arp, step) == arpGroupOf(arp, par);
}

static bool turnLights(const Arp &arp, int32_t turn, uint8_t par) {
  const uint8_t steps = passLength(arp);
  const int32_t turns = turnsPerPass(arp);
  const int32_t place = wrapped(turn, turns);
  uint8_t order[PARS];
  if (arp.mode == ARP_MODE_RANDOM) randomPass((turn - place) / turns, order);
  for (uint8_t step = 0; step < steps; step++) {
    if ((int32_t)step * turns / steps != place) continue;
    if (stepLights(arp, order, step, par)) return true;
  }
  return false;
}

static bool lastTurn(const Arp &arp, float turns, uint8_t par, Pulse &out) {
  const int32_t turn = (int32_t)floorf(turns);
  const int32_t reach = 2 * (int32_t)passLength(arp);
  for (int32_t back = 0; back < reach; back++) {
    if (!turnLights(arp, turn - back, par)) continue;
    out = { (float)(turn - back), 1.0f, (uint32_t)(turn - back) };
    return true;
  }
  return false;
}

static bool lastRipple(const Arp &arp, float turns, uint8_t par, Pulse &out) {
  const uint8_t steps = passLength(arp);
  const float apart = fabsf(arp.spread);
  const int32_t pass = (int32_t)floorf(turns / (float)steps);
  bool found = false;
  for (int32_t which = pass; which >= pass - 1; which--) {
    uint8_t order[PARS];
    if (arp.mode == ARP_MODE_RANDOM) randomPass(which, order);
    for (uint8_t step = 0; step < steps; step++) {
      if (!stepLights(arp, order, step, par)) continue;
      const float start = (float)(which * steps) + (float)step * apart;
      if (start > turns || (found && start <= out.start)) continue;
      out = { start, (float)steps, (uint32_t)(which * steps + step) };
      found = true;
    }
  }
  return found;
}

uint8_t livePulses(const Arp &arp, bool ripple, float turns, uint8_t par, Pulse *out) {
  if (!ripple) {
    Pulse pulse;
    if (!lastTurn(arp, turns, par, pulse) || turns - pulse.start >= 1.0f) return 0;
    out[0] = pulse;
    return 1;
  }
  const uint8_t steps = passLength(arp);
  const float apart = fabsf(arp.spread);
  const int32_t pass = (int32_t)floorf(turns / (float)steps);
  uint8_t count = 0;
  for (int32_t which = pass; which >= pass - 1; which--) {
    uint8_t order[PARS];
    if (arp.mode == ARP_MODE_RANDOM) randomPass(which, order);
    for (uint8_t step = 0; step < steps && count < LIVE_PULSES; step++) {
      if (!stepLights(arp, order, step, par)) continue;
      const float start = (float)(which * steps) + (float)step * apart;
      if (start > turns || turns - start >= (float)steps) continue;
      out[count++] = { start, (float)steps, (uint32_t)(which * steps + step) };
    }
  }
  return count;
}

bool lastPulse(const Arp &arp, bool ripple, float turns, uint8_t par, Pulse &out) {
  return ripple ? lastRipple(arp, turns, par, out) : lastTurn(arp, turns, par, out);
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

  uint8_t order[PARS];
  if (arp.mode == ARP_MODE_RANDOM) randomPass(pass, order);
  for (uint8_t step = 0; step < steps; step++) {
    const float start = ripple ? (float)step * fabsf(arp.spread)
                               : (float)((int32_t)step * length / steps);
    for (uint8_t par = 0; par < PARS; par++) {
      if (stepLights(arp, order, step, par)) mark(out, start, par);
    }
  }
}

}
