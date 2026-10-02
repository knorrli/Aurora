#include "midi_in.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "patch_sync.h"
#include "player.h"

static bool ours(uint8_t channel) { return channel == AURORA_MIDI_CHANNEL; }

static void handleClock() { player::get().clockTick(micros()); }
static void handleStart() { player::get().clockStart(micros()); }
static void handleContinue() { player::get().clockContinue(micros()); }
static void handleStop() { player::get().clockStop(); }

static void handleProgramChange(uint8_t channel, uint8_t program) {
    if (ours(channel)) player::get().programChange(program, micros());
}

static void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    if (ours(channel)) player::get().controlChange(control, value);
}

static void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!ours(channel)) return;
    if (velocity == 0) player::get().noteOff(note, micros());
    else player::get().noteOn(note, micros());
}

static void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (ours(channel)) player::get().noteOff(note, micros());
}

namespace midi_in {

void begin() {
    usbMIDI.setHandleClock(handleClock);
    usbMIDI.setHandleStart(handleStart);
    usbMIDI.setHandleContinue(handleContinue);
    usbMIDI.setHandleStop(handleStop);
    usbMIDI.setHandleProgramChange(handleProgramChange);
    usbMIDI.setHandleControlChange(handleControlChange);
    usbMIDI.setHandleNoteOn(handleNoteOn);
    usbMIDI.setHandleNoteOff(handleNoteOff);
    usbMIDI.setHandleSysEx(patch_sync::onSysEx);
}

void read() {
    while (usbMIDI.read()) { }
}

}
