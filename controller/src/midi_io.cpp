#include "midi_io.h"

#include <MIDI.h>
#include "aurora_protocol.h"
#include "tempo.h"

// Single MIDI instance on the hardware UART. In/out share the same
// `Serial` — it's full-duplex; RX reads bytes arriving via the opto on
// D0, TX sends bytes out D1 to the DIN-OUT circuit.
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, gMIDI);

// ---------------------------------------------------------------------------
// Incoming message handlers
// ---------------------------------------------------------------------------

static void on_clock() {
    // External clock tick. Feed it into the tempo tracker — it decides
    // whether to lock to external or stick with tap.
    tempo::on_external_clock();
}

static void on_start() {
    tempo::on_external_start();
    // Forward to brain. Option A means we generate our own clock, but
    // Start/Continue/Stop still get forwarded so the brain knows about
    // transport state.
    midi_io::send_start();
}

static void on_continue() {
    tempo::on_external_continue();
    midi_io::send_continue();
}

static void on_stop() {
    tempo::on_external_stop();
    midi_io::send_stop();
}

static void on_program_change(byte, byte pc) {
    // Upstream PC (e.g. from a foot controller) — forward to brain.
    midi_io::send_program_change(pc);
}

static void on_control_change(byte, byte cc, byte value) {
    midi_io::send_control_change(cc, value);
}

static void on_note_on(byte, byte note, byte velocity) {
    midi_io::send_note_on(note, velocity);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace midi_io {

void begin() {
    // The library drops channel messages on any other channel before a
    // handler sees them; clock and transport carry no channel and still
    // arrive.
    gMIDI.begin(AURORA_MIDI_CHANNEL);

    // We need real-time messages (clock, start/continue/stop) parsed even
    // when the parser is mid-message. MIDI library does this by default for
    // real-time. Program Change / CC / Note On get dispatched via handlers.
    gMIDI.setHandleClock(on_clock);
    gMIDI.setHandleStart(on_start);
    gMIDI.setHandleContinue(on_continue);
    gMIDI.setHandleStop(on_stop);
    gMIDI.setHandleProgramChange(on_program_change);
    gMIDI.setHandleControlChange(on_control_change);
    gMIDI.setHandleNoteOn(on_note_on);

    // We disable automatic THRU — we do our own merging / re-emission
    // (Option A). If THRU was on, every incoming byte would also leave
    // on TX, which would fight our synthesized clock.
    gMIDI.turnThruOff();
}

void tick() {
    // Drain everything pending. MIDI library is non-blocking; read()
    // returns false when there's no byte to consume.
    while (gMIDI.read()) { /* handlers fire as side-effects */ }
}

// --- Outgoing wrappers -----------------------------------------------------

void send_clock()                                   { gMIDI.sendRealTime(midi::Clock); }
void send_start()                                   { gMIDI.sendRealTime(midi::Start); }
void send_continue()                                { gMIDI.sendRealTime(midi::Continue); }
void send_stop()                                    { gMIDI.sendRealTime(midi::Stop); }
void send_program_change(uint8_t pc)                { gMIDI.sendProgramChange(pc, AURORA_MIDI_CHANNEL); }
void send_control_change(uint8_t cc, uint8_t value) { gMIDI.sendControlChange(cc, value, AURORA_MIDI_CHANNEL); }
void send_note_on(uint8_t note, uint8_t velocity)   { gMIDI.sendNoteOn(note, velocity, AURORA_MIDI_CHANNEL); }
void send_note_off(uint8_t note)                    { gMIDI.sendNoteOff(note, 0, AURORA_MIDI_CHANNEL); }

} // namespace midi_io
