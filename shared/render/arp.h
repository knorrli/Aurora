#pragma once

#include <stdint.h>

namespace render {

struct Arp {
  uint8_t mode;
  bool reverse;
};

Arp readArp(const uint8_t *dialed);

uint8_t arpGroupCount(const Arp &arp);

uint8_t arpGroupOf(const Arp &arp, uint8_t par);

bool arpTurnLights(const Arp &arp, int32_t turn, uint8_t par);

bool lastTurnOf(const Arp &arp, int32_t turn, uint8_t par, int32_t &lit);

float rippleDelay(const Arp &arp, uint8_t par);

}
