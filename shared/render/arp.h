#pragma once

#include <stdint.h>

#include "reading.h"
#include "routes.h"

namespace render {

static const uint8_t MAX_ARP_MEMBERS = STRIPS > PARS ? STRIPS : PARS;

struct Arp {
  uint8_t mode;
  bool reverse;
  float rateBeats;
  float gateBeats;
  uint8_t members;
  uint32_t draw;
  Envelope envelopes[AURORA_ENVELOPES];
};

Arp readArp(const float *dialed, const Modulation *modulation, uint8_t members);

uint8_t arpGroupCount(const Arp &arp);

uint8_t arpGroupOf(const Arp &arp, uint8_t member);

uint8_t passLength(const Arp &arp);

bool reversed(const Arp &arp);

struct ArpRoute {
  uint8_t route;
  uint8_t target;
  uint8_t arp;
  uint8_t source;
  float amount;
  Wave wave;
};

bool readArpRoute(const float *dialed, uint8_t route, ArpRoute &out);

struct Note {
  int32_t step;
  float age;
};

bool lastNote(const ArpRoute &route, const Arp &arp, float beatsAgo, uint8_t member, Note &out);

float arpLevel(const ArpRoute &route, const Arp &arp, float beatsAgo, uint8_t member);

static const uint8_t PASS_MARKS = 32;

struct ArpPass {
  float beats;
  float at;
  float length;
  uint8_t count;
  float starts[PASS_MARKS];
  uint8_t pars[PASS_MARKS];
};

void passAt(const ArpRoute &route, const Arp &arp, ArpPass &out);

}
