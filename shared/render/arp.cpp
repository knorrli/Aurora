#include "arp.h"

#include "aurora_protocol.h"
#include "render.h"
#include "render_math.h"

namespace render {

static const uint8_t SHUFFLE_SALT = 71;
static const uint8_t RIPPLE_SALT = 113;

Arp readArp(const uint8_t *dialed) {
  return { aurora_arp_mode(dialed[CC_ARP_MODE]), aurora_switch_is_on(dialed[CC_ARP_REVERSE]) };
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

static uint8_t randomTurn(int32_t turn) {
  const int32_t step = wrapped(turn, PARS);
  const int32_t pass = (turn - step) / PARS;
  uint8_t order[PARS];
  shuffledPass(pass, order);
  if (PARS > 2 && step < 2) {
    uint8_t before[PARS];
    shuffledPass(pass - 1, before);
    if (order[0] == before[PARS - 1]) return order[1 - step];
  }
  return order[step];
}

static uint8_t groupAtTurn(const Arp &arp, int32_t turn) {
  const uint8_t length = passLength(arp);
  int32_t step = wrapped(turn, length);
  if (arp.reverse && hasDirection(arp.mode)) step = length - 1 - step;
  if (arp.mode == ARP_MODE_BOUNCE && step >= PARS) return (uint8_t)(2 * PARS - 2 - step);
  return (uint8_t)step;
}

bool arpTurnLights(const Arp &arp, int32_t turn, uint8_t par) {
  if (arp.mode == ARP_MODE_RANDOM) return randomTurn(turn) == par;
  return groupAtTurn(arp, turn) == arpGroupOf(arp, par);
}

bool lastTurnOf(const Arp &arp, int32_t turn, uint8_t par, int32_t &lit) {
  const int32_t reach = 2 * (int32_t)passLength(arp);
  for (int32_t back = 0; back < reach; back++) {
    if (!arpTurnLights(arp, turn - back, par)) continue;
    lit = turn - back;
    return true;
  }
  return false;
}

float rippleDelay(const Arp &arp, uint8_t par) {
  if (arp.mode == ARP_MODE_RANDOM) return unitHash(par, 0, RIPPLE_SALT);
  const Arp flowing = { arp.mode == ARP_MODE_BOUNCE ? (uint8_t)ARP_MODE_SEQUENCE : arp.mode,
                        arp.reverse };
  const uint8_t count = arpGroupCount(flowing);
  uint8_t group = arpGroupOf(flowing, par);
  if (flowing.reverse && hasDirection(flowing.mode)) group = count - 1 - group;
  return (float)group / (float)count;
}

}
