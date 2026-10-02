#include "morph.h"

#include <math.h>

#include "engines.h"
#include "render.h"
#include "routes.h"

namespace render {

static const float TURN = 128.0f;

static bool performed(uint8_t cc) {
  switch (cc) {
    case CC_FADER_COLOR:
    case CC_FADER_MOTION:
    case CC_FADER_EXTENT:
      return true;
    default:
      return false;
  }
}

static bool switched(uint8_t cc) {
  switch (cc) {
    case CC_PALETTE:
    case CC_PAR_HUE_LAYOUT:
    case CC_TEMPO_DIVISION:
    case CC_ARP_MODE:
    case CC_SHAPE_BOUNCE:
    case CC_FIELD_FORM:
    case CC_FIELD_DIRECTION:
      return true;
    default:
      return false;
  }
}

static bool routeDestination(uint8_t cc) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    if (cc == aurora_route_cc(route, ROUTE_DESTINATION)) return true;
  }
  return false;
}

static bool routeByteOfPatch(uint8_t cc) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    if (cc >= aurora_route_cc(route, 0) && cc < aurora_route_cc(route, 0) + ROUTE_FIELDS) return true;
  }
  return false;
}

void controlsOf(const uint8_t *bytes, float *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) out[i] = (float)bytes[i];
}

void composeOneshot(const float *live, const uint8_t *oneshot, const uint8_t *marks, float *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    const uint8_t cc = (uint8_t)i;
    const bool overridden = oneshot && marks[cc] && !performed(cc) && !routeByteOfPatch(cc);
    out[cc] = overridden ? (float)oneshot[cc] : live[cc];
  }
  for (uint8_t route = 0; oneshot && route < AURORA_ROUTES; route++) {
    const uint8_t destination = aurora_route_cc(route, ROUTE_DESTINATION);
    const uint8_t target = aurora_route_target(roundedControl(out[destination]));
    if (target && marks[target]) out[destination] = AURORA_ROUTE_DEFAULTS[ROUTE_DESTINATION];
  }
  for (uint8_t route = AURORA_ROUTES; route < RENDER_ROUTES; route++) {
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      out[routeByte(route, field)] = oneshot
          ? oneshot[aurora_route_cc((uint8_t)(route - AURORA_ROUTES), field)]
          : AURORA_ROUTE_DEFAULTS[field];
    }
  }
}

static bool switchLike(uint8_t cc) { return switched(cc) || routeDestination(cc); }

static float distance(uint8_t cc, float from, float to) {
  const float apart = to - from;
  if (!circular(cc)) return apart;
  return apart - TURN * floorf((apart + TURN / 2) / TURN);
}

static float settle(uint8_t cc, float value) {
  if (circular(cc)) return value - TURN * floorf(value / TURN);
  return fminf(fmaxf(value, 0.0f), 127.0f);
}

static void holdRoutesChangingDestination(const float *switches, const float *to, float *out) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t destination = aurora_route_cc(route, ROUTE_DESTINATION);
    if (roundedControl(switches[destination]) == roundedControl(to[destination])) continue;
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      const uint8_t cc = aurora_route_cc(route, field);
      out[cc] = switches[cc];
    }
  }
}

void blendPatches(const float *from, const float *to, float position, const float *switches,
                  bool switchesFromStart, float *out) {
  const uint8_t hiddenFrom = hiddenEngines(from);
  const uint8_t hiddenTo = hiddenEngines(to);
  const uint8_t appearing = hiddenFrom & ~hiddenTo;
  const uint8_t vanishing = hiddenTo & ~hiddenFrom;

  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    const uint8_t cc = (uint8_t)i;
    if (performed(cc)) {
      out[cc] = from[cc];
    } else if (switchLike(cc)) {
      out[cc] = (switchesFromStart && (engineOf(cc) & appearing)) ? to[cc] : switches[cc];
    } else {
      const uint8_t engine = showsEngine(cc) ? 0 : engineOf(cc);
      if (engine & appearing) out[cc] = to[cc];
      else if ((engine & vanishing) && position < 1.0f) out[cc] = from[cc];
      else out[cc] = settle(cc, from[cc] + distance(cc, from[cc], to[cc]) * position);
    }
  }
  holdRoutesChangingDestination(switches, to, out);
}

void mixLayers(const float *live, const uint8_t (*layers)[AURORA_PATCH_CC_COUNT],
               const float *positions, float *out) {
  const uint8_t *base = layers[PATCH_LAYER_BASE];
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    const uint8_t cc = (uint8_t)i;
    if (performed(cc) || switchLike(cc)) {
      out[cc] = live[cc];
      continue;
    }
    float value = live[cc];
    for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
      value += positions[layer] * distance(cc, base[cc], layers[layer][cc]);
    }
    out[cc] = settle(cc, value);
  }
}

}
