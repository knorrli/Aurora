#pragma once

#include <stdint.h>

#include "bend.h"
#include "reading.h"
#include "routes.h"

namespace render {

static const uint8_t NO_CELL = 255;

struct CellsFrame {
  float beats;
  float start;
  float lfo;
  float lfoBeats;
  const BendTable *bend;
  float logical[STRIP_SAMPLES];
};

struct CellsStrip {
  uint8_t index;
  const float *dialed;
  const Reading *reading;
  const Modulation *live;
  const PadLevels *pad;
};

struct CellSample {
  float level;
  float body;
  float across;
  float place;
  uint8_t cell;
};

struct DrawnCells {
  CellSample samples[STRIP_SAMPLES];
  Hsv colors[MAX_CELLS];
  StripMarks marks;
};

float envelopeLevelAt(const Envelope &envelope, float gateBeats, uint8_t end, float stepBeats, float age);

float curveLevel(const Cells &cells, float along);

void readCurve(const Cells &cells, uint8_t strip, float *out);

void startCells(float beats, float start, float lfo, float lfoBeats, const Cells &cells, CellsFrame &out);

void drawCells(const CellsFrame &frame, const CellsStrip &strip, DrawnCells &out);

}
