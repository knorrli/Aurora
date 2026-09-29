#include "clocks.h"

#include <math.h>

namespace render {

static const float LFO_ANCHOR_CYCLES = 2.0f;

float clockPhase(Clock &clock, float beats, float rate) {
  if (rate != clock.rate) {
    clock.offset += beats * (clock.rate - rate);
    clock.rate = rate;
  }
  return beats * rate + clock.offset;
}

float anchoredLfoPhase(Motion &motion, float beats, float rate) {
  const float elapsed = beats - motion.lastLfoBeats;
  motion.lastLfoBeats = beats;

  if (elapsed < 0.0f) {
    motion.lfo.offset = 0.0f;
    motion.lfo.rate = rate;
    return beats * rate;
  }

  const float phase = clockPhase(motion.lfo, beats, rate);
  const float drift = motion.lfo.offset - roundf(motion.lfo.offset);
  if (fabsf(drift) < 0.0001f) return phase;

  float pull = elapsed * rate / LFO_ANCHOR_CYCLES;
  if (pull > 1.0f) pull = 1.0f;
  motion.lfo.offset -= drift * pull;
  return phase - drift * pull;
}

}
