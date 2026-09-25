#pragma once

#include <stdint.h>

namespace tempo {

void begin();
void advance();

float quarterNotes();
bool beatStarted();
bool running();

void onClock();
void onStart();
void onContinue();
void onStop();
void setDivision(uint8_t division);

}
