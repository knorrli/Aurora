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
// 8-channel mode — so the blocks are A001, A009, A017, A025. A row's place is
// the renderer's wash number, so the table runs in the order the four stand
// on stage, first to fourth. Channel layout is in docs/wiring.md § "Fixture profile — BeamZ BCC145". Trims
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

namespace dmx_out {

void begin() {
    dmx.begin();
}

static_assert(sizeof(fixtures) / sizeof(fixtures[0]) == render::WASHES,
              "one fixture per wash");

void tick(const render::Wash *washes) {
    // Key 0 is an override, not a look, and it reads selectedPreset for the
    // reason Aurora.ino's gate does. The washes still arrive computed; they
    // are thrown away at the write below.
    const bool blackout = (selectedPreset == PRESET_OFF);

    for (uint8_t i = 0; i < render::WASHES; i++) {
        const Fixture &fixture = fixtures[i];
        const render::Rgb rgb = washes[i].color;
        // Pull the common component out into the white channel: an RGBW
        // fixture mixing white from its color emitters is dimmer than its
        // white one and usually tinted.
        const uint8_t white = fixture.hasWhite ? min(rgb.r, min(rgb.g, rgb.b)) : 0;
        uint8_t values[8] = {
            scale8(washes[i].level, fixture.master),
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

        // Every channel, not the dimmer alone: a fixture that ignores its
        // dimmer would hold the color channels and stay lit.
        if (blackout) memset(values, 0, sizeof(values));

        dmx.set(fixture.address, values, 8);

        if (i == 0) memcpy(lastWritten, values, sizeof(lastWritten));
    }
}

const uint8_t *lastValues() {
    return lastWritten;
}

} // namespace dmx_out
