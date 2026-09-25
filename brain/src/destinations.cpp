#include "destinations.h"

#include "aurora_protocol.h"

namespace destinations {

static uint8_t dialed[AURORA_PATCH_CC_COUNT];

// Bipolar controls boot at 64, their "no departure at all". The rest boot at
// the nearest byte to what the renderers want, which for a few is not exact —
// 38 is 0.2992 where 0.3 was wanted. The alternative is a second set of
// defaults nobody can dial.
struct Dial {
  uint8_t cc;
  uint8_t value;
};

static const Dial BOOT[] = {
  { CC_WASH_LEVEL,      127 },
  { CC_WASH_SATURATION, 127 },
  { CC_SATURATION,      127 },
  { CC_VALUE,           127 },
  { CC_COLOR_RULER,      64 },
  { CC_GEN_WIDTH,        38 },
  { CC_GEN_EDGE,         19 },
  { CC_GEN_POSITION,     64 },
  { CC_GEN_SPEED,        64 },
  { CC_GEN_FAN,          64 },
  { CC_GEN_FAN_RATE,     64 },
  { CC_GEN_FAN_LFO,    64 },
  { CC_GEN_FAN_FREQ,     32 },
  { CC_PLACED_HUE,       64 },
  { CC_PLACED_WHITE,     64 },
  { CC_PLACED_DARK,      64 },
  { CC_PLACED_WIDTH,     64 },
  { CC_PLACED_EDGE,      64 },
  { CC_PLACED_SPEED,     64 },
  { CC_WANDER_HUE,       64 },
  { CC_WANDER_WHITE,     64 },
  { CC_WANDER_DARK,      64 },
  { CC_LIT_HUE,          64 },
  { CC_LIT_DARK,         64 },
  { CC_SCATTER_WIDTH,    64 },
  { CC_SCATTER_EDGE,     64 },
  { CC_SCATTER_DRIFT,    64 },
  { CC_SCATTER_LIGHT,    64 },
  { CC_SCATTER_HUE,      64 },
  { CC_SCATTER_WHITE,    64 },
};

void begin() {
  for (uint8_t i = 0; i < sizeof(BOOT) / sizeof(BOOT[0]); i++) {
    dialed[BOOT[i].cc] = BOOT[i].value;
  }
}

void store(uint8_t cc, uint8_t value) {
  if (cc < AURORA_PATCH_CC_COUNT) dialed[cc] = value;
}

uint8_t value(uint8_t cc) {
  return (cc < AURORA_PATCH_CC_COUNT) ? dialed[cc] : 0;
}

const uint8_t *all() { return dialed; }

}  // namespace destinations
