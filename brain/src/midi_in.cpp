#include "midi_in.h"

#include "dmx_out.h"

#include "Aurora.h"
#include "tempo.h"

static void handleProgramChange(uint8_t channel, uint8_t program) {
    (void)channel;
    if (!aurora_pc_is_preset(program)) return;

    selectedPreset = program;
    resetPreset(selectedPreset);
}

static uint8_t ccByte[128];

static void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) {
    (void)channel;
    if (control < 128) ccByte[control] = value;
    switch (control) {
        case CC_TEMPO_DIVISION:
            tempo::setDivision(value);
            break;
        case CC_HUE:
            setHueFromCC(value);
            break;
        case CC_SATURATION:
            setSaturationFromCC(value);
            break;
        case CC_VALUE:
            setValueFromCC(value);
            break;
        case CC_WASH_LEVEL:
            dmx_out::setLevel(map(value, 0, 127, 0, 255));
            break;
        case CC_WASH_HUE_OFFSET:
            dmx_out::setHueOffset(map(value, 0, 127, 0, 255));
            break;
        case CC_WASH_SATURATION:
            dmx_out::setSaturation(map(value, 0, 127, 0, 255));
            break;
        case CC_COLOR_REGION:   setColorRegion(value); break;
        case CC_COLOR_RULER:    setColorRuler(value); break;
        case CC_PLACED_HUE:      setPlacedHue(value); break;
        case CC_PLACED_WHITE:    setPlacedWhite(value); break;
        case CC_PLACED_DARK:     setPlacedDark(value); break;
        case CC_PLACED_COUNT:    setPlacedCount(value); break;
        case CC_PLACED_WIDTH:    setPlacedWidth(value); break;
        case CC_PLACED_EDGE:     setPlacedEdge(value); break;
        case CC_PLACED_SPEED:    setPlacedSpeed(value); break;
        case CC_WANDER_HUE:      setWanderHue(value); break;
        case CC_WANDER_WHITE:    setWanderWhite(value); break;
        case CC_WANDER_DARK:     setWanderDark(value); break;
        case CC_WANDER_RATE:     setWanderRate(value); break;
        case CC_WANDER_SCALE:    setWanderScale(value); break;
        case CC_LIT_HUE:         setLitHue(value); break;
        case CC_LIT_WHITE:       setLitWhite(value); break;
        case CC_LIT_DARK:        setLitDark(value); break;
        case CC_GEN_WIDTH:       setGeneratorWidth(value); break;
        case CC_GEN_COUNT:       setGeneratorCount(value); break;
        case CC_GEN_EDGE:        setGeneratorEdge(value); break;
        case CC_GEN_TAIL:        setGeneratorTail(value); break;
        case CC_GEN_POSITION:    setGeneratorPosition(value); break;
        case CC_GEN_SPEED:       setGeneratorSpeed(value); break;
        case CC_GEN_FAN:         setGeneratorFan(value); break;
        case CC_GEN_JITTER:      setGeneratorJitter(value); break;
        case CC_GEN_PULSE_RATE:  setGeneratorPulseRate(value); break;
        case CC_GEN_ALTERNATE:   setGeneratorAlternate(value); break;
        case CC_GEN_BOUNCE:      setGeneratorBounce(value); break;

        // The pulse, destination by destination. The strips' brightness kept
        // the three numbers it has always had; the rest are the 100–114
        // block, three apiece in the order amount, shape, skew.
        case CC_GEN_PULSE_DEPTH: setPulseAmount(PULSE_TO_LIGHT, value); break;
        case CC_GEN_PULSE_SHAPE: setPulseShape(PULSE_TO_LIGHT, value); break;
        case CC_GEN_PULSE_SKEW:  setPulseSkew(PULSE_TO_LIGHT, value); break;

        case CC_PULSE_WIDTH:       setPulseAmount(PULSE_TO_WIDTH, value); break;
        case CC_PULSE_WIDTH_SHAPE: setPulseShape(PULSE_TO_WIDTH, value); break;
        case CC_PULSE_WIDTH_SKEW:  setPulseSkew(PULSE_TO_WIDTH, value); break;

        case CC_PULSE_HUE:       setPulseAmount(PULSE_TO_HUE, value); break;
        case CC_PULSE_HUE_SHAPE: setPulseShape(PULSE_TO_HUE, value); break;
        case CC_PULSE_HUE_SKEW:  setPulseSkew(PULSE_TO_HUE, value); break;

        case CC_PULSE_PAR_LEVEL:       setPulseAmount(PULSE_TO_PAR_LEVEL, value); break;
        case CC_PULSE_PAR_LEVEL_SHAPE: setPulseShape(PULSE_TO_PAR_LEVEL, value); break;
        case CC_PULSE_PAR_LEVEL_SKEW:  setPulseSkew(PULSE_TO_PAR_LEVEL, value); break;

        case CC_PULSE_PAR_HUE:       setPulseAmount(PULSE_TO_PAR_HUE, value); break;
        case CC_PULSE_PAR_HUE_SHAPE: setPulseShape(PULSE_TO_PAR_HUE, value); break;
        case CC_PULSE_PAR_HUE_SKEW:  setPulseSkew(PULSE_TO_PAR_HUE, value); break;

        case CC_PULSE_PAR_SAT:       setPulseAmount(PULSE_TO_PAR_SAT, value); break;
        case CC_PULSE_PAR_SAT_SHAPE: setPulseShape(PULSE_TO_PAR_SAT, value); break;
        case CC_PULSE_PAR_SAT_SKEW:  setPulseSkew(PULSE_TO_PAR_SAT, value); break;

        case CC_MODE_FLAGS:
            faderAltModeEnabled = value & MODE_BIT_FADER_ALT;
            presetAltModeEnabled = value & MODE_BIT_PRESET_ALT;
            break;
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
}

void tick() {
    while (usbMIDI.read()) { }
}

const uint8_t *ccBytes() { return ccByte; }

} // namespace midi_in
