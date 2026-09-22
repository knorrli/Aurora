#include "dmx_out.h"

#include "Aurora.h"
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

// Serial4 (TX = pin 17): Serial2, Serial3 and Serial5 all have their TX
// pin inside the OctoWS2811 reservation. See docs/wiring.md § "DMX OUT".
static teensydmx::Sender dmx{Serial4};

struct Fixture {
    uint16_t address;
    bool hasWhite;
    uint8_t trim[4];   // R, G, B, W color balance — 255 is unity
    uint8_t master;    // scales the dimmer; a PAR at full dwarfs the strips
};

// Four BeamZ BCC145 of our own, chained alongside the strips, each in its
// 8-channel mode — so the blocks are A001, A009, A017, A025. Channel
// layout is in docs/wiring.md § "Fixture profile — BeamZ BCC145". Trims
// and master are unity until calibrated against the strips. A fixture
// that is not plugged in simply ignores its channels, so this table is
// safe to run with fewer connected.
static Fixture fixtures[] = {
    {  1, true, { 255, 255, 255, 255 }, 255 },
    {  9, true, { 255, 255, 255, 255 }, 255 },
    { 17, true, { 255, 255, 255, 255 }, 255 },
    { 25, true, { 255, 255, 255, 255 }, 255 },
};

static uint8_t lastWritten[8] = { 0 };

// Deliberately independent of PRESET_OFF: blacking the washes out under a
// running pattern is its own control. The "everything off" gesture is the
// controller sending PC 0 and CC_WASH_LEVEL 0 together.
static uint8_t washLevel = 255;
static uint8_t washHueOffset = 0;

namespace dmx_out {

void begin() {
    dmx.begin();
}

void tick() {
    // Color is converted at full value and brightness is carried by the
    // fixture's own dimmer, so the emitters stay near full scale where
    // they have the most resolution. Scaling RGBW down instead — the only
    // option the 4-channel personality offers — bands on slow fades at
    // the low levels the washes normally sit at.
    CHSV hsv = presetColor;
    const uint8_t level = scale8(hsv.value, washLevel);
    hsv.value = 255;
    hsv.hue += washHueOffset;

    CRGB rgb;
    hsv2rgb_rainbow(hsv, rgb);

    // Pull the common component out into the white channel: an RGBW
    // fixture mixing white from its color emitters is dimmer than its
    // white one and usually tinted.
    const uint8_t common = min(rgb.r, min(rgb.g, rgb.b));

    for (uint8_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        const Fixture &fixture = fixtures[i];
        const uint8_t white = fixture.hasWhite ? common : 0;
        uint8_t values[8] = {
            scale8(level, fixture.master),
            0,
            scale8(rgb.r - white, fixture.trim[0]),
            scale8(rgb.g - white, fixture.trim[1]),
            scale8(rgb.b - white, fixture.trim[2]),
            scale8(white,         fixture.trim[3]),
            // Macro above 50 starts an auto sequence that overrides
            // color entirely, so it and its speed channel stay at zero.
            0,
            0,
        };

        dmx.set(fixture.address, values, 8);

        if (i == 0) memcpy(lastWritten, values, sizeof(lastWritten));
    }
}

void setLevel(uint8_t level) {
    washLevel = level;
}

void setHueOffset(uint8_t offset) {
    washHueOffset = offset;
}

const uint8_t *lastValues() {
    return lastWritten;
}

} // namespace dmx_out
