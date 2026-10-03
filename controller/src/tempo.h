#pragma once

#include <stdint.h>

namespace tempo {

void begin(uint32_t micros);
void update(uint32_t micros);

void incomingTick(uint32_t micros);
void incomingStart(uint32_t micros);
void incomingContinue();
void incomingStop();

void tap(uint32_t micros);

uint32_t microsPerBeat();

}
