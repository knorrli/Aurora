#include "midi_in.h"

#include "destinations.h"
#include "patch_sync.h"

#include "Aurora.h"
#include "tempo.h"

static void handleProgramChange(uint8_t channel, uint8_t program) {
    (void)channel;
    if (!aurora_pc_is_preset(program)) return;

    selectedPreset = program;
    resetPreset(selectedPreset);
}

static void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    (void)channel;

    // Every control lands in the same store, whether or not anything reads it
    // yet. A renderer converts it when it draws, because a modulation route
    // pushes the byte and the conversion has to see the pushed value.
    destinations::store(control, value);

    // What is left below is the controls that do more than be stored: the
    // transport, the switches, which have no middle for a push to land in,
    // and the pulse's sends, which are on their way out with the route block.
    switch (control) {
        case CC_TEMPO_DIVISION:
            tempo::setDivision(value);
            break;
        case CC_COLOR_REGION:  setColorRegion(value); break;
        case CC_COLOR_RULER:   setColorRuler(value); break;
        case CC_GEN_ALTERNATE: setGeneratorAlternate(value); break;
        case CC_GEN_BOUNCE:    setGeneratorBounce(value); break;

        // The pulse, destination by destination. Each send is an amount and
        // a wave; the skew numbers beside them in the 101–115 block are dead
        // now that a wave is one byte, and leave with the block.
        case CC_GEN_PULSE_DEPTH: setPulseAmount(PULSE_TO_LIGHT, value); break;
        case CC_GEN_PULSE_SHAPE: setPulseWave(PULSE_TO_LIGHT, value); break;

        case CC_PULSE_WIDTH:       setPulseAmount(PULSE_TO_WIDTH, value); break;
        case CC_PULSE_WIDTH_SHAPE: setPulseWave(PULSE_TO_WIDTH, value); break;

        case CC_PULSE_HUE:       setPulseAmount(PULSE_TO_HUE, value); break;
        case CC_PULSE_HUE_SHAPE: setPulseWave(PULSE_TO_HUE, value); break;

        case CC_PULSE_PAR_LEVEL:       setPulseAmount(PULSE_TO_PAR_LEVEL, value); break;
        case CC_PULSE_PAR_LEVEL_SHAPE: setPulseWave(PULSE_TO_PAR_LEVEL, value); break;

        case CC_PULSE_PAR_HUE:       setPulseAmount(PULSE_TO_PAR_HUE, value); break;
        case CC_PULSE_PAR_HUE_SHAPE: setPulseWave(PULSE_TO_PAR_HUE, value); break;

        case CC_PULSE_PAR_SAT:       setPulseAmount(PULSE_TO_PAR_SAT, value); break;
        case CC_PULSE_PAR_SAT_SHAPE: setPulseWave(PULSE_TO_PAR_SAT, value); break;
    }
}

static void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
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
