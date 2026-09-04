// Blinks a DMX fixture at address 1 on and off once a second, with the
// Teensy's onboard LED mirroring the DMX state.
//
// Reading the result:
//   PAR blinks in time with the onboard LED — the whole path works.
//   LED blinks, PAR dark  — wiring, fixture address, or the fixture is
//                           still in auto / sound-active mode.
//   LED dark              — the sketch is not running; upload problem.

#include <Arduino.h>
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

// Serial4 (TX = pin 17) because Serial2, Serial3 and Serial5 all have
// their TX pin inside the OctoWS2811 reservation. See docs/wiring.md.
teensydmx::Sender dmx{Serial4};

constexpr uint16_t kFirstChannel = 1;
constexpr uint16_t kChannelCount = 8;
constexpr uint32_t kBlinkIntervalMs = 1000;

// The fixture's channel map is still unknown, so drive a whole block at
// once: whichever channel turns out to be the dimmer is in here.
static void writeBlock(uint8_t value) {
    for (uint16_t i = 0; i < kChannelCount; i++) {
        dmx.set(kFirstChannel + i, value);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    dmx.begin();
}

void loop() {
    static bool on = false;
    on = !on;

    writeBlock(on ? 255 : 0);
    digitalWrite(LED_BUILTIN, on);

    delay(kBlinkIntervalMs);
}
