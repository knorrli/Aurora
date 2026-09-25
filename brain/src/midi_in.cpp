#include "midi_in.h"

#include "destinations.h"
#include "patch_sync.h"

#include "Aurora.h"
#include "tempo.h"

static void handleProgramChange(uint8_t channel, uint8_t program) {
    if (channel != AURORA_MIDI_CHANNEL) return;
    if (!aurora_pc_is_preset(program)) return;

    selectedPreset = program;
    resetPreset(selectedPreset);
}

static void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    if (channel != AURORA_MIDI_CHANNEL) return;

    // Every control lands in the same store, whether or not anything reads it
    // yet. A renderer converts it when it draws, because a modulation route
    // pushes the byte and the conversion has to see the pushed value.
    destinations::store(control, value);

    // Tempo division is the one control that does more than be stored: it
    // re-times the clock, which is not a renderer's to read.
    if (control == CC_TEMPO_DIVISION) tempo::setDivision(value);
}

static void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel != AURORA_MIDI_CHANNEL) return;
    // Velocity 0 is a note-off in disguise; every MIDI source is entitled
    // to send one and it must not read as a trigger.
    if (note == NOTE_TRIGGER_FLASH && velocity > 0) fireTrigger();
}

namespace midi_in {

void begin() {
    usbMIDI.setHandleClock(tempo::onClock);
    usbMIDI.setHandleStart(tempo::onStart);
    usbMIDI.setHandleContinue(tempo::onContinue);
    usbMIDI.setHandleStop(tempo::onStop);
    usbMIDI.setHandleProgramChange(handleProgramChange);
    usbMIDI.setHandleControlChange(handleControlChange);
    usbMIDI.setHandleNoteOn(handleNoteOn);
    usbMIDI.setHandleSysEx(patch_sync::onSysEx);
}

void tick() {
    while (usbMIDI.read()) { }
}

} // namespace midi_in
