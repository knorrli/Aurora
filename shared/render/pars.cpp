#include "pars.h"

#include <math.h>

#include "arp.h"
#include "palettes.h"
#include "reading.h"
#include "render_math.h"

namespace render {

static const uint8_t HUE_DRAW_SALT = 29;
static const uint32_t SHORTEST_PAR_PULSE_MILLISECONDS = 25;
static const uint8_t PAR_LIT_LEVEL = 16;
static const uint8_t CYCLE_DRAW_SALT = 97;

struct ArpRoute {
  uint8_t target;
  uint8_t arp;
  float amount;
  uint8_t wave;
  RouteTiming timing;
};

static bool readArpRoute(const float *dialed, uint8_t route, ArpRoute &out) {
  const uint8_t aimedAt = roundedControl(dialed[routeByte(route, ROUTE_DESTINATION)]);
  out.arp = aurora_route_arp(aimedAt);
  out.target = aurora_route_target(aimedAt);
  if (out.arp == ARP_UNISON || !out.target) return false;
  out.amount = signedOf(dialed[routeByte(route, ROUTE_AMOUNT)]);
  if (fabsf(out.amount) < 0.001f) return false;
  out.wave = roundedControl(dialed[routeByte(route, ROUTE_WAVE)]);
  out.timing = routeTiming(dialed, route);
  return true;
}

static float turnsAt(const ArpRoute &route, float lfo) {
  return routeTurns(route.timing, lfo, 0.0f);
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

static void addArpRoutes(const float *dialed, const Arp &arp, float lfo, uint8_t par,
                          Modulation &modulation) {
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    ArpRoute arpRoute;
    if (!readArpRoute(dialed, route, arpRoute)) continue;
    modulation.amount[arpRoute.target] += arpRoute.amount * arpLevel(arpRoute, arp, lfo, par);
  }
}

static float drawnHue(const float *dialed, const Arp &arp, float lfo, uint8_t par) {
  bool drawn = false;
  float latest = 0.0f;
  uint32_t seed = 0;
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    ArpRoute arpRoute;
    Pulse pulse;
    if (!readArpRoute(dialed, route, arpRoute) || !lastPulseOf(arpRoute, arp, lfo, par, pulse)) {
      continue;
    }
    const float litAt = (pulse.start + arpRoute.timing.phase) / (float)arpRoute.timing.ratio;
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

static float bandPlace(const float *dialed, const Arp &arp, float lfo, uint8_t par,
                       float range) {
  const uint8_t layout = aurora_hue_layout(roundedControl(dialed[CC_PAR_HUE_LAYOUT]));
  if (layout == HUE_LAYOUT_RANDOM) return fabsf(range) * drawnHue(dialed, arp, lfo, par);
  const Arp grouping = { groupingOf(layout), 1.0f };
  const uint8_t groups = arpGroupCount(grouping);
  if (groups < 2) return 0.0f;
  return range * (2.0f * (float)arpGroupOf(grouping, par) / (float)(groups - 1) - 1.0f);
}

static void parModulation(const float *dialed, const Modulation &modulation, const Arp &arp, float lfo,
                      uint8_t par, Modulation &out) {
  out = modulation;
  addArpRoutes(dialed, arp, lfo, par, out);
}

bool firstArpPass(const float *dialed, const Modulation &modulation, float lfo, ArpPass &out) {
  for (uint8_t route = 0; route < RENDER_ROUTES; route++) {
    ArpRoute arpRoute;
    if (!readArpRoute(dialed, route, arpRoute)) continue;
    passAt(readArp(dialed, &modulation), arpRoute.arp == ARP_RIPPLE, turnsAt(arpRoute, lfo), out);
    return true;
  }
  return false;
}

uint8_t routedAtPar(const float *dialed, const Modulation &modulation, float lfo, uint8_t cc,
                    uint8_t par) {
  Modulation atPar;
  parModulation(dialed, modulation, readArp(dialed, &modulation), lfo, par, atPar);
  return displayedByte(cc, routed(dialed, &atPar, cc));
}

static uint8_t hueByte(float hue) { return (uint8_t)(int32_t)lroundf(wrappedHue(hue)); }

void readPars(const float *dialed, const Modulation &modulation, float lfo, Frame &out) {
  const Arp arp = readArp(dialed, &modulation);
  const float stripsHue = routedColor(dialed, &modulation).h;
  const uint8_t palette = roundedControl(dialed[CC_PALETTE]);
  out.stripsHue = hueByte(stripsHue);

  Modulation atPar;
  for (uint8_t par = 0; par < PARS; par++) {
    parModulation(dialed, modulation, arp, lfo, par, atPar);
    auto at = [&](uint8_t cc) { return controlValue(cc, routed(dialed, &atPar, cc)); };
    const float place = bandPlace(dialed, arp, lfo, par, at(CC_PAR_HUE_RANGE));
    out.parHuePlaces[par] = place;
    const float hue = stripsHue + at(CC_PAR_HUE_OFFSET) + place;
    out.parHues[par] = hueByte(hue);
    out.pars[par] = { paletteColor(palette, hue, at(CC_PAR_SATURATION)),
                      (uint8_t)lroundf(at(CC_PAR_VALUE)) };
  }
}

static uint8_t levelOf(const Par &par) {
  const Rgb &color = par.color;
  const uint8_t brightest = color.r > color.g ? (color.r > color.b ? color.r : color.b)
                                              : (color.g > color.b ? color.g : color.b);
  return scale8(par.value, brightest);
}

static Par heldPar(uint32_t milliseconds, ParHold &hold, const Par &rendered) {
  const uint8_t level = levelOf(rendered);
  const bool renderedLit = level >= PAR_LIT_LEVEL;
  const bool settled = milliseconds - hold.sinceMilliseconds >= SHORTEST_PAR_PULSE_MILLISECONDS;

  if (hold.lit && !settled) {
    if (level > hold.brightestLevel) {
      hold.brightest = rendered;
      hold.brightestLevel = level;
    }
    return hold.brightest;
  }
  if (hold.lit && !renderedLit) {
    hold.lit = false;
    hold.sinceMilliseconds = milliseconds;
    return rendered;
  }
  if (!hold.lit && renderedLit) {
    if (!settled) return { rendered.color, 0 };
    hold.lit = true;
    hold.sinceMilliseconds = milliseconds;
    hold.brightest = rendered;
    hold.brightestLevel = level;
  }
  return rendered;
}

void holdParPulses(uint32_t milliseconds, Wall &wall, Frame &out) {
  for (uint8_t par = 0; par < PARS; par++) {
    out.pars[par] = heldPar(milliseconds, wall.parHolds[par], out.pars[par]);
  }
}

}
