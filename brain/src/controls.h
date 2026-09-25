#pragma once

#include <stdint.h>

namespace controls {

void begin();
void store(uint8_t cc, uint8_t value);
const uint8_t *all();

}
