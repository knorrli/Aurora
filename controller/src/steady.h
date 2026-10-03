#pragma once

#include <stdint.h>

class Steady {
 public:
    Steady(uint8_t agreeing, int16_t initial) : agreeing(agreeing), candidate(initial), accepted(initial) {}

    bool settle(int16_t reading) {
        if (reading != candidate) {
            candidate = reading;
            count = 0;
        }
        if (count < agreeing) count++;
        if (count < agreeing || candidate == accepted) return false;
        accepted = candidate;
        return true;
    }

    int16_t value() const { return accepted; }

 private:
    uint8_t agreeing;
    uint8_t count = 0;
    int16_t candidate;
    int16_t accepted;
};

class Every {
 public:
    explicit Every(uint32_t periodMicros) : periodMicros(periodMicros) {}

    bool due(uint32_t micros) {
        if (micros - lastMicros < periodMicros) return false;
        lastMicros = micros;
        return true;
    }

 private:
    uint32_t periodMicros;
    uint32_t lastMicros = 0;
};
