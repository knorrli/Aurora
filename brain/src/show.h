#pragma once

#include <stdint.h>

#include <render.h>

namespace show {

void select(uint8_t program);
void advance(bool beatStarted, bool transportRunning);
bool blackout();
const render::Frame &render(float quarterNotes);

}
