#pragma once

#include <stdint.h>

#include "color8.h"

// The palettes a patch picks with CC_PALETTE: what a hue walks through in
// place of the rainbow. Index 0 is the rainbow itself.
namespace render {

uint8_t paletteCount();
const char *paletteName(uint8_t index);

// Hue to color, before saturation and brightness. An index past the end is
// the rainbow.
Rgb paletteRgb(uint8_t index, uint8_t hue);

}  // namespace render
