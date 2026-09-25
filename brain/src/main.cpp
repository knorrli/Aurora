#include <Arduino.h>

#include "controls.h"
#include "flash.h"
#include "midi_in.h"
#include "par_output.h"
#include "patch_store.h"
#include "show.h"
#include "strip_output.h"
#include "tempo.h"

static const uint32_t TEMPO_LED_ON_MS = 40;

static uint32_t lastBeatMs = 0;

static void showTempoLed() {
    const uint32_t now = millis();
    if (tempo::beatStarted()) lastBeatMs = now;
    digitalWrite(LED_BUILTIN, now - lastBeatMs < TEMPO_LED_ON_MS);
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    strip_output::begin();
    par_output::begin();
    controls::begin();
    patch_store::begin();
    midi_in::begin();
    tempo::begin();
    strip_output::showStartupSequence();
}

void loop() {
    midi_in::read();
    tempo::advance();
    show::advance(tempo::beatStarted(), tempo::running());

    const render::Frame &frame = show::render(tempo::quarterNotes());
    render::Rgb pixels[render::STRIPS * render::PIXELS];
    memcpy(pixels, frame.pixels, sizeof(pixels));
    flash::drawOver(pixels);

    strip_output::show(pixels, show::blackout());
    par_output::show(frame.pars, show::blackout());
    showTempoLed();
}
