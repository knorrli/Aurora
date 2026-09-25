#pragma once

#include <stdint.h>

namespace render {

struct BendTable;

const BendTable *bendFor(float amount, float at);

float bentCells(const BendTable *bend, bool perCell, float pixel, float cellLength, uint8_t count);

void readBend(const BendTable *bend, bool perCell, float cellLength, uint8_t count, float *out);

}
