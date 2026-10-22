#pragma once

#include <stdint.h>

#include "routes.h"

namespace render {

static const uint8_t MAX_ARP_MEMBERS = STRIPS > PARS ? STRIPS : PARS;

struct Arp {
  uint8_t mode;
  float spread;
  bool reverse;
  uint8_t members;
  uint32_t draw;
};

Arp readArp(const float *dialed, const Modulation *modulation, uint8_t members);

uint8_t arpGroupCount(const Arp &arp);

uint8_t arpGroupOf(const Arp &arp, uint8_t member);

uint8_t passLength(const Arp &arp);

int32_t turnsPerPass(const Arp &arp);

bool reversed(const Arp &arp);

struct Pulse {
  float start;
  float length;
  uint32_t id;
};

bool lastPulse(const Arp &arp, bool ripple, float turns, uint8_t member, Pulse &out);

static const uint8_t LIVE_PULSES = 8;

uint8_t livePulses(const Arp &arp, bool ripple, float turns, uint8_t member, Pulse *out);

struct ArpRoute {
  uint8_t target;
  uint8_t arp;
  float amount;
  Wave wave;
  RouteTiming timing;
};

bool readArpRoute(const float *dialed, uint8_t route, ArpRoute &out);

Arp drawnFor(const Arp &arp, uint8_t route);

float arpLevel(const ArpRoute &route, const Arp &arp, float turns, uint8_t member);

static const uint8_t PASS_MARKS = 32;

struct ArpPass {
  float turns;
  float at;
  float length;
  uint8_t count;
  float starts[PASS_MARKS];
  uint8_t pars[PASS_MARKS];
};

void passAt(const Arp &arp, bool ripple, float turns, ArpPass &out);

}
