#include "tails.h"

#include <math.h>

#include "render_math.h"

namespace render {

void clearTails(Wall &wall) { wall.tails.empty = true; }

static inline uint16_t historySlot(int32_t step) {
  const int32_t slot = step % (int32_t)TAIL_STEPS;
  return (uint16_t)(slot < 0 ? slot + TAIL_STEPS : slot);
}

static const float TAIL_JUMP_CELLS = 0.25f;

static void recordCenter(TailHistory &history, const TailFrame &frame, bool fresh, float center) {
  if (fresh) {
    history.firstStep = frame.step + 1;
    history.lastCenter = center;
    return;
  }
  const float elapsed = frame.beats - frame.lastBeats;
  for (int32_t at = frame.lastStep + 1; at <= frame.step; at++) {
    const float k = (elapsed > 0.0f)
        ? ((float)at / (float)TAIL_STEPS_PER_BEAT - frame.lastBeats) / elapsed : 1.0f;
    history.centers[historySlot(at)] = history.lastCenter + k * (center - history.lastCenter);
  }
  history.lastCenter = center;
}

float recordTail(TailHistory &history, const TailFrame &frame, float center, float placement) {
  if (frame.fresh) history.unwrapCells = 0.0f;
  else if (frame.flipped) {
    history.unwrapCells += roundf(history.lastCenter - (center + history.unwrapCells));
  }
  bool jumped = false;
  if (!frame.fresh) {
    const float moved = placement - history.lastPlacement;
    const float wraps = frame.bouncing ? 0.0f : roundf(moved);
    history.unwrapCells -= wraps;
    jumped = fabsf(moved - wraps) > TAIL_JUMP_CELLS;
  }
  history.lastPlacement = placement;
  const float unwrapped = center + history.unwrapCells;
  recordCenter(history, frame, frame.fresh || jumped, unwrapped);
  return unwrapped;
}

static void stampReached(Tail &tail, float from, float to, float reach,
                         float newerCells, float newerAge, float olderCells, float olderAge) {
  const float walked = olderCells - newerCells;
  const int32_t first = (int32_t)ceilf(from * (float)TAIL_BINS - 0.5f);
  for (int32_t bin = first;; bin++) {
    const float point = ((float)bin + 0.5f) / (float)TAIL_BINS;
    if (point > to) break;
    if (point <= from) continue;
    int32_t slot = bin % (int32_t)TAIL_BINS;
    if (slot < 0) slot += TAIL_BINS;
    if (tail.age[slot] >= 0.0f) continue;
    const float k =
        clampUnit((fabsf(walked) > 0.000001f) ? (point - reach - newerCells) / walked : 1.0f);
    tail.age[slot] = newerAge + k * (olderAge - newerAge);
  }
}

void walkTail(const TailHistory &history, int32_t newestStep, float beats, float center,
              float halfWidth, float tailBeats, Tail &out) {
  for (uint16_t i = 0; i < TAIL_BINS; i++) out.age[i] = -1.0f;
  out.lengthCells = 0.0f;
  out.direction = 0.0f;

  float low = center - halfWidth;
  float high = center + halfWidth;
  stampReached(out, low - 0.000001f, high, 0.0f, center, 0.0f, center, 0.0f);

  float newerCells = center;
  float newerAge = 0.0f;
  const int32_t oldest = newestStep - (int32_t)TAIL_STEPS + 1;
  for (int32_t step = newestStep; step >= history.firstStep && step >= oldest; step--) {
    if (newerAge >= tailBeats) break;
    const float olderCells = history.centers[historySlot(step)];
    const float olderAge = beats - (float)step / (float)TAIL_STEPS_PER_BEAT;
    const float walked = newerCells - olderCells;

    if (out.direction == 0.0f && fabsf(walked) > 0.00001f) {
      out.direction = (walked > 0.0f) ? 1.0f : -1.0f;
    }
    const float within = (olderAge <= tailBeats || olderAge <= newerAge)
        ? 1.0f : (tailBeats - newerAge) / (olderAge - newerAge);
    out.lengthCells += fabsf(walked) * within;

    if (high - low < 1.0f) {
      if (olderCells + halfWidth > high) {
        const float reached = fminf(olderCells + halfWidth, low + 1.0f);
        stampReached(out, high, reached, halfWidth, newerCells, newerAge, olderCells, olderAge);
        high = reached;
      }
      if (olderCells - halfWidth < low) {
        const float reached = fmaxf(olderCells - halfWidth, high - 1.0f);
        stampReached(out, reached, low, -halfWidth, newerCells, newerAge, olderCells, olderAge);
        low = reached;
      }
    }
    newerCells = olderCells;
    newerAge = olderAge;
  }
}

float tailAgeAt(const Tail &tail, float cells) {
  uint16_t bin = (uint16_t)(fract(cells) * (float)TAIL_BINS);
  if (bin >= TAIL_BINS) bin = TAIL_BINS - 1;
  return tail.age[bin];
}

}
