#pragma once

#include "render.h"

namespace render {

float clockPhase(Clock &clock, float beats, float rate);

float anchoredLfoPhase(Motion &motion, float beats, float rate);

}
