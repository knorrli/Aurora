#pragma once

#include <stdint.h>

#include "color8.h"

namespace render {

uint8_t paletteCount();
const char *paletteName(uint8_t index);

Rgb paletteColor(uint8_t palette, float hue, float saturation);

}
