#include "midi_link.h"

#include <Arduino.h>
#include <MIDI.h>

#include "aurora_protocol.h"
#include "recall.h"
#include "songs.h"
#include "tempo.h"

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, din);

namespace {

bool isChannelMessage(midi::MidiType type) {
    return type >= midi::NoteOff && type <= midi::PitchBend;
}

void forward() {
    din.send(din.getType(), din.getData1(), din.getData2(), din.getChannel());
}

void handle(uint32_t micros) {
    const midi::MidiType type = din.getType();
    switch (type) {
        case midi::Clock: tempo::incomingTick(micros); return;
        case midi::Start: tempo::incomingStart(micros); return;
        case midi::Continue: tempo::incomingContinue(); return;
        case midi::Stop: tempo::incomingStop(); return;
        default: break;
    }
    if (!isChannelMessage(type)) return;
    if (type == midi::ProgramChange && din.getChannel() == AURORA_MIDI_CHANNEL) {
        recall::forwardProgram(din.getData1());
        return;
    }
    forward();
}

}

namespace midi_link {

void begin() {
    din.begin(MIDI_CHANNEL_OMNI);
    din.turnThruOff();
    usbMIDI.setHandleSysEx(songs::onSysEx);
}

void read(uint32_t micros) {
    while (din.read()) handle(micros);
    while (usbMIDI.read()) { }
}

void sendNoteOn(uint8_t note, uint8_t velocity) { din.sendNoteOn(note, velocity, AURORA_MIDI_CHANNEL); }
void sendNoteOff(uint8_t note) { din.sendNoteOff(note, 0, AURORA_MIDI_CHANNEL); }
void sendProgram(uint8_t program) { din.sendProgramChange(program, AURORA_MIDI_CHANNEL); }
void sendControl(uint8_t control, uint8_t value) { din.sendControlChange(control, value, AURORA_MIDI_CHANNEL); }
void sendClock() { din.sendClock(); }
void sendStart() { din.sendStart(); }
void sendContinue() { din.sendContinue(); }
void sendStop() { din.sendStop(); }

}
