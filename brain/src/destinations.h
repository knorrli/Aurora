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

// Every byte, indexed by CC number — what a patch is, see DESIGN.md
// § "Patch storage". This is the only place the brain knows what a control
// was set to, because a renderer converts on the way out and the conversion
// cannot be inverted: a count is round(20^(value/127)).
//
// A control nobody has sent reads its boot value rather than 0, so a snapshot
// taken before the brain has been driven records what is lit rather than a
// wall of zeroes.
const uint8_t *all();

}  // namespace destinations
