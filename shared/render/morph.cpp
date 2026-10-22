#include "morph.h"

#include <math.h>

#include "engines.h"
#include "reading.h"
#include "render.h"
#include "routes.h"

namespace render {

static const float TURN = 128.0f;
static const float ROUTE_SILENT = (float)AURORA_ROUTE_DEFAULTS[ROUTE_AMOUNT];

static bool switched(uint8_t control) {
  switch (control) {
    case CONTROL_PALETTE:
    case CONTROL_PAR_HUE_LAYOUT:
    case CONTROL_TEMPO_DIVISION:
    case CONTROL_ARP_MODE:
    case CONTROL_ARP_DIRECTION:
    case CONTROL_ARP_RATE:
    case CONTROL_CELL_ENDS:
    case CONTROL_CELL_END:
    case CONTROL_CELL_RATE:
    case CONTROL_CELL_DIRECTION:
    case CONTROL_ENVELOPE_1_MOVES:
    case CONTROL_ENVELOPE_2_MOVES:
    case CONTROL_ENVELOPE_3_MOVES:
    case CONTROL_FIELD_FORM:
    case CONTROL_FIELD_DIRECTION:
      return true;
    default:
      return false;
  }
}

static bool routeSwitch(uint8_t control) {
  uint8_t route, field;
  if (!aurora_route_field_of(control, route, field)) return false;
  switch (field) {
    case ROUTE_DESTINATION:
    case ROUTE_SOURCE:
    case ROUTE_CYCLE:
    case ROUTE_RATIO:
    case ROUTE_WAVE:
    case ROUTE_PATTERN:
    case ROUTE_HOLD:
    case ROUTE_POLARITY:
    case ROUTE_ARP:
      return true;
    default:
      return false;
  }
}

static bool routeControlOfPatch(uint8_t control) {
  uint8_t route, field;
  return aurora_route_field_of(control, route, field);
}

void controlsOf(const uint8_t *bytes, float *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) out[i] = (float)bytes[i];
}

static bool switchLike(uint8_t control) { return switched(control) || routeSwitch(control); }

static float distance(uint8_t control, float from, float to) {
  const float apart = to - from;
  if (!circular(control)) return apart;
  return apart - TURN * floorf((apart + TURN / 2) / TURN);
}

static float settle(uint8_t control, float value) {
  if (circular(control)) return value - TURN * floorf(value / TURN);
  return fminf(fmaxf(value, 0.0f), 127.0f);
}

static void holdRoutesChangingDestination(const float *switches, const float *to, float *out) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t destination = aurora_route_control(route, ROUTE_DESTINATION);
    if (roundedControl(switches[destination]) == roundedControl(to[destination])) continue;
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      const uint8_t control = aurora_route_control(route, field);
      out[control] = switches[control];
    }
  }
}

static float towardAmount(float amount, float share) { return ROUTE_SILENT + (amount - ROUTE_SILENT) * share; }

void composeOneshot(const float *live, const uint8_t *oneshot, const uint8_t *marks, float strength,
                    float *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) {
    const uint8_t control = (uint8_t)i;
    out[control] = live[control];
    if (!oneshot || !marks[control] || routeControlOfPatch(control)) continue;
    out[control] = switched(control) ? (float)oneshot[control]
                           : settle(control, live[control] + distance(control, live[control], oneshot[control]) * strength);
  }
  for (uint16_t i = routeControl(ECHO_ROUTE_FIRST, 0); i < RENDER_CONTROL_COUNT; i++) out[i] = live[i];
  for (uint8_t route = 0; oneshot && route < RENDER_ROUTES; route++) {
    if (oneshotRoute(route)) continue;
    const uint8_t target = routeTarget(live, route);
    if (!target || !marks[target]) continue;
    const uint16_t amount = routeControl(route, ROUTE_AMOUNT);
    out[amount] = towardAmount(live[amount], 1.0f - strength);
    out[routeShare(route)] = live[routeShare(route)] * (1.0f - strength);
  }
  for (uint8_t route = AURORA_ROUTES; route < ECHO_ROUTE_FIRST; route++) {
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      out[routeControl(route, field)] = oneshot
          ? oneshot[aurora_route_control((uint8_t)(route - AURORA_ROUTES), field)]
          : AURORA_ROUTE_DEFAULTS[field];
    }
    out[routeShare(route)] = oneshot ? strength : 0.0f;
    if (oneshot) {
      const uint16_t amount = routeControl(route, ROUTE_AMOUNT);
      out[amount] = towardAmount(out[amount], strength);
    }
  }
}

void blendPatches(const float *from, const float *to, float position, const float *switches,
                  bool switchesFromStart, float *out) {
  const uint8_t hiddenFrom = hiddenEngines(from);
  const uint8_t hiddenTo = hiddenEngines(to);
  const uint8_t appearing = hiddenFrom & ~hiddenTo;
  const uint8_t vanishing = hiddenTo & ~hiddenFrom;

  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) {
    const uint8_t control = (uint8_t)i;
    if (switchLike(control)) {
      out[control] = (switchesFromStart && (engineOf(control) & appearing)) ? to[control] : switches[control];
    } else {
      const uint8_t engine = showsEngine(control) ? 0 : engineOf(control);
      if (engine & appearing) out[control] = to[control];
      else if ((engine & vanishing) && position < 1.0f) out[control] = from[control];
      else out[control] = settle(control, from[control] + distance(control, from[control], to[control]) * position);
    }
  }
  holdRoutesChangingDestination(switches, to, out);
}

static const float SETTINGS_FLIP_AT = 0.5f;
static const uint8_t ENVELOPE_CONTROLS = ENVELOPE_STAGES + 1;

struct Usage {
  uint8_t routes;
  uint8_t envelopes;
  bool lfo;
  bool arp;
  uint8_t hiddenEngines;
};

static Usage usageOf(const float *controls) {
  Usage usage = {};
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const float amount = signedOf(controls[aurora_route_control(route, ROUTE_AMOUNT)]);
    if (fabsf(amount) > 0.001f) usage.routes |= (uint8_t)(1 << route);
  }
  const uint8_t playing = usage.routes;
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    uint8_t aimed, field;
    if (!(playing & (1 << route))) continue;
    if (aurora_route_field_of(routeTarget(controls, route), aimed, field) && field == ROUTE_AMOUNT) {
      usage.routes |= (uint8_t)(1 << aimed);
    }
  }
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    if (!(usage.routes & (1 << route))) continue;
    const uint8_t source = aurora_route_source(roundedControl(controls[aurora_route_control(route, ROUTE_SOURCE)]));
    if (aurora_source_is_envelope(source)) usage.envelopes |= (uint8_t)(1 << (source - ROUTE_SOURCE_ENVELOPE_1));
    const bool arpeggiates = routeArpeggiates(controls, route);
    if (source == ROUTE_SOURCE_LFO && !arpeggiates) usage.lfo = true;
    if (arpeggiates) usage.arp = true;
  }
  usage.hiddenEngines = hiddenEngines(controls);
  return usage;
}

static bool uses(const Usage &usage, uint8_t control) {
  uint8_t route, field;
  if (aurora_route_field_of(control, route, field)) return field == ROUTE_AMOUNT || (usage.routes & (1 << route));
  const uint8_t engine = showsEngine(control) ? 0 : engineOf(control);
  if (engine & usage.hiddenEngines) return false;
  if (control >= CONTROL_ENVELOPE_1_MOVES && control < CONTROL_ENVELOPE_1_MOVES + AURORA_ENVELOPES * ENVELOPE_CONTROLS) {
    return usage.envelopes & (1 << ((control - CONTROL_ENVELOPE_1_MOVES) / ENVELOPE_CONTROLS));
  }
  switch (control) {
    case CONTROL_LFO_RATE: return usage.lfo;
    case CONTROL_ARP_MODE:
    case CONTROL_ARP_DIRECTION:
    case CONTROL_ARP_RATE:
    case CONTROL_ARP_GATE:
      return usage.arp;
    default:
      return true;
  }
}

struct LayerMix {
  const float *live;
  const uint8_t (*layers)[AURORA_PATCH_CONTROLS];
  const float *positions;
  Usage usages[AURORA_PATCH_LAYERS];
};

static bool contributes(const LayerMix &mix, uint8_t layer, uint8_t control) {
  return mix.positions[layer] > 0.0f && uses(mix.usages[layer], control)
      && mix.layers[layer][control] != mix.layers[PATCH_LAYER_BASE][control];
}

static float strongest(const LayerMix &mix, uint8_t control, float from, const bool *skipped) {
  float value = mix.live[control];
  float strongestAt = 0.0f;
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    if (skipped && skipped[layer]) continue;
    if (!contributes(mix, layer, control) || mix.positions[layer] < from || mix.positions[layer] <= strongestAt) continue;
    value = (float)mix.layers[layer][control];
    strongestAt = mix.positions[layer];
  }
  return value;
}

static float mixedControl(const LayerMix &mix, uint8_t control, const bool *skipped) {
  if (!uses(mix.usages[PATCH_LAYER_BASE], control)) return strongest(mix, control, 0.0f, skipped);
  if (switchLike(control)) return strongest(mix, control, SETTINGS_FLIP_AT, skipped);
  float value = mix.live[control];
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    if ((skipped && skipped[layer]) || !contributes(mix, layer, control)) continue;
    value += mix.positions[layer] * distance(control, mix.layers[PATCH_LAYER_BASE][control], mix.layers[layer][control]);
  }
  return settle(control, value);
}

static bool routeSettingsDiffer(uint8_t route, const uint8_t *base, const uint8_t *layer) {
  for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
    const uint8_t control = aurora_route_control(route, field);
    if (routeSwitch(control) && base[control] != layer[control]) return true;
  }
  return false;
}

static void silenceRoute(uint8_t route, float *out) {
  for (uint8_t field = 0; field < ROUTE_FIELDS; field++) out[routeControl(route, field)] = AURORA_ROUTE_DEFAULTS[field];
  out[routeShare(route)] = 0.0f;
}

static void mixRoute(const LayerMix &mix, uint8_t route, float *out) {
  const uint8_t *base = mix.layers[PATCH_LAYER_BASE];
  const uint8_t bit = (uint8_t)(1 << route);
  const bool baseUses = mix.usages[PATCH_LAYER_BASE].routes & bit;
  bool echoes[AURORA_PATCH_LAYERS] = {};
  float echoed = 0.0f;
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    echoes[layer] = baseUses && mix.positions[layer] > 0.0f && (mix.usages[layer].routes & bit)
                 && routeSettingsDiffer(route, base, mix.layers[layer]);
    if (echoes[layer]) echoed += mix.positions[layer];
  }
  for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
    const uint8_t control = aurora_route_control(route, field);
    out[control] = mixedControl(mix, control, echoes);
  }
  const uint8_t amount = aurora_route_control(route, ROUTE_AMOUNT);
  out[amount] = towardAmount(out[amount], fmaxf(0.0f, 1.0f - echoed));
  out[routeShare(route)] = fmaxf(0.0f, 1.0f - echoed);
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    const uint8_t echo = echoRoute(layer, route);
    if (!echoes[layer]) {
      silenceRoute(echo, out);
      continue;
    }
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      out[routeControl(echo, field)] = (float)mix.layers[layer][aurora_route_control(route, field)];
    }
    out[routeControl(echo, ROUTE_AMOUNT)] = towardAmount((float)mix.layers[layer][amount], mix.positions[layer]);
    out[routeShare(echo)] = mix.positions[layer];
  }
}

void usedControls(const uint8_t *bytes, bool *out) {
  float controls[AURORA_PATCH_CONTROLS];
  controlsOf(bytes, controls);
  const Usage usage = usageOf(controls);
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) out[i] = uses(usage, (uint8_t)i);
}

void silenceRenderRoutes(float *out) {
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) out[routeShare(route)] = 1.0f;
  for (uint8_t route = AURORA_ROUTES; route < RENDER_ROUTES; route++) silenceRoute(route, out);
}

void mixLayers(const float *live, const uint8_t (*layers)[AURORA_PATCH_CONTROLS],
               const float *positions, float *out) {
  LayerMix mix = { live, layers, positions, {} };
  mix.usages[PATCH_LAYER_BASE] = usageOf(live);
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    if (positions[layer] <= 0.0f) continue;
    float controls[AURORA_PATCH_CONTROLS];
    controlsOf(layers[layer], controls);
    mix.usages[layer] = usageOf(controls);
  }
  for (uint16_t i = 0; i < AURORA_FIRST_ROUTE_CONTROL; i++) out[i] = mixedControl(mix, (uint8_t)i, nullptr);
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) mixRoute(mix, route, out);
  for (uint8_t route = AURORA_ROUTES; route < ECHO_ROUTE_FIRST; route++) silenceRoute(route, out);
}

void morphCorners(const float *live, const uint8_t *const *controls, const uint8_t *const *marks,
                  const float *weights, int8_t switchCorner, float *out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) {
    const uint8_t control = (uint8_t)i;
    out[control] = live[control];
    if (routeControlOfPatch(control)) continue;
    if (switchLike(control)) {
      if (switchCorner >= 0 && controls[switchCorner] && marks[switchCorner][control]) {
        out[control] = controls[switchCorner][control];
      }
      continue;
    }
    float value = live[control];
    for (uint8_t corner = 0; corner < AURORA_CORNERS; corner++) {
      if (!controls[corner] || !marks[corner][control]) continue;
      value += weights[corner] * distance(control, live[control], controls[corner][control]);
    }
    out[control] = settle(control, value);
  }
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    const uint8_t target = routeTarget(live, route);
    float covered = 0.0f;
    for (uint8_t corner = 0; target && corner < AURORA_CORNERS; corner++) {
      if (controls[corner] && marks[corner][target]) covered += weights[corner];
    }
    const uint8_t amount = aurora_route_control(route, ROUTE_AMOUNT);
    out[amount] = towardAmount(live[amount], 1.0f - fminf(covered, 1.0f));
  }
}

}
