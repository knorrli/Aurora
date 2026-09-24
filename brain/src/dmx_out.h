// dmx_out — drives the wash fixtures.
//
// Every frame each configured fixture gets the color the strips are
// showing, rotated by CC_WASH_HUE_OFFSET and scaled by CC_WASH_LEVEL,
// written as 8 channels with brightness on the fixture's own dimmer.
// The macro channel must stay below 50 or the fixture starts an auto
// sequence that overrides color entirely.
//
// Fixture addresses and calibration are hardcoded below; changing venue
// means editing the table and reflashing.

#ifndef AURORA_BRAIN_DMX_OUT_H
#define AURORA_BRAIN_DMX_OUT_H

#include <stdint.h>

namespace dmx_out {

void begin();
void tick();

// What the generator's pulse is doing to the washes this frame. `level` and
// `saturation` are signed fractions of the way from the dialed value to one
// of its limits, the sign picking which; `hueOffset` is a plain rotation in
// hue units. Written before tick() and cleared by it, so a preset that never
// writes one leaves the washes unpushed rather than inheriting the last
// frame the generator drew.
void setPulsePush(float level, float hueOffset, float saturation);

// The eight channel values last written to the first fixture, for the
// debug line.
const uint8_t *lastValues();

} // namespace dmx_out

#endif // AURORA_BRAIN_DMX_OUT_H
