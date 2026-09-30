#pragma once

#include "render.h"

namespace render {

static const float SETTLE_BEATS = 2.0f;

float clockPhase(Clock &clock, float beats, float rate);

float anchoredPhase(Clock &clock, float beats, float elapsed, float rate);

void pullToWhole(float &cycles, float elapsed, float rate);

float anchoredLfoPhase(Motion &motion, float beats, float rate);

}
