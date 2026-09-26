#pragma once

#include <stdint.h>

#include "routes.h"

namespace render {

struct Arp {
  uint8_t mode;
  float spread;
};

Arp readArp(const uint8_t *dialed, const Pushes *pushes);

uint8_t arpGroupCount(const Arp &arp);

uint8_t arpGroupOf(const Arp &arp, uint8_t par);

struct Pulse {
  float start;
  float length;
  uint32_t id;
};

bool lastPulse(const Arp &arp, bool ripple, float turns, uint8_t par, Pulse &out);

static const uint8_t LIVE_PULSES = 8;

uint8_t livePulses(const Arp &arp, bool ripple, float turns, uint8_t par, Pulse *out);

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
