// midi_io — MIDI I/O for the Aurora controller.
//
// Wraps the Arduino MIDI Library (forty-seven effects). Responsibilities:
//
//   * Initialize the hardware UART for MIDI.
//   * Receive clock / PC / CC / note from upstream (DAW, foot controller)
//     and dispatch to handlers in other modules.
//   * Send clock / PC / CC / note to the brain.
//
// The library itself handles the 3-byte parser, running status, etc. We
// install callbacks to bridge incoming events to the rest of the firmware.

#ifndef AURORA_CONTROLLER_MIDI_IO_H
#define AURORA_CONTROLLER_MIDI_IO_H

#include <Arduino.h>
#include <stdint.h>

namespace midi_io {

// Call once from setup(). Initializes the UART at MIDI speed and registers
// incoming-message handlers.
void begin();

// Call every loop. Pumps the incoming MIDI parser. Returns the number of
// bytes consumed (useful for frame budgeting if we ever care).
void tick();

// --- Outgoing messages to the brain ----------------------------------------

void send_clock();                    // 0xF8
void send_start();                    // 0xFA
void send_continue();                 // 0xFB
void send_stop();                     // 0xFC
void send_program_change(uint8_t pc);
void send_control_change(uint8_t cc, uint8_t value);
void send_note_on(uint8_t note, uint8_t velocity);
void send_note_off(uint8_t note);

} // namespace midi_io

#endif // AURORA_CONTROLLER_MIDI_IO_H
