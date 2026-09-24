#include "routes.h"

#include <math.h>

namespace render {

static inline float fract(float x) { return x - floorf(x); }

static inline float raisedCosine(float x) {
  const float t = (x < 0.0f) ? 0.0f : (x > 1.0f ? 1.0f : x);
  return 0.5f - 0.5f * cosf((float)M_PI * t);
}

// Saw down has to come before square, or the two blend into something that is
// neither. In this order it is one decay getting shorter and harder.
float pulseWave(float phase, uint8_t wave) {
  float attack, decay, hard;
  if (wave <= GEN_WAVE_SAW_DOWN) {
    decay = (float)wave / (float)GEN_WAVE_SAW_DOWN;
    attack = 1.0f - decay;
    hard = 0.0f;
  } else {
    attack = 0.0f;
    const float toSquare = (float)(wave - GEN_WAVE_SAW_DOWN)
                         / (float)(GEN_WAVE_SQUARE - GEN_WAVE_SAW_DOWN);
    hard = (toSquare > 1.0f) ? 1.0f : toSquare;
    decay = (wave <= GEN_WAVE_SQUARE)
        ? 1.0f - 0.5f * toSquare
        : 0.5f * powf(GEN_PULSE_MIN_WIDTH / 0.5f,
                      (float)(wave - GEN_WAVE_SQUARE)
                          / (float)(127 - GEN_WAVE_SQUARE));
  }

  const float u = fract(phase);
  if (decay > 0.0f && u <= decay) {
    // Dividing by what is left of softness is what turns the decay into a
    // cliff: at the hard end everything above the floor saturates.
    const float soft = (1.0f - hard < 0.001f) ? 0.001f : 1.0f - hard;
    const float shaped = raisedCosine(1.0f - u / decay) / soft;
    return (shaped > 1.0f) ? 1.0f : shaped;
  }
  if (attack > 0.0f && u >= 1.0f - attack) {
    return raisedCosine((u - (1.0f - attack)) / attack);
  }
  return 0.0f;
}

// A rate feeds a running total, so a push on one accumulates and the wall
// drifts instead of returning. Tempo division is an index, not a level.
static bool refused(uint8_t cc) {
  switch (cc) {
    case CC_TEMPO_DIVISION:
    case CC_PLACED_SPEED:
    case CC_WANDER_RATE:
    case CC_GEN_SPEED:
    case CC_GEN_FAN_RATE:
    case CC_GEN_PULSE_RATE:
    case CC_SCATTER_RATE:
      return true;
    default:
      return false;
  }
}

// 0 and 127 are the same place, so there is no limit to travel toward.
static bool circular(uint8_t cc) {
  switch (cc) {
    case CC_HUE:
    case CC_WASH_HUE_OFFSET:
    case CC_GEN_POSITION:
    case CC_GEN_FAN_PHASE:
      return true;
    default:
      return false;
  }
}

// The fan's own controls shape the strips' shifted reading of the clock, so a
// route aimed at one while reading that would need its own phase to compute
// what sets its own phase. Count sets the cell geometry and is read before the
// strip loop opens. The washes have no strip to be offset from.
static bool plainClock(uint8_t cc) {
  switch (cc) {
    case CC_WASH_LEVEL:
    case CC_WASH_HUE_OFFSET:
    case CC_WASH_SATURATION:
    case CC_GEN_COUNT:
    case CC_GEN_FAN:
    case CC_GEN_FAN_FREQ:
    case CC_GEN_FAN_PHASE:
    case CC_GEN_FAN_RANDOM:
    case CC_GEN_FAN_PULSE:
      return true;
    default:
      return false;
  }
}

static inline float bipolar(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

void gatherRoutes(const uint8_t *dialed, float plainPhase, float stripPhase,
                  Pushes &out) {
  for (uint16_t i = 0; i < AURORA_PATCH_CC_COUNT; i++) out.amount[i] = 0.0f;

  for (uint8_t r = 0; r < AURORA_ROUTES; r++) {
    const uint8_t dest = dialed[aurora_route_cc(r, ROUTE_DESTINATION)];
    // CC 0 is never assigned, so it is free to mean "not aimed anywhere".
    if (dest == 0 || refused(dest)) continue;

    const float amount = bipolar(dialed[aurora_route_cc(r, ROUTE_AMOUNT)]);
    if (amount > -0.001f && amount < 0.001f) continue;

    const uint8_t ratio =
        aurora_route_ratio(dialed[aurora_route_cc(r, ROUTE_RATIO)]);
    const uint8_t wave = dialed[aurora_route_cc(r, ROUTE_WAVE)];
    const float phase = plainClock(dest) ? plainPhase : stripPhase;

    out.amount[dest] += amount * pulseWave(phase * (float)ratio, wave);
  }
}

uint8_t routed(const uint8_t *dialed, const Pushes *pushes, uint8_t cc) {
  const uint8_t base = dialed[cc];
  if (!pushes) return base;
  float amount = pushes->amount[cc];
  if (amount > -0.001f && amount < 0.001f) return base;

  if (amount > 1.0f) amount = 1.0f;
  else if (amount < -1.0f) amount = -1.0f;

  if (circular(cc)) {
    // Half the wheel at a full amount. Whether that span suits every circular
    // control is not settled.
    const int16_t turned = (int16_t)base + (int16_t)lroundf(amount * 64.0f);
    return (uint8_t)((turned % 128 + 128) % 128);
  }

  const float limit = (amount >= 0.0f) ? 127.0f : 0.0f;
  const long reached = lroundf((float)base + fabsf(amount) * (limit - (float)base));
  return (uint8_t)(reached < 0 ? 0 : (reached > 127 ? 127 : reached));
}

}  // namespace render
