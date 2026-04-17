// Aurora_Controller — Nano firmware for the controller node.
//
// Job of this firmware:
//   - Scan local controls (keypad, faders, touchpad, mode switches,
//     tap-tempo button, mic trigger).
//   - Parse incoming MIDI from D0/RX: clock from DAW, PC from foot
//     controller, anything else chained upstream.
//   - Track tempo (external clock OR local tap, with priority /
//     fallback).
//   - Flash the tempo LED in sync with whichever source is live.
//   - Emit a unified MIDI stream on D1/TX to the brain per Option A in
//     DESIGN.md: we always re-emit fresh 24-PPQN clock from our
//     internal tempo, never pass external bytes verbatim.
//
// Stub — to be filled in when the split work begins. See DESIGN.md
// for the full plan.

#include <Arduino.h>
#include "aurora_protocol.h"

void setup() {
  // TODO:
  //   - Serial.begin(31250) on hardware UART (D0/D1) for DIN MIDI.
  //   - pinMode setup for keypad, faders, switches, tap button,
  //     tempo LED, mic trigger.
  //   - Initialize tempo tracker (external-pending until first clock).
}

void loop() {
  // TODO:
  //   - Scan controls, translate to MIDI PC/CC/note.
  //   - Pump incoming MIDI parser.
  //   - Update tempo: if external clock arrived this tick, track it;
  //     if tap was pressed, override; else fall back to last-known.
  //   - Emit our own 24 PPQN clock byte at the right moments.
  //   - Emit PC / CC / note as control changes happen.
  //   - Drive tempo LED on beat.
}
