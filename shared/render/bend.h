#pragma once

#include <stdint.h>

namespace render {

struct BendTable;

const BendTable *bendFor(float amount, float at);

float cellsAtStart(float count);

float bentCells(const BendTable *bend, bool perCell, float pixel, float cellLength, float count);

float pixelOfCells(const BendTable *bend, bool perCell, float cells, float cellLength, float count);

void readBend(const BendTable *bend, bool perCell, float cellLength, float count, float *out);

}
