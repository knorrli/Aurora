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

}
