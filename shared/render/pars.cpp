#include "pars.h"

#include <math.h>

#include "arp.h"
#include "palettes.h"
#include "reading.h"
#include "render_math.h"

namespace render {

static const uint8_t HUE_DRAW_SALT = 29;

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
  if (out.arp == ARP_OFF || !out.target) return false;
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

static float arpLevel(const ArpRoute &route, const Arp &arp, float lfo, uint8_t par) {
  const float turns = turnsAt(route, lfo);
  if (route.arp == ARP_RIPPLE) return lfoWave(turns - rippleDelay(arp, par), route.wave);

  const float turn = floorf(turns);
  if (!arpTurnLights(arp, (int32_t)turn, par)) return 0.0f;
  return lfoWave(turns - turn - waveRise(route.wave), route.wave);
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
    if (!readArpRoute(dialed, route, arpRoute) || arpRoute.arp != ARP_TURNS) continue;
    int32_t lit;
    if (!lastTurnOf(arp, (int32_t)floorf(turnsAt(arpRoute, lfo)), par, lit)) continue;
    const float litAt = ((float)lit + arpRoute.delay) / (float)arpRoute.ratio;
    if (drawn && litAt <= latest) continue;
    drawn = true;
    latest = litAt;
    seed = (uint32_t)lit * AURORA_ROUTES + route;
  }
  const float draw = drawn ? unitHash(seed, par, HUE_DRAW_SALT) : unitHash(par, 0, HUE_DRAW_SALT);
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
  const Arp grouping = { groupingOf(layout), false };
  const uint8_t groups = arpGroupCount(grouping);
  if (groups < 2) return 0.0f;
  return range * (2.0f * (float)arpGroupOf(grouping, par) / (float)(groups - 1) - 1.0f);
}

static void parPushes(const uint8_t *dialed, const Pushes &pushes, const Arp &arp, float lfo,
                      uint8_t par, Pushes &out) {
  out = pushes;
  pushArpRoutes(dialed, arp, lfo, par, out);
}

uint8_t routedAtPar(const uint8_t *dialed, const Pushes &pushes, float lfo, uint8_t cc,
                    uint8_t par) {
  Pushes atPar;
  parPushes(dialed, pushes, readArp(dialed), lfo, par, atPar);
  return routed(dialed, &atPar, cc);
}

void readPars(const uint8_t *dialed, const Pushes &pushes, float lfo, Frame &out) {
  const Arp arp = readArp(dialed);
  const Hsv dialedStrips = dialedColor(dialed);
  const uint8_t stripsHue = routedColor(dialed, &pushes).h;

  Pushes atPar;
  for (uint8_t par = 0; par < PARS; par++) {
    parPushes(dialed, pushes, arp, lfo, par, atPar);
    auto at = [&](uint8_t cc) { return dialedValue(cc, routed(dialed, &atPar, cc)); };
    const float place = bandPlace(dialed, arp, lfo, par, at(CC_PAR_HUE_RANGE));
    const int32_t hue = (int32_t)stripsHue + (int32_t)at(CC_PAR_HUE_OFFSET) + (int32_t)lroundf(place);
    const uint8_t saturation = scale8(dialedStrips.s, (uint8_t)at(CC_PAR_SATURATION));
    out.pars[par] = { paletteColor(dialed[CC_PALETTE], (uint8_t)(hue & 255), saturation),
                      scale8(dialedStrips.v, (uint8_t)at(CC_PAR_VALUE)) };
  }
}

}
