#include "dmx_out.h"

#include "aurora.h"
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

// Serial4 (TX = pin 17): Serial2, Serial3 and Serial5 all have their TX
// pin inside the OctoWS2811 reservation. See docs/wiring.md § "DMX OUT".
static teensydmx::Sender dmx{Serial4};

struct Fixture {
    uint16_t address;
    bool hasWhite;
    uint8_t trim[4];   // R, G, B, W — 255 is unity
    uint8_t master;    // overall output scale; a PAR at full dwarfs the strips
};

// Four BeamZ BCC145 of our own, chained alongside the strips, each in its
// 4-channel mode — so the blocks are D001, D005, D009, D013. Trims and
// master are unity until calibrated against the strips at rehearsal. A
// fixture that is not plugged in simply ignores its channels, so this
// table is safe to run with fewer connected.
static Fixture fixtures[] = {
    {  1, true, { 255, 255, 255, 255 }, 255 },
    {  5, true, { 255, 255, 255, 255 }, 255 },
    {  9, true, { 255, 255, 255, 255 }, 255 },
    { 13, true, { 255, 255, 255, 255 }, 255 },
};

static uint8_t lastWritten[4] = { 0, 0, 0, 0 };

namespace dmx_out {

void begin() {
    dmx.begin();
}

void tick() {
    CRGB rgb = CRGB::Black;
    if (currentPreset != PRESET_OFF) {
        hsv2rgb_rainbow(presetColor, rgb);
    }

    // Pull the common component out into the white channel: an RGBW
    // fixture mixing white from its colour emitters is dimmer than its
    // white one and usually tinted.
    const uint8_t common = min(rgb.r, min(rgb.g, rgb.b));

    for (uint8_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        const Fixture &fixture = fixtures[i];
        const uint8_t white = fixture.hasWhite ? common : 0;
        uint8_t values[4] = {
            scale8(scale8(rgb.r - white, fixture.trim[0]), fixture.master),
            scale8(scale8(rgb.g - white, fixture.trim[1]), fixture.master),
            scale8(scale8(rgb.b - white, fixture.trim[2]), fixture.master),
            scale8(scale8(white,         fixture.trim[3]), fixture.master),
        };

        dmx.set(fixture.address, values, fixture.hasWhite ? 4 : 3);

        if (i == 0) memcpy(lastWritten, values, sizeof(lastWritten));
    }
}

const uint8_t *lastValues() {
    return lastWritten;
}

} // namespace dmx_out
