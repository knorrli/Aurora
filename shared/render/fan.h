#pragma once

#include <stdint.h>

#include "reading.h"

namespace render {

float fanWave(const Fan &fan, uint8_t stripIndex);

void readFan(const Reading &plain, FanReading &out);

}
