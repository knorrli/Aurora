#pragma once

#include <stdint.h>

// Every modulatable control, held as the byte that arrived rather than as
// whatever the renderer wants. A route pushes in CC units, so the push has to
// land before the conversion: a count pushed after converting would step
// linearly while its own fader steps geometrically.
namespace destinations {

// Once from setup(), before anything reads.
void begin();

void store(uint8_t cc, uint8_t value);

// The dialed byte. render::routed() is the same byte with the routes applied.
uint8_t value(uint8_t cc);

// Every byte, indexed by CC number — what a patch is, see DESIGN.md § "Patch
// storage". The only place the brain knows what a control was set to, since a
// renderer converts on the way out and a count is round(20^(value/127)),
// which cannot be inverted. A control nobody has sent reads its boot value
// rather than 0.
const uint8_t *all();

}  // namespace destinations
