#pragma once

#include <stdint.h>

#include "render.h"

namespace render {

static const uint16_t TAIL_BINS = 256;

struct Tail {
  float age[TAIL_BINS];
  float lengthCells;
  float direction;
};

struct TailFrame {
  float beats;
  float lastBeats;
  int32_t step;
  int32_t lastStep;
  bool fresh;
  bool bouncing;
  bool flipped;
};

float recordTail(TailHistory &history, const TailFrame &frame, float center, float placement);

void walkTail(const TailHistory &history, int32_t newestStep, float beats, float center,
              float halfWidth, float tailBeats, Tail &out);

float tailAgeAt(const Tail &tail, float cells);

}
