#pragma once

#include <stdint.h>

namespace faders {

enum Fader : uint8_t { COLOR, MOTION, EXTENT, COUNT };

void begin();
void sendAll();
void update(uint32_t micros);

void pushDown(Fader fader, uint32_t micros);
void pushUp(Fader fader, uint32_t micros);

bool cueing();
uint8_t playing(Fader fader);
uint8_t standing(Fader fader);

}
