#include "fan.h"

#include <math.h>

#include "render_math.h"

namespace render {

static const uint8_t FAN_RANDOMIZE_SALT = 118;

static inline float fanTriangle(float u) {
  return (u < 0.5f) ? (4.0f * u - 1.0f) : (3.0f - 4.0f * u);
}

float fanWave(const Fan &fan, uint8_t stripIndex) {
  const float ordered = fanTriangle(fract(fan.phase + fan.frequency * (float)stripIndex));
  if (fan.randomize < 0.0001f) return ordered;
  const float drawn = (float)hash8(stripIndex, 0, FAN_RANDOMIZE_SALT) / 255.0f * 2.0f - 1.0f;
  return ordered + (drawn - ordered) * fan.randomize;
}

void readFan(const Reading &plain, FanReading &out) {
  const Fan &fan = plain.fan;
  for (uint8_t i = 0; i < STRIPS; i++) out.values[i] = fanWave(fan, i);
  for (uint16_t i = 0; i < FAN_CURVE_POINTS; i++) {
    const float at = (float)i / (float)FAN_CURVE_STEPS_PER_STRIP;
    out.curve[i] = fanTriangle(fract(fan.phase + fan.frequency * at));
  }
  out.turns = fan.frequency * (float)(STRIPS - 1);
  out.stillAt = (fabsf(fan.speedPixels) > 0.0001f)
      ? -plain.shape.speedPixels / fan.speedPixels : 2.0f;
  out.spread = fan.spread * 2.0f;
  out.speed = fan.speedPixels / MAX_SPEED_PIXELS_PER_BEAT;
  out.lfo = fan.lfo * 2.0f;
  out.randomize = fan.randomize;
}

}
