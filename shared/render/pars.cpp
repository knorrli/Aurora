#include "pars.h"

#include <math.h>

#include "arp.h"
#include "palettes.h"
#include "reading.h"
#include "render_math.h"

namespace render {

static const uint8_t HUE_DRAW_SALT = 29;
static const uint8_t CYCLE_DRAW_SALT = 97;

struct ArpRoute {
  uint8_t route;
  uint8_t target;
  uint8_t arp;
  float amount;
  uint8_t ratio;
  uint8_t wave;
  float delay;
};

static bool readArpRoute(const uint8_t *dialed, uint8_t route, ArpRoute &out) {
  const uint8_t aimedAt = dialed[aurora_route_cc(route, ROUTE_DESTINATION)];
  out.route = route;
  out.arp = aurora_route_arp(aimedAt);
  out.target = aurora_route_target(aimedAt);
  if (out.arp == ARP_UNISON || !out.target) return false;
  out.amount = bipolarOf(dialed[aurora_route_cc(route, ROUTE_AMOUNT)]);
  if (fabsf(out.amount) < 0.001f) return false;
  out.ratio = aurora_route_ratio(dialed[aurora_route_cc(route, ROUTE_RATIO)]);
  out.wave = dialed[aurora_route_cc(route, ROUTE_WAVE)];
  out.delay = (float)dialed[aurora_route_cc(route, ROUTE_PHASE)] / 128.0f;
  return true;
}

static float turnsAt(const ArpRoute &route, float lfo) {
  return lfo * (float)route.ratio - route.delay;
}

static bool lastPulseOf(const ArpRoute &route, const Arp &arp, float lfo, uint8_t par,
                        Pulse &pulse) {
  return lastPulse(arp, route.arp == ARP_RIPPLE, turnsAt(route, lfo), par, pulse);
}

static float arpLevel(const ArpRoute &route, const Arp &arp, float lfo, uint8_t par) {
  const float turns = turnsAt(route, lfo);
  Pulse pulses[LIVE_PULSES];
  const uint8_t count = livePulses(arp, route.arp == ARP_RIPPLE, turns, par, pulses);
  float level = 0.0f;
  for (uint8_t i = 0; i < count; i++) {
    const float into = (turns - pulses[i].start) / pulses[i].length;
    level = fmaxf(level, lfoWave(into - waveRise(route.wave), route.wave));
  }
  return level;
}

static void pushArpRoutes(const uint8_t *dialed, const Arp &arp, float lfo, uint8_t par,
                          Pushes &pushes) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    ArpRoute arpRoute;
    if (!readArpRoute(dialed, route, arpRoute)) continue;
    pushes.amount[arpRoute.target] += arpRoute.amount * arpLevel(arpRoute, arp, lfo, par);
  }
}

static float drawnHue(const uint8_t *dialed, const Arp &arp, float lfo, uint8_t par) {
  bool drawn = false;
  float latest = 0.0f;
  uint32_t seed = 0;
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    ArpRoute arpRoute;
    Pulse pulse;
    if (!readArpRoute(dialed, route, arpRoute) || !lastPulseOf(arpRoute, arp, lfo, par, pulse)) {
      continue;
    }
    const float litAt = (pulse.start + arpRoute.delay) / (float)arpRoute.ratio;
    if (drawn && litAt <= latest) continue;
    drawn = true;
    latest = litAt;
    seed = pulse.id * AURORA_ROUTES + route;
  }
  const float draw = drawn ? unitHash(seed, par, HUE_DRAW_SALT)
                           : unitHash((uint32_t)(int32_t)floorf(lfo), par, CYCLE_DRAW_SALT);
  return 2.0f * draw - 1.0f;
}

static uint8_t groupingOf(uint8_t layout) {
  switch (layout) {
    case HUE_LAYOUT_EVENS_ODDS: return ARP_MODE_EVENS_ODDS;
    case HUE_LAYOUT_PAIRS: return ARP_MODE_PAIRS;
    case HUE_LAYOUT_MIRROR: return ARP_MODE_MIRROR;
    default: return ARP_MODE_SEQUENCE;
  }
}

static float bandPlace(const uint8_t *dialed, const Arp &arp, float lfo, uint8_t par,
                       float range) {
  const uint8_t layout = aurora_hue_layout(dialed[CC_PAR_HUE_LAYOUT]);
  if (layout == HUE_LAYOUT_RANDOM) return fabsf(range) * drawnHue(dialed, arp, lfo, par);
  const Arp grouping = { groupingOf(layout), 1.0f };
  const uint8_t groups = arpGroupCount(grouping);
  if (groups < 2) return 0.0f;
  return range * (2.0f * (float)arpGroupOf(grouping, par) / (float)(groups - 1) - 1.0f);
}

static void parPushes(const uint8_t *dialed, const Pushes &pushes, const Arp &arp, float lfo,
                      uint8_t par, Pushes &out) {
  out = pushes;
  pushArpRoutes(dialed, arp, lfo, par, out);
}

bool firstArpPass(const uint8_t *dialed, const Pushes &pushes, float lfo, ArpPass &out) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    ArpRoute arpRoute;
    if (!readArpRoute(dialed, route, arpRoute)) continue;
    passAt(readArp(dialed, &pushes), arpRoute.arp == ARP_RIPPLE, turnsAt(arpRoute, lfo), out);
    return true;
  }
  return false;
}

uint8_t routedAtPar(const uint8_t *dialed, const Pushes &pushes, float lfo, uint8_t cc,
                    uint8_t par) {
  Pushes atPar;
  parPushes(dialed, pushes, readArp(dialed, &pushes), lfo, par, atPar);
  return routed(dialed, &atPar, cc);
}

void readPars(const uint8_t *dialed, const Pushes &pushes, float lfo, Frame &out) {
  const Arp arp = readArp(dialed, &pushes);
  const Hsv dialedStrips = dialedColor(dialed);
  const uint8_t stripsHue = routedColor(dialed, &pushes).h;

  Pushes atPar;
  for (uint8_t par = 0; par < PARS; par++) {
    parPushes(dialed, pushes, arp, lfo, par, atPar);
    auto at = [&](uint8_t cc) { return dialedValue(cc, routed(dialed, &atPar, cc)); };
    const float place = bandPlace(dialed, arp, lfo, par, at(CC_PAR_HUE_RANGE));
    out.parHuePlaces[par] = place;
    const int32_t hue = (int32_t)stripsHue + (int32_t)at(CC_PAR_HUE_OFFSET) + (int32_t)lroundf(place);
    const uint8_t saturation = scale8(dialedStrips.s, (uint8_t)at(CC_PAR_SATURATION));
    out.pars[par] = { paletteColor(dialed[CC_PALETTE], (uint8_t)(hue & 255), saturation),
                      scale8(dialedStrips.v, (uint8_t)at(CC_PAR_VALUE)) };
  }
}

}
