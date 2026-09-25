#pragma once

#include <stdint.h>

namespace render {

struct Rgb {
  uint8_t r, g, b;
};

inline uint8_t scale8(uint8_t i, uint8_t scale) {
  return (uint8_t)(((uint16_t)i * (1 + (uint16_t)scale)) >> 8);
}

inline uint8_t scale8Video(uint8_t i, uint8_t scale) {
  return (uint8_t)((((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0));
}

inline Rgb scaleVideo(Rgb c, uint8_t scale) {
  return { scale8Video(c.r, scale), scale8Video(c.g, scale), scale8Video(c.b, scale) };
}

inline Rgb rainbowRgb(uint8_t hue) {
  const uint8_t offset8 = (uint8_t)((hue & 0x1f) << 3);
  const uint8_t third = scale8(offset8, 256 / 3);
  const uint8_t twoThirds = scale8(offset8, (256 * 2) / 3);

  uint8_t r, g, b;
  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
      else { r = 171; g = 85 + third; b = 0; }
    } else {
      if (!(hue & 0x20)) { r = 171 - twoThirds; g = 170 + third; b = 0; }
      else { r = 0; g = 255 - third; b = third; }
    }
  } else {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 0; g = 171 - twoThirds; b = 85 + twoThirds; }
      else { r = third; g = 0; b = 255 - third; }
    } else {
      if (!(hue & 0x20)) { r = 85 + third; g = 0; b = 171 - third; }
      else { r = 170 + third; g = 0; b = 85 - third; }
    }
  }
  return { r, g, b };
}

inline Rgb withSaturationAndValue(Rgb c, uint8_t saturation, uint8_t value) {
  uint8_t r = c.r, g = c.g, b = c.b;
  if (saturation != 255) {
    if (saturation == 0) {
      r = 255; g = 255; b = 255;
    } else {
      const uint8_t desaturation = scale8Video(255 - saturation, 255 - saturation);
      const uint8_t saturationScale = 255 - desaturation;
      r = scale8(r, saturationScale) + desaturation;
      g = scale8(g, saturationScale) + desaturation;
      b = scale8(b, saturationScale) + desaturation;
    }
  }

  if (value != 255) {
    const uint8_t dimmed = scale8Video(value, value);
    if (dimmed == 0) {
      r = 0; g = 0; b = 0;
    } else {
      r = scale8(r, dimmed);
      g = scale8(g, dimmed);
      b = scale8(b, dimmed);
    }
  }

  return { r, g, b };
}

}
