// tempo — tempo tracker + clock generator for the Aurora controller.
//
// Responsibilities:
//   * Track tempo from one of two sources:
//       - External MIDI clock (DAW, pedalboard) — 24 PPQN
//       - Local tap tempo button
//     Whichever is more recently active is the live source.
//   * Emit our own 24 PPQN clock to the brain at that tempo (Option A —
//     we always generate fresh clock; we never forward external bytes).
//   * Drive the tempo LED once per beat so the performer can see lock.
//
// State machine (simplified):
//
//     no-clock ──external clock arrives──▶ external
//     external ──tap pressed──────────────▶ tap
//     tap      ──external clock arrives──▶ external (back)
//     any      ──stop (no clock for N ms)─▶ free-running at last tempo

#ifndef AURORA_CONTROLLER_TEMPO_H
#define AURORA_CONTROLLER_TEMPO_H

#include <stdint.h>

namespace tempo {

enum class Source : uint8_t { None, External, Tap };

// Call once from setup().
void begin();

// Call every loop.
//  - Emits clock bytes when due.
//  - Updates tempo LED.
//  - Handles source timeout / fallback.
void tick();

// Notifications from midi_io
void on_external_clock();
void on_external_start();
void on_external_continue();
void on_external_stop();

// Notification from the tap tempo button scanner
void on_tap();

// --- Queries ----------------------------------------------------------------

Source current_source();
uint16_t bpm();                // 0 if unknown
bool     running();            // are we currently emitting clock?

} // namespace tempo

#endif // AURORA_CONTROLLER_TEMPO_H
