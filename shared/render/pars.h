#pragma once

#include <stdint.h>

#include "render.h"
#include "routes.h"

namespace render {

void readPars(const uint8_t *dialed, const Pushes &pushes, float beats, float lfo, float lfoBeats,
              Frame &out);

}
