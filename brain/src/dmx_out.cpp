#include "dmx_out.h"

#include "Aurora.h"
#include "routes.h"
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

static float pulseLevel = 0.0f;
static float pulseHue = 0.0f;
static float pulseSaturation = 0.0f;

// A push is a fraction of the way from the dialed value toward one of its
// limits, and its sign picks which — so a wash at full has nowhere to go up
// and everything stays inside the channel either way. Same rule the
// generator pushes width and the strips' color by.
static uint8_t pushToward(uint8_t base, float push, uint8_t low, uint8_t high) {
    const float limit = (push >= 0.0f) ? (float)high : (float)low;
    const float amount = (push < 0.0f) ? -push : push;
    return (uint8_t)((float)base + (amount > 1.0f ? 1.0f : amount) * (limit - (float)base));
}

namespace dmx_out {

void begin() {
    dmx.begin();
}

void tick() {
    // The washes' own three take their routes; presetColor below does not,
    // because a push reaches one fixture family and CC 38-40 are the strips'.
    const uint8_t washLevel = map(routes::value(CC_WASH_LEVEL), 0, 127, 0, 255);
    const uint8_t washHueOffset =
        map(routes::value(CC_WASH_HUE_OFFSET), 0, 127, 0, 255);
    const uint8_t washSaturation =
        map(routes::value(CC_WASH_SATURATION), 0, 127, 0, 255);

    // Color is converted at full value and brightness is carried by the
    // fixture's own dimmer, so the emitters stay near full scale where
    // they have the most resolution. Scaling RGBW down instead — the only
    // option the 4-channel personality offers — bands on slow fades at
    // the low levels the washes normally sit at.
    const uint8_t master = pushToward(washLevel, pulseLevel, 0, 255);

    // Key 0 is an override, not a look, and it reads selectedPreset for the
    // reason Aurora.ino's gate does. Nothing above is skipped: the frame is
    // computed and then thrown away at the write below.
    const bool blackout = (selectedPreset == PRESET_OFF);

    CHSV hsv = presetColor;
    const uint8_t level = scale8(hsv.value, master);
    hsv.value = 255;
    hsv.hue += washHueOffset + (int16_t)pulseHue;
    // A scale down rather than a setting: the washes are a relationship to
    // the strips, and this is where the pulse's push measures from.
    hsv.saturation = pushToward(scale8(hsv.saturation, washSaturation),
                                pulseSaturation, 0, 255);

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

        // Every channel, not the dimmer alone: a fixture that ignores its
        // dimmer would hold the color channels and stay lit.
        if (blackout) memset(values, 0, sizeof(values));

        dmx.set(fixture.address, values, 8);

        if (i == 0) memcpy(lastWritten, values, sizeof(lastWritten));
    }

    // Consumed, not held. Only the generator writes a push, and only while
    // it is the preset being drawn; clearing here is what stops the last
    // frame it drew from following the washes into the next preset.
    pulseLevel = 0.0f;
    pulseHue = 0.0f;
    pulseSaturation = 0.0f;
}

void setPulsePush(float level, float hueOffset, float saturation) {
    pulseLevel = level;
    pulseHue = hueOffset;
    pulseSaturation = saturation;
}

const uint8_t *lastValues() {
    return lastWritten;
}

} // namespace dmx_out
