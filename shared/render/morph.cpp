#include "morph.h"

#include <math.h>

#include "engines.h"
#include "render.h"
#include "routes.h"

namespace render {

static const int16_t TURN = 128;

static bool performed(uint8_t cc) {
  switch (cc) {
    case CC_FADER_COLOR:
    case CC_FADER_EXTENT:
    case CC_FADER_MOTION:
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

void composeOneshot(const uint8_t *live, const uint8_t *oneshot, const uint8_t *marks, uint8_t *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    const uint8_t cc = (uint8_t)i;
    const bool overridden = oneshot && marks[cc] && !performed(cc) && !routeByteOfPatch(cc);
    out[cc] = overridden ? oneshot[cc] : live[cc];
  }
  for (uint8_t route = 0; oneshot && route < AURORA_ROUTES; route++) {
    const uint8_t destination = aurora_route_cc(route, ROUTE_DESTINATION);
    const uint8_t target = aurora_route_target(out[destination]);
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

static float distance(uint8_t cc, uint8_t from, uint8_t to) {
  const int16_t apart = (int16_t)to - (int16_t)from;
  if (!circular(cc)) return (float)apart;
  return (float)((apart + TURN + TURN / 2) % TURN - TURN / 2);
}

static uint8_t settle(uint8_t cc, float value) {
  const int32_t rounded = (int32_t)floorf(value + 0.5f);
  if (circular(cc)) return (uint8_t)(((rounded % TURN) + TURN) % TURN);
  return (uint8_t)(rounded < 0 ? 0 : (rounded > 127 ? 127 : rounded));
}

static void holdRoutesChangingDestination(const uint8_t *switches, const uint8_t *to, uint8_t *out) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t destination = aurora_route_cc(route, ROUTE_DESTINATION);
    if (switches[destination] == to[destination]) continue;
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      const uint8_t cc = aurora_route_cc(route, field);
      out[cc] = switches[cc];
    }
  }
}

void blendPatches(const uint8_t *from, const uint8_t *to, float position, const uint8_t *switches,
                  bool switchesFromStart, uint8_t *out) {
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
      else out[cc] = settle(cc, (float)from[cc] + distance(cc, from[cc], to[cc]) * position);
    }
  }
  holdRoutesChangingDestination(switches, to, out);
}

void mixLayers(const uint8_t *base, const uint8_t (*layers)[AURORA_PATCH_CC_COUNT],
               const float *positions, uint8_t *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) {
    const uint8_t cc = (uint8_t)i;
    if (performed(cc) || switchLike(cc)) {
      out[cc] = base[cc];
      continue;
    }
    float value = (float)base[cc];
    for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
      value += positions[layer] * distance(cc, base[cc], layers[layer][cc]);
    }
    out[cc] = settle(cc, value);
  }
}

}
