#include <Arduino.h>
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

teensydmx::Sender dmx{Serial4};

constexpr uint16_t kFirstChannel = 1;
constexpr uint16_t kChannelCount = 8;
constexpr uint32_t kBlinkIntervalMs = 1000;

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
