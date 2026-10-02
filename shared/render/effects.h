#pragma once

#include "render.h"

namespace render {

void darkenOutside(Frame &frame, const bool *marked, float depth);

void blendToward(Frame &frame, const Rgb *pixels, const Par *pars, const bool *marked, float share);

void blackOut(Frame &frame, const bool *marked);

void flashWhite(Frame &frame, const bool *marked);

}
