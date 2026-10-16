#pragma once

#include <stdint.h>

namespace render {

struct BendTable;

const BendTable *bendFor(float amount, float at);

float bendSpeedRatio(float amount);

float unbentAlong(const BendTable *bend, float along);

float bentAlong(const BendTable *bend, float along);

void readBend(const BendTable *bend, float *out);

}
