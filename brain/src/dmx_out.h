// dmx_out — drives the wash fixtures.
//
// Every frame each configured fixture gets the wash the renderer worked out
// (render::washFrom in shared/render/generator.cpp), written as 8 channels
// with brightness on the fixture's own dimmer. The macro channel must stay
// below 50 or the fixture starts an auto sequence that overrides color
// entirely.
//
// Fixture addresses and calibration are hardcoded below; changing venue
// means editing the table and reflashing.

#ifndef AURORA_BRAIN_DMX_OUT_H
#define AURORA_BRAIN_DMX_OUT_H

#include <stdint.h>

#include <render.h>

namespace dmx_out {

void begin();
void tick(const render::Wash &wash);

// The eight channel values last written to the first fixture, for the
// debug line.
const uint8_t *lastValues();

} // namespace dmx_out

#endif // AURORA_BRAIN_DMX_OUT_H
