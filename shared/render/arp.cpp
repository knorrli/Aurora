#include "arp.h"

#include <math.h>

#include "aurora_protocol.h"
#include "cells.h"
#include "render.h"
#include "render_math.h"

namespace render {

static const uint32_t SHUFFLE_SALT = 71;
static const uint32_t DRAW_STRIDE = 256;
static const int32_t MAX_NOTES_SCANNED = 256;

Arp readArp(const float *dialed, const Modulation *modulation, uint8_t members) {
  auto at = [&](uint8_t control) { return controlValue(control, routed(dialed, modulation, control)); };
  Arp arp;
  arp.mode = aurora_arp_mode(roundedControl(dialed[CONTROL_ARP_MODE]));
  arp.reverse = aurora_switch_is_on(roundedControl(dialed[CONTROL_ARP_DIRECTION]));
  arp.rateBeats = at(CONTROL_ARP_RATE);
  arp.gateBeats = at(CONTROL_ARP_GATE);
  arp.members = members;
  arp.draw = 0;
  for (uint8_t envelope = 0; envelope < AURORA_ENVELOPES; envelope++) {
    readEnvelope(dialed, modulation, envelope, arp.envelopes[envelope]);
  }
  return arp;
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

static int32_t wrapped(int32_t value, int32_t length) {
  const int32_t rest = value % length;
  return rest < 0 ? rest + length : rest;
}

static int32_t passOf(int32_t step, int32_t length) { return (step - wrapped(step, length)) / length; }

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

static bool placeLights(const Arp &arp, const uint8_t *order, uint8_t place, uint8_t member) {
  return arp.mode == ARP_MODE_RANDOM ? order[place] == member
                                     : groupAtStep(arp, place) == arpGroupOf(arp, member);
}

static bool stepLights(const ArpRoute &route, const Arp &arp, int32_t step, uint8_t member) {
  if (route.arp == ARP_UNISON) return true;
  const int32_t length = passLength(arp);
  uint8_t order[MAX_ARP_MEMBERS];
  if (arp.mode == ARP_MODE_RANDOM) randomPass(arp, passOf(step, length), order);
  return placeLights(arp, order, (uint8_t)wrapped(step, length), member);
}

static bool once(const Arp &arp) { return arp.rateBeats <= 0.0f; }

static float noteLife(const ArpRoute &route, const Arp &arp) {
  if (!aurora_source_is_envelope(route.source)) return arp.gateBeats;
  return arp.gateBeats + arp.envelopes[route.source - ROUTE_SOURCE_ENVELOPE_1].release;
}

static float noteLevel(const ArpRoute &route, const Arp &arp, float age) {
  if (aurora_source_is_envelope(route.source)) {
    const Envelope &envelope = arp.envelopes[route.source - ROUTE_SOURCE_ENVELOPE_1];
    return envelopeLevelAt(envelope, arp.gateBeats, CELL_END_GONE, arp.rateBeats, age);
  }
  if (age >= arp.gateBeats) return 0.0f;
  return lfoWave(age / arp.gateBeats - waveRise(route.wave.shape), route.wave);
}

struct Timeline {
  float now;
  int32_t latest;
};

static Arp drawnFor(const Arp &arp, uint8_t route) {
  Arp drawn = arp;
  drawn.draw = routeDraw(route);
  return drawn;
}

static Timeline timelineOf(const ArpRoute &route, const Arp &arp, float beatsAgo) {
  const PlayerClock clock = routeClock(route.route);
  if (once(arp)) return { clock.beats - clock.start - beatsAgo, 0 };
  const float now = clock.beats - beatsAgo;
  return { now, (int32_t)floorf(now / arp.rateBeats) };
}

static float ageOf(const Arp &arp, const Timeline &timeline, int32_t step) {
  return once(arp) ? timeline.now : timeline.now - (float)step * arp.rateBeats;
}

static int32_t earliestStep(const Arp &arp, const Timeline &timeline, int32_t scanned) {
  return once(arp) ? 0 : timeline.latest - scanned + 1;
}

bool readArpRoute(const float *dialed, uint8_t route, ArpRoute &out) {
  if (!routeArpeggiates(dialed, route)) return false;
  out.route = route;
  out.arp = aurora_route_arp(roundedControl(dialed[routeControl(route, ROUTE_ARP)]));
  out.target = routeTarget(dialed, route);
  out.source = aurora_route_source(roundedControl(dialed[routeControl(route, ROUTE_SOURCE)]));
  if (out.source != ROUTE_SOURCE_LFO && !aurora_source_is_envelope(out.source)) return false;
  out.amount = signedOf(dialed[routeControl(route, ROUTE_AMOUNT)]);
  if (fabsf(out.amount) < 0.001f) return false;
  out.wave = waveOf(dialed, route);
  return true;
}

bool lastNote(const ArpRoute &route, const Arp &arp, float beatsAgo, uint8_t member, Note &out) {
  const Arp drawn = drawnFor(arp, route.route);
  const Timeline timeline = timelineOf(route, drawn, beatsAgo);
  const int32_t reach = once(drawn) ? 1 : 2 * (int32_t)passLength(drawn);
  for (int32_t step = timeline.latest; step >= earliestStep(drawn, timeline, reach); step--) {
    const float age = ageOf(drawn, timeline, step);
    if (age < 0.0f || !stepLights(route, drawn, step, member)) continue;
    out = { step, age };
    return true;
  }
  return false;
}

float arpLevel(const ArpRoute &route, const Arp &arp, float beatsAgo, uint8_t member) {
  const Arp drawn = drawnFor(arp, route.route);
  const Timeline timeline = timelineOf(route, drawn, beatsAgo);
  const float life = noteLife(route, drawn);
  float level = 0.0f;
  for (int32_t step = timeline.latest; step >= earliestStep(drawn, timeline, MAX_NOTES_SCANNED); step--) {
    const float age = ageOf(drawn, timeline, step);
    if (age >= life) break;
    if (age < 0.0f || !stepLights(route, drawn, step, member)) continue;
    level = fmaxf(level, noteLevel(route, drawn, age));
  }
  return level;
}

static void mark(ArpPass &out, float start, uint8_t member) {
  if (out.count >= PASS_MARKS) return;
  out.starts[out.count] = start;
  out.pars[out.count] = member;
  out.count++;
}

void passAt(const ArpRoute &route, const Arp &arp, ArpPass &out) {
  const Arp drawn = drawnFor(arp, route.route);
  const Timeline timeline = timelineOf(route, drawn, 0.0f);
  out.length = drawn.gateBeats;
  out.count = 0;
  if (once(drawn)) {
    out.beats = drawn.gateBeats;
    out.at = fminf(fmaxf(timeline.now, 0.0f), out.beats);
    for (uint8_t member = 0; member < drawn.members; member++) {
      if (stepLights(route, drawn, 0, member)) mark(out, 0.0f, member);
    }
    return;
  }
  const int32_t length = route.arp == ARP_UNISON ? 1 : passLength(drawn);
  const int32_t first = passOf(timeline.latest, length) * length;
  out.beats = (float)length * drawn.rateBeats;
  out.at = timeline.now - (float)first * drawn.rateBeats;
  for (int32_t place = 0; place < length; place++) {
    for (uint8_t member = 0; member < drawn.members; member++) {
      if (stepLights(route, drawn, first + place, member)) mark(out, (float)place * drawn.rateBeats, member);
    }
  }
}

}
