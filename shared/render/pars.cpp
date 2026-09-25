#include "pars.h"

#include <math.h>

#include "palettes.h"
#include "reading.h"
#include "render_math.h"

namespace render {

static const uint16_t SHUFFLE_LOOKBACK_CYCLES = 128;
static const uint8_t HUE_SHUFFLE_SALT = 29;
static const uint8_t LFO_SHUFFLE_SALT = 71;

static void shuffleSlots(float cycles, float chance, uint8_t salt, uint8_t *slots) {
  for (uint8_t i = 0; i < PARS; i++) slots[i] = i;
  if (chance < 0.001f) return;

  int32_t cycle = (int32_t)floorf(cycles);
  for (uint16_t back = 0; back < SHUFFLE_LOOKBACK_CYCLES; back++, cycle--) {
    const float roll = ((float)hash8((uint32_t)cycle, PARS, salt) + 0.5f) / 256.0f;
    if (roll >= chance) continue;
    for (uint8_t i = PARS - 1; i > 0; i--) {
      const uint8_t j = hash8((uint32_t)cycle, i, salt) % (i + 1);
      const uint8_t held = slots[i];
      slots[i] = slots[j];
      slots[j] = held;
    }
    return;
  }
}

static Par parAt(const uint8_t *dialed, const Pushes *pushes, float hueFromSpread) {
  auto at = [&](uint8_t cc) { return (uint8_t)dialedValue(cc, routed(dialed, pushes, cc)); };
  const Hsv strips = dialedColor(dialed);
  const int32_t hue =
      (int32_t)strips.h + at(CC_PAR_HUE_OFFSET) + (int32_t)lroundf(hueFromSpread);
  const uint8_t saturation = scale8(strips.s, at(CC_PAR_SATURATION));
  return { paletteColor(dialed[CC_PALETTE], (uint8_t)(hue & 255), saturation),
           scale8(strips.v, at(CC_PAR_VALUE)) };
}

void readPars(const uint8_t *dialed, const Pushes &pushes, float beats, float lfo, float lfoBeats,
              Frame &out) {
  auto at = [&](uint8_t cc) { return dialedValue(cc, routed(dialed, &pushes, cc)); };
  const float hueShuffleBeats =
      dialedValue(CC_PAR_HUE_SHUFFLE_EVERY, dialed[CC_PAR_HUE_SHUFFLE_EVERY]);
  uint8_t hueSlots[PARS];
  uint8_t lfoSlots[PARS];
  shuffleSlots(beats / hueShuffleBeats, at(CC_PAR_HUE_SHUFFLE), HUE_SHUFFLE_SALT, hueSlots);
  shuffleSlots(lfo, at(CC_PAR_LFO_SHUFFLE), LFO_SHUFFLE_SALT, lfoSlots);
  const float hueStep = at(CC_PAR_HUE_SPREAD);
  const float lfoStep = at(CC_PAR_LFO_SPREAD);

  Pushes parPushes;
  for (uint8_t i = 0; i < PARS; i++) {
    out.parLfo[i] = lfo - lfoStep * (float)lfoSlots[i];
    gatherRoutes(dialed, lfoBeats, out.parLfo[i], out.parLfo[i], parPushes);
    out.pars[i] = parAt(dialed, &parPushes, hueStep * (float)hueSlots[i]);
  }
}

}
