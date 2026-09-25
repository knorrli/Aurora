#pragma once

#include <stdint.h>

#include "render.h"
#include "routes.h"

namespace render {

void readPars(const uint8_t *dialed, const Pushes &pushes, float lfo, Frame &out);

uint8_t routedAtPar(const uint8_t *dialed, const Pushes &pushes, float lfo, uint8_t cc,
                    uint8_t par);

}
