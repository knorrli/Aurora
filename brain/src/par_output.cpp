#include "par_output.h"

#include <TeensyDMX.h>
#include <string.h>

namespace par_output {

static qindesign::teensydmx::Sender dmx{Serial4};

static const uint8_t CHANNELS_PER_PAR = 8;

struct Fixture {
    uint16_t address;
    uint8_t trim[4];
    uint8_t master;
};

static const Fixture fixtures[] = {
    {  1, { 255, 255, 255, 255 }, 255 },
    {  9, { 255, 255, 255, 255 }, 255 },
    { 17, { 255, 255, 255, 255 }, 255 },
    { 25, { 255, 255, 255, 255 }, 255 },
};

static_assert(sizeof(fixtures) / sizeof(fixtures[0]) == render::PARS, "one fixture per PAR");

void begin() {
    dmx.begin();
}

void show(const render::Par *pars, bool blackout) {
    for (uint8_t i = 0; i < render::PARS; i++) {
        const Fixture &fixture = fixtures[i];
        const render::Rgb rgb = pars[i].color;
        const uint8_t white = min(rgb.r, min(rgb.g, rgb.b));
        uint8_t channels[CHANNELS_PER_PAR] = {
            render::scale8(pars[i].value, fixture.master),
            0,
            render::scale8(rgb.r - white, fixture.trim[0]),
            render::scale8(rgb.g - white, fixture.trim[1]),
            render::scale8(rgb.b - white, fixture.trim[2]),
            render::scale8(white, fixture.trim[3]),
            0,
            0,
        };
        if (blackout) memset(channels, 0, sizeof(channels));
        dmx.set(fixture.address, channels, CHANNELS_PER_PAR);
    }
}

}
