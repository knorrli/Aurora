#include "Aurora.h"

#include "destinations.h"
#include "tempo.h"

// The generator itself is shared/render/generator.cpp, which the editor runs
// too. This is only where its frame lands on the strips.
static render::Motion motion;
static render::Frame frame;

void Generator(CHSV color) {
  (void)color;
  render::renderGenerator(destinations::all(), tempo::beats(), motion, frame);
  showRendered(frame.pixels);
}

const render::Wash &generatorWash() { return frame.wash; }
