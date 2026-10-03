#pragma once

#include <stdint.h>

namespace recall {

const uint8_t FULL_STRENGTH = 127;

void setBank(uint8_t bank);

void keyDown(uint8_t key);
void keyUp();

void stepDown(int8_t direction);
void stepUp();

void oneshotDown(uint8_t place, uint8_t velocity);
void oneshotUp(uint8_t place);

void forwardProgram(uint8_t program);
void songsReplaced();

}
