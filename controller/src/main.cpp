#include <Arduino.h>

#include "faders.h"
#include "indicators.h"
#include "keypad.h"
#include "mic.h"
#include "midi_link.h"
#include "pad.h"
#include "pedal.h"
#include "rotary.h"
#include "songs.h"
#include "tempo.h"

void setup() {
    analogReadResolution(12);
    midi_link::begin();
    songs::begin();
    rotary::begin();
    keypad::begin();
    faders::begin();
    pad::begin();
    pedal::begin();
    mic::begin();
    indicators::begin();
    tempo::begin(micros());
    faders::sendAll();
    pad::sendAll();
}

void loop() {
    const uint32_t now = micros();
    midi_link::read(now);
    tempo::update(now);
    mic::update(now);
    keypad::update(now);
    rotary::update(now);
    pedal::update(now);
    faders::update(now);
    pad::update(now);
    indicators::update(now);
}
