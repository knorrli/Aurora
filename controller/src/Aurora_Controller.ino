// Aurora_Controller — Nano firmware for the controller node.
//
// Architecture (see DESIGN.md):
//
//     DAW / FootCtl ──MIDI──▶  controller  ──MIDI──▶  brain
//
// The controller is a MIDI router + merger. It:
//   - Reads all local controls (keypad, faders, touchpad, switches,
//     tap-tempo, mic trigger).
//   - Receives external MIDI (DAW clock, foot-pedal PC messages).
//   - Runs a tempo tracker that locks to whichever source is live
//     (external clock takes priority; tap is fallback).
//   - Emits a unified MIDI stream on its DIN OUT to the brain.
//
// Per Option A in DESIGN.md, we do not pass external clock bytes through
// verbatim — we re-emit our own 24-PPQN clock from our internal tempo
// estimate. Start/Continue/Stop are forwarded.
//
// Pin map:    see pins.h
// Protocol:   see shared/aurora_protocol.h
// Wiring:     see docs/wiring.md

#include "pins.h"
#include "midi_io.h"
#include "tempo.h"
#include "controls.h"

void setup() {
    midi_io::begin();
    tempo::begin();
    controls::begin();
}

void loop() {
    midi_io::tick();   // pump incoming MIDI (triggers handlers in tempo + midi_io)
    tempo::tick();     // emit our clock on schedule, drive tempo LED
    controls::tick();  // scan controls, emit MIDI on change
}
