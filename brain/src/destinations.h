#pragma once

#include <stdint.h>

// Every modulatable control, held as the byte that arrived rather than as
// whatever the renderer wants. A modulation route names its destination by
// CC number and pushes it in CC units, so the push has to land before the
// conversion: a count pushed after converting would step linearly while its
// own fader steps geometrically, and the same route and the same hand would
// disagree. See docs/modulation.md § "How a push lands".
//
// The cost is that conversions run per frame instead of once per MIDI
// message — at worst 43 controls against the 900 shape evaluations the frame
// already does.
namespace destinations {

// Call once from setup(), before anything reads: boot values live here, not
// in each renderer's statics.
void begin();

void store(uint8_t cc, uint8_t value);

// What a renderer converts: the dialed byte plus whatever is pushing it.
// Nothing pushes yet; routes arrive with the CC map.
uint8_t value(uint8_t cc);

}  // namespace destinations
