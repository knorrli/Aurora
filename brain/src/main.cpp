#include <Arduino.h>

#include "midi_in.h"
#include "par_output.h"
#include "patch_store.h"
#include "player.h"
#include "strip_output.h"

static const float TEMPO_LED_ON_BEATS = 0.08f;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    strip_output::begin();
    par_output::begin();
    patch_store::begin();
    player::begin();
    midi_in::begin();
    strip_output::showStartupSequence();
}

void loop() {
    midi_in::read();
    const render::Frame &frame = player::get().frame(micros());
    strip_output::show(frame.pixels);
    par_output::show(frame.pars);
    const float beats = player::get().beats();
    digitalWrite(LED_BUILTIN, beats - floorf(beats) < TEMPO_LED_ON_BEATS);
}
