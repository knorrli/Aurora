#include "midi_in.h"

#include <Arduino.h>
#include <render.h>

#include "aurora_protocol.h"
#include "controls.h"
#include "flash.h"
#include "patch_sync.h"
#include "show.h"
#include "tempo.h"

static void handleProgramChange(uint8_t channel, uint8_t program) {
    if (channel == AURORA_MIDI_CHANNEL) show::select(program);
}

static void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    if (channel != AURORA_MIDI_CHANNEL) return;
    controls::store(control, value);
    if (control == CC_TEMPO_DIVISION) tempo::setDivision(value);
}

static void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel != AURORA_MIDI_CHANNEL || note != NOTE_TRIGGER_FLASH || velocity == 0) return;
    flash::fire(render::dialedColor(controls::all()).h);
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

void read() {
    while (usbMIDI.read()) { }
}

}
