#pragma once

#include "render.h"

namespace render {

struct Travel {
  float beats;
  float elapsed;
  bool bouncing;
  bool walled;
  bool flipped;
};

struct StripTravel {
  float lapsPerBeat;
  float shiftLaps;
  float positionCells;
  float fanOffset;
  float width;
};

inline float directionOf(float lapsPerBeat) { return (lapsPerBeat >= 0.0f) ? 1.0f : -1.0f; }

float travelCenter(Clock &travelClock, Clock &swingClock, Anchor &anchor, const Travel &travel,
                   const StripTravel &strip);

}
