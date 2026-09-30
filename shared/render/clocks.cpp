#include "clocks.h"

#include <math.h>

namespace render {

static const float ANCHOR_CYCLES = 2.0f;

float clockPhase(Clock &clock, float beats, float rate) {
  if (rate != clock.rate) {
    clock.offset += beats * (clock.rate - rate);
    clock.rate = rate;
  }
  return beats * rate + clock.offset;
}

float anchoredPhase(Clock &clock, float beats, float elapsed, float rate) {
  if (elapsed < 0.0f) {
    clock.offset = 0.0f;
    clock.rate = rate;
    return beats * rate;
  }

  const float phase = clockPhase(clock, beats, rate);
  const float before = clock.offset;
  pullToWhole(clock.offset, elapsed, rate);
  return phase + clock.offset - before;
}

void pullToWhole(float &cycles, float elapsed, float rate) {
  const float drift = cycles - roundf(cycles);
  if (fabsf(drift) < 0.0001f) return;
  float pull = (rate == 0.0f) ? elapsed / SETTLE_BEATS : elapsed * fabsf(rate) / ANCHOR_CYCLES;
  if (pull > 1.0f) pull = 1.0f;
  cycles -= drift * pull;
}

float anchoredLfoPhase(Motion &motion, float beats, float rate) {
  const float elapsed = beats - motion.lastLfoBeats;
  motion.lastLfoBeats = beats;
  return anchoredPhase(motion.lfo, beats, elapsed, rate);
}

}
