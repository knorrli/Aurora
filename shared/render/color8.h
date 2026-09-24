#pragma once

#include <stdint.h>

// FastLED's byte maths and its rainbow HSV conversion, ported so the brain
// and the browser convert color with the same code. Ported from FastLED
// 3.10 as the brain builds it: FASTLED_SCALE8_FIXED is 1 and SCALE8_C is 1
// on the Teensy, and hsv2rgb_rainbow runs with Y1 on and Y2, G2 and Gscale
// off. Every hue the wall has ever been dialed to was chosen through this
// conversion, which widens and brightens yellow; a textbook HSV is not a
// substitute.
namespace render {

struct Rgb {
  uint8_t r, g, b;
};

inline uint8_t scale8(uint8_t i, uint8_t scale) {
  return (uint8_t)(((uint16_t)i * (1 + (uint16_t)scale)) >> 8);
}

// Never scales a lit channel to zero.
inline uint8_t scale8Video(uint8_t i, uint8_t scale) {
  return (uint8_t)((((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0));
}

inline Rgb scaleVideo(Rgb c, uint8_t scale) {
  return { scale8Video(c.r, scale), scale8Video(c.g, scale), scale8Video(c.b, scale) };
}

inline Rgb hsvRainbow(uint8_t hue, uint8_t sat, uint8_t val) {
  const uint8_t offset8 = (uint8_t)((hue & 0x1f) << 3);
  const uint8_t third = scale8(offset8, 256 / 3);
  const uint8_t twothirds = scale8(offset8, (256 * 2) / 3);

  uint8_t r, g, b;
  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
      else { r = 171; g = 85 + third; b = 0; }
    } else {
      if (!(hue & 0x20)) { r = 171 - twothirds; g = 170 + third; b = 0; }
      else { r = 0; g = 255 - third; b = third; }
    }
  } else {
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) { r = 0; g = 171 - twothirds; b = 85 + twothirds; }
      else { r = third; g = 0; b = 255 - third; }
    } else {
      if (!(hue & 0x20)) { r = 85 + third; g = 0; b = 171 - third; }
      else { r = 170 + third; g = 0; b = 85 - third; }
    }
  }

  if (sat != 255) {
    if (sat == 0) {
      r = 255; g = 255; b = 255;
    } else {
      const uint8_t desat = scale8Video(255 - sat, 255 - sat);
      const uint8_t satscale = 255 - desat;
      r = scale8(r, satscale) + desat;
      g = scale8(g, satscale) + desat;
      b = scale8(b, satscale) + desat;
    }
  }

  if (val != 255) {
    const uint8_t v = scale8Video(val, val);
    if (v == 0) {
      r = 0; g = 0; b = 0;
    } else {
      r = scale8(r, v);
      g = scale8(g, v);
      b = scale8(b, v);
    }
  }

  return { r, g, b };
}

}  // namespace render
