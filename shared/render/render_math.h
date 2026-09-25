#pragma once

#include <math.h>
#include <stdint.h>

namespace render {

static const float TURN = 6.28318530718f;

inline float fract(float x) { return x - floorf(x); }

inline uint8_t hash8(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 73856093u ^ b * 19349663u ^ c * 83492791u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return (uint8_t)h;
}

inline uint32_t hash32(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B1u + b * 0x85EBCA77u + c * 0xC2B2AE3Du;
  h ^= h >> 16;
  h *= 0x85EBCA6Bu;
  h ^= h >> 13;
  h *= 0xC2B2AE35u;
  h ^= h >> 16;
  return h;
}

inline float unitHash(uint32_t a, uint32_t b, uint32_t c) {
  return ((float)(hash32(a, b, c) >> 8) + 0.5f) / 16777216.0f;
}

inline float clampUnit(float x) { return (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x); }

inline float bumpAt(float offset, float width, float edge) {
  const float halfCore = width * 0.5f;
  const float distance = fabsf(offset);
  if (distance <= halfCore) return 1.0f;

  const float spread = edge * (1.0f - width) * 0.5f;
  const float beyond = distance - halfCore;
  if (spread > 0.0001f && beyond < spread) {
    const float k = 1.0f - (beyond / spread);
    return k * k * (3.0f - 2.0f * k);
  }
  return 0.0f;
}

}
