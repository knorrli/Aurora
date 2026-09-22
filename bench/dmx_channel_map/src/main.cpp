// Walks channels 1-4 one at a time so the fixture's channel layout can
// be read by eye, no laptop needed.
//
// Each channel is announced by that many blinks of the onboard LED with
// the fixture dark, then held lit for a few seconds. One blink then red
// means channel 1 is red, and so on.
//
// If every channel stays dark on its own, the block starts with a master
// dimmer instead: nothing lights until that channel is up too.

#include <Arduino.h>
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

teensydmx::Sender dmx{Serial4};

constexpr uint16_t kFirstChannel = 1;
constexpr uint16_t kChannelCount = 4;
constexpr uint32_t kHoldMs = 2500;

// Well below full: at 255 the PAR is too bright to judge color by eye.
constexpr uint8_t kLevel = 100;

static void announce(uint8_t blinks) {
    for (uint8_t i = 0; i < blinks; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN, LOW);
        delay(250);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    dmx.begin();
}

void loop() {
    for (uint16_t i = 0; i < kChannelCount; i++) {
        const uint16_t channel = kFirstChannel + i;

        announce(i + 1);
        Serial.printf("channel %u -> %u\n", channel, kLevel);

        dmx.set(channel, kLevel);
        delay(kHoldMs);
        dmx.set(channel, 0);
        delay(600);
    }

    delay(1500);
}
