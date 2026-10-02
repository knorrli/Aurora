#pragma once

#include <stdint.h>

#include "arp.h"
#include "render.h"
#include "routes.h"

namespace render {

void readPars(const uint8_t *dialed, const Modulation &modulation, float lfo, Frame &out);

void holdParPulses(uint32_t milliseconds, Wall &wall, Frame &out);

bool firstArpPass(const uint8_t *dialed, const Modulation &modulation, float lfo, ArpPass &out);

uint8_t routedAtPar(const uint8_t *dialed, const Modulation &modulation, float lfo, uint8_t cc,
                    uint8_t par);

}
