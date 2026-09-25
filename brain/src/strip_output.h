#pragma once

#include <render.h>

namespace strip_output {

void begin();
void showStartupSequence();
void show(const render::Rgb *pixels, bool blackout);

}
