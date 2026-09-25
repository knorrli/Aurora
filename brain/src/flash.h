#pragma once

#include <stdint.h>

#include <render.h>

namespace flash {

void fire(uint8_t hue);
void drawOver(render::Rgb *pixels);

}
