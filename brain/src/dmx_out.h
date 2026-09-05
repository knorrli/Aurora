// dmx_out — colour echo to venue fixtures.
//
// Scope is deliberately tiny: every frame, each configured fixture gets
// the colour the strips are showing. No choreography, no per-preset
// behaviour. See DESIGN.md § "DMX OUT for venue fixtures — color echo".
//
// Fixture addresses and calibration are hardcoded below; changing venue
// means editing the table and reflashing.

#ifndef AURORA_BRAIN_DMX_OUT_H
#define AURORA_BRAIN_DMX_OUT_H

#include <stdint.h>

namespace dmx_out {

void begin();
void tick();

// Channel values last written to the first fixture, for the debug line.
const uint8_t *lastValues();

} // namespace dmx_out

#endif // AURORA_BRAIN_DMX_OUT_H
