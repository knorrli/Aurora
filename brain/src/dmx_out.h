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

// Wash master, 0–255. Independent of the strips, but not of PRESET_OFF,
// which darkens everything. See CC_WASH_LEVEL in shared/aurora_protocol.h.
void setLevel(uint8_t level);

// Rotates the washes off the strips' hue; 0 matches them. See
// CC_WASH_HUE_OFFSET in shared/aurora_protocol.h.
void setHueOffset(uint8_t offset);

// Scales the washes down from the strips' saturation, 0-255; 255 matches
// them and 0 is white. See CC_WASH_SATURATION in shared/aurora_protocol.h.
void setSaturation(uint8_t saturation);

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
