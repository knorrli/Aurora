#pragma once

#include "render.h"

namespace render {

struct Travel {
  float beats;
  float elapsed;
  bool bouncing;
  bool flipped;
  float cellLength;
};

struct StripTravel {
  float speedPixels;
  float shiftPixels;
  float positionCells;
  float fanOffset;
  float width;
};

inline float directionOf(float speedPixels) { return (speedPixels >= 0.0f) ? 1.0f : -1.0f; }

float travelCenter(Clock &travelClock, Clock &swingClock, Anchor &anchor, const Travel &travel,
                   const StripTravel &strip);

}
