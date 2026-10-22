#pragma once

#include <stdint.h>

#include "arp.h"
#include "render.h"
#include "routes.h"

namespace render {

void readPars(const float *dialed, const Modulation &modulation, float lfo, Frame &out);

void holdParPulses(uint32_t milliseconds, Wall &wall, Frame &out);

bool firstArpPass(const float *dialed, const Modulation &modulation, ArpPass &out);

uint8_t routedAtPar(const float *dialed, const Modulation &modulation, uint8_t control, uint8_t par);

}
